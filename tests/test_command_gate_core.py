"""Check the staged command gate with a host TAS model."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/command-gate-core"


class CommandGateCoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix="case-", dir=BUILD)
        cls.addClassCleanup(cls.work.cleanup)
        profile = os.environ.get("SAROO_COMMAND_GATE_PROFILE", "sanitizer")
        if profile not in ("sanitizer", "release"):
            raise ValueError("unknown profile")
        flags = (["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                 if profile == "sanitizer" else ["-O2"])
        cls.binary = Path(cls.work.name) / "test"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-g",
                   "-Wall", "-Wextra", "-Werror", "-pthread", *flags,
                   "-I", str(ROOT / "Firm_Saturn"),
                   str(ROOT / "tests/command_gate_core.c"), "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (BUILD / (f"{profile}.compile.log")).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))

    def invoke(self, mode):
        result = subprocess.run([str(self.binary), mode], capture_output=True,
                                text=True, timeout=30,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                     "ASAN_OPTIONS": "detect_leaks=0"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS", result.stdout)

    def test_boot_owner_and_sticky_fault(self):
        self.invoke("boot")

    def test_reply_domains_and_reset_cleared_values(self):
        self.invoke("replies")

    def test_two_cpu_contention(self):
        self.invoke("contention")


if __name__ == "__main__":
    unittest.main()
