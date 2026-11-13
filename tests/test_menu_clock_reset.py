"""Run the two actual BIOS clock hooks and the ROM installer with modeled MMIO."""
import os
from pathlib import Path
import re
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
            defines = (b'#define SMPC_RESET_POLL_BUDGET ', b'#define RESENAB ',
                       b'#define RESDISA ')
            code += b'\n'.join(line for line in header.splitlines()
                               if line.startswith(defines)) + b'\n'
            plain_header = header.replace(b'static inline int smpc_', b'static int smpc_')
            names = ('smpc_reset_command', 'smpc_enable_reset', 'smpc_disable_reset')
            code += b'\n'.join(function(plain_header, name) for name in names
                               if f'int {name}('.encode() in plain_header)
        if b'void scu_set_asr0(' in header:
            code += re.search(rb'^#define SCU_ABUS_DUMMY_ADDR .+$', header, re.M)[0] + b'\n'
            plain_header = header.replace(b'static inline __attribute__((always_inline)) void',
                                          b'static void')
            code += function(plain_header, 'scu_set_asr0')
        code += function(load, '_call_04c8')
        code += function(load, 'my_06b0') + function(rom, 'jump_06b0') + function(rom, 'card_init')
        # Execute the real player handoff up to its first destructive write.
        player, found, _ = function(load, 'my_cdplayer').partition(
            b'memset((u8*)0x0600a000, 0, 0xf6000);')
        assert found
        player = player.replace(b'*(u8 *)(0x2010001f)', b'COMREG')
        player = player.replace(b'go = (void*)*(u32*)(0x060002dc);',
                                b'go = player_prepare;')
        player = player.replace(b'go = (void*)0x00280000;', b'go = player_clock;')
        player = player.replace(b'memcpy((u8*)0x00280000, _call_04c8, 64);',
                                b'player_copy_clock();')
        for name in (b'DMAOR', b'CHCR0', b'CHCR1', b'DRCR0', b'DRCR1',
                     b'SAR0', b'DAR0', b'TCR0', b'SAR1', b'DAR1', b'TCR1'):
            width, address = re.search(
                rb'^#define ' + name + rb'\s+REG(8|16|32)?\s*\((0x[\da-f]+)\)',
                header, re.M).groups()
            register = address + b', ' + (width or b'32')
            player = re.sub(rb'\b' + name + rb'\s*=\s*([^;]+);',
                            rb'player_write(' + register + rb', \1);', player)
            player = player.replace(b'(void)' + name + b';',
                                    b'player_read(' + register + b');')
        for width in (b'8', b'16', b'32'):
            for register in (rb'\*\(u' + width + rb'\s*\*\)', b'REG' + width):
                player = re.sub(register + rb'\((0x[\da-f]+)u?\)\s*=\s*([^;]+);',
                                rb'player_write(\1, ' + width + rb', \2);', player)
        code += player + b'player_replace_work_ram();\n}\n'
        for original, replacement in (
            (b'(void*)(0x04c8)', b'bios_clock'), (b'(void*)0x1800', b'bios_scu'),
            (b'(void)REG32(SCU_ABUS_DUMMY_ADDR)', b'abus_read(SCU_ABUS_DUMMY_ADDR)'),
            (b'ASR0 =', b'*asr_write_port() ='),
            (b'*(u32*)(0x25fe00b0)', b'*asr_write_port()'),
            (b'*(u32*)(0x25fe00b8)', b'aref'),
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
    def test_player_waits_for_reset_disable_with_display_off(self): self.scenario('player')
    def test_player_disables_then_purges_then_enables_cache(self): self.scenario('cache')
    def test_player_stops_dma_before_reconfiguring_channels(self): self.scenario('dma')
    def test_clock_restores_complete_an_abus_read_before_asr0(self): self.scenario('abus')
    def test_player_does_not_replace_work_ram_on_reset_failure(self): self.scenario('player-failure')


if __name__ == '__main__':
    unittest.main()
