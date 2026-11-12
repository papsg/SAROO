"""Check refresh spacing at the SDRAM command pins in public FPGA RTL."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import ROOT, declarations_first


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGARefreshIntervalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_FPGA_INTERVAL_SOURCE', ROOT / 'FPGA'))
        tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(tmp.cleanup)
        out = Path(tmp.name)
        controller = out / 'tsdram.v'
        controller.write_bytes(declarations_first((source / controller.name).read_bytes()))
        cls.binary = out / 'refresh.vvp'
        run = subprocess.run(['iverilog', '-g2012', '-s', 'tb', '-o', cls.binary,
                              controller, ROOT / 'tests/fpga_refresh_interval_tb.v'],
                             capture_output=True, text=True, timeout=30)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)

    def case(self, mode, target, limit, config=781, maximum=None):
        with self.subTest(mode=mode, config=config):
            run = subprocess.run(['vvp', self.binary, f'+mode={mode}', f'+target={target}',
                                  f'+limit={limit}', f'+cfg={config}'],
                                 capture_output=True, text=True, timeout=90)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            match = re.search(r'PASS mode=\d+ refreshes=(\d+) max_interval=(\d+) config=\d+', run.stdout)
            self.assertIsNotNone(match, run.stdout)
            if maximum is not None:
                self.assertEqual(int(match[2]), maximum)
            if mode in (0, 1):
                self.assertEqual(int(match[1]), target + 1)

    def test_full_refresh_windows(self):
        for mode, maximum in ((0, 719), (1, 728)):
            self.case(mode, 8192, maximum, maximum=maximum)

    def test_reload_boundaries_and_overrides(self):
        for config, maximum in ((1, 10), (63, 65), (64, 66), (65, 10),
                                (128, 75), (390, 337), (538, 485),
                                (717, 664), (1023, 970)):
            self.case(0 if config < 128 else 1, 16, maximum, config, maximum)

    def test_zero_disables_autonomous_refresh(self):
        self.case(4, 1, 781, 0)

    def test_external_refresh_and_acknowledgment_collision(self):
        for mode in (2, 3):
            self.case(mode, 32, 728)


if __name__ == '__main__':
    unittest.main()
