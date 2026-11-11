"""Test the real companion ROM admission and copy on modeled SH-2 memory."""
# Adapted from paps-g 198a079f. No game ROM or device is required.
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import function


ROOT = Path(__file__).resolve().parents[1]


class MenuROMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_ROM_SOURCE', ROOT / 'Firm_Saturn/game_patch.c'))
        raw = source.read_bytes()
        cls.legacy = b'int prepare_game_rom(' not in raw
        code = b''
        if not cls.legacy:
            code = raw[raw.index(b'#define ROM_IMAGE_ADDR'):raw.index(b'int prepare_game_rom(')]
            code += function(raw, 'prepare_game_rom')
        for name in ('jump_copy_code', 'ULTRAMAN_patch', 'kof95_handle'):
            code += function(raw, name)
        code = code.replace(b'(void*)type', b'rom_jump(type)')
        code = code.replace(b'(void*)0x200000', b'copy_and_go')
        code = re.sub(rb'\*\(u(16|32)\*\)\((0x[0-9a-fA-F]+)\)', rb'REG\1(\2)', code)

        build = ROOT / 'tests/build/menu-rom'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'rom.inc').write_bytes(code)
        (cls.target / 'game_patch.c').write_bytes(raw)
        cls.binary = cls.target / 'menu-rom'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        if cls.legacy:
            flags.append('-DLEGACY_ROM_READ')
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
                   str(ROOT / 'tests/menu_rom.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

    def case(self, mode, kind, value=0):
        with self.subTest(mode=mode, kind=kind, value=value):
            result = subprocess.run([str(self.binary), str(mode), str(kind), str(value)],
                                    capture_output=True, timeout=15,
                                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_complete_roms_copy_the_full_payload(self):
        for kind in range(3):
            self.case(0, kind, 0x200000)

    def test_missing_failed_and_short_reads_cannot_enter_rom_code(self):
        for kind in range(3):
            for value in (-1, -4, -19, 0, 1, 0x100000, 0x1fffff):
                self.case(0, kind, value)

    def test_skipped_unrelated_and_rom_free_ips_need_no_file(self):
        if self.legacy:
            self.skipTest('The baseline has no ROM preflight')
        for kind in range(3):
            for value in range(3):
                self.case(1, kind, value)

    def test_failed_next_launch_cannot_reuse_the_previous_rom(self):
        if self.legacy:
            self.skipTest('The baseline has no ROM preflight')
        for kind in range(3):
            self.case(2, kind)

    def test_wrong_callback_cannot_consume_the_staged_rom(self):
        if self.legacy:
            self.skipTest('The baseline has no ROM preflight')
        for kind in range(3):
            self.case(3, kind)


if __name__ == '__main__':
    unittest.main()
