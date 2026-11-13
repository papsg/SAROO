"""Check the selected FPGA reset domain and the retained CD state."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest

from test_fpga_fmc_stream import build_top, fpga_sources
from test_fpga_memory import ROOT


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGAClockRetentionTests(unittest.TestCase):
    def run_retention(self, inputs, phase=0):
        binary = build_top(self, inputs, 'fpga_clock_retention_tb.v',
                           fixture_flags=('-DSAROO_ABUS_TEST', '-DSAROO_FIFO_MODEL'),
                           extra_fixtures=('fpga_fmc_stream_tb.v',))
        return subprocess.run(['vvp', binary, f'+phase={phase}'],
                              capture_output=True, text=True, timeout=15)

    def test_sysres_quiets_the_bus_and_preserves_cd(self):
        source = Path(os.environ.get('SAROO_FPGA_RESET_SOURCE', ROOT / 'FPGA'))
        for phase in (0, 4, 9):
            with self.subTest(phase=phase):
                result = self.run_retention(fpga_sources(source), phase)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn('PASS three SYSRES pulses', result.stdout)

    def test_resetting_the_cartridge_mapping_loses_retained_cd_selection(self):
        inputs = fpga_sources()
        marker = b"if(NRESET==0)\n\t\t\tss_reg_ctrl <= 16'h0100;"
        self.assertIn(marker, inputs['SSMaster.v'])
        inputs['SSMaster.v'] = inputs['SSMaster.v'].replace(
            marker, b"if(!NRESET || !ss_bus_ready)\n\t\t\tss_reg_ctrl <= 16'h0100;")
        result = self.run_retention(inputs)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('SYSRES changed the CD selection or cartridge map', result.stdout)


if __name__ == '__main__':
    unittest.main()
