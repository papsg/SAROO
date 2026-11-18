"""Exercise the selected menu_run function across an actual restart."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuWarningRedrawTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_SOURCE', ROOT / 'Firm_Saturn'))
        code = function((source / 'conio.c').read_bytes(), 'menu_run')
        cls.temp = tempfile.TemporaryDirectory(prefix='saroo-menu-warning-')
        cls.directory = Path(cls.temp.name)
        (cls.directory / 'actual.inc').write_bytes(code)
        flags = ['-O2'] if os.environ.get('SAROO_MENU_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.directory / 'menu-warning'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11',
                   '-DSAROO_SAVE_GENERATION=1', '-g', '-Wall', '-Wextra', '-Werror',
                   *flags, str(ROOT / 'tests/menu_warning_redraw.c'),
                   '-I', str(cls.directory), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        if result.returncode:
            cls.temp.cleanup()
            raise RuntimeError(result.stdout + result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def case(self, number):
        result = subprocess.run([str(self.binary), str(number)], capture_output=True,
                                text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_persistent_failure_is_visible_after_restart(self): self.case(0)
    def test_changed_failure_is_visible_after_restart(self): self.case(1)
    def test_healthy_channel_has_no_warning(self): self.case(2)
    def test_busy_status_has_no_warning(self): self.case(3)


if __name__ == '__main__':
    unittest.main()
