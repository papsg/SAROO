"""Check the current SH-2 menu SCI initialization and error recovery."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Firm_Saturn/main.c"


class SCIContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = SOURCE.read_bytes()
        start = raw.index(b"void sci_init(void)")
        end = raw.index(b"/**********************************************************/", start)
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-menu-sci-")
        work = Path(cls.temp.name)
        (work / "sci.inc").write_bytes(raw[start:end])
        cls.binary = work / "sci-contract"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=gnu11", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-I", str(work),
            str(ROOT / "tests/menu_sci_contract.c"), "-o", str(cls.binary),
        ], check=True, capture_output=True, timeout=30)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_init_and_receive_recovery(self):
        result = subprocess.run(
            [str(self.binary)], capture_output=True, timeout=10,
            env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"},
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b"pass", result.stdout)


if __name__ == "__main__":
    unittest.main()
