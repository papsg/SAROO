"""Run the stock startup file-read block with the real FatFs types and limits."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

TESTS = Path(__file__).resolve().parent
MCU = TESTS.parent / "Firm_MCU"


class MenuLoadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(tmp.cleanup)
        output = Path(tmp.name)
        source = (MCU / "Saturn/saturn_main.c").read_bytes()
        start = source.index(b'\tretv = f_open(&fp, "/SAROO/ssfirm.bin", FA_READ);')
        end = source.index(b'\t// ', source.index(b'\tretv = f_read(', start))
        block = source[start:end]
        assert block.count(b"(void*)FIRM_ADDR") == 1
        (output / "menu_load.inc").write_bytes(
            b"static void load_menu(void) { FIL fp; int retv; UINT rv;\n"
            + block.replace(b"(void*)FIRM_ADDR", b"menu")
            + b"    ++continued;\n}\n"
        )
        limits = re.findall(rb"^#define (?:FIRM_ADDR|IMGINFO_ADDR) \S+",
                            (MCU / "Saturn/cdc.h").read_bytes(), re.M)
        assert len(limits) == 2
        (output / "menu_load_limits.inc").write_bytes(b"\n".join(limits) + b"\n")
        cls.bin = output / "menu-load"
        subprocess.run(
            [os.environ.get("CC", "cc"), "-std=c11", "-g", "-Wall", "-Wextra", "-Werror"]
            + shlex.split(os.environ.get("SAROO_TEST_CFLAGS", "-O1 -fsanitize=address,undefined"))
            + ["-I", output, "-I", MCU / "FatFS", TESTS / "mcu_menu_load.c", "-o", cls.bin],
            check=True, timeout=60,
        )

    def cases(self, sizes, scenarios):
        for size in sizes:
            for scenario in scenarios:
                with self.subTest(size=size, scenario=scenario):
                    result = subprocess.run([self.bin, str(size), str(scenario)],
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stderr)

    def test_supported_lengths(self):
        self.cases((8, 9, 4096, 524287, 524288), (0, 5))

    def test_extent_outside_reservation(self):
        self.cases((0, 1, 7, 524289, 0x80000000, 0xffffffff), (0,))
        self.cases(((hi << 32) | lo for hi in (1, 2, 0xffffffff)
                    for lo in (0, 8, 4096, 524288, 0xffffffff)), (0,))

    def test_read_must_complete_before_startup(self):
        self.cases((8, 4096, 524288), (2, 3, 4))

    def test_missing_file(self):
        self.cases((0, 8, 0x100000008), (1,))
