"""Independent raw-sector oracle and cross-language SSO1 checks."""
# Adapted from paps-g f7791566. All disc sectors here are synthetic.
import copy
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[1]
PRODUCT = b'T-TEST    '
RAW_BYTES = 2352
HEADER_BYTES = 24
RECORD_BYTES = 348


def gf_mul(a, b):
    value = 0
    while b:
        if b & 1:
            value ^= a
        b >>= 1
        a <<= 1
        if a & 256:
            a ^= 0x11d
    return value


def cd_parity(raw, rows):
    """Solve the two RS equations for each P or Q word over GF(256)."""
    total = bytearray()
    weighted = bytearray()
    for row in rows:
        a = b = 0
        for offset in row:
            a ^= raw[offset]
            b = gf_mul(b, 2) ^ raw[offset]
        # Zero value and weighted sums after appending the two parity bytes.
        p = gf_mul(gf_mul(b, 4) ^ a, 244)  # inverse of 2 XOR 1
        total.append(p)
        weighted.append(p ^ a)
    return total + weighted


def canonical(raw):
    result = bytearray(raw)
    edc = 0
    for byte in raw[:2064]:
        for bit in range(8):
            carry = (edc ^ (byte >> bit)) & 1
            edc = (edc >> 1) ^ (0xd8018001 if carry else 0)
    result[2064:2068] = edc.to_bytes(4, 'little')
    result[2068:2076] = bytes(8)
    result[2076:2248] = cd_parity(result, (
        range(12 + column, 2076, 86) for column in range(86)))
    result[2248:] = cd_parity(result, (
        [12 + (86 * (row // 2) + row % 2 + 88 * column) % 2236
         for column in range(43)] for row in range(52)))
    return bytes(result)


def sector(fad, byte=0x71, ip=False, version=b'V1.000'):
    raw = bytearray(2352)
    raw[:12] = b'\0' + b'\xff' * 10 + b'\0'
    raw[12:16] = bytes.fromhex(f'{fad // 4500:02}{fad // 75 % 60:02}{fad % 75:02}01')
    raw[16:2064] = bytes([byte]) * 2048
    if ip:
        raw[16:32] = b'SEGA SEGASATURN '
        raw[48:64] = PRODUCT + version
    return canonical(raw)


def seal(pack):
    pack[20:24] = bytes(4)
    pack[20:24] = zlib.crc32(pack).to_bytes(4, 'big')


class SectorOverlayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/sector-overlay'
        build.mkdir(parents=True, exist_ok=True)
        cls.output = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        mcu = ROOT / 'Firm_MCU/Saturn'
        cls.decoder = cls.output / 'decoder'
        cls.tool = ROOT / 'tools/target/debug/sector-overlay'
        cargo = ['cargo', 'build', '--locked', '--manifest-path', ROOT / 'tools/Cargo.toml',
                 '-p', 'sector-overlay']
        subprocess.run(cargo, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=120)
        flags = ['-O2'] if os.environ.get('SAROO_OVERLAY_PROFILE') == 'release' else [
            '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-Wall', '-Wextra', '-Werror',
                   *flags, '-I', mcu, ROOT / 'tests/sector_overlay.c', mcu / 'sector_overlay.c',
                   '-o', cls.decoder]
        subprocess.run(command, check=True, timeout=60)

    def setUp(self):
        folder = tempfile.TemporaryDirectory(dir=self.output)
        self.addCleanup(folder.cleanup)
        self.path = Path(folder.name)
        self.cue = self.path / 'fixture.cue'
        self.track = self.path / 'mixed data.bin'
        self.output_pack = self.path / f'{zlib.crc32(PRODUCT):08X}.SOV'
        self.recipe_path = self.path / 'recipe.json'
        self.first = sector(150, ip=True)
        self.last = sector(156)
        # First FILE: two raw sectors, one cooked sector at INDEX 00
        # and two at INDEX 01; then a generated postgap. The second FILE
        # starts immediately after that postgap.
        (self.path / 'first.bin').write_bytes(self.first + sector(151) + bytes(3 * 2048))
        self.track.write_bytes(self.last + sector(157, byte=0x72))
        self.cue.write_text(
            '\ufeffFILE "first.bin" BINARY\r\n'
            '  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n'
            '  TRACK 02 MODE2/2048\r\n    INDEX 00 00:00:02\r\n'
            '    INDEX 01 00:00:03\r\n    POSTGAP 00:00:01\r\n'
            'FILE "mixed data.bin" BINARY\r\n  TRACK 03 MODE1/2352\r\n'
            '    PREGAP 00:00:00\r\n    INDEX 01 00:00:00\r\n')
        self.recipe = {
            'abi': 2, 'product_id': 'T-TEST', 'title': 'Synthetic disc', 'origin': 'Public test',
            'summary': 'Change two bytes', 'rationale': 'Check complete sectors',
            'tracks': [{'number': 3, 'sha256': hashlib.sha256(self.track.read_bytes()).hexdigest()}],
            'writes': [{'fad': 156, 'offset': 1057, 'before': '7171', 'after': 'ab10',
                        'effect': 'Replace two test bytes', 'reason': 'Check raw parity'}],
        }

    def compile(self, expected=True):
        self.recipe_path.write_text(json.dumps(self.recipe))
        result = subprocess.run([self.tool, 'compile', self.recipe_path, self.cue, self.output_pack],
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode == 0, expected, result.stderr + result.stdout)
        return result

    def run_decoder(self, *args):
        result = subprocess.run([self.decoder, *args], capture_output=True, text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_compiler_decoder_and_independent_raw_tail(self):
        original_hash = hashlib.sha256(self.track.read_bytes()).digest()
        self.compile()
        pack = self.output_pack.read_bytes()
        self.assertEqual(pack[:20], b'SSO1\x01\x01\0\0' + PRODUCT + b'\x01\x74')
        self.assertEqual(int.from_bytes(pack[24:28], 'big'), 156)
        self.assertEqual(int.from_bytes(pack[28:32], 'big'), zlib.crc32(self.last))
        expected = bytearray(self.last)
        expected[1057:1059] = b'\xab\x10'
        expected = canonical(expected)
        self.assertEqual(pack[84:], expected[2064:])
        (self.path / 'original').write_bytes(self.last)
        (self.path / 'expected').write_bytes(expected)
        self.run_decoder('apply', self.output_pack, self.path / 'original', self.path / 'expected')
        self.compile(False)  # Never overwrite an existing pack or source.
        self.assertEqual(self.output_pack.read_bytes(), pack)
        self.assertEqual(hashlib.sha256(self.track.read_bytes()).digest(), original_hash)

    def test_wrong_media_preimage_geometry_or_overlap_creates_no_pack(self):
        original = copy.deepcopy(self.recipe)
        for field, value in [('before', '1212'), ('fad', 155), ('offset', 2063),
                             ('fad', 157), ('after', '7171')]:
            with self.subTest(field=field, value=value):
                self.recipe = copy.deepcopy(original)
                self.recipe['writes'][0][field] = value
                self.compile(False)
                self.assertFalse(self.output_pack.exists())
        self.recipe = copy.deepcopy(original)
        self.recipe['writes'] += self.recipe['writes']
        self.compile(False)
        self.recipe = copy.deepcopy(original)
        self.recipe['tracks'][0]['sha256'] = '0' * 64
        self.compile(False)
        self.recipe = copy.deepcopy(original)
        self.recipe['product_id'] = 'WRONG'
        self.compile(False)

    def test_decoder_rejects_structural_errors_with_valid_pack_checksum(self):
        self.compile()
        pack = self.output_pack.read_bytes()
        for at, value in [(4, 4), (5, 13), (6, 1), (7, 0x80), (32, 5), (33, 1),
                          (36, 8), (38, 3), (39, 1), (56, 1), (88, 1)]:
            with self.subTest(at=at, value=value):
                corrupt = bytearray(pack)
                corrupt[at] = value
                seal(corrupt)
                self.output_pack.write_bytes(corrupt)
                self.run_decoder('reject', self.output_pack)
        self.output_pack.write_bytes(pack[:-1])
        self.run_decoder('reject', self.output_pack)
        self.output_pack.write_bytes(pack[:-1] + bytes([pack[-1] ^ 1]))
        self.run_decoder('reject', self.output_pack)

    def test_twelve_records_include_the_last_sector_and_reject_thirteen(self):
        sectors = [sector(fad, ip=fad == 150) for fad in range(150, 164)]
        self.track.write_bytes(b''.join(sectors))
        self.cue.write_text('FILE "mixed data.bin" BINARY\n'
                            '  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n')
        self.recipe['tracks'] = [{'number': 1, 'sha256':
                                  hashlib.sha256(self.track.read_bytes()).hexdigest()}]
        self.recipe['writes'] = [dict(self.recipe['writes'][0], fad=fad)
                                 for fad in range(162, 150, -1)]
        self.compile()
        pack = self.output_pack.read_bytes()
        self.assertEqual(len(pack), 4200)
        self.assertEqual(pack[5], 12)
        for index in range(12):
            offset = HEADER_BYTES + index * RECORD_BYTES
            self.assertEqual(int.from_bytes(pack[offset:offset + 4], 'big'), 151 + index)
        original = self.path / 'original'
        expected = self.path / 'expected'
        original.write_bytes(sectors[12])
        edited = bytearray(sectors[12])
        edited[1057:1059] = b'\xab\x10'
        expected.write_bytes(canonical(edited))
        self.run_decoder('apply', self.output_pack, original, expected)
        self.output_pack.unlink()
        self.recipe['writes'].append(dict(self.recipe['writes'][0], fad=163))
        self.compile(False)
        self.assertFalse(self.output_pack.exists())

    def test_exact_ip_version_changes_only_the_filename(self):
        self.compile()
        legacy = self.output_pack.read_bytes()
        disc_id = PRODUCT + b'V1.000'
        self.recipe['disc_id'] = disc_id.decode()
        self.output_pack = self.path / f'{zlib.crc32(disc_id):08X}.SOV'
        self.compile()
        self.assertEqual(self.output_pack.read_bytes(), legacy)
        self.assertEqual(subprocess.check_output(
            [self.tool, 'name', self.recipe_path], text=True, timeout=10).strip(),
            self.output_pack.name)
        for invalid in ('WRONG     V1.000', 'T-TEST    V1.00\n', 'T-TEST V1.000',
                        'T-TEST    V1.001'):
            with self.subTest(disc_id=repr(invalid)):
                self.recipe['disc_id'] = invalid
                self.output_pack = self.path / f'{zlib.crc32(invalid.encode()):08X}.SOV'
                self.compile(False)
                self.assertFalse(self.output_pack.exists())

    def test_contiguous_edit_and_guard_refuse_changed_data_without_writes(self):
        self.recipe['writes'][0].update(before='71' * 12, after='aa' * 12)
        self.recipe['guards'] = [{'fad': 157, 'reason': 'Bind the neighboring literal'}]
        self.recipe['replaces_callback'] = True
        self.compile()
        pack = self.output_pack.read_bytes()
        self.assertEqual(pack[4:8], bytes([3, 2, 0, 0x80]))
        self.assertEqual(pack[32], 0x80 | 12)
        original = self.path / 'original'
        expected = self.path / 'expected'
        original.write_bytes(self.last)
        changed = bytearray(self.last)
        changed[1057:1069] = bytes([0xaa]) * 12
        expected.write_bytes(canonical(changed))
        self.run_decoder('apply', self.output_pack, original, expected, '0')
        original.write_bytes(sector(157, 0x72))
        expected.write_bytes(original.read_bytes())
        self.run_decoder('guard', self.output_pack, original, expected, '1')
        for at, value in ((33, 1), (38, 1), (36, 255), (32, 0x80 | 45),
                          (RECORD_BYTES + 84, 1)):
            with self.subTest(at=at):
                bad = bytearray(pack)
                bad[at] = value
                seal(bad)
                self.output_pack.write_bytes(bad)
                self.run_decoder('reject', self.output_pack)

    def test_long_replacements_require_a_single_bounded_edit(self):
        original = copy.deepcopy(self.recipe)
        for width in (5, 12, 40, 44):
            self.recipe = copy.deepcopy(original)
            self.recipe['writes'][0].update(before='71' * width, after='aa' * width)
            self.compile()
            raw, edited = self.path / 'original', self.path / 'expected'
            raw.write_bytes(self.last)
            value = bytearray(self.last)
            value[1057:1057 + width] = bytes([0xaa]) * width
            edited.write_bytes(canonical(value))
            self.run_decoder('apply', self.output_pack, raw, edited)
            self.output_pack.unlink()
        for change in ('too-long', 'past-data', 'same-sector-edit', 'edited-guard',
                       'duplicate-guard'):
            with self.subTest(change=change):
                self.recipe = copy.deepcopy(original)
                self.recipe['writes'][0].update(before='71' * 12, after='aa' * 12)
                if change == 'too-long':
                    self.recipe['writes'][0].update(before='71' * 45, after='aa' * 45)
                elif change == 'past-data':
                    self.recipe['writes'][0]['offset'] = 2060
                elif change == 'same-sector-edit':
                    self.recipe['writes'].append(dict(original['writes'][0], offset=1200))
                else:
                    self.recipe['guards'] = [{'fad': 156 if change == 'edited-guard' else 157,
                                             'reason': 'Guard collision'}]
                    if change == 'duplicate-guard':
                        self.recipe['guards'] *= 2
                self.compile(False)
                self.assertFalse(self.output_pack.exists())

    def test_guard_only_abi_two_keeps_short_edits_and_original_guard_sectors(self):
        self.recipe['guards'] = [{'fad': 157, 'reason': 'Bind another original sector'}]
        self.compile()
        self.assertEqual(self.output_pack.read_bytes()[4:8], bytes([2, 2, 0, 0]))
        original = self.path / 'original'
        expected = self.path / 'expected'
        original.write_bytes(sector(157, 0x72))
        expected.write_bytes(original.read_bytes())
        self.run_decoder('guard', self.output_pack, original, expected, '1')

    def test_bundle_compiles_each_source_and_preserves_original_sector_guards(self):
        second = self.path / 'other'
        second.mkdir()
        other_sector = sector(156, 0x73)
        other_cue = second / 'disc.cue'
        # Use a generated gap to retain FAD 156 without a large data file.
        (second / 'ip.bin').write_bytes(self.first)
        (second / 'other.bin').write_bytes(other_sector)
        other_cue.write_text('FILE "ip.bin" BINARY\n  TRACK 01 MODE1/2352\n'
                             '    INDEX 01 00:00:00\n    POSTGAP 00:00:05\n'
                             'FILE "other.bin" BINARY\n  TRACK 02 MODE1/2352\n'
                             '    INDEX 01 00:00:00\n')
        other = copy.deepcopy(self.recipe)
        other['writes'][0]['before'] = '7373'
        other['tracks'] = [{'number': 2, 'sha256': hashlib.sha256(other_sector).hexdigest()}]
        other_path = second / 'recipe.json'
        other_path.write_text(json.dumps(other))
        self.recipe_path.write_text(json.dumps(self.recipe))
        command = [self.tool, 'bundle', self.output_pack, self.recipe_path, self.cue,
                   other_path, other_cue]
        result = subprocess.run(command, text=True, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        pack = self.output_pack.read_bytes()
        self.assertEqual(pack[4:8], bytes([1, 2, 1, 0]))
        original, edited = self.path / 'original', self.path / 'expected'
        records = [pack[HEADER_BYTES + i * RECORD_BYTES:
                        HEADER_BYTES + (i + 1) * RECORD_BYTES] for i in range(2)]
        for source in (self.last, other_sector):
            original.write_bytes(source)
            value = bytearray(source)
            value[1057:1059] = b'\xab\x10'
            edited.write_bytes(canonical(value))
            index = next(i for i, r in enumerate(records) if
                         int.from_bytes(r[4:8], 'big') == zlib.crc32(source))
            self.run_decoder('apply', self.output_pack, original, edited, str(index))
        self.output_pack.unlink()
        result = subprocess.run([*command[:5], self.recipe_path, self.cue],
                                text=True, capture_output=True, timeout=20)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.output_pack.exists())
        for field, value in (('replaces_callback', True), ('disc_id', 'T-TEST    V1.000'),
                             ('product_id', 'WRONG')):
            bad = copy.deepcopy(other)
            bad[field] = value
            other_path.write_text(json.dumps(bad))
            result = subprocess.run(command, text=True, capture_output=True, timeout=20)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(self.output_pack.exists())

    def test_bundle_selects_the_twelfth_record_and_refuses_a_thirteenth(self):
        command = [self.tool, 'bundle', self.output_pack]
        originals = {}
        for index in range(13):
            path = self.path / str(index)
            path.mkdir()
            raw = sector(151, 0x71 + index)
            originals[zlib.crc32(raw)] = raw
            (path / 'track.bin').write_bytes(self.first + raw)
            cue = path / 'source.cue'
            cue.write_text('FILE "track.bin" BINARY\n  TRACK 01 MODE1/2352\n'
                           '    INDEX 01 00:00:00\n')
            recipe = copy.deepcopy(self.recipe)
            recipe['writes'][0].update(fad=151, before=f'{0x71 + index:02x}' * 2)
            recipe['tracks'] = [{'number': 1, 'sha256':
                                 hashlib.sha256((path / 'track.bin').read_bytes()).hexdigest()}]
            recipe_path = path / 'source.json'
            recipe_path.write_text(json.dumps(recipe))
            command += [recipe_path, cue]
        result = subprocess.run(command[:-2], text=True, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        pack = self.output_pack.read_bytes()
        self.assertEqual(len(pack), 4200)
        self.assertEqual(pack[5:7], bytes([12, 1]))
        last_record = pack[HEADER_BYTES + 11 * RECORD_BYTES:]
        raw = originals[int.from_bytes(last_record[4:8], 'big')]
        original, edited = self.path / 'original', self.path / 'expected'
        original.write_bytes(raw)
        changed = bytearray(raw)
        changed[1057:1059] = b'\xab\x10'
        edited.write_bytes(canonical(changed))
        self.run_decoder('apply', self.output_pack, original, edited, '11')
        self.output_pack.unlink()
        result = subprocess.run(command, text=True, capture_output=True, timeout=20)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.output_pack.exists())
