"""Verify coherent Saturn writes with the public FPGA top and memory path."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest

from test_fpga_fmc_stream import build_top, fpga_sources
from test_fpga_memory import ROOT


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGAAbusWriteTests(unittest.TestCase):
    def build(self, inputs):
        return build_top(self, inputs, 'fpga_abus_write_tb.v',
                         fixture_flags=('-DSAROO_ABUS_TEST',),
                         extra_fixtures=('fpga_fmc_stream_tb.v',))

    def run_bus(self, binary, phase=0, idle=0, skew=0):
        return subprocess.run(['vvp', binary, f'+phase={phase}', f'+idle={idle}', f'+skew={skew}'],
                              capture_output=True, text=True, timeout=30)

    def test_data_phase_writes(self):
        source = Path(os.environ.get('SAROO_FPGA_ABUS_SOURCE', ROOT / 'FPGA'))
        binary = self.build(fpga_sources(source))
        for phase in range(10):
            for idle in (0, 1):
                for skew in (0, 4):
                    with self.subTest(phase=phase, idle=idle, skew=skew):
                        result = self.run_bus(binary, phase, idle, skew)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn('PASS 13 Saturn writes, 2 SDRAM commands and 1 FIFO command',
                                      result.stdout)

    def test_live_ram_address_corrupts_a_captured_write(self):
        inputs = fpga_sources()
        marker = b'? physical_write_addr_a :'
        self.assertIn(marker, inputs['memhub.v'])
        inputs['memhub.v'] = inputs['memhub.v'].replace(marker,
                                                       b'? physical_addr_a :')
        binary = self.build(inputs)
        outcomes = [self.run_bus(binary, phase) for phase in range(10)]
        failures = [result.stdout + result.stderr for result in outcomes if result.returncode]
        self.assertTrue(failures)
        for result in failures:
            self.assertIn('SDRAM did not get the admitted Saturn address', result)


if __name__ == '__main__':
    unittest.main()
