"""Exercise the firmware's CD pacing code with buffered timer registers."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CDPacingTimerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_CD_TIMER_SOURCE', ROOT / 'Firm_MCU/Saturn'))
        raw = (source / 'saturn_main.c').read_bytes()
        legacy = b'void TIM2_IRQHandler(void)' not in raw
        shared = not legacy and b'osSemaphoreAcquire(sem_wait_disc, remaining' in raw
        cls.allocations = 4 if shared else 5
        signatures = (
            (b'void TIM6_DAC_IRQHandler(void)', b'void hw_delay(int us)', b'void tim6_init(void)')
            if legacy else
            (b'static int cd_sync_init(void)', b'void TIM2_IRQHandler(void)',
             b'static int hw_delay(int us)', b'static void cd_timer_init(void)')
        )
        functions = []
        for signature in signatures:
            start = raw.index(signature + b'\n{')
            functions.append(raw[start:raw.index(b'\n}', start) + 2])
        if legacy:
            start = raw.index(b'sem_wait_irq   = osSemaphoreNew')
            end = raw.index(b'mutex_fs = osMutexNew(NULL);', start) + len(b'mutex_fs = osMutexNew(NULL);')
            functions.append(b'static void legacy_sync_init(void) {\n' + raw[start:end] + b'\n}')

        build = ROOT / 'tests/build/cd-pacing-timer'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'timer-functions.inc').write_bytes(b'\n'.join(functions) + b'\n')
        config = ROOT / 'Firm_MCU/RTE/CMSIS/RTX_Config.h'
        defines = subprocess.check_output([
            *shlex.split(os.environ.get('CC', 'cc')), '-E', '-dM', '-x', 'c', str(config)
        ], timeout=30)
        tick = re.search(rb'^#define OS_TICK_FREQ .+$', defines, re.M)[0]
        boot = (ROOT / 'Firm_MCU/Main/main.c').read_bytes()
        clocks = re.search(rb'RCC->APB1LENR\s*=\s*(0x[0-9a-fA-F]+);', boot)[1]
        (cls.target / 'timer-config.h').write_bytes(tick + b'\n#define BOOT_APB1_CLOCKS ' + clocks + b'\n')
        flags = ['-O2'] if os.environ.get('SAROO_CDC_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'timer'
        command = [
            *shlex.split(os.environ.get('CXX', 'c++')), '-std=c++17', '-g', '-Wall', '-Wextra',
            '-Werror', '-Wno-unused-function', '-Wno-unused-variable', *flags,
            f'-DCD_TIMER_LEGACY={int(legacy)}', f'-DCD_TIMER_SHARED={int(shared)}', '-I', str(cls.target),
            str(ROOT / 'tests/cd_pacing_timer.cpp'), '-o', str(cls.binary)
        ]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def run_case(self, *arguments):
        result = subprocess.run(
            [str(self.binary), *arguments], capture_output=True, text=True, timeout=10,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('pass: CD pacing', result.stdout)

    def test_first_interval_loads_prescaler_without_software_irq(self):
        for inherited in ('0', '17', '1999'):
            with self.subTest(inherited=inherited):
                self.run_case('first', inherited)

    def test_valid_intervals_after_prescaler_has_already_loaded(self):
        self.run_case('ordinary')

    def test_full_count_beyond_old_16_bit_reload(self):
        self.run_case('long')

    def test_largest_positive_delay_has_no_signed_overflow(self):
        self.run_case('limit')

    def test_retired_timer_and_unrelated_reset_bits(self):
        self.run_case('ownership')

    def test_stale_irq_does_not_post_completion(self):
        self.run_case('stale')

    def test_expiry_retires_pending_irq_and_late_token_before_retry(self):
        for race in ('missing', 'pending', 'posted'):
            with self.subTest(race=race):
                self.run_case('timeout', race)

    def test_sync_objects_retire_cleanly_on_each_allocation_failure(self):
        for allocation in range(1, self.allocations + 1):
            with self.subTest(allocation=allocation):
                self.run_case('allocation', str(allocation))

    def test_pause_wakes_long_delay_and_preserves_retry(self):
        for request in ('1', '-2'):
            with self.subTest(request=request):
                self.run_case('pause', request)

    def test_unrelated_notifications_and_late_irqs_cannot_shorten_wait(self):
        self.run_case('hints')

    def test_absolute_deadline_survives_tick_wrap_and_repeated_wakes(self):
        self.run_case('deadline')


if __name__ == '__main__':
    unittest.main()
