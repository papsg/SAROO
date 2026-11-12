"""Actual audio queue, IRQs and service with independent DMA/FIFO events."""
# Adapted from paps-g 104b25fb/e8a47c86/247c1dbd; public source only.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AudioDMATests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_AUDIO_SOURCE', ROOT / 'Firm_MCU/Saturn/cdda_i2s.c'))
        raw = source.read_bytes()
        end = raw.index(b'\n}', raw.index(b'int fill_audio_buffer(')) + 2
        code = raw[raw.index(b'#define ABUF_NUM'):end]
        start = code.index(b'void spi2_set_notify(')
        end = code.index(b'\n}', start) + 2
        code = code[:start] + code[end:]
        # MMIO proxies and physical-address casts only; arithmetic stays u32.
        code = code.replace(b'volatile uint32_t *', b'Register *')
        code = code.replace(b'(u32)', b'(uintptr_t)')
        code = code.replace(b'(u8 *)', b'(u8 *)(uintptr_t)')
        code = code.replace(b'(u8*)', b'(u8*)(uintptr_t)')
        worker = Path(os.environ.get('SAROO_AUDIO_MAIN', ROOT / 'Firm_MCU/Saturn/saturn_main.c')).read_bytes()
        marker = b'if(cdb.play_type!=PLAYTYPE_FILE && cdb.play_type!=PLAYTYPE_DIR && cdb.play_track->mode==3)'
        begin = worker.rindex(marker, 0, worker.index(b'int asize = fill_audio_buffer'))
        worker_tail = worker[begin:]
        # Stop before either the old split delay or the checked timer call.
        pacing = re.search(rb'^[ \t]*if\s*\(\s*(?:asize\s*<=|hw_delay\s*\()', worker_tail, re.M)
        if pacing is None:
            raise ValueError('Audio admission fixture needs a CD pacing boundary')
        worker_code = worker_tail[:pacing.start()]
        build = ROOT / 'tests/build/audio-dma'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        continuity = b'void spi2_cancel(' in raw
        cls.continuity = continuity
        (cls.target / 'audio.inc').write_bytes(f'#define TEST_LINK_CONTINUITY {int(continuity)}\n'.encode() + code)
        (cls.target / 'audio-worker.inc').write_bytes(worker_code)
        start = worker.index(b'static int acknowledge_drive_request(')
        handoff = worker[start:worker.index(b'\n}', start) + 2]
        helper = b'static void retire_cdda('
        if helper in worker:
            start = worker.index(helper)
            handoff = worker[start:worker.index(b'\n}', start) + 2] + b'\n' + handoff
        (cls.target / 'audio-ack.inc').write_bytes(handoff)
        start = worker.index(b'\n_read_error:')
        errors = worker[start:worker.index(b'\n}', start)].rstrip()
        if not errors.endswith(b'\n\t}'):
            raise ValueError('Expected the disk worker loop to end after the error paths')
        (cls.target / 'audio-error.inc').write_bytes(errors[:-3])
        inputs = {'cdda_i2s.c': raw, 'saturn_main.c': worker, 'audio_dma.cpp': (ROOT / 'tests/audio_dma.cpp').read_bytes(),
                  'test_audio_dma.py': Path(__file__).read_bytes()}
        for name, data in inputs.items():
            (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_AUDIO_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'audio-dma'
        args = [*shlex.split(os.environ.get('CXX', 'c++')), '-std=gnu++17', '-g',
                '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags,
                '-I', str(cls.target), str(cls.target / 'audio_dma.cpp'), '-o', str(cls.binary)]
        result = subprocess.run(args, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(args, indent=2) + '\n')
        result.check_returncode()
        cls.results = []

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def run_case(self, case):
        if case >= 38 and not self.continuity:
            self.skipTest('The parent has no clock-preserving cancellation or quiet link')
        result = subprocess.run([str(self.binary), str(case)], capture_output=True,
                                timeout=120 if case in (30, 38) else 30, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'case-{case}.log').write_bytes(result.stdout + result.stderr)
        self.results.append({'case': case, 'returncode': result.returncode})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b'pass', result.stdout)

    def test_startup_flags_fifo_and_normal_drain(self):self.run_case(0)
    def test_full_32_sector_capacity_and_backpressure(self):self.run_case(1)
    def test_monotonic_positions_wrap_without_capacity_loss(self):self.run_case(2)
    def test_low_water_never_preloads_an_unpublished_bank(self):self.run_case(3)
    def test_late_publication_works_after_the_half_transfer_flag(self):self.run_case(4)
    def test_dma_progress_during_an_unpublished_copy_is_safe(self):self.run_case(5)
    def test_explicit_end_flushes_one_final_sector(self):self.run_case(6)
    def test_startup_bus_stall_uses_bounded_timer_retries(self):self.run_case(7)
    def test_underrun_resumes_without_an_interrupt_storm(self):self.run_case(8)
    def test_guard_miss_takes_transfer_error_recovery_without_unsafe_read(self):self.run_case(9)
    def test_initial_dma_stop_can_finish_asynchronously(self):self.run_case(10)

    def test_near_boundary_publication_has_bounded_latency(self):self.run_case(11)
    def test_new_data_resumes_while_the_quiet_link_runs(self):self.run_case(12)
    def test_pending_completion_cannot_publish_from_a_stale_read_position(self):self.run_case(13)
    def test_delayed_stop_preserves_data_published_during_its_retry(self):self.run_case(14)

    def test_clock_failure_retries_without_storm_and_retains_fifo_tail(self):self.run_case(15)
    def test_repeated_restarts_acknowledge_all_retained_flags(self):self.run_case(16)
    def test_transfer_error_retains_fifo_tail_without_replaying_payload(self):self.run_case(17)

    def test_early_silence_resume_preserves_the_preceding_fifo_tail(self):self.run_case(18)
    def test_worker_flushes_only_accepted_final_audio_without_remaining_repeats(self):self.run_case(19)

    def test_nonfatal_fifo_warning_preserves_normal_and_pending_completion(self):self.run_case(20)
    def test_fifo_warning_during_startup_keeps_bounded_readiness_retry(self):self.run_case(21)
    def test_memory_backpressure_recovers_with_or_without_i2s_underrun(self):self.run_case(22)

    def test_partial_error_resumes_exactly_across_channels_banks_and_wrap(self):self.run_case(23)
    def test_error_with_latched_underrun_preserves_refilled_fifo(self):self.run_case(24)
    def test_repeated_partial_errors_preserve_all_32_queued_sectors(self):self.run_case(25)
    def test_error_coalesced_with_completion_retires_the_correct_sector(self):self.run_case(26)
    def test_persistent_dma_fault_retries_without_an_irq_storm(self):self.run_case(27)
    def test_startup_transfer_error_preserves_partially_filled_fifo(self):self.run_case(28)

    def test_publication_during_residual_dma_never_writes_a_protected_bank(self):self.run_case(29)
    def test_every_remaining_sector_count_preserves_exact_payload_and_stereo_phase(self):self.run_case(30)
    def test_delayed_error_stop_preserves_late_completion_accounting(self):self.run_case(31)

    def test_quiet_bank_error_resumes_promptly_on_the_correct_stereo_channel(self):self.run_case(32)


    def test_recovery_stop_finishes_frame_and_stale_irqs_preserve_deadline(self):self.run_case(33)
    def test_stalled_recovery_suspend_uses_two_real_timer_updates(self):self.run_case(34)

    def test_completion_arriving_between_irq_snapshot_and_clear_is_retained(self):self.run_case(35)
    def test_error_arriving_between_irq_snapshot_and_clear_is_retained(self):self.run_case(36)

    def test_reader_replacement_and_failure_keep_only_staged_old_audio(self):self.run_case(37)
    def test_cancellation_preserves_stereo_phase_at_every_position(self):self.run_case(38)
    def test_pending_cancel_waits_for_dma_and_an_in_progress_copy(self):self.run_case(39)
    def test_startup_and_idle_send_valid_silence(self):self.run_case(40)
    def test_publication_recovers_an_exhausted_quiet_stream(self):self.run_case(41)

if __name__ == '__main__':
    unittest.main()
