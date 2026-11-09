"""Native cover CLI on independently encoded, disposable artwork only."""
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

from cover_fixtures import INDEX, bmp, library
from covertool_support import CoverToolBuild

ORIGINAL = b'Existing output must survive a failed operation.'


def name(serial='GAME', hashed=0x12345678):
    return f'cover_{serial}~{hashed:08X}.bmp'


class CoverToolTests(CoverToolBuild):
    def invoke(self, root, *args, ok=True, binary=None, env=None):
        result = subprocess.run([str(binary or self.binary), *map(str, args)], cwd=root,
                                capture_output=True, timeout=60,
                                env={'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1', **(env or {})})
        self.results.append(dict(args=list(map(str, args)), returncode=result.returncode,
                                 stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace')))
        self.assertNotIn(b'AddressSanitizer', result.stderr)
        self.assertNotIn(b'runtime error:', result.stderr)
        self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)
        self.assertEqual(list(root.glob('.saroo-cover-*')), [])
        return result

    def pack_one(self, root, data, filename=None):
        filename = filename or name()
        (root / filename).write_bytes(data)
        (root / 'input.txt').write_text(filename + '\n')
        return filename

    def test_bitmap_rows_orientation_headers_and_complete_payload(self):
        for w, h, dib, top, gap in [(128, 128, 40, False, 0), (128, 192, 124, False, 0),
                                    (1, 1, 40, False, 0), (3, 7, 52, True, 5),
                                    (127, 97, 56, False, 16), (17, 3, 108, True, 0),
                                    (128, 192, 124, False, 200)]:
            with self.subTest(w=w, h=h, dib=dib), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);data, palette, pixels = bmp(w, h, dib=dib, top=top, gap=gap)
                self.pack_one(root, data);self.invoke(root, '-p', 'input.txt')
                self.assertEqual((root / 'cover.bin').read_bytes(), library([(b'GAME', 0x12345678, w, h, palette, pixels)]))
                self.invoke(root, '-u', 'cover.bin')
                exported = (root / name()).read_bytes()
                stride = (w + 3) & ~3
                self.assertEqual(len(exported), 1078 + stride * h)
                self.assertEqual(exported[54:1078], palette)
                for y in range(h):
                    row = exported[1078 + y * stride:1078 + (y + 1) * stride]
                    self.assertEqual(row[:w], pixels[(h - y - 1) * w:(h - y) * w])
                    self.assertEqual(row[w:], bytes(stride - w))
                before = (root / 'cover.bin').read_bytes()
                self.invoke(root, '-p', 'coverlist.txt')
                self.assertEqual((root / 'cover.bin').read_bytes(), before)

    def test_deduplication_checks_palette_dimensions_and_hash_collisions(self):
        palette = bytes([90]) * 1024
        a = bytes([100]) * 16
        b = bytes([101, 98, 101]) + a[3:]
        self.assertEqual(zlib.adler32(a), zlib.adler32(b))
        palette_collision = bytes([91, 88, 91]) + palette[3:]
        self.assertEqual(zlib.adler32(palette), zlib.adler32(palette_collision))
        short_a, short_b = bytes([100, 100, 100]), bytes([0, 100, 99, 101])
        self.assertEqual(zlib.adler32(short_a), zlib.adler32(short_b))
        variants = [(4, 4, palette, a), (4, 4, bytes([91]) * 1024, a),
                    (4, 4, palette, b), (4, 4, palette_collision, a),
                    (8, 2, palette, a), (4, 4, palette, a),
                    (3, 1, palette, short_a), (4, 1, palette, short_b)]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);names = []
            for i, (w, h, pal, pixels) in enumerate(variants):
                filename = name(f'G{i}')
                (root / filename).write_bytes(bmp(w, h, palette=pal, pixels=pixels)[0]);names.append(filename)
            (root / 'input.txt').write_text('\n'.join(names) + '\n')
            self.invoke(root, '-p', 'input.txt');blob = (root / 'cover.bin').read_bytes()
            offsets = [struct.unpack_from('<I', blob, i * 32 + 16)[0] for i in range(len(variants))]
            self.assertEqual(len(set(offsets[:4])), 4)
            self.assertEqual(offsets[0], offsets[4])
            self.assertEqual(offsets[0], offsets[5])
            self.assertNotEqual(offsets[6], offsets[7])
            for offset, (w, h, pal, pixels) in zip(offsets, variants):
                self.assertEqual(blob[offset:offset + 1024 + w * h], pal + pixels)

    def test_malformed_bitmaps_preserve_previous_packed_file(self):
        good = bmp()[0]
        cases = [b'', good[:53], good[:1077], good[:-1]]
        for offset, value, fmt in [(0, 0, '<H'), (14, 64, '<I'), (18, 0, '<I'),
                                    (18, 129, '<I'), (18, 0xffffffff, '<I'),
                                    (22, 0, '<I'), (22, 193, '<I'), (22, 0x80000000, '<I'),
                                    (26, 0, '<H'), (28, 24, '<H'), (30, 1, '<I'),
                                    (46, 128, '<I'), (10, 54, '<I'), (10, 0xffffffff, '<I'),
                                    (2, len(good) + 1, '<I'), (2, 0, '<I')]:
            bad = bytearray(good);struct.pack_into(fmt, bad, offset, value);cases.append(bytes(bad))
        for i, data in enumerate(cases):
            with self.subTest(case=i), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);self.pack_one(root, data);(root / 'cover.bin').write_bytes(ORIGINAL)
                self.invoke(root, '-p', 'input.txt', ok=False)
                self.assertEqual((root / 'cover.bin').read_bytes(), ORIGINAL)

    def test_list_errors_and_duplicate_identities_preserve_packed_file(self):
        valid = name()
        cases = ['', '\n', 'missing.bmp\n', valid + '\nmissing.bmp\n',
                 valid + '\n' + valid + '\n', 'x' * 4096 + '\n', valid + '\0ignored\n',
                 'wrong-name.bmp\n', 'cover_ABCDEFGHIJKLM~12345678.bmp\n',
                 'cover_GAME~12.bmp\n', 'cover_%00~12345678.bmp\n', 'cover_%GG~12345678.bmp\n']
        for i, content in enumerate(cases):
            with self.subTest(case=i), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);self.pack_one(root, bmp()[0]);(root / 'cover.bin').write_bytes(ORIGINAL)
                # Create malformed identity files, so parsing is tested after open.
                first = content.split('\n')[0]
                if first and len(first) < 200 and '\0' not in first:
                    (root / first).write_bytes(bmp()[0])
                (root / 'input.txt').write_bytes(content.encode())
                self.invoke(root, '-p', 'input.txt', ok=(i == 0))
                if i:
                    self.assertEqual((root / 'cover.bin').read_bytes(), ORIGINAL)

    def test_invalid_cover_headers_rejected_before_extraction(self):
        _, pal, pixels = bmp(4, 4)
        good = library([(b'GAME', 0x12345678, 4, 4, pal, pixels)])
        cases = [good[:INDEX - 1], good[:-1]]
        for offset, value, fmt in [(16, INDEX - 1, '<I'), (16, 0xffffffff, '<I'),
                                   (20, 0, '<H'), (20, 129, '<H'), (22, 0, '<H'), (22, 193, '<H')]:
            bad = bytearray(good);struct.pack_into(fmt, bad, offset, value);cases.append(bytes(bad))
        duplicate = bytearray(good);duplicate[32:64] = duplicate[:32];cases.append(bytes(duplicate))
        for i, blob in enumerate(cases):
            with self.subTest(case=i), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);(root / 'input.bin').write_bytes(blob)
                (root / 'coverlist.txt').write_bytes(ORIGINAL);(root / name()).write_bytes(ORIGINAL)
                self.invoke(root, '-u', 'input.bin', ok=False)
                self.assertEqual((root / 'coverlist.txt').read_bytes(), ORIGINAL)
                self.assertEqual((root / name()).read_bytes(), ORIGINAL)

    def test_full_serial_fields_and_encoded_names_round_trip(self):
        _, pal, pixels = bmp(4, 4)
        for serial in [b'ABCDEFGHIJKL', b'../X', b'X\\Y%~', bytes([0xff, 0x81])]:
            with self.subTest(serial=serial), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);blob = library([(serial, 0xabcdef01, 4, 4, pal, pixels)])
                (root / 'input.bin').write_bytes(blob);self.invoke(root, '-u', 'input.bin')
                filename = (root / 'coverlist.txt').read_text().strip()
                self.assertNotIn('/', filename);self.assertNotIn('\\', filename)
                self.assertTrue((root / filename).is_file())
                self.invoke(root, '-p', 'coverlist.txt')
                self.assertEqual((root / 'cover.bin').read_bytes(), blob)

    def test_output_links_special_files_input_alias_and_permissions(self):
        for kind in ['link', 'fifo', 'directory', 'input-alias', 'mode']:
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);self.pack_one(root, bmp()[0]);target = root / 'cover.bin'
                (root / 'victim').write_bytes(ORIGINAL)
                if kind == 'link': target.symlink_to('victim')
                elif kind == 'fifo': os.mkfifo(target)
                elif kind == 'directory': target.mkdir()
                elif kind == 'input-alias': os.link(root / 'input.txt', target)
                else: target.write_bytes(ORIGINAL);target.chmod(0o640)
                self.invoke(root, '-p', 'input.txt', ok=(kind == 'mode'))
                self.assertEqual((root / 'victim').read_bytes(), ORIGINAL)
                if kind == 'mode': self.assertEqual(target.stat().st_mode & 0o777, 0o640)
                if kind == 'input-alias': self.assertEqual(target.read_text(), (root / 'input.txt').read_text())

    def native_metadata(self, mode, flags, protected):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            data, palette, pixels = bmp(4, 4)
            if mode == 'pack':
                (root / 'empty.txt').write_text('')
                args = ['-p', 'empty.txt']; target = root / 'cover.bin'; expected = library([])
            else:
                source = library([(b'GAME', 0x12345678, 4, 4, palette, pixels)])
                (root / 'input.bin').write_bytes(source)
                args = ['-u', 'input.bin']
                target = root / (name() if mode == 'image' else 'coverlist.txt')
                expected = data if mode == 'image' else (name() + '\n').encode()
            target.write_bytes(ORIGINAL); target.chmod(0o640)
            # This macOS Python lacks os.setxattr/getxattr. Use the installed
            # native xattr utility for real filesystem metadata, without mocks.
            subprocess.run(['/usr/bin/xattr', '-w', 'com.saroo.spec-test',
                            'preserved metadata', str(target)],
                           check=True, capture_output=True, timeout=5)
            before = target.stat()
            try:
                os.chflags(target, flags)
                result = subprocess.run([str(self.binary), *args], cwd=root,
                    capture_output=True, timeout=15,
                    env={'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1'})
                after = target.stat()
                temporary = [dict(name=p.name, bytes=p.stat().st_size, flags=p.stat().st_flags)
                             for p in root.glob('.saroo-cover-*')]
                self.results.append(dict(case='native-metadata', mode=mode, flags=flags,
                    protected=protected, returncode=result.returncode,
                    stdout=result.stdout.decode(errors='replace'),
                    stderr=result.stderr.decode(errors='replace'), temporary=temporary,
                    output_flags=after.st_flags, output_bytes=after.st_size,
                    original_preserved=target.read_bytes()==ORIGINAL))
                self.assertNotIn(b'AddressSanitizer', result.stderr)
                self.assertNotIn(b'runtime error:', result.stderr)
                self.assertEqual(result.returncode==0, not protected, result.stderr)
                self.assertEqual(target.read_bytes(), ORIGINAL if protected else expected)
                self.assertEqual(after.st_flags, flags)
                self.assertEqual((after.st_uid, after.st_gid, stat.S_IMODE(after.st_mode)),
                                 (before.st_uid, before.st_gid, stat.S_IMODE(before.st_mode)))
                attribute = subprocess.check_output(['/usr/bin/xattr', '-p', '-x',
                    'com.saroo.spec-test', str(target)], timeout=5)
                self.assertEqual(bytes.fromhex(attribute.decode()), b'preserved metadata')
                if mode != 'pack': self.assertEqual((root / 'input.bin').read_bytes(), source)
                self.assertEqual(temporary, [])
            finally:
                # Only disposable files created in this fixture are unprotected,
                # after recording the tool's result and original flags/content.
                for path in root.iterdir():
                    if path.stat().st_flags: os.chflags(path, 0)

    @unittest.skipUnless(sys.platform == 'darwin', 'Native macOS file flags required')
    def test_protected_native_metadata_refuses_without_abandoned_staging(self):
        for mode in ('pack', 'image', 'manifest'):
            for flags in (stat.UF_IMMUTABLE, stat.UF_APPEND, stat.UF_IMMUTABLE | stat.UF_APPEND):
                with self.subTest(mode=mode, flags=flags): self.native_metadata(mode, flags, True)

    @unittest.skipUnless(sys.platform == 'darwin', 'Native macOS metadata required')
    def test_ordinary_native_metadata_survives_complete_publication(self):
        for mode in ('pack', 'image', 'manifest'):
            for flags in (0, stat.UF_NODUMP):
                with self.subTest(mode=mode, flags=flags): self.native_metadata(mode, flags, False)

    def test_export_names_preserve_case_distinct_ids_on_case_insensitive_disks(self):
        _, pal, pixels = bmp(4, 4)
        blob = library([(b'A', 0x12345678, 4, 4, pal, pixels),
                        (b'a', 0x12345678, 4, 4, pal, pixels)])
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);(root / 'input.bin').write_bytes(blob)
            self.invoke(root, '-u', 'input.bin')
            names = (root / 'coverlist.txt').read_text().splitlines()
            self.assertEqual(len({filename.casefold() for filename in names}), 2)
            self.assertEqual(len(list(root.glob('*.bmp'))), 2)
            self.invoke(root, '-p', 'coverlist.txt')
            result = (root / 'cover.bin').read_bytes()
            self.assertEqual(result[:16], blob[:16])
            self.assertEqual(result[32:48], blob[32:48])

    def test_unpack_never_replaces_its_input(self):
        _, pal, pixels = bmp(4, 4);blob = library([(b'GAME', 0x12345678, 4, 4, pal, pixels)])
        for filename in ['coverlist.txt', name()]:
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);(root / filename).write_bytes(blob)
                self.invoke(root, '-u', filename, ok=False)
                self.assertEqual((root / filename).read_bytes(), blob)

    def test_disc_hash_is_independent_and_errors_return_failure(self):
        for raw in (False, True):
            with self.subTest(raw=raw), tempfile.TemporaryDirectory() as temp:
                root = Path(temp);ip = bytearray(256)
                ip[:16] = b'SEGA SEGASATURN ';ip[32:48] = b'T-12345H V1.000 '
                ip[48:112] = bytes(range(64))
                (root / 'disc.data').write_bytes((bytes(16) if raw else b'') + ip)
                out = self.invoke(root, '-i', 'disc.data').stdout
                self.assertIn(f'{zlib.adler32(bytes(range(64))):08X}'.encode(), out)
                self.assertIn(b'cover_T-12345H~', out)
        for data in [b'', b'X' * 255, b'X' * 512]:
            with tempfile.TemporaryDirectory() as temp:
                root = Path(temp);(root / 'bad').write_bytes(data)
                self.invoke(root, '-i', 'bad', ok=False)

    def test_full_index_and_capacity_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);source = root / 'template';source.write_bytes(bmp(1, 1)[0])
            names = []
            for i in range(16384):
                filename = name(f'G{i:05d}');os.link(source, root / filename);names.append(filename)
            (root / 'input.txt').write_text('\n'.join(names) + '\n')
            self.invoke(root, '-p', 'input.txt')
            result = (root / 'cover.bin').read_bytes()
            self.assertEqual(len(result), INDEX + 1025)
            self.assertEqual(result[(16384 - 1) * 32:(16384 - 1) * 32 + 6], b'G16383')
            with (root / 'input.txt').open('a') as out: out.write(names[0] + '\n')
            self.invoke(root, '-p', 'input.txt', ok=False)
            self.assertEqual((root / 'cover.bin').read_bytes(), result)


if __name__ == '__main__':
    unittest.main()
