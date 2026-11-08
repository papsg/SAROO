"""Plain and odd-byte Saturn backup fixtures with fragmented allocation tables."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib

from savetool_support import NativeSaveTest, BUILD, ROOT
from save_fixtures import saturn_fixture, disk_bytes, raw_save
import shlex



class SaturnBackupTests(NativeSaveTest):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.reader_build = tempfile.TemporaryDirectory(prefix="saturn-reader-", dir=BUILD)
        cls.addClassCleanup(cls.reader_build.cleanup)
        cls.reader = Path(cls.reader_build.name) / "read-exact"
        common = Path(cls.reader_build.name) / "main.o"
        flags = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-O1", "-g",
                 "-fno-strict-aliasing", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        commands = [[*flags, "-Dmain=upstream_cli_main", "-c", str(cls.source / "main.c"), "-o", str(common)],
                    [*flags, "-I", str(cls.source), str(ROOT / "tests/savetool_saturn_read.c"),
                     str(common), str(cls.source / "sr_bup.c"), str(cls.source / "sr_mems.c"), "-o", str(cls.reader)]]
        with (BUILD / "SaturnBackupTests.reader.compile.log").open("wb") as log:
            for command in commands:
                result = subprocess.run(command, capture_output=True, timeout=60)
                log.write(result.stdout + result.stderr)
                result.check_returncode()

    def invoke(self, directory, *args):
        result = subprocess.run([str(self.binary), 'archive.bin', *map(str, args)],
                                cwd=directory, capture_output=True, timeout=3,
                                env={'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'})
        self.assertNotIn(b'runtime error:', result.stderr)
        self.assertNotIn(b'AddressSanitizer', result.stderr)
        return result

    def test_export_payload_and_metadata_across_table_boundaries(self):
        sizes = (0, 1, 2, 3, 17, 27, 28, 29, 30, 31, 86, 87, 144, 145,
                 782, 783, 840, 841, 898, 899, 2580, 2581, 29548, 29549, 29550)
        for size in sizes:
            data = bytes((i * 13 + 17) % 251 for i in range(size))
            image, _ = saturn_fixture([(b'SAVE_A00001', data)])
            for odd in (False, True):
                for switch, extension in (('-s', '.SRO'), ('-x', '.BUP')):
                    with self.subTest(size=size, odd=odd, output=extension), self.case() as temp:
                        directory = Path(temp)
                        archive = directory / 'archive.bin'
                        source = disk_bytes(image, odd)
                        archive.write_bytes(source)
                        result = self.invoke(directory, switch, 0)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        exported = (directory / ('SAVE_A00001' + extension)).read_bytes()
                        self.assertEqual(len(exported), 64 + size)
                        self.assertEqual(hashlib.sha256(exported[64:]).digest(), hashlib.sha256(data).digest())
                        if extension == '.SRO':
                            self.assertEqual(struct.unpack_from('>I', exported, 28)[0], size)
                            self.assertEqual(exported[32:42], b'Generated!')
                            self.assertEqual(exported[43], 1)
                            self.assertEqual(exported[44:48], b'\x01\x02\x03\x04')
                            self.assertEqual(struct.unpack_from('>I', exported, 12)[0], zlib.crc32(exported[16:]))
                        else:
                            self.assertEqual(exported[:4], b'Vmem')
                            self.assertEqual(struct.unpack_from('>I', exported, 44)[0], size)
                            self.assertEqual(exported[28:38], b'Generated!')
                            self.assertEqual(exported[39], 1)
                            self.assertEqual(exported[40:44], b'\x01\x02\x03\x04')
                        self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(source).digest())

    def test_list_and_export_distinct_fragmented_saves(self):
        entries = [(b'SAVE_A00001', b'first' * 401), (b'SAVE_B00002', b'second' * 317)]
        image, _ = saturn_fixture(entries)
        for odd in (False, True):
            with self.subTest(odd=odd), self.case() as temp:
                directory = Path(temp)
                (directory / 'archive.bin').write_bytes(disk_bytes(image, odd))
                listed = self.invoke(directory)
                self.assertEqual(listed.returncode, 0, listed.stdout + listed.stderr)
                self.assertIn(b'Save_0: SAVE_A00001\n', listed.stdout)
                self.assertIn(b'Save_1: SAVE_B00002\n', listed.stdout)
                for index, (name, data) in enumerate(entries):
                    self.assertEqual(self.invoke(directory, '-s', index).returncode, 0)
                    self.assertEqual((directory / (name.decode() + '.SRO')).read_bytes()[64:], data)

    def test_corrupt_chains_are_rejected_before_listing_or_export(self):
        original, locations = saturn_fixture([(b'SAVE_A00001', b'A' * 2048),
                                              (b'SAVE_B00002', b'B' * 2048)])
        first, second = locations
        cases = {}

        def change(name, offset, value):
            image = bytearray(original)
            image[offset:offset + len(value)] = value
            cases[name] = image

        for name, block in [('reserved', 1), ('outside', 512), ('high', 65535),
                            ('self', first['blocks'][0]), ('other-header', second['blocks'][0]),
                            ('shared-data', second['blocks'][1])]:
            change(name, first['table'][0], struct.pack('>H', block))
        change('duplicate', first['table'][1], struct.pack('>H', first['blocks'][1]))
        change('early-terminator', first['table'][0], bytes(2))
        change('no-terminator', first['table'][-1], struct.pack('>H', 1))
        change('empty-name', first['header'] + 4, b'\0')
        for size in (32769, 0xffffffff):
            change(f'oversized-{size}', first['header'] + 30, struct.pack('>I', size))
        capacity = len(first['blocks']) * 58 - 30
        change('payload-exceeds-chain', first['header'] + 30, struct.pack('>I', capacity + 1))
        for reason, image in cases.items():
            for odd in (False, True):
                for args in ([], ['-s', '0'], ['-x', '0']):
                    with self.subTest(reason=reason, odd=odd, args=args), self.case() as temp:
                        directory = Path(temp)
                        archive = directory / 'archive.bin'
                        source = disk_bytes(image, odd)
                        archive.write_bytes(source)
                        result = self.invoke(directory, *args)
                        self.assertNotEqual(result.returncode, 0, result.stdout)
                        self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(source).digest())
                        self.assertEqual({p.name for p in directory.iterdir()}, {'archive.bin'})

    def test_unsupported_extents_and_false_odd_signature_are_rejected(self):
        image, _ = saturn_fixture([])
        cases = [image + bytes(1), image + bytes(32768), disk_bytes(image, True) + bytes(1)]
        wrong = disk_bytes(image, True)
        wrong[9] = ord('X')  # Initial odd-byte "Back" is not the full format signature.
        cases.append(wrong)
        for index, source in enumerate(cases):
            with self.subTest(index=index), self.case() as temp:
                directory = Path(temp)
                archive = directory / 'archive.bin'
                archive.write_bytes(source)
                self.assertNotEqual(self.invoke(directory).returncode, 0)
                self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(source).digest())

    def test_odd_payload_reader_does_not_write_an_extra_byte(self):
        with self.case() as temp:
            directory = Path(temp)
            image, _ = saturn_fixture([(b'SAVE_A00001', b'ABC')])
            (directory / 'archive.bin').write_bytes(image)
            result = subprocess.run([str(self.reader), 'archive.bin'], cwd=directory,
                                    capture_output=True, timeout=3,
                                    env={'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'})
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_saturn_archive_remains_read_only(self):
        image, _ = saturn_fixture([(b'SAVE_A00001', b'Original')])
        for odd in (False, True):
            for args in (['-s', 0, '-d'], ['-s', 0, '-i', 'incoming.SRO'], ['-c', 'GAME']):
                with self.subTest(odd=odd, args=args), self.case() as temp:
                    directory = Path(temp); source = disk_bytes(image, odd)
                    archive = directory / 'archive.bin'; archive.write_bytes(source)
                    incoming = raw_save(b'SAVE_A00001', b'Replacement')
                    (directory / 'incoming.SRO').write_bytes(incoming)
                    self.assertNotEqual(self.invoke(directory, *args).returncode, 0)
                    self.assertEqual(archive.read_bytes(), source)
                    self.assertEqual((directory / 'incoming.SRO').read_bytes(), incoming)
                    self.assertEqual({p.name for p in directory.iterdir()}, {'archive.bin', 'incoming.SRO'})


if __name__ == '__main__':
    unittest.main()
