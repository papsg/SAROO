"""Actual menu command handler and public FPGA register behavior."""
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

from test_file_transfer import protocol_definitions
from test_menu_cdplayer import function

ROOT = Path(__file__).resolve().parents[1]


class MenuCommandCompletionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_COMPLETION_SOURCE', ROOT / 'Firm_MCU/Saturn'))
        raw = (source / 'saturn_main.c').read_bytes()
        header = (source / 'cdc.h').read_bytes()
        build = ROOT / 'tests/build/menu-command'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        start = raw.index(b'void ss_cmd_handle(void)')
        (cls.target / 'command.inc').write_bytes(raw[start:raw.index(b'\n}', start) + 2])
        directory = b'\tcase SSCMD_BINDIR:' in raw
        (cls.target / 'protocol.h').write_bytes(protocol_definitions(header) +
                                                f'#define HAS_BIN_DIRECTORY {int(directory)}\n'.encode())
        publish = re.search(rb'^[ \t]*\*\(u32\*\)\(SYSINFO_ADDR\+0x2c\) = MENU_ACK_VERSION;', raw, re.M)
        (cls.target / 'publish.inc').write_bytes(publish[0] if publish else b'')
        publish_bin = re.search(rb'^[ \t]*\*\(u32\*\)\(SYSINFO_ADDR\+0x34\) = BIN_DIRECTORY_VERSION;',
                                raw, re.M)
        (cls.target / 'publish-bin.inc').write_bytes(publish_bin[0] if publish_bin else b'')

        menu = Path(os.environ.get('SAROO_MENU_STARTUP_SOURCE', ROOT / 'Firm_Saturn'))
        main = (menu / 'main.c').read_bytes()
        main_header = (menu / 'main.h').read_bytes()
        definitions = dict(re.findall(rb'^#define (\w+)\s+(\S+)', protocol_definitions(header), re.M))
        if b'static int start_menu_services(' in main:
            for name in (b'MENU_ACK_VERSION', b'SSCMD_SAVE_ACK', b'SSCMD_STARTUP', b'SSCMD_SELECT'):
                assert re.search(rb'^#define ' + name + rb'\s+(\S+)', main_header, re.M)[1] == definitions[name]
            start = main.index(b'u32 LE32(void *ptr)')
            code = main[start:main.index(b'\n}', start) + 3]
            code += function(main_header.replace(b'static inline int timer_reached',
                                                 b'static int timer_reached'), 'timer_reached')
            code += function(main, 'menu_command_wait') + function(main, 'start_menu_services')
            assert b'if (start_menu_services())' in function(main, '_main')
        else:
            body = function(main, '_main')
            start = body.index(b'SS_ARG = 0;')
            end = body.index(b'SS_CMD = SSCMD_STARTUP;', start) + len(b'SS_CMD = SSCMD_STARTUP;')
            code = b'static int start_menu_services(void) {\n' + body[start:end] + b'\nreturn 0;\n}\n'
        code += function(main, 'select_notify')
        code += function(main, 'LE32W')
        code += function(main, 'stm32_puts').replace(b'u32 stm32_base', b'uintptr_t stm32_base')
        constants = b''.join(re.search(rb'^#define ' + name + rb'\b[^\r\n]+', main_header, re.M)[0] + b'\n'
                             for name in (b'MS2TICK', b'MENU_ITEMS', b'SELECT_ENTER', b'SELECT_EXIT'))
        (cls.target / 'menu-start.inc').write_bytes(constants + code)

        inputs = {'saturn_main.c': raw, 'cdc.h': header, 'menu-main.c': main,
                  'menu-main.h': main_header, 'SSMaster.v': (ROOT / 'FPGA/SSMaster.v').read_bytes()}
        for name in ('menu_command_completion.c', 'test_menu_command_completion.py', 'menu_command_tb.v'):
            inputs[name] = (ROOT / 'tests' / name).read_bytes()
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()
        }, indent=2) + '\n')
        cls.binary = cls.target / 'menu-command'
        flags = ['-O2'] if os.environ.get('SAROO_FILE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-function', '-Wno-unused-variable', *flags,
                   '-I', str(cls.target), str(ROOT / 'tests/menu_command_completion.c'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, mode):
        run = subprocess.run([str(self.binary), mode], capture_output=True, text=True, timeout=15,
                             env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(mode=mode, returncode=run.returncode, stdout=run.stdout, stderr=run.stderr))
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn(f'PASS {mode} ', run.stdout)

    def test_capability_uses_only_its_reserved_word(self):
        self.invoke('capability')

    def test_complete_opcode_space_and_legacy_behavior(self):
        self.invoke('dispatch')

    def test_startup_finishes_all_services_and_reports_catalog_failure(self):
        self.invoke('startup')

    def test_selection_preserves_full_index_and_enter_exit_order(self):
        self.invoke('selection')

    def test_operation_errors_and_stop_before_unload(self):
        self.invoke('results')

    def test_bin_directory_uses_its_tagged_request_and_capability(self):
        self.invoke('bin-directory')

    def test_hot_reset_drains_retained_request_before_startup(self):
        self.invoke('reset-drain')

    def test_timeout_and_retry_preserve_old_command_argument_and_payload(self):
        self.invoke('reset-timeout')

    def test_cold_and_legacy_pairs_keep_their_expected_startup_semantics(self):
        self.invoke('reset-legacy')

    def test_preview_waits_for_prior_command_before_reusing_argument(self):
        self.invoke('preview-drain')

    @unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
    def test_irq_retirement_and_command_completion_are_separate(self):
        raw = (ROOT / 'FPGA/SSMaster.v').read_bytes()
        start = raw.index(b'\treg st_irq_cmd;')
        irq = raw[start:raw.index(b'///////////////////////////////////////////////////////', start)]
        start = raw.index(b'\t// Saturn set and STM32 reset')
        registers = raw[start:raw.index(b'\treg[6:0] ss_timer_feed;', start)]
        self.assertRegex(raw, rb'wire\s+NRESET\s*=\s*ST_GPIO0\s*;')
        self.assertNotIn(b'SS_RST', irq + registers)
        dut = self.target / 'command.v'
        dut.write_bytes(
            b'module command_dut(input mclk,NRESET,SS_RST,st_wr_start,ss_wr_start,ss_reg_cs,\n'
            b'input [24:0] fsmc_addr, input [23:0] SS_ADDR,\n'
            b'input [15:0] st_wdata,SS_DATA,\n'
            b'output reg [15:0] ss_reg_cmd,ss_reg_data, output pending);\n'
            + irq + registers + b'\nassign pending=st_irq_cmd;\nendmodule\n')
        binary = self.target / 'menu-command.vvp'
        compile_result = subprocess.run(
            ['iverilog', '-g2012', '-s', 'tb', '-o', str(binary), str(dut),
             str(ROOT / 'tests/menu_command_tb.v')], capture_output=True, text=True, timeout=30)
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
        run = subprocess.run(['vvp', str(binary)], capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn('PASS 28 menu/file/save requests', run.stdout)
        self.results.append(dict(mode='fpga', returncode=run.returncode, stdout=run.stdout, stderr=run.stderr))


if __name__ == '__main__':
    unittest.main()
