"""Check the stock BootLoader FMC setup against RM0433 Rev. 8."""
# Adapted from paps-g 8345fa51.
import os
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class FMCConfigurationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ.get('SAROO_FMC_SOURCE', ROOT / 'Firm_MCU/Main/main.c'))
        body = path.read_bytes().split(b'void device_init(void)', 1)[1].split(b'\n}', 1)[0]
        boot = body.split(b'#ifdef BOOT', 1)[1].split(b'#endif', 1)[0]
        fields = re.findall(rb'FMC_Bank1_R->BTCR\[([01])\]\s*=\s*(0x[0-9a-fA-F]+);', boot)
        assert [index for index, _ in fields] == [b'0', b'1']
        cls.control, cls.timing = (int(word, 16) for _, word in fields)

    def test_reserved_control_bits_match_muxed_mode_contract(self):
        # RM0433 Table 184 reserves bit 7 as one and these other bits as zero.
        self.assertEqual(self.control & ((0x1f<<26) | (3<<22) | (1<<10) | (1<<7)), 1<<7)

    def test_nor_bus_control_fields_keep_existing_behavior(self):
        value = self.control
        self.assertTrue(value & (1<<31))           # FMC enable
        self.assertTrue(value & (1<<15))           # asynchronous NWAIT
        self.assertTrue(value & (1<<12))           # write enable
        self.assertFalse(value & (1<<14))          # shared timing
        self.assertFalse(value & (1<<9))           # active-low NWAIT
        self.assertEqual((value >> 4) & 3, 1)       # 16-bit width
        self.assertEqual((value >> 2) & 3, 2)       # NOR memory type
        self.assertEqual(value & 3, 3)             # muxed address/data and bank enable

    def test_nor_timing_fields_match_stock_waits(self):
        value = self.timing
        self.assertEqual(value >> 20, 0)
        self.assertEqual((value>>16) & 15, 2)        # turnaround
        self.assertEqual((value>>8) & 255, 6)        # data setup
        self.assertEqual((value>>4) & 15, 1)         # address hold
        self.assertEqual(value & 15, 2)             # address setup
