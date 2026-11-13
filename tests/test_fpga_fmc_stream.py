"""Verify the public FMC transaction at the FPGA pins."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import MODULES, ROOT, declarations_first


def fpga_sources(source=ROOT / 'FPGA'):
    return {name: (source / name).read_bytes() for name in ('SSMaster.v', *MODULES)}


def build_top(test, inputs, bench, *, fixture_flags=(), extra_fixtures=()):
    temp = tempfile.TemporaryDirectory()
    test.addCleanup(temp.cleanup)
    root = Path(temp.name)
    for name, source in inputs.items():
        (root / name).write_bytes(declarations_first(source))
    binary = root / 'top.vvp'
    result = subprocess.run(
        ['iverilog', '-g2012', '-s', 'tb', '-o', binary, *fixture_flags,
         *(root / name for name in inputs), ROOT / 'tests' / bench,
         *(ROOT / 'tests' / name for name in extra_fixtures)],
        capture_output=True, text=True, timeout=30)
    test.assertEqual(result.returncode, 0, result.stdout + result.stderr)
    return binary


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGAFmcTests(unittest.TestCase):
    def run_stream(self, binary, phase=0, tail=20, group=2, refresh=0):
        return subprocess.run(
            ['vvp', binary, f'+phase={phase}', f'+tail={tail}', f'+group={group}',
             f'+refresh={refresh}'], capture_output=True, text=True, timeout=30)

    def test_individual_and_grouped_transfers(self):
        source = Path(os.environ.get('SAROO_FPGA_FMC_SOURCE', ROOT / 'FPGA'))
        binary = build_top(self, fpga_sources(source), 'fpga_fmc_stream_tb.v')
        for phase in range(10):
            for tail, group, refresh in ((20, 1, 0), (20, 2, 0), (30, 4, 1)):
                with self.subTest(phase=phase, tail=tail, group=group, refresh=refresh):
                    result = self.run_stream(binary, phase, tail, group, refresh)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('PASS 576 writes, 67 reads', result.stdout)

    def test_wait_must_rearm_even_with_chip_select_held(self):
        inputs = fpga_sources()
        marker = b'wire st_wait_reset = !NRESET || ST_CS || !ST_ALE;'
        self.assertIn(marker, inputs['SSMaster.v'])
        inputs['SSMaster.v'] = inputs['SSMaster.v'].replace(
            marker, b'wire st_wait_reset = !NRESET || ST_CS;')
        binary = build_top(self, inputs, 'fpga_fmc_stream_tb.v')
        result = self.run_stream(binary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('second halfword reused the previous NWAIT release', result.stdout)

    def test_wait_cannot_follow_the_later_cache_acknowledgment(self):
        inputs = fpga_sources()
        marker = b'nwait_b | read_complete_b | write_complete_b'
        self.assertIn(marker, inputs['memhub.v'])
        inputs['memhub.v'] = inputs['memhub.v'].replace(marker, b'nwait_b')
        binary = build_top(self, inputs, 'fpga_fmc_stream_tb.v')
        result = self.run_stream(binary)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('NWAIT missed the SDRAM write completion edge', result.stdout)


if __name__ == '__main__':
    unittest.main()
