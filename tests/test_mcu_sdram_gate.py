"""Exercise the stock one-pass SDRAM gate from the current MCU source."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Firm_MCU/Saturn/saturn_main.c"


class SDRAMGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = SOURCE.read_bytes()
        start = raw.index(b"\tif(debug_flags&0xc0000000){")
        cursor = raw.index(b"{", start)
        depth = 1
        end = cursor + 1
        while depth:
            depth += (raw[end] == ord("{")) - (raw[end] == ord("}"))
            end += 1

        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-sdram-gate-")
        work = Path(cls.temp.name)
        (work / "startup-gate.inc").write_bytes(raw[start:end] + b"\n")
        cls.binary = work / "gate"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-I", str(work),
            str(ROOT / "tests/mcu_sdram_test_gate.c"), "-o", str(cls.binary),
        ], check=True, capture_output=True, timeout=30)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def scenario(self, flags, errors):
        result = subprocess.run(
            [str(self.binary), hex(flags), str(errors)], capture_output=True,
            timeout=5, env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"},
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_disabled_does_no_work(self):
        for flags in (0, 0x3fffffff):
            with self.subTest(flags=flags):
                self.scenario(flags, 1)

    def test_passing_one_pass_continues(self):
        for flags in (0x80000000, 0xbfffffff):
            with self.subTest(flags=flags):
                self.scenario(flags, 0)

    def test_failed_one_pass_stops(self):
        for flags in (0x80000000, 0xbfffffff):
            for errors in (1, 8388608):
                with self.subTest(flags=flags, errors=errors):
                    self.scenario(flags, errors)

    def test_continuous_keeps_repeating(self):
        for flags in (0x40000000, 0x7fffffff, 0xc0000000, 0xffffffff):
            for errors in (0, 1):
                with self.subTest(flags=flags, errors=errors):
                    self.scenario(flags, errors)


if __name__ == "__main__":
    unittest.main()
