"""Exercise actual boot initialization and CD-player media changes."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuCDInitializationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        load = (source / 'game_load.c').read_bytes()
        code = function(load, 'my_bios_loadcd_init')
        code = code.replace(b'(u32*)(0x06000278)', b'&bios_state[0]')
        code = code.replace(b'(u32*)(0x0600027c)', b'&bios_state[1]')
        code += function(load, 'cdp_auth')
        launch = function(load, 'bios_cd_cmd')
        launch = launch.replace(b'(const char *)0x06002020', b'game_id')
        # Keep the actual startup admission through the ROM preflight.
        code += launch[:launch.index(b'\t\t// emulate bios_loadcd_boot')]
        code += b'\t\tboot_admitted++;\n\t}\n\treturn 0;\n}\n'

        build = ROOT / 'tests/build/menu-cd-initialization'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'actual.inc').write_bytes(code)
        cls.binary = cls.target / 'menu-cd-initialization'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_CD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
             '-Wall', '-Wextra', '-Werror', '-Wno-unused-variable', *flags,
             '-I', str(cls.target), str(ROOT / 'tests/menu_cd_initialization.c'),
             '-o', str(cls.binary)], capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def scenario(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True,
                                text=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'{name}.log').write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_initialization_preserves_successful_command_order(self):
        self.scenario('healthy')

    def test_initialization_stops_at_first_failure(self):
        self.scenario('init-errors')

    def test_direct_boot_requires_initialization(self):
        self.scenario('boot')

    def test_player_restarts_initialization_after_failure(self):
        self.scenario('auth-errors')

    def test_player_keeps_success_and_retry_paths(self):
        self.scenario('auth-healthy')

    def test_open_or_missing_media_clears_every_player_stage(self):
        self.scenario('media')


if __name__ == '__main__':
    unittest.main()
