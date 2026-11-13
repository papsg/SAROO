"""Run the actual menu and MCU save protocol at a modeled register boundary."""
# Adapted from paps-g 74999ab0.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest
from test_file_transfer import command_dispatch


ROOT = Path(__file__).resolve().parents[1]


def mcu_dispatch(raw):
    function = raw.index(b'void ss_cmd_handle(void)')
    start = re.search(rb'^[ \t]*case SSCMD_SSAVE:', raw[function:], re.M).start() + function
    end = raw.index(b'\tcase SSCMD_STARTUP:', start)
    dispatch = command_dispatch(raw)
    return (b'static void mcu_command(void) { int retv; u32 cmd = SS_CMD;\n'
            + dispatch + b'\n' + raw[start:end] + b'default: break;\n} }\n')


class SaveAcknowledgmentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        current_menu = ROOT / 'Firm_Saturn'
        current_mcu = ROOT / 'Firm_MCU/Saturn'
        menu = Path(os.environ.get('SAROO_ACK_MENU_DIR', current_menu))
        mcu = Path(os.environ.get('SAROO_ACK_MCU_DIR', current_mcu))
        build = ROOT / 'tests/build/save-ack'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))

        dispatch = (mcu / 'saturn_main.c').read_bytes()
        (cls.target / 'mcu-command.inc').write_bytes(mcu_dispatch(dispatch))
        publish = re.search(rb'^[ \t]*\*\(u32\*\)\(SYSINFO_ADDR\+0x10\) = SAVE_ACK_VERSION;', dispatch, re.M)
        (cls.target / 'publish.inc').write_bytes(publish[0] if publish else b'/* Legacy MCU. */')

        save = (menu / 'game_save.c').read_bytes()
        start, end = save.index(b'#define MEMS_HDR'), save.index(b'static void set_bitmap(')
        (cls.target / 'menu-command.inc').write_bytes(save[start:end])
        main = (menu / 'main.c').read_bytes()
        start = main.index(b'u32 LE32(void *ptr)')
        (cls.target / 'le32.inc').write_bytes(main[start:main.index(b'\n}', start) + 2])
        latch = re.search(rb'^[ \t]*save_ack = LE32.*$', main, re.M)
        (cls.target / 'latch.inc').write_bytes(latch[0] if latch else b'save_ack = 0;')

        definitions = []
        input_paths = ((menu / 'main.h', current_menu / 'main.h'),
                       (mcu / 'cdc.h', current_mcu / 'cdc.h'))
        for name in (b'SSCMD_SSAVE', b'SSCMD_LSAVE', b'SSCMD_LMEMS', b'SSCMD_SMEMS',
                     b'SSCMD_FILERD', b'SSCMD_FILEWR', b'SSCMD_PRINTF', b'SSCMD_SELECT',
                     b'SSCMD_SAVE_ACK', b'SAVE_ACK_VERSION'):
            values = []
            for selected, fallback in input_paths:
                pattern = rb'^#define ' + name + rb'\s+(\S+)'
                found = re.search(pattern, selected.read_bytes(), re.M)
                if not found and name in (b'SSCMD_SAVE_ACK', b'SAVE_ACK_VERSION'):
                    found = re.search(pattern, fallback.read_bytes(), re.M)
                if not found:
                    raise AssertionError(f'Missing save command constant {name!r}')
                values.append(found[1])
            if values[0] != values[1]:
                raise AssertionError(f'Menu and MCU disagree on {name!r}')
            definitions.append(b'#define ' + name + b' ' + values[0])
        listbin = re.search(rb'^#define SSCMD_LISTBIN\s+\S+', (mcu / 'cdc.h').read_bytes(), re.M)
        definitions.append(listbin[0])
        (cls.target / 'commands.h').write_bytes(b'\n'.join(definitions) + b'\n')
        inputs = {
            'menu_main.c': main, 'menu_game_save.c': save, 'mcu_saturn_main.c': dispatch,
            'menu_main.h': (menu / 'main.h').read_bytes(), 'mcu_cdc.h': (mcu / 'cdc.h').read_bytes(),
        }
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()
        }, indent=2) + '\n')

        cls.binary = cls.target / 'save-ack'
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        if b'int save_command(' in save:
            flags.append('-DHAS_SAVE_ACK')
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
                   str(ROOT / 'tests/save_ack.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

    def invoke(self, mode):
        result = subprocess.run([str(self.binary), mode], capture_output=True, text=True,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'},
                                timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS ' + mode, result.stdout)
        print(result.stdout.strip())

    def test_capability_word_and_menu_latch(self):
        self.invoke('capability')

    def test_dispatch_all_commands_and_preserve_legacy_results(self):
        self.invoke('dispatch')

    def test_tagged_replies_and_full_width_arguments(self):
        self.invoke('completion')

    def test_reset_keeps_pending_saves_and_dirty_bits(self):
        self.invoke('reset')

    @unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
    def test_public_fpga_transports_tags_and_clears_on_reset(self):
        raw = (ROOT / 'FPGA/SSMaster.v').read_bytes()
        start = raw.index(b'\t// Saturn set and STM32 reset')
        end = raw.index(b'\treg[6:0] ss_timer_feed;', start)
        wrapper = self.target / 'command.v'
        wrapper.write_bytes(b'''module command_dut(input mclk,NRESET,st_wr_start,ss_wr_start,ss_reg_cs,
input [24:0] fsmc_addr, input [23:0] SS_ADDR, input [15:0] st_wdata,SS_DATA,
output reg [15:0] ss_reg_cmd,ss_reg_data);
''' + raw[start:end] + b'\nendmodule\n')
        binary = self.target / 'save-ack.vvp'
        compile_result = subprocess.run(
            ['iverilog', '-g2012', '-s', 'tb', '-o', str(binary), str(wrapper),
             str(ROOT / 'tests/save_ack_tb.v')],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
        result = subprocess.run(['vvp', str(binary)], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS 12 tagged file/save opcodes', result.stdout)


if __name__ == '__main__':
    unittest.main()
