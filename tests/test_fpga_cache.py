"""Compare accepted writes and cache reads using the production FPGA modules."""
import itertools
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import MODULES, ROOT, build_memory_model


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGACacheTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_FPGA_CACHE_SOURCE', ROOT / 'FPGA'))
        output = ROOT / 'tests/build/fpga-cache'
        output.mkdir(parents=True, exist_ok=True)
        target = Path(tempfile.mkdtemp(prefix='case-', dir=output))
        cls.binary = build_memory_model({name: (source / name).read_bytes() for name in MODULES},
                                        target / 'rtl', 'fpga_cache_tb.v')

    def check_case(self, **options):
        with self.subTest(**options):
            args = [f'+{name}={value}' for name, value in options.items()]
            result = subprocess.run(['vvp', str(self.binary), *args],
                                    capture_output=True, text=True, timeout=20)
            log = result.stdout + result.stderr
            self.assertEqual(result.returncode, 0, log)
            self.assertIn('PASS ', log)

    def test_simultaneous_write_regressions(self):
        self.check_case(word_a=0, word_b=1)
        self.check_case(word_a=0, word_b=0)
        self.check_case(word_a=2, word_b=2, mask_a=1, mask_b=2)

    def test_write_during_fill_regressions(self):
        for reader in range(2):
            self.check_case(word_a=2, first=reader, fill_phase=0)

    def test_valid_controls_with_masked_or_well_separated_writes(self):
        self.check_case(mask_a=3, mask_b=3)
        for first in range(2):
            self.check_case(first=first, skew=32, mask_a=1, mask_b=2)

    def test_all_word_and_mask_pairs(self):
        for a, b, mask_a, mask_b in itertools.product(range(4), repeat=4):
            self.check_case(word_a=a, word_b=b, mask_a=mask_a, mask_b=mask_b)

    def test_both_write_arrival_orders(self):
        for first, skew, words in itertools.product(range(2), (1, 2, 4, 8, 16, 32), ((0, 1), (3, 3))):
            self.check_case(first=first, skew=skew, word_a=words[0], word_b=words[1], mask_a=1, mask_b=2)

    def test_write_across_all_fill_phases(self):
        for reader, phase, word, mask in itertools.product(range(2), range(4), range(4), range(4)):
            self.check_case(first=reader, fill_phase=phase, word_a=word, mask_a=mask)


if __name__ == '__main__':
    unittest.main()
