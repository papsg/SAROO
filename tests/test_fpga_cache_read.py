"""Verify read completion and SDRAM command counts using production FPGA RTL."""
# Adapted from paps-g 6b156a19.
import itertools
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import MODULES, ROOT, build_memory_model


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGACacheReadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_FPGA_CACHE_READ_SOURCE', ROOT / 'FPGA'))
        output = ROOT / 'tests/build/fpga-cache-read'
        output.mkdir(parents=True, exist_ok=True)
        target = Path(tempfile.mkdtemp(prefix='case-', dir=output))
        inputs = {name: (source / name).read_bytes() for name in MODULES}
        cls.binary = build_memory_model(inputs, target / 'rtl', 'fpga_cache_read_tb.v')

    def check_case(self, **options):
        with self.subTest(**options):
            args = [f'+{name}={value}' for name, value in options.items()]
            result = subprocess.run(['vvp', str(self.binary), *args],
                                    capture_output=True, text=True, timeout=20)
            log = result.stdout + result.stderr
            self.assertEqual(result.returncode, 0, log)
            self.assertIn('PASS valid cache reads', log)

    def test_reset_tag_regressions(self):
        for port in range(2):
            self.check_case(mode=0, port=port)

    def test_partial_fill_regressions(self):
        for port in range(2):
            self.check_case(port=port)

    def test_valid_controls(self):
        for port in range(2):
            self.check_case(port=port, gap=8)
            self.check_case(mode=2, port=port)

    def test_reset_tag_all_words(self):
        for port, word in itertools.product(range(2), range(4)):
            self.check_case(mode=0, port=port, first_word=word)

    def test_same_line_waits_and_hits_share_one_fill(self):
        for port, first, following, gap in itertools.product(range(2), range(4), range(4), (0, 1, 2, 3, 8)):
            self.check_case(port=port, first_word=first, next_word=following, gap=gap)

    def test_successive_misses_keep_burst_ownership(self):
        for port, first, following in itertools.product(range(2), range(4), range(4)):
            self.check_case(mode=2, port=port, first_word=first, next_word=following)


if __name__ == '__main__':
    unittest.main()
