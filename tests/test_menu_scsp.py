"""Run the real menu startup mixer writes against a byte-addressed SCSP model."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuSCSPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_SCSP_SOURCE', ROOT / 'Firm_Saturn'))
        startup = function((source / 'main.c').read_bytes(), '_main')
        body, found, _ = startup.partition(b'\tdetect_bios_type();')
        assert found
        body = re.sub(rb'\*\(u8\*\s*\)\((0x[\da-f]+)\)\s*=\s*([^;]+);',
                      rb'write_byte(\1, \2);', body)
        body = re.sub(rb'\*\(u16\*\s*\)\((0x[\da-f]+)\)\s*=\s*([^;]+);',
                      rb'write_word(\1, \2);', body)
        body = re.sub(rb'REG16\((0x[\da-f]+)\)\s*=\s*([^;]+);',
                      rb'write_word(\1, \2);', body)
        build = ROOT / 'tests/build/menu-scsp'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'startup.inc').write_bytes(body + b'\treturn 0;\n}\n')
        cls.binary = cls.target / 'menu-scsp'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g', '-Wall',
             '-Wextra', '-Werror', *flags, '-I', str(cls.target),
             str(ROOT / 'tests/menu_scsp.c'), '-o', str(cls.binary)],
            check=True, timeout=60)

    def scenario(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True,
                                text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_each_input_uses_one_aligned_word(self): self.scenario('width')
    def test_input_level_and_stereo_pan_match_register_fields(self): self.scenario('stereo')
    def test_master_volume_and_neighbor_registers_are_preserved(self): self.scenario('neighbors')


if __name__ == '__main__':
    unittest.main()
