"""Common actual-CLI build/invocation support for native save regressions."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/savetool"


class NativeSaveTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.source = Path(os.environ.get("SAROO_SAVETOOL_SOURCE", ROOT / "tools/savetool")).resolve(strict=True)
        selected = os.environ.get("SAROO_SAVETOOL_BINARY")
        if selected:
            cls.binary = Path(selected).resolve(strict=True)
        else:
            cls.build = tempfile.TemporaryDirectory(prefix="compile-", dir=BUILD)
            cls.addClassCleanup(cls.build.cleanup)
            cls.binary = Path(cls.build.name) / "sst"
            command = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-O1", "-g",
                       "-Wall", "-fno-strict-aliasing", "-fsanitize=address,undefined",
                       "-fno-omit-frame-pointer", *map(str, sorted(cls.source.glob("*.c"))),
                       "-o", str(cls.binary)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            (BUILD / (cls.__name__+".compile.log")).write_text(result.stdout+result.stderr)
            if result.returncode:
                raise RuntimeError(result.stdout+result.stderr)

    def case(self):
        return tempfile.TemporaryDirectory(prefix="case-", dir=BUILD)

    def invoke(self, directory, *arguments, ok=True):
        result = subprocess.run([str(self.binary), *map(str, arguments)], cwd=directory,
                                capture_output=True, text=True, errors="replace", timeout=30,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                     "ASAN_OPTIONS": "detect_leaks=0"})
        self.assertEqual(result.returncode == 0, ok, result.stdout+result.stderr)
        self.assertNotIn("runtime error:", result.stderr)
        self.assertNotIn("AddressSanitizer", result.stderr)
        return result
