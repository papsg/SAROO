"""Exercise the actual menu's CD admission and IP loading at a modeled boundary."""
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


def function(source, name):
    match = re.search(rb'^(?:static )?(?:int|void) ' + name.encode() + rb'\([^;\n]*\)\s*\n\{', source, re.M)
    assert match, name
    return source[match.start():source.index(b'\n}', match.end()) + 3]

def command_functions(source):
    helper = function(source, 'cdc_reply') if b'static int cdc_reply(' in source else b''
    return helper + function(source, 'cdc_cmd')


class MenuCDPlayerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/menu-cdplayer'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        block = (source / 'cdblock.c').read_bytes()
        load = (source / 'game_load.c').read_bytes()
        menu = (source / 'main.c').read_bytes()
        code = command_functions(block).replace(b'int cdc_cmd(', b'int actual_cdc_cmd(')
        code += function(block, 'cdc_end_trans').replace(b'int cdc_end_trans(', b'int actual_end_trans(')
        code += function(block, 'cdc_get_toc').replace(b'int cdc_get_toc(', b'int actual_get_toc(')
        ip = function(load, 'my_bios_loadcd_read')
        # Translate only the fixed SH-2 addresses into guarded host storage.
        for before, after in ((b'(u8*)0x06002000', b'ip_buffer'),
                              (b'(char*)0x06002000', b'(char*)ip_buffer'),
                              (b'(u8*)0x06002020', b'(ip_buffer+0x20)'),
                              (b'(u8*)0x06002060', b'(ip_buffer+0x60)'),
                              (b'(u16*)(0x06000380)', b'(&bios_flags[0])'),
                              (b'(u16*)(0x060003a0)', b'(&bios_flags[1])')):
            ip = ip.replace(before, after)
        code += ip
        launch = function(load, 'bios_cd_cmd')
        launch = launch.replace(b'(const char *)0x06002020', b'(const char *)(ip_buffer+0x20)')
        code += launch[:launch.index(b'\t\t// emulate bios_loadcd_boot')] + b'\t}\n\treturn 0;\n}\n'
        code += function(menu, 'pad_interrupt_enable').replace(b'#ifdef __sh__', b'#if 1')
        code += function(load, 'my_0344')
        code += function(load, 'cdp_boot').replace(b'(void*)(0x02000f00)', b'menu_entry').replace(
            b'(u32)orig_0344', b'(uintptr_t)orig_0344')
        inputs = {'cdblock.c': block, 'game_load.c': load, 'main.c': menu, 'actual.inc': code,
                  'menu_cdplayer.c': (ROOT / 'tests/menu_cdplayer.c').read_bytes(),
                  'test_menu_cdplayer.py': Path(__file__).read_bytes()}
        for name, content in inputs.items():
            (cls.target / name).write_bytes(content)
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(content).hexdigest() for name, content in inputs.items()
        }, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_MENU_CD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'menu-cdplayer'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Wno-unused-variable', '-Werror', *flags,
                   str(cls.target / 'menu_cdplayer.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode(errors='replace'))

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def cases(self, *cases):
        for case in cases:
            with self.subTest(case=case):
                result = subprocess.run([str(self.binary), str(case)], capture_output=True,
                                        text=True, timeout=10,
                                        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
                self.results.append(dict(case=case, returncode=result.returncode,
                                         stdout=result.stdout, stderr=result.stderr))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_command_requires_both_admission_events(self):
        self.cases(0, 1, 2, 3, 4)

    def test_toc_never_reads_after_rejected_command(self):
        self.cases(5, 6, 7)

    def test_audio_and_bad_toc_do_not_request_data(self):
        self.cases(8, 9, 10, 11, 12, 13, 27, 28, 29)

    def test_ip_transfer_admission_completion_and_signature(self):
        self.cases(14, 15, 16, 17, 18, 19, 20)

    def test_fast_play_completion_and_game_headers_survive(self):
        self.cases(21, 22, 23)

    def test_player_preserves_virtual_and_optical_callbacks(self):
        self.cases(24, 25, 26)

    def test_rom_read_error_returns_before_bios_entry_for_every_launch_type(self):
        self.cases(*range(33, 48))

    def test_failed_data_end_preserves_readiness_and_output(self):
        self.cases(30, 31, 32)

    def test_start_retires_the_bios_player_before_menu_initialization(self):
        self.cases(48)

    def test_idle_and_game_launch_keep_the_bios_player_callback(self):
        self.cases(49)


if __name__ == '__main__':
    unittest.main()
