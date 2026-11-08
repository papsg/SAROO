"""Exercise bounded SAROO slot selection, creation and save allocation via the CLI."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import unittest

from save_fixtures import raw_save, bup_save, slot_fixture
from savetool_support import NativeSaveTest

UNIT = 65536



class SaveSlotTests(NativeSaveTest):
    def invoke(self, directory, *args, timeout=3):
        result = subprocess.run([str(self.binary), 'archive.bin', *map(str, args)],
                                cwd=directory, capture_output=True, timeout=timeout,
                                env={'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'})
        self.assertNotIn(b'runtime error:', result.stderr)
        self.assertNotIn(b'AddressSanitizer', result.stderr)
        return result

    def test_bad_slot_indices_and_missing_file_storage_are_rejected(self):
        image, _ = slot_fixture()
        for slot in (-2, 0, 2, 4095, 4096, 2147483647):
            with self.subTest(slot=slot), self.case() as temp:
                directory = Path(temp)
                archive = directory / 'archive.bin'
                archive.write_bytes(image)
                self.assertNotEqual(self.invoke(directory, '-t', slot).returncode, 0)
                self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(image).digest())
        with self.case() as temp:
            directory = Path(temp)
            image[2 * 16:2 * 16 + 4] = b'LOST'
            (directory / 'archive.bin').write_bytes(image)
            self.assertNotEqual(self.invoke(directory).returncode, 0)

    def test_malformed_slot_metadata_cannot_reach_any_save_operation(self):
        original, locations = slot_fixture([(b'SAVE_A00001', b'A' * 256),
                                             (b'SAVE_B00002', b'B' * 256)])
        header, extents = locations[0]
        neighbor, other = locations[1]
        cases = {}

        def change(name, offset, data):
            image = bytearray(original)
            image[offset:offset + len(data)] = data
            cases[name] = image

        change('bad-magic', UNIT, b'BROKEN!!')
        change('bad-size', UNIT + 8, struct.pack('>I', UNIT // 2))
        for block_size in (0, 64, 65535):
            change(f'block-size-{block_size}', UNIT + 12, struct.pack('>H', block_size))
        for block in (512, 65535):
            change(f'header-{block}', UNIT + 62, struct.pack('>H', block))
        change('header-cycle', neighbor + 62, struct.pack('>H', (header - UNIT) // 128))
        change('missing-chain', header + 64, bytes(64))
        change('empty-name', header, b'\0')
        for size in (0, 510 * 128 + 1, 0xffffffff):
            change(f'payload-{size}', header + 12, struct.pack('>I', size))
        for name, block in [('reserved', 0), ('self-header', (header - UNIT) // 128),
                            ('neighbor-header', (neighbor - UNIT) // 128),
                            ('shared-data', (other[0][0] - UNIT) // 128)]:
            offset = header + 64 + block // 8
            change(name, offset, bytes([original[offset] | (1 << (block % 8))]))
        for name, block in [('unallocated-header', (header - UNIT) // 128),
                            ('unallocated-data', (extents[0][0] - UNIT) // 128),
                            ('unreserved-global', 0)]:
            offset = UNIT + 64 + block // 8
            change(name, offset, bytes([original[offset] & ~(1 << (block % 8))]))
        commands = [[], ['-s', '0'], ['-s', '0', '-d'],
                    ['-s', '0', '-i', 'incoming.SRO'], ['-i', 'incoming.SRO']]
        for reason, image in cases.items():
            for args in commands:
                with self.subTest(reason=reason, args=args), self.case() as temp:
                    directory = Path(temp)
                    archive = directory / 'archive.bin'
                    archive.write_bytes(image)
                    (directory / 'incoming.SRO').write_bytes(raw_save(b'NEW00000000', b'N' * 256))
                    result = self.invoke(directory, '-t', '1', *args)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(image).digest())
                    self.assertEqual({p.name for p in directory.iterdir()}, {'archive.bin', 'incoming.SRO'})

    def test_resized_slot_saves_round_trip_and_preserve_neighbors(self):
        for old_size, size in ((32, 64), (64, 32), (0, 2048), (2048, 0),
                               (128, 129), (4097, 128), (32, 508 * 128), (508 * 128, 32)):
            with self.subTest(before=old_size, after=size), self.case() as temp:
                directory = Path(temp)
                image, locations = slot_fixture([(b'SAVE_A00001', b'A' * old_size),
                                                 (b'SAVE_B00002', b'neighbor')])
                archive = directory / 'archive.bin'
                archive.write_bytes(image)
                data = bytes((i * 7 + 11) % 251 for i in range(size))
                (directory / 'incoming.SRO').write_bytes(raw_save(b'SAVE_A00001', data))
                result = self.invoke(directory, '-t', 1, '-s', 0, '-i', 'incoming.SRO')
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                actual = archive.read_bytes()
                self.assertEqual(len(actual), len(image))
                self.assertEqual(actual[:UNIT], image[:UNIT])
                neighbor, extents = locations[1]
                for offset, count in [(neighbor, 128), *extents]:
                    self.assertEqual(actual[offset:offset + count], image[offset:offset + count])
                self.assertEqual(struct.unpack_from('>H', actual, UNIT + 14)[0],
                                 512 - 1 - 2 - 1 - (size + 127) // 128)
                self.assertEqual(self.invoke(directory, '-t', 1, '-s', 0).returncode, 0)
                self.assertEqual((directory / 'SAVE_A00001.SRO').read_bytes()[64:], data)
                self.assertEqual(self.invoke(directory, '-t', 1, '-s', 1).returncode, 0)
                self.assertEqual((directory / 'SAVE_B00002.SRO').read_bytes()[64:], b'neighbor')

    def test_failed_slot_allocation_preserves_archive(self):
        for reason in ('full', 'oversized', 'new-oversized', 'empty-name'):
            with self.subTest(reason=reason), self.case() as temp:
                directory = Path(temp)
                image, _ = slot_fixture([(b'SAVE_A00001', b'A' * 256)])
                size, args, name = 4097, ['-s', '0'], b'SAVE_A00001'
                if reason == 'full':
                    image[UNIT + 64:UNIT + 128] = bytes([255]) * 64
                elif reason in ('oversized', 'new-oversized'):
                    size = 510 * 128 + 1
                    if reason == 'new-oversized': args = []
                else:
                    args, name = [], b''
                archive = directory / 'archive.bin'
                archive.write_bytes(image)
                (directory / 'incoming.SRO').write_bytes(raw_save(name, bytes(size)))
                self.assertNotEqual(self.invoke(directory, '-t', 1, *args, '-i', 'incoming.SRO').returncode, 0)
                self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(image).digest())

    def test_slot_creation_handles_short_ids_holes_duplicates_and_limits(self):
        for gameid in ('G', 'A' * 16):
            with self.subTest(gameid=gameid), self.case() as temp:
                directory = Path(temp)
                image, _ = slot_fixture(gameids=(None, b'KEEP'))
                archive = directory / 'archive.bin'
                archive.write_bytes(image)
                self.assertEqual(self.invoke(directory, '-c', gameid).returncode, 0)
                actual = archive.read_bytes()
                self.assertEqual(len(actual), len(image), 'Reusing a hole grew the archive')
                self.assertEqual(actual[2 * UNIT:], image[2 * UNIT:])
                self.assertEqual(actual[16:32], gameid.encode().ljust(16, b'\0'))
                self.assertEqual(actual[UNIT:UNIT + 8], b'SaroSave')
                self.assertEqual(self.invoke(directory, '-t', 1).returncode, 0)
        for gameid in ('', 'X' * 17):
            with self.subTest(gameid=gameid), self.case() as temp:
                directory = Path(temp)
                image, _ = slot_fixture()
                archive = directory / 'archive.bin'
                archive.write_bytes(image)
                self.assertNotEqual(self.invoke(directory, '-c', gameid).returncode, 0)
                self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(image).digest())
        with self.case() as temp:
            directory = Path(temp)
            image, _ = slot_fixture(gameids=(None, b'KEEP'))
            archive = directory / 'archive.bin'
            archive.write_bytes(image)
            self.assertEqual(self.invoke(directory, '-c', 'KEEP').returncode, 0)
            self.assertEqual(hashlib.sha256(archive.read_bytes()).digest(), hashlib.sha256(image).digest(),
                             'Duplicated an ID after a directory gap')
            listed = self.invoke(directory)
            self.assertIn(b'[KEEP]', listed.stdout)
            self.assertEqual(self.invoke(directory, '-c', 'NEW').returncode, 0)
            self.assertEqual(self.invoke(directory, '-c', 'APPEND').returncode, 0)
            self.assertEqual(archive.stat().st_size, len(image) + UNIT)
            self.assertEqual(self.invoke(directory, '-t', 3).returncode, 0)

    def test_stale_counts_do_not_break_slot_import_or_deletion(self):
        for stale in (0, 65535):
            for operation in ('import', 'delete'):
                with self.subTest(cached_free=stale, operation=operation), self.case() as temp:
                    directory = Path(temp)
                    image, locations = slot_fixture([(b'SAVE_A00001', b'A' * 256),
                                                     (b'SAVE_B00002', b'keep')])
                    struct.pack_into('>H', image, UNIT + 14, stale)
                    image[UNIT + 127] |= 0x80  # Retain an old orphan at block 511.
                    archive = directory / 'archive.bin'
                    archive.write_bytes(image)
                    self.assertEqual(self.invoke(directory, '-t', 1).returncode, 0)
                    self.assertEqual(archive.read_bytes(), image)
                    if operation == 'import':
                        (directory / 'incoming.SRO').write_bytes(raw_save(b'NEW00000000', b'N' * 1024))
                        args = ['-i', 'incoming.SRO']
                    else:
                        args = ['-s', '0', '-d']
                    result = self.invoke(directory, '-t', 1, *args)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    actual = archive.read_bytes()
                    self.assertEqual(actual[:UNIT], image[:UNIT])
                    self.assertTrue(actual[UNIT + 127] & 0x80)
                    free = 512 - sum(bin(byte).count('1') for byte in actual[UNIT + 64:UNIT + 128])
                    self.assertEqual(struct.unpack_from('>H', actual, UNIT + 14)[0], free)
                    neighbor, extents = locations[1]
                    # A new tail save legitimately changes the neighbor's next
                    # pointer, but its name/metadata/bitmap/payload are preserved.
                    for offset, count in [(neighbor, 62), (neighbor + 64, 64), *extents]:
                        self.assertEqual(actual[offset:offset + count], image[offset:offset + count])
                    index = 1 if operation == 'import' else 0
                    self.assertEqual(self.invoke(directory, '-t', 1, '-s', index).returncode, 0)
                    self.assertEqual((directory / 'SAVE_B00002.SRO').read_bytes()[64:], b'keep')
                    if operation == 'import':
                        self.assertEqual(self.invoke(directory, '-t', 1, '-s', 2).returncode, 0)
                        self.assertEqual((directory / 'NEW00000000.SRO').read_bytes()[64:], b'N' * 1024)

    def test_new_save_capacity_fragmented_blocks_and_empty_archive(self):
        for size, bup in [(0, False), (1, False), (128, False), (129, False),
                          (510 * 128, False), (510 * 128, True)]:
            with self.subTest(size=size, bup=bup), self.case() as temp:
                directory = Path(temp)
                image, _ = slot_fixture(gameids=(b'GAME', b'KEEP'))
                archive = directory / 'archive.bin'; archive.write_bytes(image)
                data = bytes((i * 11 + 3) % 251 for i in range(size))
                encoder = bup_save if bup else raw_save
                (directory / 'incoming').write_bytes(encoder(b'NEW00000000', data))
                self.assertEqual(self.invoke(directory, '-t', 1, '-i', 'incoming').returncode, 0)
                actual = archive.read_bytes()
                self.assertEqual(actual[:UNIT], image[:UNIT])
                self.assertEqual(actual[2 * UNIT:], image[2 * UNIT:])
                self.assertEqual(struct.unpack_from('>H', actual, UNIT + 14)[0], 510 - (size + 127) // 128)
                self.assertEqual(self.invoke(directory, '-t', 1, '-s', 0).returncode, 0)
                self.assertEqual((directory / 'NEW00000000.SRO').read_bytes()[64:], data)
        with self.case() as temp:
            directory = Path(temp); image, _ = slot_fixture()
            image[UNIT + 64:UNIT + 128] = b'\xff' * 64
            for block in [100, 510, 511]: image[UNIT + 64 + block // 8] &= ~(1 << (block % 8))
            struct.pack_into('>H', image, UNIT + 14, 0)  # Stale low count.
            archive = directory / 'archive.bin'; archive.write_bytes(image)
            data = b'F' * 129; (directory / 'incoming').write_bytes(raw_save(b'FRAGMENT001', data))
            self.assertEqual(self.invoke(directory, '-t', 1, '-i', 'incoming').returncode, 0)
            actual = archive.read_bytes()
            self.assertEqual(struct.unpack_from('>H', actual, UNIT + 62)[0], 100)
            self.assertEqual(actual[UNIT + 64:UNIT + 128], b'\xff' * 64)
            self.assertEqual(actual[UNIT + 100 * 128 + 64:UNIT + 101 * 128], b'\0' * 63 + b'\xc0')
            self.assertEqual(struct.unpack_from('>H', actual, UNIT + 14)[0], 0)
            self.assertEqual(self.invoke(directory, '-t', 1, '-s', 0).returncode, 0)
            self.assertEqual((directory / 'FRAGMENT001.SRO').read_bytes()[64:], data)
        with self.case() as temp:
            directory = Path(temp); image = bytearray(UNIT); image[:16] = b'Saroo Save File\0'
            archive = directory / 'archive.bin'; archive.write_bytes(image)
            self.assertEqual(self.invoke(directory, '-c', 'FIRST').returncode, 0)
            self.assertEqual(archive.stat().st_size, 2 * UNIT)
            self.assertEqual(self.invoke(directory, '-t', 1).returncode, 0)

    def test_full_slot_directory_rejects_creation_before_writing(self):
        with self.case() as temp:
            directory = Path(temp)
            header = bytearray(UNIT)
            header[:16] = b'Saroo Save File\0'
            for slot in range(1, 4096):
                header[slot * 16:slot * 16 + 16] = f'GAME{slot:012d}'.encode()
            archive = directory / 'archive.bin'
            with archive.open('wb') as f:
                f.write(header)
                f.truncate(4096 * UNIT)  # Sparse payload; only directory creation is exercised.
            before = hashlib.sha256()
            with archive.open('rb') as f:
                for block in iter(lambda: f.read(1024 * 1024), b''): before.update(block)
            self.assertNotEqual(self.invoke(directory, '-c', 'NO-ROOM', timeout=15).returncode, 0)
            self.assertEqual(archive.stat().st_size, 4096 * UNIT)
            after = hashlib.sha256()
            with archive.open('rb') as f:
                for block in iter(lambda: f.read(1024 * 1024), b''): after.update(block)
            self.assertEqual(after.digest(), before.digest())


if __name__ == '__main__':
    unittest.main()
