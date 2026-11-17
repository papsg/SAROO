"""Actual optional exFAT zero-tail, write/seek and persistence contracts.

Adapted from paps-g SSR-165 / 14edd9cd for the public FatFs layout.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import unittest

from fatfs_upcase_media import Volume, inspect, rolling

ROOT = Path(__file__).resolve().parents[1]
BASELINE = os.environ.get('SAROO_VALID_DATA_BASELINE') == '1'


class FatFsValidDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_VALID_DATA_SOURCE',
                                     ROOT / 'Firm_MCU/FatFS'))
        cls.target = Path(os.environ.get('SAROO_VALID_DATA_OUTPUT',
                                         ROOT / 'tests/build/fatfs-valid-data-test'))
        cls.target.mkdir(parents=True, exist_ok=True)
        cls.commands = []
        cls.results = []
        cls.binaries = {}
        for name, changes in [('tiny', {}), ('private', {'FF_FS_TINY': 0}),
                              ('readonly', {'FF_FS_READONLY': 1}), ('formatter', {'FF_USE_MKFS': 1})]:
            changes = {'FF_FS_REENTRANT': 1, 'FF_USE_LFN': 2, **changes}
            folder = cls.target / name
            if folder.exists():
                shutil.rmtree(folder)
            shutil.copytree(source, folder)
            config = folder / 'ffconf.h'
            text = config.read_text()
            for key, value in changes.items():
                text, count = re.subn(r'(#define\s+' + key + r')\s+\d+',
                                     lambda match: match[1] + ' ' + str(value), text)
                assert count == 1
            config.write_text(text)
            cls.binaries[name] = folder / 'test-valid-data'
            cls.call([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-g',
                      '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                      '-fno-omit-frame-pointer', '-pthread', '-DSAROO_FAT32_ONLY=0',
                      *(['-DSSR165_BASELINE'] if BASELINE else []),
                      '-I' + str(folder), ROOT / 'tests/fatfs_valid_data.c',
                      folder / 'ff.c', folder / 'ffunicode.c', '-o', cls.binaries[name]],
                     'compile-' + name)
        cls.bases = {}
        for clusters, size in [(1, 32771), (4, 32771), (4, 2 * 1024**2 + 1)]:
            label = f'format-{clusters}-{size}'
            path = cls.target / (label + '.img')
            with path.open('wb') as image:
                image.truncate(64 * 1024**2)
            cls.call([cls.binaries['formatter'], path, 'format', 0, size,
                      clusters, 0, 'n', 0, '-'], label)
            raw = inspect(path)
            assert raw['no_crosslinks_or_orphans'] and not raw['invalid_name_hashes']
            cls.bases[clusters, size] = path
        cls.base_hashes = {str(path): cls.digest(path) for path in cls.bases.values()}

    @staticmethod
    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    @classmethod
    def call(cls, command, label):
        command = list(map(str, command))
        result = subprocess.run(command, capture_output=True, timeout=180,
                                env={**os.environ, 'ASAN_OPTIONS': 'halt_on_error=1',
                                     'UBSAN_OPTIONS': 'halt_on_error=1'})
        (cls.target / (label + '.stdout')).write_bytes(result.stdout)
        (cls.target / (label + '.stderr')).write_bytes(result.stderr)
        cls.commands.append({'label': label, 'command': command, 'exit': result.returncode,
                             'stdout': result.stdout.decode(errors='replace'),
                             'stderr': result.stderr.decode(errors='replace')})
        (cls.target / 'commands.json').write_text(json.dumps(cls.commands, indent=2) + '\n')
        assert result.returncode == 0 and not result.stderr, (label, result.stdout, result.stderr)
        return json.loads(result.stdout) if result.stdout else None

    @classmethod
    def tearDownClass(cls):
        for name, before in cls.base_hashes.items():
            assert cls.digest(Path(name)) == before

    def setUp(self):
        if BASELINE and self._testMethodName != 'test_baseline_returns_raw_undefined_tail':
            self.skipTest('baseline run selects only the pre-correction control')

    def test_baseline_returns_raw_undefined_tail(self):
        if not BASELINE:
            self.skipTest('run against the public parent with SAROO_VALID_DATA_BASELINE=1')
        path, mapping = self.prepare(1, 0, False, 32771)
        before = self.digest(path)
        result = self.call([self.binaries['tiny'], path, 'baseline-tail', 0,
                            32771, 0, 0, 'n', 0, mapping], 'baseline-raw-tail')
        self.assertTrue(result['baseline_raw_tail'])
        self.assertEqual(result['writes'], 0)
        self.assertEqual(self.digest(path), before)

    @staticmethod
    def keep_entry(volume):
        found = []
        for position in volume.entries:
            if volume.data[position] != 0x85:
                continue
            secondary = volume.data[position + 1]
            length = volume.data[position + 35]
            units = []
            for offset in range(64, (secondary + 1) * 32, 32):
                units.extend(struct.unpack_from('<15H', volume.data, position + offset + 2))
            if struct.pack('<' + 'H' * length, *units[:length]).decode('utf-16le') == 'KEEP.BIN':
                found.append(position)
        assert len(found) == 1
        return found[0]

    def prepare(self, clusters, valid, fragmented, size):
        # Retain canonical sources, exact hashes and construction/operation logs;
        # reuse one private image rather than storing hundreds of 64-MiB copies.
        path = self.target / 'working.img'
        shutil.copyfile(self.bases[clusters, size], path)
        volume = Volume(path)
        entry = self.keep_entry(volume)
        first = volume.u32(entry + 52)
        old = list(range(first, first + (size + volume.cs * 512 - 1) // (volume.cs * 512)))
        if fragmented:
            data = b''.join(volume.data[volume.at(c):volume.at(c) + volume.cs * 512] for c in old)
            for cluster in old:
                volume.allocated(cluster, False)
                volume.put32(volume.fat * 512 + cluster * 4, 0)
            new = []
            for cluster in range(1000, volume.count + 2, 2):
                if not volume.allocated(cluster):
                    new.append(cluster)
                if len(new) == len(old):
                    break
            assert len(new) == len(old)
            for index, cluster in enumerate(new):
                volume.allocated(cluster, True)
                volume.put32(volume.fat * 512 + cluster * 4, new[index + 1] if index + 1 < len(new) else 0xffffffff)
                volume.data[volume.at(cluster):volume.at(cluster) + volume.cs * 512] = data[index * volume.cs * 512:(index + 1) * volume.cs * 512]
            volume.data[entry + 33] &= ~2
            volume.put32(entry + 52, new[0])
            old = new
        volume.put64(entry + 40, valid)
        block = bytes(volume.data[entry:entry + (volume.data[entry + 1] + 1) * 32])
        struct.pack_into('<H', volume.data, entry + 2, rolling(block[:2] + block[4:], 16))
        volume.save()
        mapping = self.target / 'working.map'
        mapping.write_text('\n'.join(str(volume.at(c) // 512 + j) for c in old for j in range(volume.cs)) + '\n')
        if valid <= size:
            raw = inspect(path)
            assert raw['no_crosslinks_or_orphans'] and not raw['invalid_name_hashes']
            assert next(x for x in raw['files'] if x['name'] == 'KEEP.BIN')['valid_bytes'] == valid
        return path, mapping

    def case(self, label, binary, mode, clusters, valid, position=0, amount=0,
             fragmented=False, fault='n', ordinal=0, size=32771):
        path, mapping = self.prepare(clusters, valid, fragmented, size)
        before = self.digest(path)
        result = self.call([self.binaries[binary], path, mode, valid, size,
                            position, amount, fault, ordinal, mapping], label)
        after = self.digest(path)
        if mode in ['read', 'tail-fault', 'threads', 'refuse', 'refuse-truncate']:
            self.assertEqual(before, after)
        else:
            raw = inspect(path)
            self.assertTrue(raw['no_crosslinks_or_orphans'])
            self.assertFalse(raw['invalid_name_hashes'])
            neighbor = next(x for x in raw['files'] if x['name'] == 'NEIGHBOR.BIN')
            self.assertEqual(neighbor['payload_sha256'], hashlib.sha256(bytes([0xb4]) * 513).hexdigest())
            volume = Volume(path)
            entry = self.keep_entry(volume)
            first = volume.u32(entry + 52)
            length = volume.u64(entry + 56)
            valid_after = volume.u64(entry + 40)
            if volume.data[entry + 33] & 2:
                chain = list(range(first, first + (length + volume.cs * 512 - 1) // (volume.cs * 512)))
            else:
                chain = volume.chain(first) if first else []
            payload = b''.join(volume.data[volume.at(c):volume.at(c) + volume.cs * 512] for c in chain)[:length]
            logical = payload[:valid_after] + bytes(length - valid_after)
            initial = 0 if mode == 'replace' else valid
            expected = bytearray(bytes([0xc9]) * min(initial, length) + bytes(max(0, length - initial)))
            if mode in ['write', 'append', 'replace', 'sync-fault']:
                start = size if mode == 'append' else position
                count = result['user_count']
                expected[start:start + count] = bytes(0xa5 ^ (i % 47) for i in range(count))
                self.assertEqual(len(expected), length)
                if not result['write_status']:
                    self.assertEqual(valid_after, max(initial, start + amount) if amount else initial)
            elif mode == 'truncate':
                self.assertEqual((length, valid_after), (position, min(valid, position)))
            elif mode == 'seek-expand':
                self.assertEqual((length, valid_after), (position, valid))
            self.assertEqual(logical, expected)
        self.results.append({'label': label, 'binary': binary, 'mode': mode,
                             'clusters': clusters, 'size': size, 'initial_valid': valid,
                             'position': position, 'amount': amount, 'fragmented': fragmented,
                             'fault': fault, 'ordinal': ordinal, 'before': before,
                             'after': after, 'result': result})
        (self.target / 'results.json').write_text(json.dumps(self.results, indent=2) + '\n')
        return result

    def test_zero_tails_in_direct_cached_fragmented_and_readonly_reads(self):
        for binary in ['tiny', 'private', 'readonly']:
            for clusters in [1, 4]:
                for valid in [0, 1, 511, 512, 513, 2047, 2048, 2049, 32771]:
                    self.case(f'read-{binary}-{clusters}-{valid}', binary, 'tail-fault',
                              clusters, valid, fragmented=valid in [1, 2049])

    def test_writes_gaps_and_append_preserve_raw_valid_data_and_neighbors(self):
        for binary in ['tiny', 'private']:
            for clusters in [1, 4]:
                for valid, position in [(0, 0), (1, 0), (1, 1), (1, 513), (511, 512),
                                        (513, 2049), (2049, 1), (32771, 32770), (1, 32790)]:
                    self.case(f'write-{binary}-{clusters}-{valid}-{position}', binary,
                              'write', clusters, valid, position, 7,
                              fragmented=position in [513, 2049])
                for valid in [0, 1, 513, 32771]:
                    self.case(f'append-{binary}-{clusters}-{valid}', binary, 'append',
                              clusters, valid, amount=7, fragmented=valid == 513)

    def test_truncate_seek_expansion_and_replacement_keep_valid_lengths(self):
        for binary in ['tiny', 'private']:
            for clusters in [1, 4]:
                for valid, position in [(0, 0), (1, 0), (513, 1), (1, 513), (2049, 2048), (32771, 32771)]:
                    self.case(f'truncate-{binary}-{clusters}-{valid}-{position}', binary,
                              'truncate', clusters, valid, position)
                self.case(f'expand-{binary}-{clusters}', binary, 'seek-expand',
                          clusters, 513, 40001, fragmented=True)
                self.case(f'replace-{binary}-{clusters}', binary, 'replace', clusters, 1, amount=7)

    def test_malformed_valid_length_refuses_before_read_or_truncate(self):
        for binary in ['tiny', 'private', 'readonly']:
            for valid in [32772, 0xffffffffffffffff]:
                self.case(f'invalid-{binary}-{valid}', binary, 'refuse', 1, valid)
                if binary != 'readonly':
                    self.case(f'invalid-truncate-{binary}-{valid}', binary, 'refuse-truncate', 1, valid)

    def test_gap_io_failures_keep_user_counts_position_and_sync_recovery(self):
        for binary in ['tiny', 'private']:
            probe = self.case('fault-probe-' + binary, binary, 'write', 1, 1, 8193, 1027, fragmented=True)
            for kind, key in [('r', 'reads'), ('w', 'writes')]:
                for ordinal in range(1, probe[key] + 1):
                    result = self.case(f'fault-{binary}-{kind}-{ordinal}', binary, 'write',
                                       1, 1, 8193, 1027, fragmented=True, fault=kind, ordinal=ordinal)
                    self.assertEqual((result['fault_hits'], result['write_status']), (1, 1))

    def test_sync_failures_do_not_lose_the_short_valid_end(self):
        for binary in ['tiny', 'private']:
            probe = self.case('sync-probe-' + binary, binary, 'sync-fault', 1, 513, 2049, 7)
            for kind, key in [('r', 'reads'), ('w', 'writes'), ('s', 'syncs')]:
                for ordinal in range(1, probe[key] + 1):
                    result = self.case(f'sync-{binary}-{kind}-{ordinal}', binary, 'sync-fault',
                                       1, 513, 2049, 7, fault=kind, ordinal=ordinal)
                    self.assertEqual((result['fault_hits'], result['sync_status']), (1, 1))

    def test_each_file_keeps_its_valid_end_with_interleaved_threads(self):
        for binary in ['tiny', 'private', 'readonly']:
            self.case('threads-' + binary, binary, 'threads', 4, 513, fragmented=True)

    def test_zero_byte_writes_and_small_gaps_do_not_initialize_a_large_tail(self):
        for binary in ['tiny', 'private']:
            self.case('zero-write-' + binary, binary, 'write', 1, 1, 8193, 0)
            result = self.case('small-gap-' + binary, binary, 'write', 4, 1, 17, 1, size=2 * 1024**2 + 1)
            self.assertLess(result['filesystem_writes'], 16)
            result = self.case('large-gap-' + binary, binary, 'write', 4, 1, 1024**2 + 1, 3, size=2 * 1024**2 + 1)
            self.assertGreaterEqual(result['filesystem_writes'], 2048)


if __name__ == '__main__':
    unittest.main()
