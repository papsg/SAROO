"""Actual menu clock, repeat and scrolling code at synthetic timer boundaries."""
import os
from pathlib import Path
import subprocess
import hashlib
import json
import shlex
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MenuTimerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_TIMER_DIR', ROOT / 'Firm_Saturn'))
        inputs = {name: (source / name).read_bytes() for name in ('main.c', 'main.h', 'conio.c')}
        main, header, conio = (inputs[name] for name in ('main.c', 'main.h', 'conio.c'))
        build = ROOT / 'tests/build/menu-timer'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build)); cls.results = []
        start = main.index(b'const int HZ = 1000000;')
        clock = main[start:main.index(b'\n/********', start)]
        start = header.index(b'static inline int timer_reached')
        helper = header[start:header.index(b'\n}', start)+2]
        macros = b'\n'.join(line for line in header.splitlines()
                            if line.startswith((b'#define MS2TICK', b'#define TICK2',
                                                b'#define SS_TIMER_HI', b'#define SS_TIMER_LO')))
        pad = conio.index(b'#define PAD_REPEAT_TIME')
        pad_end = conio.index(b'/******************************************************************************/', pad)
        scroll = conio.index(b'#define SEL_IDLE_TIME')
        scroll_end = conio.index(b'static void sel_init', scroll)
        (cls.target / 'timer.inc').write_bytes(macros+b'\n'+helper+b'\n'+clock+b'\n'+conio[pad:pad_end]+conio[scroll:scroll_end])
        for name in ('menu_timer.c', 'test_menu_timer.py'): inputs[name] = (ROOT / 'tests' / name).read_bytes()
        for name, data in inputs.items(): (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps({name: hashlib.sha256(data).hexdigest()
            for name, data in inputs.items()}, indent=2)+'\n')
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'menu-timer'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-fno-strict-aliasing', *flags, '-I', str(cls.target),
                   str(cls.target / 'menu_timer.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(command, indent=2)+'\n')
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def invoke(self, case, value=0):
        result = subprocess.run([str(self.binary), case, str(value)], timeout=10,
                                capture_output=True, text=True,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=case, value=value, returncode=result.returncode,
            stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(case + ': pass', result.stdout)

    def test_clock_carry_and_modulo_boundaries(self):
        for value in range(3):
            with self.subTest(value=value): self.invoke('clock', value)

    def test_startup_clears_counter_via_the_high_word(self):
        self.invoke('reset')

    def test_split_read_retries_after_a_low_word_carry(self):
        self.invoke('order')

    def test_zero_and_full_width_sleeps_preserve_elapsed_time(self):
        for value in range(5):
            with self.subTest(value=value):
                self.invoke('sleep', value)

    def test_conversions_handle_full_width_and_evaluate_once(self):
        self.invoke('conversion')

    def test_repeat_delay_rate_release_and_missing_samples(self):
        for value in range(5):
            with self.subTest(value=value): self.invoke('repeat', value)

    def test_late_repeat_never_creates_an_event_burst(self):
        for value in range(5):
            with self.subTest(value=value): self.invoke('late', value)

    def test_both_scrolling_waits_cross_signed_and_unsigned_boundaries(self):
        for value in range(5):
            with self.subTest(value=value): self.invoke('scroll', value)


if __name__ == '__main__':
    unittest.main()
