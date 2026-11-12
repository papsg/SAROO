"""Run the two actual BIOS clock hooks and the ROM installer with modeled MMIO."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuClockResetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        load = (source / 'game_load.c').read_bytes()
        rom = (source / 'game_patch.c').read_bytes()
        header = (source / 'smpc.h').read_bytes()
        code = b''
        if b'int smpc_enable_reset(' in header:
            defines = b'\n'.join(line for line in header.splitlines() if line.startswith(
                (b'#define SMPC_RESET_POLL_BUDGET ', b'#define RESENAB ')))
            helper = function(header.replace(b'static inline int smpc_enable_reset',
                                             b'static int smpc_enable_reset'), 'smpc_enable_reset')
            code += defines + b'\n' + helper
        code += function(load, 'my_06b0') + function(rom, 'jump_06b0') + function(rom, 'card_init')
        for original, replacement in (
            (b'(void*)(0x04c8)', b'bios_clock'), (b'(void*)0x1800', b'bios_scu'),
            (b'*(u32*)(0x25fe00b0)', b'asr'), (b'*(u32*)(0x25fe00b8)', b'aref'),
            (b'*(u32*)(0x06000324)', b'0'), (b'*(u32*)(0x06000358)', b'save_vector'),
            (b'*(u32*)(0x0600026c)', b'player_vector'),
            (b'*(u32*)(0x06000320)', b'(*clock_vector())'),
        ):
            code = code.replace(original, replacement)
        # Exercise the SH-2 installer's exact copy requests on host memory.
        code = code.replace(b'#ifdef __sh__\n', b'').replace(b'#endif\n', b'')
        build = ROOT / 'tests/build/menu-clock-reset'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'actual.inc').write_bytes(code)
        cls.binary = cls.target / 'clock-reset'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_CD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
                   str(ROOT / 'tests/menu_clock_reset.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def scenario(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True, text=True,
                                timeout=20, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_busy_smpc_and_fresh_scanline_precede_submission(self): self.scenario('admission')
    def test_external_latch_and_busy_admission_expire(self): self.scenario('refusal')
    def test_completion_uses_remaining_poll_budget(self): self.scenario('completion')
    def test_valid_hooks_preserve_bios_calls_and_bus_restore(self): self.scenario('healthy')
    def test_rom_clock_vector_publishes_after_both_copies(self): self.scenario('install')


if __name__ == '__main__':
    unittest.main()
