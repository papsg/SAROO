"""Exercise selected launch binding with an owned command peer model."""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from tests.test_menu_channel_core import write_protocol


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/save-launch-core"


class SaveLaunchCoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix="case-", dir=BUILD)
        cls.addClassCleanup(cls.work.cleanup)
        work = Path(cls.work.name)
        write_protocol(work)
        profile = os.environ.get("SAROO_SAVE_LAUNCH_PROFILE", "sanitizer")
        if profile not in ("sanitizer", "release"):
            raise ValueError("unknown profile")
        flags = (["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                 if profile == "sanitizer" else ["-O2"])
        cls.binary = work / "test"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-g",
                   "-Wall", "-Wextra", "-Werror", *flags,
                   "-I", str(work), "-I", str(ROOT / "Firm_Saturn"),
                   str(ROOT / "tests/save_launch_core.c"), "-o", str(cls.binary)]
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

    def test_startup_game_audio_and_legacy_session(self):
        self.invoke("normal")

    def test_capability_pending_recovery_and_reply_refusal(self):
        self.invoke("refusals")

    def test_physical_key_prepared_after_save_work(self):
        self.invoke("physical")


if __name__ == "__main__":
    unittest.main()
