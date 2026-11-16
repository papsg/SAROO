"""Actual public FatFs/save startup with stale FAT32 free-space hints.

Adapted from paps-g 8f8b63c5 (SSR-106), with public-layout integration and
independent raw ownership/payload and next-hint controls by papsg.
"""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import unittest

from mcu_save_support import MCUSaveTest, make_fat32
from test_fatfs_expand import sparse_copy


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def raw_audit(path):
    """Read on-disk links independently of FatFs; FSInfo counts need not agree."""
    with path.open('rb') as disk:
        boot = disk.read(512)
        reserved = struct.unpack_from('<H', boot, 14)[0]
        copies, csize, length = boot[16], boot[13], u32(boot, 36)
        first_data = reserved + copies * length
        clusters = (u32(boot, 32) - first_data) // csize
        disk.seek(reserved * 512)
        fat = disk.read(length * 512)
        for index in range(1, copies):
            assert disk.read(length * 512) == fat
        claimed, files = {}, {}

        def chain(first, owner):
            result = []
            while first:
                assert 2 <= first < clusters + 2 and first not in claimed, (owner, first)
                claimed[first] = owner
                disk.seek((first_data + (first - 2) * csize) * 512)
                result.append(disk.read(csize * 512))
                link = u32(fat, first * 4) & 0x0fffffff
                if link >= 0x0ffffff8:
                    break
                first = link
                assert first >= 2
            return b''.join(result)

        def directory(first, parent):
            data = chain(first, parent)
            for offset in range(0, len(data), 32):
                item = data[offset:offset + 32]
                if not item[0]:
                    break
                if item[0] in (0xe5, 0x2e) or item[11] & 8:
                    continue
                base, extension = item[:8].rstrip(b' ').decode('ascii'), item[8:11].rstrip(b' ').decode('ascii')
                name = parent + base + ('.' + extension if extension else '')
                cluster = (struct.unpack_from('<H', item, 20)[0] << 16) | struct.unpack_from('<H', item, 26)[0]
                if item[11] & 16:
                    directory(cluster, name + '/')
                else:
                    content = chain(cluster, name)
                    size = u32(item, 28)
                    assert len(content) == ((size + csize * 512 - 1) // (csize * 512)) * csize * 512
                    files[name] = content[:size]

        directory(u32(boot, 44), '/')
        assert files.get('/NEIGHBOR.BIN') == b'\xc9' * 1024
        if '/TARGET.BIN' in files:
            assert files['/TARGET.BIN'] == b'\xc9' * len(files['/TARGET.BIN'])
        for name, prefix, header_size in [('/SAROO/SS_SAVE.BIN', b'Saroo Save File', 65536),
                ('/SAROO/SS_MEMS.BIN', bytes.fromhex('5361726f4d656d73008000001f780000ff'), 8192)]:
            if name in files:
                expected = prefix + bytes(header_size - len(prefix))
                assert files[name][:header_size] == expected[:min(header_size, len(files[name]))]
        return dict(free=sum(not (u32(fat, i * 4) & 0x0fffffff) for i in range(2, clusters + 2)),
                    owned=len(claimed), files={n: dict(size=len(b), sha256=hashlib.sha256(b).hexdigest()) for n, b in files.items()})


class FatFsFSInfoTests(MCUSaveTest):
    c_source = 'fatfs_fsinfo.c'
    extra_harnesses = ('fatfs_fsinfo.c', 'test_fatfs_fsinfo.py',
                       'test_fatfs_expand.py', 'test_fatfs_seek.py')
    fixture = 'startup'

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prepared = cls.target / 'prepared.img'
        make_fat32(cls.prepared)
        result, _ = cls.execute(cls.prepared, 'prepare', 0)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    @classmethod
    def execute(cls, image, mode, expected):
        result = subprocess.run([str(cls.binary), str(image), mode, str(expected)],
            capture_output=True, timeout=30, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith(b'{')]
        with image.open('rb') as stream:
            image_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
        cls.results.append(dict(image=image.name, mode=mode, status=result.returncode,
            stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace'),
            rows=rows, image_sha256=image_hash, raw=raw_audit(image)))
        return result, rows

    def case(self, hint, mode, full=False, next_hint=None):
        image = self.target / (self._testMethodName + '-' + str(hint) + '-' + str(next_hint) + '.img')
        sparse_copy(self.prepared, image)
        with image.open('r+b') as disk:
            boot = disk.read(512)
            reserved = struct.unpack_from('<H', boot, 14)[0]
            copies, length = boot[16], u32(boot, 36)
            clusters = (u32(boot, 32) - reserved - copies * length) // boot[13]
            disk.seek(reserved * 512)
            fat = bytearray(disk.read(length * 512))
            free = 0
            for cluster in range(2, clusters + 2):
                if not u32(fat, cluster * 4) & 0x0fffffff:
                    free += 1
                    if full:
                        struct.pack_into('<I', fat, cluster * 4, 0x0ffffff7)
            if full:
                free = 0
                for index in range(copies):
                    disk.seek((reserved + index * length) * 512)
                    disk.write(fat)
            if next_hint == 'last':
                next_hint = clusters + 1
            elif next_hint == 'outside':
                next_hint = clusters + 2
            for sector in (1, 7):
                disk.seek(sector * 512 + 488)
                disk.write(struct.pack('<I', free if hint == 'accurate' else hint))
                if next_hint is not None:
                    disk.write(struct.pack('<I', next_hint))
            disk.seek(512 + 492)
            saved_next = u32(disk.read(4), 0)
        result, rows = self.execute(image, mode, free)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, b'')
        self.assertEqual(rows[0]['mounted_hint'], saved_next)

    def test_allocation_ignores_stale_zero_and_low_counts(self):
        for hint in (0, 1, 'accurate', 0xffffffff):
            with self.subTest(hint=hint): self.case(hint, 'allocate')

    def test_explicit_query_recounts_then_reuses_the_mounted_count(self):
        for hint in (0, 1, 'accurate'):
            with self.subTest(hint=hint): self.case(hint, 'query')

    def test_real_startup_creates_and_reopens_both_archives(self):
        for hint in (0, 1, 'accurate'):
            with self.subTest(hint=hint): self.case(hint, 'startup')

    def test_full_fat_refuses_safely_with_high_or_accurate_count(self):
        for hint in (100, 'accurate'):
            with self.subTest(hint=hint): self.case(hint, 'full', full=True)

    def test_query_read_error_preserves_output_and_allows_retry(self):
        self.case(0, 'read-failure')

    def test_next_cluster_hints_remain_range_safe(self):
        for hint in (0, 1, 2, 'last', 'outside', 0xffffffff):
            with self.subTest(hint=hint): self.case(1, 'allocate', next_hint=hint)


class GenericFatFsFSInfoTests(FatFsFSInfoTests):
    generic_formats = True


if __name__ == '__main__':
    unittest.main()
