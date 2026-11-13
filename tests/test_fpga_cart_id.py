"""Check the terminal extended-RAM cartridge ID word."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest

from test_fpga_fmc_stream import build_top, fpga_sources
from test_fpga_memory import ROOT


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGACartridgeIdTests(unittest.TestCase):
    def test_terminal_cs1_word_in_all_cartridge_modes(self):
        source = Path(os.environ.get('SAROO_FPGA_CART_ID_SOURCE', ROOT / 'FPGA'))
        binary = build_top(self, fpga_sources(source), 'fpga_cart_id_tb.v',
                           fixture_flags=('-DSAROO_ABUS_TEST',),
                           extra_fixtures=('fpga_fmc_stream_tb.v',))
        result = subprocess.run(['vvp', binary], capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS terminal CS1 word and 32767 neighboring words', result.stdout)


if __name__ == '__main__':
    unittest.main()
