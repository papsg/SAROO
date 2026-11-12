"""Check the stock BootLoader FMC setup against RM0433 Rev. 8."""
# Adapted from paps-g 8345fa51.
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
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


# Adapted from paps-g cb70560a. Exercise both public SystemInit variants.
class AXIStartupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ.get('SAROO_SYSTEM_INIT_SOURCE', ROOT / 'Firm_MCU/Startup/system_init.c'))
        body = path.read_bytes()
        start = body.index(b'void SystemInit (void)')
        end = body.index(b'\n}', start) + 2
        body = body[start:end]
        body = body.replace(b'((__IO uint32_t *)0x51008108U)', b'axi_target()')
        body = body.replace(b'(uint32_t)&__Vectors', b'(uint32_t)(uintptr_t)&__Vectors')
        temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(temporary.cleanup)
        target = Path(temporary.name)
        (target / 'system-init.inc').write_bytes(body)
        flags = ['-O2'] if os.environ.get('SAROO_STARTUP_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binaries = {}
        for kind, defines in (('app', []), ('boot', ['-DBOOT'])):
            out = target / kind
            build = subprocess.run([*shlex.split(os.environ.get('CXX', 'c++')), '-std=c++17',
                                    '-Wall', '-Wextra', '-Werror', *flags, *defines,
                                    '-I', target, ROOT / 'tests/mcu_startup.cpp', '-o', out],
                                   capture_output=True, text=True, timeout=30)
            if build.returncode:
                raise RuntimeError(build.stderr)
            cls.binaries[kind] = out

    def check_revision(self, revision, expected):
        for kind, binary in self.binaries.items():
            for device in (0x450, 0x7450):
                for register in (0, 2, 3, 0xa5000002):
                    with self.subTest(build=kind, revision=revision, device=device, register=register):
                        result = subprocess.run([binary, str((revision << 16) | device),
                                                 str(register), str(expected)],
                                                capture_output=True, text=True, timeout=5)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_affected_yw_revision_updates_axi_before_startup(self):
        self.check_revision(0x1003, 1)

    def test_other_revisions_leave_axi_register_untouched(self):
        for revision in (0x1001, 0x2001, 0x2003, 0xffff):
            self.check_revision(revision, 0)
