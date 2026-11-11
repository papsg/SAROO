"""Verify the actual file path against the FPGA's modeled RAM decoder."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from mcu_save_support import make_fat32
from test_menu_file_transfer import MenuFileTransferTests as TransferFixture


class MenuFileRAMTests(unittest.TestCase):
    c_source = 'menu_file_ram.c'
    setUpClass = classmethod(TransferFixture.setUpClass.__func__)
    tearDownClass = classmethod(TransferFixture.tearDownClass.__func__)

    def invoke(self, address, size, mode=2, writing=0, short=0, fail=0, native=0):
        case = (address, size, mode, writing, short, fail, native)
        with self.subTest(address=hex(address), size=size, mode=mode, writing=writing,
                          short=short, fail=fail, native=native):
            with tempfile.TemporaryDirectory(dir=self.target) as directory:
                card = Path(directory) / 'card.img'
                make_fat32(card)
                result = subprocess.run([str(self.binary), str(card), *map(str, case)],
                                        capture_output=True, text=True, timeout=30,
                                        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
            self.results.append(dict(case=case, returncode=result.returncode,
                                     stdout=result.stdout, stderr=result.stderr))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotIn('runtime error:', result.stderr)
            spans = 1
            if mode == 2 and not native:
                spans += sum(address < boundary < address + size
                             for boundary in range(0x02400000, 0x02800001, 0x80000))
            self.assertEqual(int(re.search(r'transfers=(\d+) pass', result.stdout)[1]),
                             fail or spans)

    def test_each_one_mib_mirror_reads_and_writes_the_cpu_visible_bytes(self):
        for address in range(0x02400000, 0x02800000, 0x80000):
            for writing in (0, 1):
                self.invoke(address + 3, 1029, writing=writing)

    def test_crossing_mirror_and_window_boundaries_preserves_the_file_and_ram(self):
        for address in (0x023ffff0, 0x0247fff0, 0x025ffff0, 0x027ffff0):
            for writing in (0, 1):
                self.invoke(address, 0x80021, writing=writing)
        for writing in (0, 1):
            self.invoke(0x02400000, 0x400000, writing=writing)
            self.invoke(0x023ffff0, 0x400021, writing=writing)

    def test_other_cart_modes_keep_linear_transfers(self):
        for mode in (0, 1, 3):
            for writing in (0, 1):
                self.invoke(0x0247fff0, 0x80021, mode=mode, writing=writing)

    def test_short_reads_and_later_failures_keep_the_completed_prefix(self):
        self.invoke(0x0247fff0, 39, short=1)
        for writing in (0, 1):
            self.invoke(0x0247fff0, 39, writing=writing, fail=2)

    def test_native_mcu_addresses_keep_their_physical_location(self):
        for writing in (0, 1):
            self.invoke(0x02480003, 1029, writing=writing, native=1)


del TransferFixture
if __name__ == '__main__':
    unittest.main()
