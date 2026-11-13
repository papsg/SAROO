"""Verify transceiver direction and isolation from public FPGA source."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import ROOT


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGADataDirectionTests(unittest.TestCase):
    def test_transceiver_direction_precedes_chip_select(self):
        source = Path(os.environ.get('SAROO_FPGA_DIRECTION_SOURCE', ROOT / 'FPGA'))
        raw = (source / 'SSMaster.v').read_bytes()
        equations = re.findall(rb'^\s*assign SS_DATA_(?:DIR|OE)\s*=.*?;', raw, re.M | re.S)
        self.assertEqual(len(equations), 2)
        module = b'''module data_direction_dut(
    input SS_FC1, SS_CS0, SS_CS1, SS_CS2, SS_RD, SS_WR0, SS_WR1,
    input ss_cdc_cs, ss_cdc_en, ss_bus_ready, output SS_DATA_DIR, SS_DATA_OE
);
''' + b'\n'.join(equations) + b'\nendmodule\n'
        with tempfile.TemporaryDirectory() as out:
            target = Path(out)
            fixture = target / 'direction.v'
            fixture.write_bytes(module)
            binary = target / 'direction.vvp'
            result = subprocess.run(
                ['iverilog', '-g2012', '-s', 'tb', '-o', binary, fixture,
                 ROOT / 'tests/fpga_data_direction_tb.v'],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run(['vvp', binary], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PASS 15 Saturn data-direction and isolation cases', result.stdout)


if __name__ == '__main__':
    unittest.main()
