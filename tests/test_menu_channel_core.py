"""Exercise the staged channel with the production command gate and wire IDs."""

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/menu-channel-core"
WIRE_NAMES = (
    "SSCMD_SAVE_ACK", "SSCMD_SELECT", "SSCMD_CHECK", "SSCMD_UPDATE",
    "SSCMD_LISTDISC", "SSCMD_STARTUP", "SSCMD_LOADDISC", "SSCMD_LSAVE",
    "SELECT_ENTER", "SELECT_EXIT",
)


def write_protocol(work):
    header = (ROOT / "Firm_Saturn/main.h").read_text()
    lines = []
    for name in WIRE_NAMES:
        match = re.search(rf"^#define\s+{name}\s+\S+\s*$", header, re.M)
        if not match:
            raise AssertionError(f"missing menu wire constant: {name}")
        lines.append(match.group())
    (work / "menu-channel-protocol.h").write_text("\n".join(lines) + "\n")


class MenuChannelCoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix="case-", dir=BUILD)
        cls.addClassCleanup(cls.work.cleanup)
        work = Path(cls.work.name)
        write_protocol(work)
        profile = os.environ.get("SAROO_MENU_CHANNEL_PROFILE", "sanitizer")
        if profile not in ("sanitizer", "release"):
            raise ValueError("unknown profile")
        flags = (["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                 if profile == "sanitizer" else ["-O2"])
        cls.binary = work / "test"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-g",
                   "-Wall", "-Wextra", "-Werror", *flags,
                   "-I", str(work), "-I", str(ROOT / "Firm_Saturn"),
                   str(ROOT / "tests/menu_channel_core.c"), "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (BUILD / f"{profile}.compile.log").write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))

    def invoke(self, mode):
        result = subprocess.run([str(self.binary), mode], capture_output=True,
                                text=True, timeout=30,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                     "ASAN_OPTIONS": "detect_leaks=0"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS", result.stdout)

    def test_capability_and_shared_ownership(self):
        self.invoke("capability")

    def test_explicit_reply_domains(self):
        self.invoke("replies")

    def test_key_preparation_and_drain_timeout(self):
        self.invoke("key")


if __name__ == "__main__":
    unittest.main()
