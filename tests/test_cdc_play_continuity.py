"""Run actual PLAY and disk-task code with a deterministic command handoff."""

import os
from pathlib import Path
import shlex
import subprocess
import unittest

import test_cdc_drive as drive


class CDPlayContinuityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        drive.CDDriveTests.setUpClass()
        target = drive.CDDriveTests.target
        tests = Path(__file__).parent
        harness = (tests / 'cdc_drive.c').read_bytes()
        (target / 'drive-harness.inc').write_bytes(harness[:harness.index(b'int main(')])
        cls.binary = target / 'play-continuity'
        flags = ['-O2'] if os.environ.get('SAROO_CDC_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                   '-Wno-unused-label', '-Wno-unused-parameter', '-Wno-unused-variable',
                   '-pthread', *flags, '-I', str(target),
                   str(tests / 'cdc_play_continuity.c'), '-o', str(cls.binary)]
        build = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (target / 'continuity-compile.log').write_text(build.stdout + build.stderr)
        if build.returncode:
            raise RuntimeError(build.stderr)

    def run_case(self, case):
        run = subprocess.run([str(self.binary), case], capture_output=True, text=True,
                             env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'},
                             timeout=30)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn('pass: actual worker', run.stdout)

    def test_active_and_old_endpoint_updates_keep_reader(self):
        self.run_case('range')

    def test_shorter_range_completes_or_repeats_without_outside_read(self):
        self.run_case('shorter')

    def test_keep_pickup_honors_explicit_data_track_range(self):
        self.run_case('pickup')

    def test_paused_reader_update_skips_another_seek_delay(self):
        self.run_case('paused')

    def test_midrange_resume_enters_play_with_delay_configured(self):
        self.run_case('resume')

    def test_timed_out_update_cannot_publish_on_late_ack(self):
        self.run_case('timeout')

    def test_reader_failure_before_ack_uses_fresh_startup(self):
        self.run_case('error')

    def test_explicit_reposition_and_file_replacement_pause(self):
        self.run_case('replace')

    def test_fresh_start_delays_and_read_error_does_not_signal_end(self):
        self.run_case('controls')


if __name__ == '__main__':
    unittest.main()
