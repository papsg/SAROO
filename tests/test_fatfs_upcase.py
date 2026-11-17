"""Volume-specific exFAT case authority, raw metadata and I/O refusals.

Adapted from paps-g SSR-163 / f3714b9e for the public FatFs source layout.
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

from fatfs_upcase_media import Volume, inspect

ROOT = Path(__file__).resolve().parents[1]
BASELINE = os.environ.get('SAROO_UPCASE_BASELINE') == '1'

class FatFsUpcaseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_FATFS_DIR', ROOT / 'Firm_MCU/FatFS'))
        cls.target = Path(os.environ.get('SAROO_UPCASE_OUTPUT',
                                        ROOT / 'tests/build/fatfs-upcase-test'))
        cls.target.mkdir(parents=True, exist_ok=True)
        cls.commands = []
        cls.binaries = {}
        for tag, changes in [('generic', {}), ('readonly', {'FF_FS_READONLY': 1}),
                             ('two', {'FF_VOLUMES': 2}), ('formatter', {'FF_USE_MKFS': 1})]:
            changes = {'FF_FS_REENTRANT': 1, 'FF_USE_LFN': 2, **changes}
            folder = cls.target / tag
            if folder.exists():
                shutil.rmtree(folder)
            shutil.copytree(source, folder)
            config = folder / 'ffconf.h'
            text = config.read_text()
            for name, value in changes.items():
                text, count = re.subn(r'(#define ' + name + r')\s+\d+',
                                     lambda match: match[1] + ' ' + str(value), text)
                assert count == 1
            config.write_text(text)
            cls.binaries[tag] = folder / 'test-upcase'
            cls.call([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-g',
                      '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                      '-fno-omit-frame-pointer', '-pthread', '-DSAROO_FAT32_ONLY=0',
                      *(['-DSSR163_BASELINE'] if BASELINE else []),
                      '-I' + str(folder), str(ROOT / 'tests/fatfs_upcase.c'),
                      str(folder / 'ffunicode.c'), '-o', str(cls.binaries[tag])], 'compile-' + tag)
        cls.images = cls.target / 'images'
        cls.images.mkdir(exist_ok=True)
        formatted = cls.images / 'formatted.img'
        with formatted.open('wb') as output:
            output.truncate(64 * 1024**2)
        cls.call([cls.binaries['formatter'], formatted, 'format'], 'format')
        assert not inspect(formatted)['invalid_name_hashes']
        original = Volume(formatted)
        cls.mapping = original.maps()
        cls.mapping[0x250] = 0x2c6f
        cls.mapping[0x2c6f] = 0x2c6f
        # These two explicit valid volume tables remain different even if a
        # future formatter's vendor Unicode conversion changes.
        cls.custom = cls.images / 'custom.img'
        shutil.copyfile(formatted, cls.custom)
        custom = Volume(cls.custom)
        custom.replace_table(struct.pack('<65536H', *cls.mapping))
        custom.save()
        identity_map = cls.mapping[:]
        identity_map[0x250] = 0x250
        cls.identity = cls.images / 'identity.img'
        shutil.copyfile(formatted, cls.identity)
        identity = Volume(cls.identity)
        identity.replace_table(struct.pack('<65536H', *identity_map))
        identity.save()
        cls.original_encoded = original.encoded
        # Use a canonical compressed table with the independently selected pair.
        words = []
        code = 0
        while code < 65536:
            end = code
            while end < 65536 and cls.mapping[end] == end and end - code < 65535:
                end += 1
            if end - code >= 2:
                words.extend([65535, end - code])
                code = end
            else:
                words.append(cls.mapping[code])
                code += 1
        cls.compressed = struct.pack('<' + 'H' * len(words), *words)
        cls.oracle = cls.target / 'custom-mappings.bin'
        cls.oracle.write_bytes(struct.pack('<65536H', *cls.mapping))
        cls.preimage_hashes = {str(path): cls.digest(path) for path in [cls.identity, cls.custom]}

    @staticmethod
    def digest(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    @classmethod
    def call(cls, command, label, timeout=120):
        result = subprocess.run(list(map(str, command)), capture_output=True, timeout=timeout,
                                env={**os.environ, 'ASAN_OPTIONS': 'halt_on_error=1',
                                     'UBSAN_OPTIONS': 'halt_on_error=1'})
        (cls.target / (label + '.stdout')).write_bytes(result.stdout)
        (cls.target / (label + '.stderr')).write_bytes(result.stderr)
        cls.commands.append({'label': label, 'command': list(map(str, command)),
                             'exit': result.returncode,
                             'stdout': result.stdout.decode(errors='replace'),
                             'stderr': result.stderr.decode(errors='replace')})
        (cls.target / 'commands.json').write_text(json.dumps(cls.commands, indent=2) + '\n')
        assert result.returncode == 0 and not result.stderr, (label, result.stdout, result.stderr)
        return json.loads(result.stdout) if result.stdout else None

    @classmethod
    def tearDownClass(cls):
        for path, digest in cls.preimage_hashes.items():
            assert cls.digest(path) == digest

    def setUp(self):
        if BASELINE and self._testMethodName != 'test_baseline_wrong_case_lookup':
            self.skipTest('baseline run selects only the pre-correction control')

    def test_baseline_wrong_case_lookup(self):
        if not BASELINE:
            self.skipTest('run against the public parent with SAROO_UPCASE_BASELINE=1')
        result = inspect(self.custom)
        self.assertFalse(result['invalid_name_hashes'])
        self.assertEqual(result['mapping_0250'], 0x2c6f)
        self.call([self.binaries['generic'], self.custom, 'lookup-old'], 'baseline-lookup')

    def copy(self, name, source=None):
        path = self.images / (name + '.img')
        shutil.copyfile(source or self.custom, path)
        return path

    def test_complete_mapping_vectors_compressed_literal_fragmented_and_later_root(self):
        literal = self.oracle.read_bytes()
        cases = [('compressed', self.compressed, False, False),
                 ('literal', literal, False, False),
                 ('fragmented', literal, True, False),
                 ('later-root', self.compressed, False, True),
                 ('empty-prefix', struct.pack('<HH', 65535, 0) * 70000 + self.compressed, False, False),
                 ('final-one', literal[:-2] + struct.pack('<HH', 65535, 1), False, False),
                 ('trailing-empty', literal + struct.pack('<HH', 65535, 0), False, False)]
        for name, encoded, fragmented, later in cases:
            with self.subTest(name=name):
                path = self.copy(name)
                volume = Volume(path)
                volume.replace_table(encoded, fragmented)
                if later:
                    volume.move_root_table()
                volume.save()
                digest = self.digest(path)
                self.call([self.binaries['generic'], path, 'maps', self.oracle], 'maps-' + name)
                self.assertEqual(self.digest(path), digest)
                # The raw oracle's strict decoder covers canonical forms; empty
                # marker tolerance uses the explicit complete mapping vector.
                if name in ['compressed', 'literal', 'fragmented', 'later-root']:
                    self.assertFalse(inspect(path)['invalid_name_hashes'])

    def test_readonly_reader_keeps_each_volume_authority(self):
        for name, path in [('custom', self.custom), ('identity', self.identity)]:
            oracle = self.target / (name + '-readonly.bin')
            oracle.write_bytes(struct.pack('<65536H', *Volume(path).maps()))
            self.call([self.binaries['readonly'], path, 'maps', oracle], 'readonly-' + name)

    def test_two_volume_threaded_reads_keep_different_case_authorities(self):
        self.call([self.binaries['two'], self.identity, 'two-volumes', self.custom], 'two-volumes')

    def test_create_rename_nested_directory_and_unlink_keep_raw_metadata_valid(self):
        path = self.copy('operations')
        self.call([self.binaries['generic'], path, 'ops'], 'operations')
        result = inspect(path)
        self.assertFalse(result['invalid_name_hashes'])
        self.assertFalse(result['case_equivalent_groups'])
        self.assertEqual({item['name'] for item in result['files']}, {'Ɐ.BIN', 'KEEP.BIN', 'ɐ.REN'})
        self.assertTrue(result['no_crosslinks_or_orphans'])
        (self.target / 'operations-oracle.json').write_text(json.dumps(result, indent=2) + '\n')

    def test_invalid_table_contracts_refuse_without_writes(self):
        for name in ['checksum', 'ascii', 'short', 'excess', 'missing', 'duplicate',
                     'unallocated', 'cycle', 'wide-length']:
            with self.subTest(name=name):
                path = self.copy('bad-' + name)
                volume = Volume(path)
                if name == 'checksum':
                    volume.data[volume.table + 4] ^= 1
                elif name == 'ascii':
                    mappings = self.mapping[:]
                    mappings[97] = 66
                    volume.replace_table(struct.pack('<65536H', *mappings))
                elif name == 'short':
                    volume.replace_table(self.compressed[:-2])
                elif name == 'excess':
                    volume.replace_table(self.compressed + b'\0\0')
                elif name == 'missing':
                    volume.data[volume.table] = 2
                elif name == 'duplicate':
                    volume.data[volume.end:volume.end + 32] = volume.data[volume.table:volume.table + 32]
                elif name == 'unallocated':
                    volume.allocated(volume.table_clusters[0], False)
                elif name == 'cycle':
                    volume.put32(volume.fat * 512 + volume.table_clusters[0] * 4, volume.table_clusters[0])
                else:
                    volume.put64(volume.table + 24, (volume.count + 1) * volume.cs * 512)
                volume.save()
                digest = self.digest(path)
                self.call([self.binaries['generic'], path, 'refuse'], 'refuse-' + name)
                self.assertEqual(self.digest(path), digest)

    def test_each_mount_read_failure_refuses_cleanly_and_retries(self):
        probe = self.call([self.binaries['generic'], self.custom, 'mount'], 'mount-probe')
        for ordinal in range(1, probe['mount_reads'] + 1):
            with self.subTest(ordinal=ordinal):
                self.call([self.binaries['generic'], self.custom, 'mount-fault', ordinal],
                          'mount-fault-' + str(ordinal))

    def test_each_lookup_read_failure_preserves_data_and_retries(self):
        probe = self.call([self.binaries['generic'], self.custom, 'lookup'], 'lookup-probe')
        for ordinal in range(1, probe['reads'] + 1):
            with self.subTest(ordinal=ordinal):
                self.call([self.binaries['generic'], self.custom, 'lookup-fault', ordinal],
                          'lookup-fault-' + str(ordinal))

    def test_creation_mapping_read_failures_precede_metadata_writes(self):
        path = self.copy('create-probe')
        volume = Volume(path)
        low = volume.at(volume.table_clusters[0]) // 512
        high = volume.at(volume.table_clusters[-1]) // 512 + volume.cs
        probe = self.call([self.binaries['generic'], path, 'create-probe', low, high, 0], 'create-probe')
        for ordinal in range(1, probe['table_reads'] + 1):
            with self.subTest(ordinal=ordinal):
                image = self.copy('create-fault-' + str(ordinal))
                digest = self.digest(image)
                self.call([self.binaries['generic'], image, 'create-fault', low, high, ordinal],
                          'create-fault-' + str(ordinal))
                self.assertEqual(self.digest(image), digest)


if __name__ == '__main__':
    unittest.main()
