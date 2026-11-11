"""Check actual query publication and physical-disc authentication callers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuCDQueryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        raw = (source / 'cdblock.c').read_bytes()
        names = ('cdc_get_status', 'cdc_get_hwinfo', 'cdc_get_cddev',
                 'cdc_get_filter_range', 'cdc_get_filter_mode', 'cdc_get_filter_connect',
                 'cdc_get_buffer_size', 'cdc_get_numsector', 'cdc_get_actsize',
                 'cdc_get_sector_info', 'cdc_auth_status', 'cdc_get_file_scope',
                 'cdc_set_size', 'cdc_auth_device', 'jhl_auth_hack')
        code = b''.join(function(raw, name) for name in names)
        # Model only the relocated routine's storage and call.
        for original, replacement in (
            (b'memcpy((u8*)0x06080000, cdc_auth_hram, 0x400);',
             b'memcpy(auth_copy, cdc_auth_hram, 0x400);'),
            (b'void (*go)(CDCMD*, int) = (void*)0x06080000;',
             b'void (*go)(CDCMD*, int) = auth_go;'),
        ):
            assert code.count(original) == 1
            code = code.replace(original, replacement)

        build = ROOT / 'tests/build/menu-cd-queries'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'actual.inc').write_bytes(code)
        cls.binary = cls.target / 'menu-cd-queries'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_CD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
             '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
             str(ROOT / 'tests/menu_cd_queries.c'), '-o', str(cls.binary)],
            capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def scenario(self, name):
        result = subprocess.run(
            [str(self.binary), name], capture_output=True, text=True, timeout=10,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'{name}.log').write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_queries_preserve_outputs_after_errors(self):
        self.scenario('errors')

    def test_queries_decode_success_and_optional_status(self):
        self.scenario('success')

    def test_authentication_requires_setup_and_valid_status(self):
        self.scenario('auth')

    def test_authentication_restores_size_and_preserves_first_error(self):
        self.scenario('restore')

    def test_device_authentication_propagates_errors(self):
        self.scenario('device')

    def test_terminal_authentication_error_restores_sector_size(self):
        self.scenario('terminal-auth')


if __name__ == '__main__':
    unittest.main()
