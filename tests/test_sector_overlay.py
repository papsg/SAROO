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


def sector(fad, byte=0x71, ip=False):
    raw = bytearray(2352)
    raw[:12] = b'\0' + b'\xff' * 10 + b'\0'
    raw[12:16] = bytes.fromhex(f'{fad // 4500:02}{fad // 75 % 60:02}{fad % 75:02}01')
    raw[16:2064] = bytes([byte]) * 2048
    if ip:
        raw[16:32] = b'SEGA SEGASATURN '
        raw[48:64] = PRODUCT + b'V1.000'
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
        for at, value in [(4, 4), (5, 9), (6, 1), (7, 0x80), (32, 5), (33, 1),
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
