"""Check the exact pinned FatFs case mapping and its compact range bounds."""
import hashlib
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
FATFS = ROOT / 'Firm_MCU/FatFS'
UNICODE_VALUES = 0x110000
OUTSIDE = (0x110000, 0xffffff, 0x7fffffff, 0x80000000, 0xffffffff)
# Little-endian DWORD outputs from the original FatFs at ba387b08.
PINNED_MAPPING = 'd5ffdd7b8d6fac63f11ac503ced440c4da7876dd18a8444a401c8bed525590fa'


class UnicodeRangeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/fatfs-case'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        source = Path(os.environ.get('SAROO_FATFS_CASE_SOURCE', FATFS / 'ffunicode.c'))
        cls.source = source.read_text()
        candidate = cls.target / 'ffunicode.c'
        candidate.write_bytes(source.read_bytes())
        cls.binary = cls.compile_map(candidate, 'case-map')

    @classmethod
    def compile_map(cls, source, name):
        binary = cls.target / name
        flags = ['-O2'] if os.environ.get('SAROO_FILE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        args = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall', '-Wextra',
                '-Werror', *flags, '-I', str(FATFS), str(ROOT / 'tests/fatfs_case.c'),
                str(source), '-o', str(binary)]
        run = subprocess.run(args, capture_output=True, text=True, timeout=60)
        (cls.target / (name + '-compile.log')).write_text(run.stdout + run.stderr)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        return binary

    def mapping(self, binary):
        output = binary.with_suffix('.bin')
        run = subprocess.run([str(binary), str(output)], capture_output=True, text=True,
                             timeout=30, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        return output.read_bytes()

    def test_all_unicode_values_and_out_of_range_inputs_match_pinned_fatfs(self):
        actual = self.mapping(self.binary)
        self.assertEqual(len(actual), (UNICODE_VALUES + len(OUTSIDE)) * 4)
        self.assertEqual(hashlib.sha256(actual).hexdigest(), PINNED_MAPPING)
        self.assertEqual(struct.unpack('<5I', actual[-20:]), OUTSIDE)
        changed = sum(i != value for i, (value,) in
                      enumerate(struct.iter_unpack('<I', actual[:UNICODE_VALUES * 4])))
        self.assertEqual(changed, 874)

    def test_reversing_the_pair_rule_changes_the_pinned_mapping(self):
        old = 'uni - ((offset + 1) & 1)'
        self.assertEqual(self.source.count(old), 1)
        mutant = self.target / 'wrong-pair.c'
        mutant.write_text(self.source.replace(old, 'uni - (offset & 1)'))
        changed = self.mapping(self.compile_map(mutant, 'wrong-pair'))
        self.assertNotEqual(hashlib.sha256(changed).hexdigest(), PINNED_MAPPING)
        self.assertEqual(struct.unpack_from('<I', changed, 0x0101 * 4)[0], 0x0101)

    def test_ranges_are_disjoint_and_binary_search_needs_at_most_seven_steps(self):
        table = re.search(r'uppercase_ranges\[\] = \{(.*?)\};', self.source, re.S)[1]
        records = [(int(a, 16), int(b, 16)) for a, b in
                   re.findall(r'\{(0x[0-9a-f]+), (0x[0-9a-f]+)\}', table)]
        deltas = re.search(r'uppercase_deltas\[\] = \{([^}]+)\}', self.source)[1]
        deltas = [int(delta) for delta in deltas.split(',')]
        self.assertEqual(len(records), 118)
        self.assertEqual(len(deltas), 18)
        self.assertLessEqual(len(records).bit_length(), 7)

        previous_end = 0
        for start, descriptor in records:
            self.assertLessEqual(previous_end, start)
            span = 1
            if descriptor & 0x8000:
                span += (descriptor >> 7) & 255
                rule = descriptor & 127
                self.assertLessEqual(rule, len(deltas))
                delta = deltas[rule - 1] if rule else -1
                self.assertGreaterEqual(start + delta, 0)
                self.assertLess(start + span - 1 + delta, 0x10000)
            previous_end = start + span
            self.assertLessEqual(previous_end, 0x10000)


if __name__ == '__main__':
    unittest.main()
