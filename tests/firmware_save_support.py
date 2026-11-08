"""Compile the actual backup module; MMIO/storage substitutions stay test-only."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/firmware-save"


class FirmwareSaveTest(unittest.TestCase):
    fixture = None

    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.build = tempfile.TemporaryDirectory(prefix=cls.fixture+"-", dir=BUILD)
        cls.addClassCleanup(cls.build.cleanup)
        target = Path(cls.build.name)
        source = Path(os.environ.get("SAROO_GAME_SAVE_SOURCE", ROOT / "Firm_Saturn/game_save.c"))
        shutil.copyfile(source, target / "game_save.c")
        for name in ["main.h", "smpc.h"]:
            shutil.copyfile(ROOT / "tests/firmware-save-stubs" / name, target / name)
        cls.binary = target / "test"
        flags = ["-O2"] if os.environ.get("SAROO_FIRMWARE_SAVE_PROFILE") == "release" else [
            "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-g",
                   "-fno-strict-aliasing", "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
                   *flags, "-I", str(target), str(ROOT / "tests" / (cls.fixture+".c")),
                   "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (BUILD / (cls.fixture+".compile.log")).write_bytes(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError((result.stdout+result.stderr).decode(errors="replace"))

    def invoke(self, *args):
        result = subprocess.run([str(self.binary), *map(str, args)], capture_output=True,
                                text=True, errors="replace", timeout=10,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                     "ASAN_OPTIONS": "detect_leaks=0"})
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertNotIn("runtime error:", result.stderr)
        self.assertNotIn("AddressSanitizer", result.stderr)
        return result
