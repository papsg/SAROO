"""Actual menu initialization with synthetic VRAM/registers and palette oracle."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MenuPaletteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_PALETTE_SOURCE', ROOT / 'Firm_Saturn/conio.c'))
        data = source.read_bytes()
        build = ROOT / 'tests/build/menu-palette'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build)); cls.results = []
        palette = data[data.index(b'static const unsigned int vga_pal['):data.index(b'#include "font_latin.h"')]
        startup = data[data.index(b'void logo_trans(void)'):data.index(b'void conio_init(void)')]
        cover = data[data.index(b'void nbg1_on(void)'):data.index(b'void vdp2_win0(')]
        (cls.target / 'palette.inc').write_bytes(palette + startup + cover)
        inputs = {'conio.c': data}
        for name in ('slogo.h', 'vdp2.h'): inputs[name] = (ROOT / 'Firm_Saturn' / name).read_bytes()
        for name in ('menu_palette.c', 'test_menu_palette.py'): inputs[name] = (ROOT / 'tests' / name).read_bytes()
        for name, content in inputs.items(): (cls.target / name).write_bytes(content)
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(content).hexdigest() for name, content in inputs.items()}, indent=2)+'\n')
        cls.binary = cls.target / 'menu-palette'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-fno-strict-aliasing', *flags, '-I', str(cls.target),
                   str(cls.target / 'menu_palette.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(command, indent=2)+'\n')
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def invoke(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=case, returncode=result.returncode,
                                 stdout=result.stdout.decode(errors='replace'),
                                 stderr=result.stderr.decode(errors='replace')))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn(b'runtime error:', result.stderr)
        self.assertIn((case + ': pass').encode(), result.stdout)

    def test_startup_and_reinitialization_set_only_the_complete_text_bank(self): self.invoke('startup')
    def test_startup_after_logo_sets_text_bank_and_preserves_other_banks(self): self.invoke('startup-logo')
    def test_vdp1_minimal_erase_rectangle_is_defined_on_every_startup(self): self.invoke('erase-bounds')
    def test_vdp2_bitmap_and_cpu_slots_obey_the_partition_schedule(self): self.invoke('vram-cycles')
    def test_original_logo_palette_and_tile_upload_remain_intact(self): self.invoke('logo')
    def test_cover_palette_endpoints_do_not_change_text_colors(self): self.invoke('cover')


if __name__ == '__main__': unittest.main()
