"""Compile the actual MCU formatter with ASan/UBSan and exercise its API."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class MCUPrintkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-mcu-printk-")
        work = Path(cls.temp.name)
        (work / "main.h").write_text(
            "#include <stdarg.h>\n"
            "typedef unsigned char u8;\n"
            "int vsnprintk(char *, int, char *, va_list);\n"
            "int printk(char *, ...);\n"
            "void uart4_puts(char *);\n"
            "void _puts(char *);\n"
        )
        cls.executables = {}
        for name, flags in (("app", []), ("boot", ["-DBOOT=1"])):
            target = work / name
            command = [
                os.environ.get("CC", "cc"), "-std=gnu11", "-O1", "-g",
                "-Wall", "-Wextra", "-Werror", "-fno-omit-frame-pointer",
                "-fsanitize=address,undefined", "-I", str(work), *flags,
                str(ROOT / "Firm_MCU/Main/printk.c"),
                str(ROOT / "tests/mcu_printk_contract.c"), "-o", str(target),
            ]
            subprocess.run(command, check=True, capture_output=True, timeout=30)
            cls.executables[name] = target

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def check_case(self, name, scenario):
        result = subprocess.run(
            [str(self.executables[name]), str(scenario)], capture_output=True,
            timeout=5, env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"},
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b"pass", result.stdout)

    def test_short_messages_and_integer_control(self):
        for name in self.executables:
            with self.subTest(name=name):
                self.check_case(name, 0)

    def test_long_admitted_path_keeps_terminator(self):
        for name in self.executables:
            with self.subTest(name=name):
                self.check_case(name, 1)

    def test_minimum_signed_integer(self):
        self.check_case("app", 2)

    def test_sign_and_alignment(self):
        self.check_case("app", 3)

    def test_huge_width_stops_at_capacity(self):
        self.check_case("app", 4)

    def test_invalid_and_one_byte_destinations(self):
        self.check_case("app", 5)


if __name__ == "__main__":
    unittest.main()
