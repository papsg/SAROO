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


def declarations_first(raw):
    """Put uninitialized declarations before use for Icarus."""
    pattern = rb'^\t(?:wire|reg)[^=;\n]+;\n'
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
        if baseline:
            cls.baseline = (Path(baseline) / 'memhub.v').read_bytes()
        else:
            cls.baseline = omit_ack_reset(cls.sources['memhub.v'], 'ab')

    def build(self, variant):
        if variant in self.variants:
            return self.variants[variant]
        target = self.target / variant
        target.mkdir()
        inputs = dict(self.sources)
        if variant == 'baseline':
            inputs['memhub.v'] = self.baseline
        elif variant in ('omit-a', 'omit-b'):
            inputs['memhub.v'] = omit_ack_reset(inputs['memhub.v'], variant[-1])
        (target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(raw).hexdigest() for name, raw in inputs.items()
        }, indent=2) + '\n')
        for name, raw in inputs.items():
            (target / name).write_bytes(declarations_first(raw))
        binary = target / 'memory.vvp'
        command = ['iverilog', '-g2012', '-s', 'tb', '-o', str(binary),
                   *[str(target / name) for name in MODULES],
                   str(ROOT / 'tests/fpga_memory_reset_tb.v')]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        (target / 'compile.log').write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.variants[variant] = binary
        return binary

    def check_case(self, variant, port, phase, mask=0, premature=False):
        with self.subTest(variant=variant, port=port, phase=phase, mask=mask):
            binary = self.build(variant)
            result = subprocess.run(['vvp', str(binary), f'+port={port}',
                                     f'+phase={phase}', f'+mask={mask}'],
                                    capture_output=True, text=True, timeout=20)
            output = result.stdout + result.stderr
            if premature:
                self.assertNotEqual(result.returncode, 0, output)
                self.assertIn(EARLY_ACK, output)
            else:
                self.assertEqual(result.returncode, 0, output)
                self.assertIn('new writes await their own command', output)

    def test_original_reset_can_release_an_unissued_write_on_both_ports(self):
        for port in (0, 1):
            self.check_case('baseline', port, 0, premature=True)

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
            self.check_case(variant, port, 0, premature=True)
            self.check_case(variant, port ^ 1, 0)


if __name__ == '__main__':
    unittest.main()
