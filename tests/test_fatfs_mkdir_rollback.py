"""Actual FatFs child cleanup with immediate unmount; no hardware operations.

Adapted from paps-g 99d0b565/4842feaa. Reuse the public source-staging and
file-backed C scaffold; independently construct FAT12/16/32 media.
"""
import hashlib
import json
import os
import struct
import subprocess
import unittest
from pathlib import Path

from mcu_save_support import make_fat32
import test_fatfs_seek as support


def make_fat12_16(path, filesystem, cluster):
    assert filesystem in ('fat12', 'fat16') and cluster in (1, 4)
    sectors = (2048 if filesystem == 'fat12' else 32768) * cluster
    fat_sectors = 6 if filesystem == 'fat12' else 128
    boot = bytearray(512)
    boot[:11] = b'\xeb\x3c\x90MSDOS5.0'
    struct.pack_into('<HBHBHHBHHHII', boot, 11, 512, cluster, 1, 2, 512,
        sectors if sectors < 65536 else 0, 0xf8, fat_sectors, 32, 16, 0,
        sectors if sectors >= 65536 else 0)
    boot[36] = 0x80
    boot[38] = 0x29
    boot[43:54] = b'MKDIR TEST '
    boot[54:62] = b'FAT12   ' if filesystem == 'fat12' else b'FAT16   '
    boot[510:512] = b'\x55\xaa'
    entries = b'\xf8\xff\xff' if filesystem == 'fat12' else b'\xf8\xff\xff\xff'
    with path.open('wb') as stream:
        stream.truncate(sectors * 512)
        stream.write(boot)
        for sector in (1, 1 + fat_sectors):
            stream.seek(sector * 512)
            stream.write(entries)


def raw_audit(path):
    """Independent packed-entry/ownership walk, without a FatFs free-count hint."""
    with path.open('rb') as stream:
        boot = stream.read(512)
        reserved = struct.unpack_from('<H', boot, 14)[0]
        root_entries = struct.unpack_from('<H', boot, 17)[0]
        total = struct.unpack_from('<H', boot, 19)[0] or struct.unpack_from('<I', boot, 32)[0]
        fat_sectors = struct.unpack_from('<H', boot, 22)[0] or struct.unpack_from('<I', boot, 36)[0]
        copies, csize = boot[16], boot[13]
        root_sector = reserved + copies * fat_sectors
        root_sectors = (root_entries * 32 + 511) // 512
        data_sector = root_sector + root_sectors
        clusters = (total - data_sector) // csize
        bits = 12 if clusters < 4085 else 16 if clusters < 65525 else 32
        stream.seek(reserved * 512)
        table = stream.read(fat_sectors * 512)
        for _ in range(1, copies):
            assert stream.read(fat_sectors * 512) == table

        def entry(cluster):
            offset = cluster + cluster // 2 if bits == 12 else cluster * (bits // 8)
            value = int.from_bytes(table[offset:offset + (4 if bits == 32 else 2)], 'little')
            return (value >> (4 if cluster & 1 else 0)) & 0xfff if bits == 12 else value & ((1 << min(bits, 28)) - 1)

        claimed, files = {}, {}
        eoc = {12: 0xff8, 16: 0xfff8, 32: 0x0ffffff8}[bits]

        def chain(first, owner):
            content = []
            while first:
                assert 2 <= first < clusters + 2 and first not in claimed
                claimed[first] = owner
                stream.seek((data_sector + (first - 2) * csize) * 512)
                content.append(stream.read(csize * 512))
                link = entry(first)
                if link >= eoc:
                    break
                assert link >= 2
                first = link
            return b''.join(content)

        def directory(data, parent):
            for offset in range(0, len(data), 32):
                item = data[offset:offset + 32]
                if not item[0]:
                    break
                if item[0] in (0xe5, 0x2e) or item[11] & 8:
                    continue
                base, extension = item[:8].rstrip(b' ').decode(), item[8:11].rstrip(b' ').decode()
                name = parent + base + ('.' + extension if extension else '')
                first = (struct.unpack_from('<H', item, 20)[0] << 16) | struct.unpack_from('<H', item, 26)[0]
                content = chain(first, name)
                if item[11] & 16:
                    directory(content, name + '/')
                else:
                    size = struct.unpack_from('<I', item, 28)[0]
                    assert len(content) == ((size + csize * 512 - 1) // (csize * 512)) * csize * 512
                    files[name] = content[:size]

        if bits == 32:
            root = chain(struct.unpack_from('<I', boot, 44)[0], '/')
        else:
            stream.seek(root_sector * 512)
            root = stream.read(root_sectors * 512)
        directory(root, '/')
        assert files.get('/NEIGHBOR.BIN') == b'\xc9' * 512
        if '/ANCHOR.BIN' in files:
            assert files['/ANCHOR.BIN'] == b'\x55' * len(files['/ANCHOR.BIN'])
        allocated = {i for i in range(2, clusters + 2) if entry(i)}
        return dict(bits=bits, orphan_clusters=sorted(allocated - claimed.keys()),
                    live_clusters=len(claimed), free=clusters-len(allocated),
                    file_sizes={n: len(v) for n, v in files.items()})


class FatFsMkdirRollbackTests(unittest.TestCase):
    fixture_name = 'fatfs-mkdir-rollback'
    fixture_sources = ('fatfs_mkdir_rollback.c', 'test_fatfs_mkdir_rollback.py',
                       'mcu_save_support.py', *support.FatFsSeekTests.fixture_sources)
    generic_formats = False
    setUpClass = classmethod(support.FatFsSeekTests.setUpClass.__func__)
    tearDownClass = classmethod(support.FatFsSeekTests.tearDownClass.__func__)

    def formats(self):
        return ('fat32',)

    def case(self, filesystem, mode, cluster=1, partial=0, lookup=0, unknown=0):
        stem = self.target / f'{filesystem}-{mode}-{cluster}-{partial}-{lookup}-{unknown}'
        image = stem.with_suffix('.img')
        if filesystem == 'fat32':
            make_fat32(image, sectors=131072 * cluster)
            if cluster != 1:
                with image.open('r+b') as stream:
                    for sector in (0, 6):
                        stream.seek(sector * 512 + 13)
                        stream.write(bytes([cluster]))
        else:
            make_fat12_16(image, filesystem, cluster)
        command = [str(self.binary), str(image), mode, str(partial), str(lookup), filesystem, str(unknown)]
        result = subprocess.run(command, capture_output=True, timeout=30,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        with image.open('rb') as stream:
            image_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
        stem.with_suffix('.command.json').write_text(json.dumps(command, indent=2) + '\n')
        stem.with_suffix('.log').write_bytes(result.stdout + result.stderr)
        record = dict(command=command, exit=result.returncode,
            stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace'),
            image_sha256=image_hash)
        try:
            record['raw'] = raw_audit(image)
        except (AssertionError, ValueError, IndexError) as error:
            record['raw_error'] = repr(error)
        self.results.append(record)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, b'')
        self.assertNotIn('raw_error', record, record)
        self.assertEqual(record['raw']['orphan_clusters'], [])

    def test_failed_clear_survives_immediate_unmount(self):
        for filesystem in self.formats():
            for cluster in (1, 4):
                for partial in (0, 1):
                    with self.subTest(filesystem=filesystem, cluster=cluster, partial=partial):
                        self.case(filesystem, 'clear', cluster, partial)

    def test_healthy_and_lookup_controls_preserve_ownership(self):
        for filesystem in self.formats():
            for mode, lookup in (('healthy', 0), ('clear', 1)):
                with self.subTest(filesystem=filesystem, mode=mode):
                    self.case(filesystem, mode, lookup=lookup)

    def test_failed_rollback_blocks_unmount_and_allocation_until_retry(self):
        for filesystem in self.formats():
            for mode in ('fat', 'sync'):
                with self.subTest(filesystem=filesystem, mode=mode):
                    self.case(filesystem, mode)

    def test_unknown_count_and_existing_owner_survive_io_and_status_refusals(self):
        for filesystem in self.formats():
            for mode in ('read', 'fat', 'mirror', 'sync'):
                with self.subTest(filesystem=filesystem, mode=mode):
                    self.case(filesystem, mode, unknown=1)

    def test_unknown_count_clear_prefix_and_healthy_controls(self):
        for filesystem in self.formats():
            for mode, cluster, partial in (('clear', 4, 1), ('clear', 1, 0), ('healthy', 1, 0)):
                with self.subTest(filesystem=filesystem, mode=mode, cluster=cluster):
                    self.case(filesystem, mode, cluster, partial, unknown=1)

    def test_uncredited_known_count_and_second_fat_retries(self):
        for filesystem in self.formats():
            for mode in ('read', 'mirror'):
                with self.subTest(filesystem=filesystem, mode=mode):
                    self.case(filesystem, mode)

    def test_failed_child_and_parent_extension_keep_separate_owners(self):
        if 'fat32' in self.formats():
            for unknown in (0, 1):
                with self.subTest(unknown=unknown): self.case('fat32', 'dual', unknown=unknown)


class GenericFatFsMkdirRollbackTests(FatFsMkdirRollbackTests):
    generic_formats = True
    # The pinned SDK requires a per-call LFN workspace with its volume mutex.
    config_replacements = ((b'#define FF_FS_REENTRANT\t0', b'#define FF_FS_REENTRANT\t1'),
                           (b'#define FF_USE_LFN\t\t1', b'#define FF_USE_LFN\t\t2'))

    def formats(self):
        return ('fat12', 'fat16', 'fat32')


if __name__ == '__main__':
    unittest.main()
