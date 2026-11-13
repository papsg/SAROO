"""Exercise the menu's actual scrolling upload with deferred DMA."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function
from test_menu_scu_dma import level0_busy_mask


class MenuSelectionDMATests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_SELECTION_SOURCE', ROOT / 'Firm_Saturn'))
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        target = Path(directory.name)
        code = function((source / 'conio.c').read_bytes(), 'put_select_item')
        (target / 'selection-dma.inc').write_bytes(level0_busy_mask(source) + code)
        cls.binary = target / 'selection-dma'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SCU_DMA_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-Wall', '-Wextra',
             '-Werror', '-Wno-unused-function', *flags, '-I', target,
             ROOT / 'tests/menu_selection_dma.c', '-o', cls.binary],
            check=True, timeout=60)

    def scenario(self, busy, foreign=0):
        result = subprocess.run([self.binary, hex(busy), hex(foreign)], timeout=10,
                                capture_output=True, text=True,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('selection DMA ownership: pass', result.stdout)

    def test_waiting_active_and_interrupted_rows_retire(self):
        for bit in (5, 4, 16):
            with self.subTest(bit=bit):
                self.scenario(1 << bit, 1 << 12)

    def test_idle_row_copies_without_waiting_for_other_channels(self):
        self.scenario(0, 1 << 19)


if __name__ == '__main__':
    unittest.main()
