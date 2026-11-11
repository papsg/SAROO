"""Execute the complete boot caller with mapped RAM and scripted CD replies."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuCDBootHandoffTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        launch = function((source / 'game_load.c').read_bytes(), 'bios_cd_cmd')
        assert launch.count(b'(void*)0x18d2') == 1
        launch = launch.replace(b'(void*)0x18d2', b'(void*)bios_boot')
        addresses = set()

        def map_address(match):
            kind, parenthesized, bare = match.groups()
            address = parenthesized or bare
            addresses.add(int(address, 16))
            return b'(' + kind + b'*)memory_at(' + address + b')'

        launch = re.sub(
            rb'\((const char|u8|u16|u32|char|void)\s*\*\)\s*'
            rb'(?:\((0x[0-9a-fA-F]+)\)|(0x[0-9a-fA-F]+))',
            map_address, launch)
        expected = {0x06002020, 0x06002060, 0x060020e0, 0x06002000, 0x06000290,
                    0x060002a0, 0x06000c00, 0x06000254, 0x06002270, 0x02000f04,
                    0x0600220c}
        if addresses != expected:
            raise ValueError('Review boot address mapping before running the fixture')
        for name in (b'read_1st', b'cdc_read_sector'):
            launch = launch.replace(b'(u32)' + name, b'(u32)(uintptr_t)' + name)
        block = (source / 'cdblock.c').read_bytes()
        header = (source / 'main.h').read_bytes()
        definitions = b''.join(
            re.search(rb'^#define ' + name + rb'[^\r\n]+', header, re.M).group() + b'\n'
            for name in (b'HIRQ_EFLS', b'SSCMD_LSAVE'))

        build = ROOT / 'tests/build/menu-cd-boot-handoff'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'constants.inc').write_bytes(definitions)
        (cls.target / 'actual.inc').write_bytes(
            function(block, 'cdc_change_dir') + function(block, 'cdc_read_file') + launch)
        cls.binary = cls.target / 'menu-cd-boot-handoff'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_CD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
             '-Wall', '-Wextra', '-Werror', '-Wno-unused-but-set-variable',
             '-fno-strict-aliasing', *flags, '-I', str(cls.target),
             str(ROOT / 'tests/menu_cd_boot_handoff.c'), '-o', str(cls.binary)],
            capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def scenario(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True,
                                text=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'{name}.log').write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_complete_boot_preserves_arguments_metadata_and_result(self):
        self.scenario('healthy')

    def test_earlier_failures_skip_late_commands(self):
        self.scenario('early')

    def test_late_failures_keep_error_and_skip_handoff(self):
        self.scenario('late')

    def test_publication_follows_both_successful_commands(self):
        self.scenario('publication')

    def test_a_later_attempt_can_complete_after_late_failure(self):
        self.scenario('retry')


if __name__ == '__main__':
    unittest.main()
