"""Verify SDRAM write acknowledgment ownership across reset in public FPGA RTL."""
# Adapted from paps-g e345a327.
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
MODULES = ('memhub.v', 'cachebus.v', 'cacheblk.v', 'tsdram.v')
EARLY_ACK = 'acknowledged before its WRITE command'
REFRESH_STALL = 'write never completed: state=0 ref_ack=1 ref_count='


def declarations_first(raw):
    """Put declarations before use for Icarus, keeping net assignments in place."""
    raw = re.sub(rb'^(\twire(?:\[[^\]\n]+\])?[ \t]+)(\w+)\s*=\s*([^;]+);\n',
                 rb'\1\2;\n\tassign \2 = \3;\n', raw, flags=re.M)
    pattern = rb'^\t(?:\(\*[^\n]*?\*\)[ \t\n]+)?(?:wire|reg)[^=;\n]+;\n'
    declarations = re.findall(pattern, raw, re.M)
    body = re.sub(pattern, b'', raw, flags=re.M)
    first = re.search(rb'^\t(?:wire|reg|assign|always|cachebus)\b', body, re.M)
    if not declarations or first is None:
        raise ValueError('Unexpected memory module declaration layout')
    return body[:first.start()] + b''.join(declarations) + body[first.start():]


def omit_ack_reset(raw, ports):
    start = raw.index(b'\t\tif(reset==0) begin')
    end = raw.index(b'\t\tend else begin', start)
    reset = raw[start:end]
    for port in ports:
        assignment = f'\t\t\tcmd_ack_{port} <= 0;\n'.encode()
        if reset.count(assignment) != 1:
            raise ValueError(f'Unexpected reset of port {port}')
        reset = reset.replace(assignment, b'')
    return raw[:start] + reset + raw[end:]


def retain_refresh_ack_on_reset(raw):
    decoded = b'\twire ref_ack = m_state == M_REFERSH;\n'
    if raw.count(decoded) != 1:
        raise ValueError('Unexpected refresh acknowledgment decode')
    return raw.replace(decoded, b'''\treg ref_ack;
\talways @(posedge clk) begin
\t\tif (reset && m_state == M_IDLE && ref_req) ref_ack <= 1'b1;
\t\telse if (reset && m_state == M_REFERSH) ref_ack <= 1'b0;
\tend
''')


def build_memory_model(inputs, target, bench, includes=None):
    target.mkdir()
    includes = includes or {}
    (target / 'inputs.json').write_text(json.dumps({
        name: hashlib.sha256(raw).hexdigest() for name, raw in {**inputs, **includes}.items()
    }, indent=2) + '\n')
    for name, raw in inputs.items():
        (target / name).write_bytes(declarations_first(raw))
    for name, raw in includes.items():
        (target / name).write_bytes(raw)
    binary = target / 'memory.vvp'
    window_port = ['-DSAROO_FC1_WINDOW'] if b'input ext_refresh_active;' in inputs['memhub.v'] else []
    fmc_port = ['-DSAROO_FMC_READY'] if b'output ready_b;' in inputs['memhub.v'] else []
    command = ['iverilog', '-g2012', *window_port, *fmc_port, *capture_flag(inputs['tsdram.v']),
               '-I', str(target), '-s', 'tb', '-o', str(binary),
               *[str(target / name) for name in MODULES], str(ROOT / 'tests' / bench)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    log = result.stdout + result.stderr
    (target / 'compile.log').write_text(log)
    if result.returncode:
        raise RuntimeError(log)
    return binary


def capture_flag(source):
    return ['-DSAROO_SDRAM_CAPTURE'] if b'input capture_clk;' in source else []


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGAMemoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        output = ROOT / 'tests/build/fpga-memory'
        output.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=output))
        cls.sources = {name: (ROOT / 'FPGA' / name).read_bytes() for name in MODULES}
        cls.variants = {}
        baseline = os.environ.get('SAROO_FPGA_MEMORY_BASELINE')
        cls.baseline = ({name: (Path(baseline) / name).read_bytes() for name in MODULES}
                        if baseline else {**cls.sources, 'memhub.v': omit_ack_reset(cls.sources['memhub.v'], 'ab')})
        refresh = os.environ.get('SAROO_FPGA_REFRESH_BASELINE')
        cls.refresh_baseline = (
            {name: (Path(refresh) / name).read_bytes() for name in MODULES} if refresh
            else {**cls.sources, 'tsdram.v': retain_refresh_ack_on_reset(cls.sources['tsdram.v'])})

    def build(self, variant):
        if variant in self.variants:
            return self.variants[variant]
        target = self.target / variant
        inputs = dict(self.sources)
        if variant == 'baseline':
            inputs = self.baseline
        elif variant == 'refresh-baseline':
            inputs = self.refresh_baseline
        elif variant in ('omit-a', 'omit-b'):
            inputs['memhub.v'] = omit_ack_reset(inputs['memhub.v'], variant[-1])
        binary = build_memory_model(inputs, target, 'fpga_memory_reset_tb.v')
        self.variants[variant] = binary
        return binary

    def check_case(self, variant, port, phase, mask=0, failure=None):
        with self.subTest(variant=variant, port=port, phase=phase, mask=mask):
            binary = self.build(variant)
            result = subprocess.run(['vvp', str(binary), f'+port={port}',
                                     f'+phase={phase}', f'+mask={mask}'],
                                    capture_output=True, text=True, timeout=20)
            output = result.stdout + result.stderr
            if failure:
                self.assertNotEqual(result.returncode, 0, output)
                self.assertIn(failure, output)
            else:
                self.assertEqual(result.returncode, 0, output)
                self.assertIn('new writes await their own command', output)

    def test_original_reset_can_release_an_unissued_write_on_both_ports(self):
        for port in (0, 1):
            self.check_case('baseline', port, 0, failure=EARLY_ACK)

    def test_other_original_reset_phases_are_valid_controls(self):
        for port in (0, 1):
            for phase in (1, 2, 3):
                self.check_case('baseline', port, phase)

    def test_both_ports_complete_their_own_write_after_reset(self):
        for port in (0, 1):
            for phase in range(4):
                for mask in range(4):
                    self.check_case('corrected', port, phase, mask)

    def test_each_reset_bit_is_required(self):
        for port, variant in enumerate(('omit-a', 'omit-b')):
            self.check_case(variant, port, 0, failure=EARLY_ACK)
            self.check_case(variant, port ^ 1, 0)

    def test_retained_refresh_ack_stalls_before_issue_but_adjacent_phase_passes(self):
        for port in (0, 1):
            self.check_case('refresh-baseline', port, 4, failure=REFRESH_STALL)
            self.check_case('refresh-baseline', port, 5)

    def test_both_ports_resume_after_either_refresh_phase(self):
        for port in (0, 1):
            for phase in (4, 5):
                for mask in range(4):
                    self.check_case('corrected', port, phase, mask)


if __name__ == '__main__':
    unittest.main()
