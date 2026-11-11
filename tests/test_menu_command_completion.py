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
        (cls.target / 'protocol.h').write_bytes(protocol_definitions(header))
        publish = re.search(rb'^[ \t]*\*\(u32\*\)\(SYSINFO_ADDR\+0x2c\) = MENU_ACK_VERSION;', raw, re.M)
        (cls.target / 'publish.inc').write_bytes(publish[0] if publish else b'')

        inputs = {'saturn_main.c': raw, 'cdc.h': header}
        for name in ('menu_command_completion.c', 'test_menu_command_completion.py', 'menu_command_tb.v'):
            inputs[name] = (ROOT / 'tests' / name).read_bytes()
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()
        }, indent=2) + '\n')
        cls.binary = cls.target / 'menu-command'
        flags = ['-O2'] if os.environ.get('SAROO_FILE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall', '-Wextra',
                   '-Werror', *flags, '-I', str(cls.target), str(ROOT / 'tests/menu_command_completion.c'),
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

    @unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
    def test_irq_retirement_and_command_completion_are_separate(self):
        raw = (ROOT / 'FPGA/SSMaster.v').read_bytes()
        start = raw.index(b'\treg st_irq_cmd;')
        irq = raw[start:raw.index(b'///////////////////////////////////////////////////////', start)]
        start = raw.index(b'\t// Saturn set and STM32 reset')
        registers = raw[start:raw.index(b'\treg[6:0] ss_timer_feed;', start)]
        dut = self.target / 'command.v'
        dut.write_bytes(
            b'module command_dut(input mclk,NRESET,st_wr_start,ss_wr_start,ss_reg_cs,\n'
            b'input [24:0] fsmc_addr, input [23:0] SS_ADDR,\n'
            b'input [15:0] ST_AD,SS_DATA,\n'
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
