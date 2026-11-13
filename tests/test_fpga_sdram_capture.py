"""Check read-word alignment at the FPGA SDRAM pins."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import ROOT, capture_flag, declarations_first


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGASdramCaptureTests(unittest.TestCase):
    def build(self, raw):
        out = tempfile.TemporaryDirectory()
        self.addCleanup(out.cleanup)
        path = Path(out.name)
        controller = path / 'tsdram.v'
        controller.write_bytes(declarations_first(raw))
        binary = path / 'capture.vvp'
        result = subprocess.run(
            ['iverilog', '-g2012', *capture_flag(raw), '-s', 'tb', '-o', binary,
             controller, ROOT / 'tests/fpga_sdram_capture_tb.v'],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return binary

    def run_case(self, binary, delay):
        return subprocess.run(['vvp', binary, f'+tsac={delay}'], capture_output=True,
                              text=True, timeout=15)

    def test_read_burst_across_access_time_limits(self):
        source = Path(os.environ.get('SAROO_FPGA_SDRAM_SOURCE', ROOT / 'FPGA'))
        binary = self.build((source / 'tsdram.v').read_bytes())
        for delay in range(3, 7):
            with self.subTest(delay=delay):
                result = self.run_case(binary, delay)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn('PASS four CL2 burst words', result.stdout)

    def test_extra_valid_cycle_skips_the_first_word(self):
        raw = (ROOT / 'FPGA/tsdram.v').read_bytes()
        marker = b'\t\t\tdata_valid <= data_window;'
        self.assertEqual(raw.count(marker), 1)
        extra = raw.replace(b'\treg data_window;', b'\treg data_window, late_valid;')
        extra = extra.replace(marker, b'\t\t\tlate_valid <= data_window;\n'
                                      b'\t\t\tdata_valid <= late_valid;')
        extra = extra.replace(b'\t\t\tdata_window <= 0;', b'\t\t\tdata_window <= 0;\n'
                                                              b'\t\t\tlate_valid <= 0;', 1)
        result = self.run_case(self.build(extra), 6)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('wrong CL2 word 0: 5678 (expected 1234)', result.stdout)


if __name__ == '__main__':
    unittest.main()
