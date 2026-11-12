"""Check physical SDRAM cache identity and geometry changes in production RTL."""
# Adapted from paps-g 7b9e51b1 and e265bd35.
import itertools
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_fpga_memory import MODULES, ROOT, build_memory_model


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGACacheGeometryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_FPGA_GEOMETRY_SOURCE', ROOT / 'FPGA'))
        output = ROOT / 'tests/build/fpga-cache-geometry'
        output.mkdir(parents=True, exist_ok=True)
        target = Path(tempfile.mkdtemp(prefix='case-', dir=output))
        cls.binary = build_memory_model({name: (source / name).read_bytes() for name in MODULES},
                                        target / 'rtl', 'fpga_cache_geometry_tb.v')

    def check_case(self, **options):
        with self.subTest(**options):
            args = [f'+{name}={value}' for name, value in options.items()]
            result = subprocess.run(['vvp', str(self.binary), *args],
                                    capture_output=True, text=True, timeout=15)
            log = result.stdout + result.stderr
            self.assertEqual(result.returncode, 0, log)
            self.assertIn('PASS SDRAM physical address', log)

    def test_ignored_address_bits_are_coherent_on_both_ports(self):
        for kind, port, mask in itertools.product(range(3), range(2), range(4)):
            for bit in range(23 + kind, 26):
                self.check_case(phase=0, kind=kind, port=port, bit=bit, mask=mask)

    def test_highest_decoded_bit_keeps_cells_distinct(self):
        for kind, port, mask in itertools.product(range(4), range(2), range(4)):
            self.check_case(phase=1, kind=kind, port=port, bit=22 + kind, mask=mask)

    def test_same_edge_read_cannot_hit_an_old_geometry(self):
        for old, kind, port, low in itertools.product(range(4), range(4), range(2), range(2)):
            if kind != old:
                self.check_case(phase=2, old=old, kind=kind, port=port, low=low)

    def test_geometry_remains_stable_until_the_last_accepted_word(self):
        for phase, old, kind, port in itertools.product(range(3, 6), range(4), range(4), range(2)):
            if kind != old:
                self.check_case(phase=phase, old=old, kind=kind, port=port)
