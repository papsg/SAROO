"""Check refresh placement, fallback and arbitration in public FPGA RTL."""
# Adapted from paps-g 8a31651a.
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import MODULES, ROOT, build_memory_model


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGARefreshWindowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_FPGA_WINDOW_SOURCE', ROOT / 'FPGA'))
        cls.root = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.root.cleanup)
        target = Path(cls.root.name)
        inputs = {name: (source / name).read_bytes() for name in MODULES}
        cls.controller = build_memory_model(inputs, target / 'controller', 'fpga_refresh_window_tb.v')
        top = (source / 'SSMaster.v').read_bytes()
        start = top.index(b'\treg sscs_s0,')
        marker = b'\twire ss_rd_start ='
        end = top.index(b'\n', top.index(marker, start))
        pattern = rb'\bss_refresh,\s*st_reg_sdram,\s*([^,\n]+)'
        active = re.search(pattern, top[top.index(b'\tmemhub _mh('):])
        signal = active[1] if active else b"1'b0"
        include = top[start:end] + b'\nwire refresh_active = ' + signal + b';\n'
        cls.memory = build_memory_model(inputs, target / 'memory', 'fpga_refresh_window_memhub_tb.v',
                                        {'fc1-synchronizer.inc': include})

    def run_case(self, binary, **options):
        result = subprocess.run(['vvp', str(binary), *(f'+{k}={v}' for k, v in options.items())],
                                capture_output=True, text=True, timeout=110)
        log = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, log)
        self.assertIn('PASS', log)
        return log

    def test_fc1_pairs_stay_inside_idle_windows_for_all_rows(self):
        self.run_case(self.controller, mode=0)

    def test_missing_fc1_returns_to_autonomous_refresh(self):
        self.run_case(self.controller, mode=1)

    def test_zero_reload_disables_only_autonomous_refresh(self):
        self.run_case(self.controller, mode=2)

    def test_busy_controller_meets_fallback_row_deadline(self):
        self.run_case(self.controller, mode=3)

    def test_early_fc1_leaves_power_on_countdown_intact(self):
        self.run_case(self.controller, mode=4)

    def test_edge_at_refresh_acknowledgment_queues_another_pair(self):
        self.run_case(self.controller, mode=5)

    def test_saturn_read_timing_with_new_mcu_requests(self):
        for low in (18, 30):
            for prebusy, write in [(-999, 0), *((p, w) for p in (0, 1, low//2, low-1) for w in (0, 1))]:
                with self.subTest(low=low, prebusy=prebusy, write=write):
                    log = self.run_case(self.memory, low=low, prebusy=prebusy, write=write)
                    self.assertIn(f'start={low} cmd={low+2} ack={low+6}', log)

    def test_already_accepted_mcu_commands_finish(self):
        for prebusy in (-3, -2, -1):
            for write in (0, 1):
                with self.subTest(prebusy=prebusy, write=write):
                    log = self.run_case(self.memory, low=18, prebusy=prebusy, write=write)
                    self.assertLessEqual(int(re.search(r'ack=(\d+)', log)[1]), 31)
