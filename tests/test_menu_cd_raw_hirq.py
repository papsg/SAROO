"""Exercise the current menu's copied CD-authentication transaction."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Firm_Saturn/cdblock.c"


def host_function(raw, fixed):
    signature = ("int " if fixed else "void ") + "cdc_auth_hram("
    start = raw.index(signature.encode())
    code = raw[start:raw.index(b"\n}", start) + 3]
    if fixed:
        # The target build force-inlines this wait into the relocated routine.
        code = code.replace(b"cdc_raw_wait(", b"wait_hirq(")
        code = code.replace(b"HIRQ = ~clear;", b"record_clear(clear);")
    code = code.replace(b"REG(0x25818000) = 0x00020002;",
                        b"write_data(0x00020002);")
    for mask in (
        b"HIRQ_CMOK|HIRQ_EHST", b"HIRQ_CMOK|HIRQ_ECPY", b"HIRQ_DRDY",
        b"HIRQ_CMOK", b"HIRQ_CMOK|HIRQ_DCHG|HIRQ_EFLS",
    ):
        code = code.replace(b"HIRQ = ~(" + mask + b");",
                            b"record_clear(" + mask + b");")
    code = code.replace(b"HIRQ = ~HIRQ_DRDY;", b"record_clear(HIRQ_DRDY);")
    return code


class RawHIRQTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = SOURCE.read_bytes()
        fixed = b"int cdc_auth_hram(" in raw
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-cd-raw-hirq-")
        work = Path(cls.temp.name)
        (work / "actual.inc").write_bytes(host_function(raw, fixed))
        cls.binary = work / "test"
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=gnu11", "-O1", "-g",
            "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer", "-DEXPECT_FIXED=" + str(int(fixed)),
            "-I", str(work), str(ROOT / "tests/menu_cd_raw_hirq.c"),
            "-o", str(cls.binary),
        ], check=True, capture_output=True, timeout=30)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def case(self, scenario):
        result = subprocess.run(
            [str(self.binary), scenario], capture_output=True, timeout=10,
            env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"},
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_command_and_completion_sequence(self):
        self.case("protocol")

    def test_each_wait_failure_is_bounded(self):
        for index in range(8):
            with self.subTest(index=index):
                self.case("fail-" + str(index))

    def test_unavailable_write_storage(self):
        self.case("put-full")

    def test_reject_and_wait_responses(self):
        for command in range(4):
            for kind in ("reject-", "wait-"):
                with self.subTest(command=command, kind=kind):
                    self.case(kind + str(command))


if __name__ == "__main__":
    unittest.main()
