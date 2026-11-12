"""Run the actual game selector against all possible disc-load replies."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuLaunchErrorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_LAUNCH_SOURCE', ROOT / 'Firm_Saturn'))
        main = (source / 'main.c').read_bytes()
        code = function(main, 'menu_cd_power') if b'int menu_cd_power(' in main else b''
        code += function(main, 'sel_handle')
        header = (source / 'main.h').read_bytes()
        names = (b'MENU_ITEMS', b'MENU_EXIT', b'SSCMD_LOADDISC', b'PAD_UP', b'PAD_DOWN',
                 b'PAD_LT', b'PAD_RT', b'PAD_A', b'PAD_Z', b'PAD_LEFT', b'PAD_C',
                 b'BUTTON_DOWN')
        definitions = b''.join(
            re.search(rb'^#define ' + name + rb'\b[^\r\n]+', header, re.M).group() + b'\n'
            for name in names)

        build = ROOT / 'tests/build/menu-launch-errors'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'selector.inc').write_bytes(code)
        (cls.target / 'constants.inc').write_bytes(definitions)
        cls.binary = cls.target / 'menu-launch-errors'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_LAUNCH_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall',
             '-Wextra', '-Werror', '-Wno-empty-body', *flags, '-I', str(cls.target),
             str(ROOT / 'tests/menu_launch_errors.c'), '-o', str(cls.binary)],
            capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def check_button(self, button):
        result = subprocess.run([str(self.binary), button], capture_output=True,
                                text=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'{button}.log').write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_game_launch_preserves_every_reply_and_bios_failure(self):
        self.check_button('A')

    def test_player_launch_preserves_every_reply_and_failed_handoff_state(self):
        self.check_button('Z')

    def test_selection_checks_cd_power_before_starting_dependent_work(self):
        self.check_button('power')


if __name__ == '__main__':
    unittest.main()
