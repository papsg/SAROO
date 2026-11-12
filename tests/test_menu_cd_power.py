"""Run the CD power commands and physical-disc menu callers on the host."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


def compile_fixture(name, code):
    build = ROOT / 'tests/build' / name
    build.mkdir(parents=True, exist_ok=True)
    target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
    (target / 'actual.inc').write_bytes(code)
    flags = ['-O2'] if os.environ.get('SAROO_MENU_CD_PROFILE') == 'release' else [
        '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
               '-Wall', '-Wextra', '-Werror', '-Wno-unused-but-set-variable',
               '-Wno-unused-function', '-Wno-empty-body', *flags,
               '-I', str(target), str(ROOT / 'tests' / f'{name}.c'), '-o', str(target / name)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (target / 'compile.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stderr)
    return target


def scenario(test, name):
    result = subprocess.run([str(test.target / test.fixture), name], capture_output=True,
                            text=True, timeout=15,
                            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (test.target / f'{name}.log').write_text(result.stdout + result.stderr)
    test.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class CDPowerTests(unittest.TestCase):
    fixture = 'menu_cd_power'

    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        header = (source / 'main.h').read_bytes()
        registers = (ROOT / 'Firm_Saturn/smpc.h').read_bytes()
        constants = b''.join(
            re.search(rb'^#define ' + name + rb'\b[^\r\n]+', data, re.M).group() + b'\n'
            for name, data in ((b'MS2TICK', header), (b'SAROO_EN', header),
                               (b'CS0_RAM4M', header), (b'CDON', registers),
                               (b'CDOFF', registers)))
        timer = function(header.replace(b'static inline int timer_reached',
                                        b'static int timer_reached'), 'timer_reached')
        block = (source / 'cdblock.c').read_bytes()
        legacy = b'int cdblock_power(' not in block
        body = function((source / 'main.c').read_bytes(), 'smpc_cmd') if legacy else function(block, 'cdblock_power')
        if b'static int cd_power_display_off_window(' in block:
            body = function(block, 'cd_power_display_off_window') + body
        body += function(block, 'cdblock_on') + function(block, 'cdblock_off')
        if legacy:
            body += b'''
static int request_on(int wait) { cdblock_on(wait); return 0; }
static int request_off(void) { cdblock_off(); return 0; }
#define cdblock_on request_on
#define cdblock_off request_off
'''
        cls.target = compile_fixture(cls.fixture, constants + timer + body)

    def test_successful_power_and_ready_order(self): scenario(self, 'healthy')
    def test_busy_admission_preserves_routing(self): scenario(self, 'idle')
    def test_late_completion_requires_fresh_takeover(self): scenario(self, 'completion')
    def test_admission_and_completion_share_one_deadline(self): scenario(self, 'budget')
    def test_wrong_completion_keeps_saroo_disconnected(self): scenario(self, 'wrong')
    def test_ready_timeout_preserves_full_register_read(self): scenario(self, 'drive')
    def test_counter_wrap_and_all_nonbusy_states(self): scenario(self, 'wrap')
    def test_visible_display_requires_a_fresh_end_of_blanking(self): scenario(self, 'visible')
    def test_disabled_display_uses_the_local_counter(self): scenario(self, 'hidden')
    def test_disabled_display_with_an_unusable_latch_expires(self): scenario(self, 'foreign')
    def test_delay_during_admission_rechecks_phase_and_deadline(self): scenario(self, 'fresh')


class CDPowerCallerTests(unittest.TestCase):
    fixture = 'menu_cd_power_callers'

    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        main = (source / 'main.c').read_bytes()
        code = function(main, 'menu_cd_power') if b'int menu_cd_power(' in main else b''
        cls.target = compile_fixture(cls.fixture, code + function(main, 'main_handle'))

    def test_failure_stops_before_player_or_authentication(self): scenario(self, 'admission')
    def test_successful_player_and_launch_preserve_order(self): scenario(self, 'healthy')
    def test_failed_takeover_during_cleanup_is_reported(self): scenario(self, 'cleanup')
    def test_failed_power_can_retry_in_the_menu(self): scenario(self, 'retry')


if __name__ == '__main__':
    unittest.main()
