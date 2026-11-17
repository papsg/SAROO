"""Selected exFAT boot-region integrity before any geometry or data use.

Adapted from paps-g SSR-164 / a817842e for the public FatFs layout.
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
import zlib

ROOT = Path(__file__).resolve().parents[1]
BASELINE = os.environ.get('SAROO_BOOT_CHECKSUM_BASELINE') == '1'


def checksum(data):
    total = 0
    for index, value in enumerate(data):
        if index not in (106, 107, 112):
            total = (((total & 1) << 31) + (total >> 1) + value) & 0xffffffff
    return total


def update_boot(path, base, size, **fields):
    with path.open('r+b') as image:
        for region in (base, base + 12):
            image.seek(region * size)
            boot = bytearray(image.read(11 * size))
            for offset, value in fields.items():
                struct.pack_into('<Q', boot, int(offset), value)
            image.seek(region * size)
            image.write(boot)
            image.write(struct.pack('<I', checksum(boot)) * (size // 4))


class FatFsBootChecksumTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_BOOT_CHECKSUM_SOURCE',
                                     ROOT / 'Firm_MCU/FatFS'))
        cls.target = Path(os.environ.get('SAROO_TEST_BOOT_CHECKSUM_OUTPUT',
                                       ROOT / 'tests/build/fatfs-boot-checksum-test'))
        cls.target.mkdir(parents=True, exist_ok=True)
        cls.commands = []
        cls.binaries = {}
        variants = [('generic', {}), ('readonly', {'FF_FS_READONLY': 1}),
                    ('formatter', {'FF_USE_MKFS': 1}), ('wide', {'FF_MAX_SS': 4096}),
                    ('wide-formatter', {'FF_MAX_SS': 4096, 'FF_USE_MKFS': 1}),
                    ('lba64', {'FF_LBA64': 1})]
        for name, changes in variants:
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
            cls.binaries[name] = folder / 'test-boot'
            cls.call([os.environ.get('CC', 'cc'), '-std=gnu11', '-O1', '-g',
                      '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                      '-fno-omit-frame-pointer', '-pthread', '-DSAROO_FAT32_ONLY=0',
                      *(['-DSSR164_BASELINE'] if BASELINE else []),
                      '-I' + str(folder), ROOT / 'tests/fatfs_boot_checksum.c',
                      folder / 'ffunicode.c', '-o', cls.binaries[name]], 'compile-' + name)
        cls.formatted = {}
        for size in (512, 1024, 2048, 4096):
            path = cls.target / ('formatted-' + str(size) + '.img')
            with path.open('wb') as image:
                image.truncate(64 * 1024**2)
            formatter = 'formatter' if size == 512 else 'wide-formatter'
            cls.call([cls.binaries[formatter], path, 'format', size], 'format-' + str(size))
            with path.open('rb') as image:
                header = image.read(24 * size)
            assert header[108] == size.bit_length() - 1
            for region in (0, 12):
                computed = checksum(header[region * size:(region + 11) * size])
                words = struct.unpack_from('<' + 'I' * (size // 4), header, (region + 11) * size)
                assert all(word == computed for word in words)
            cls.formatted[size] = path
        cls.original_hashes = {str(p): cls.digest(p) for p in cls.formatted.values()}

    @classmethod
    def call(cls, command, label):
        command = list(map(str, command))
        result = subprocess.run(command, capture_output=True, timeout=120,
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

    @staticmethod
    def digest(path):
        return hashlib.sha256(path.read_bytes()).hexdigest()

    @classmethod
    def tearDownClass(cls):
        for name, before in cls.original_hashes.items():
            assert cls.digest(Path(name)) == before

    def setUp(self):
        if BASELINE and self._testMethodName != 'test_baseline_accepts_invalid_boot_checksums':
            self.skipTest('baseline run selects only the pre-correction control')

    def test_baseline_accepts_invalid_boot_checksums(self):
        if not BASELINE:
            self.skipTest('run against the public parent with SAROO_BOOT_CHECKSUM_BASELINE=1')
        path = self.image('baseline-invalid-both')
        self.xor(path, 11 * 512)
        self.xor(path, 23 * 512)
        self.mount(path, 'baseline-invalid-both', 0)

    def image(self, label, size=512, base=0, gpt=False, span=None):
        source = self.formatted[size]
        path = self.target / (label + '.img')
        if not base:
            shutil.copyfile(source, path)
            return path
        count = source.stat().st_size // size
        with path.open('wb') as output, source.open('rb') as original:
            output.seek(base * size)
            shutil.copyfileobj(original, output)
            if gpt:
                output.truncate((base + count + 34) * size)
        update_boot(path, base, size, **{'64': base})
        length = count if span is None else span
        mbr = bytearray(size)
        mbr[510:512] = b'\x55\xaa'
        mbr[450] = 0xee if gpt else 7
        struct.pack_into('<II', mbr, 454, 1 if gpt else base,
                         path.stat().st_size // size - 1 if gpt else length)
        with path.open('r+b') as output:
            output.write(mbr)
            if gpt:
                capacity = path.stat().st_size // size
                entries = bytearray(size)
                entries[:16] = bytes.fromhex('a2a0d0ebe5b9334487c068b6b72699c7')
                entries[16:32] = bytes(range(16))
                struct.pack_into('<QQ', entries, 32, base, base + length - 1)
                # Four complete 128-byte entries, with valid primary/backup CRCs.
                for current, backup, table in ((1, capacity - 1, 2),
                                               (capacity - 1, 1, capacity - 2)):
                    header = bytearray(size)
                    header[:8] = b'EFI PART'
                    struct.pack_into('<III', header, 8, 0x10000, 92, 0)
                    struct.pack_into('<QQQQ', header, 24, current, backup, 34, capacity - 34)
                    header[56:72] = bytes(range(16, 32))
                    struct.pack_into('<QIII', header, 72, table, 4, 128,
                                     zlib.crc32(entries[:512]))
                    struct.pack_into('<I', header, 16, zlib.crc32(header[:92]))
                    output.seek(current * size)
                    output.write(header)
                    output.seek(table * size)
                    output.write(entries)
        return path

    @staticmethod
    def xor(path, offset, mask=1):
        with path.open('r+b') as image:
            image.seek(offset)
            value = image.read(1)
            assert len(value) == 1
            image.seek(offset)
            image.write(bytes([value[0] ^ mask]))

    def mount(self, path, label, expected=0, base=0, size=512, binary='generic',
              mode='mount', fault=0, span=0):
        before = self.digest(path)
        result = self.call([self.binaries[binary], path, mode, base, span,
                            expected, fault, size], label)
        self.assertEqual(self.digest(path), before)
        if mode in ('mount', 'fault'):
            self.assertEqual(result['published'], 4 if not expected else 0)
            self.assertEqual(result['writes'], 0)
            if not expected:
                self.assertEqual(result['volume_base'], base)
            else:
                self.assertEqual(result['metadata_reads'], 0)
        return result

    def test_main_and_backup_validity_policy_and_readonly(self):
        for binary in ('generic', 'readonly'):
            for main in (True, False):
                for backup in (True, False):
                    label = f'policy-{binary}-{main}-{backup}'
                    path = self.image(label)
                    if not main:
                        self.xor(path, 11 * 512)
                    if not backup:
                        self.xor(path, 23 * 512)
                    self.mount(path, label, 0 if main else 13, binary=binary)

    def test_every_checksum_word_at_each_admitted_sector_size(self):
        for size in (512, 1024, 2048, 4096):
            binary = 'generic' if size == 512 else 'wide'
            for region in ('main', 'backup'):
                label = f'words-{size}-{region}'
                path = self.image(label, size=size)
                result = self.mount(path, label, size=size, binary=binary, mode='words-' + region)
                self.assertEqual(result['words_checked'], size // 4)
                self.assertEqual(result['filesystem_writes'], 0)

    def test_covered_sector_bytes_and_first_sector_only_exclusions(self):
        path = self.image('covered')
        for sector in range(11):
            label = f'covered-sector-{sector}'
            # Non-geometry boot code/extended/reserved bytes, including the
            # same offset as an exclusion in a later sector.
            offset = sector * 512 + (120 if sector == 0 else 106)
            self.xor(path, offset)
            self.mount(path, label, 13)
            self.xor(path, offset)
        for offset, value in ((106, 2), (112, 42)):
            label = f'excluded-valid-{offset}'
            with path.open('r+b') as image:
                image.seek(offset)
                original = image.read(1)
                image.seek(offset)
                image.write(bytes([value]))
            self.mount(path, label)
            with path.open('r+b') as image:
                image.seek(offset)
                image.write(original)
        # Nonzero reserved high flag bits are a checksum-only algorithm control,
        # not a claim of a valid complete VolumeFlags field or mount contract.
        self.xor(path, 107)
        self.mount(path, 'excluded-high-byte-gate-only', 1, mode='gate')
        self.xor(path, 107)

    def test_nonzero_mbr_and_gpt_bases_short_extents_and_boot_restoration(self):
        for gpt in (False, True):
            binary = 'lba64' if gpt else 'generic'
            base = 64
            label = 'gpt' if gpt else 'mbr'
            valid = self.image(label, base=base, gpt=gpt)
            result = self.mount(valid, label + '-valid', base=base, binary=binary)
            self.assertIn(base, result['trace'])
            self.xor(valid, (base + 11) * 512 + 508)
            self.mount(valid, label + '-invalid-last-word', 13, base=base, binary=binary)
            short = self.image(label + '-short', base=base, gpt=gpt, span=11)
            result = self.mount(short, label + '-short', 13, base=base, binary=binary)
            self.assertFalse(any(base < sector < base + 24 for sector in result['trace']))

    def test_each_boot_read_error_refuses_and_retries_with_existing_discovery_policy(self):
        for kind in ('sfd', 'mbr', 'gpt'):
            base = 0 if kind == 'sfd' else 64
            binary = 'lba64' if kind == 'gpt' else 'generic'
            path = self.image('fault-' + kind, base=base, gpt=kind == 'gpt')
            probe = self.mount(path, kind + '-fault-probe', base=base, binary=binary)
            prefix = next(i for i, sector in enumerate(probe['trace'])
                          if sector >= base + 24)
            self.assertEqual(prefix, {'sfd': 13, 'mbr': 14, 'gpt': 16}[kind])
            for ordinal in range(1, prefix + 1):
                # Preserve upstream discovery: SFD/direct container errors are
                # disk errors; failed partition candidates may be skipped.
                expected = 1 if kind == 'sfd' or ordinal <= (3 if kind == 'gpt' else 1) else 13
                self.mount(path, f'{kind}-fault-{ordinal}', expected, base=base,
                           binary=binary, mode='fault', fault=ordinal)

    def test_lba_addition_and_known_extent_bounds_precede_reads(self):
        for binary in ('generic', 'wide', 'lba64'):
            result = self.call([self.binaries[binary], 'unused', 'bounds'], 'bounds-' + binary)
            self.assertTrue(result['bounds'])
            self.assertEqual(result['lba_bits'], 64 if binary == 'lba64' else 32)

    def test_discovery_can_skip_a_failed_candidate_for_a_later_valid_volume(self):
        first = 64
        count = self.formatted[512].stat().st_size // 512
        second = first + count + 64
        path = self.image('later-valid-volume', base=first)
        with path.open('r+b') as image, self.formatted[512].open('rb') as source:
            image.seek(second * 512)
            shutil.copyfileobj(source, image)
            entry = bytearray(16)
            entry[4] = 7
            struct.pack_into('<II', entry, 8, second, count)
            image.seek(446 + 16)
            image.write(entry)
        update_boot(path, second, 512, **{'64': second})
        self.xor(path, (first + 11) * 512)
        rejected = self.mount(path, 'later-valid-after-invalid-checksum', base=second)
        self.assertFalse(any(first + 24 <= sector < first + count
                             for sector in rejected['trace']))
        self.xor(path, (first + 11) * 512)
        # First candidate is now valid, but its first additional boot read fails.
        skipped = self.mount(path, 'later-valid-after-first-read-error',
                             base=second, fault=3)
        self.assertEqual(skipped['trace'][:3], [0, first, first + 1])
        self.assertFalse(any(first + 24 <= sector < first + count
                             for sector in skipped['trace']))


if __name__ == '__main__':
    unittest.main()
