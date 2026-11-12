"""Check the menu's actual SCU DMA status handling on host registers."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuSCUDMATests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_SCU_DMA_SOURCE', ROOT / 'Firm_Saturn'))
        build = ROOT / 'tests/build/menu-scu-dma'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        code = function((source / 'main.c').read_bytes(), 'scu_dmemcpy')
        (cls.target / 'scu-dmemcpy.inc').write_bytes(code)
        cls.binary = cls.target / 'menu-scu-dma'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SCU_DMA_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g', '-Wall',
             '-Wextra', '-Werror', '-Wno-pointer-to-int-cast', *flags, '-I',
             str(cls.target), str(ROOT / 'tests/menu_scu_dma.c'), '-o', str(cls.binary)],
            check=True, timeout=60)

    def scenario(self, channel, status, reads):
        result = subprocess.run([str(self.binary), str(channel), hex(status), str(reads)],
                                capture_output=True, text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_wait_for_own_status_bits(self):
        for channel, bits in enumerate(((4, 5, 16), (8, 17, 19), (12, 13))):
            for bit in bits:
                with self.subTest(channel=channel, bit=bit):
                    self.scenario(channel, 1 << bit, 2)

    def test_ignore_other_and_reserved_status_bits(self):
        channels = ({4, 5, 16}, {8, 17, 19}, {12, 13})
        all_bits = set.union(*channels, {9})
        for channel, bits in enumerate(channels):
            for bit in sorted(all_bits - bits):
                with self.subTest(channel=channel, bit=bit):
                    self.scenario(channel, 1 << bit, 1)

    def test_idle_channel_starts_immediately(self):
        for channel in range(3):
            with self.subTest(channel=channel):
                self.scenario(channel, 0, 1)


if __name__ == '__main__':
    unittest.main()
