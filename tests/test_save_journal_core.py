"""Checks for the dormant save journal record, publisher, and replayer."""

import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/save-journal"


class SaveJournalCoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.work = Path(tempfile.mkdtemp(prefix="case-", dir=BUILD))
        shutil.copyfile(ROOT / "Firm_Saturn/crc32.c", cls.work / "crc32.c")
        (cls.work / "main.h").write_text(
            "typedef unsigned int u32; typedef unsigned char u8;\n"
        )
        profile = os.environ.get("SAROO_SAVE_JOURNAL_PROFILE", "sanitizer")
        if profile not in ("sanitizer", "release"):
            raise ValueError("unknown save journal profile")
        flags = (["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                 if profile == "sanitizer" else ["-O2"])
        cls.binaries = {}
        for name in ("journal", "stream", "replay"):
            source = "test_save_journal" + ("" if name == "journal" else f"_{name}") + ".c"
            binary = cls.work / name
            command = [*shlex.split(os.environ.get("CC", "cc")), "-std=c11", "-g",
                       "-Wall", "-Wextra", "-Werror", *flags,
                       "-I", str(ROOT / "Firm_Saturn"),
                       str(ROOT / "Firm_Saturn/save_journal.c"),
                       str(cls.work / "crc32.c"), str(ROOT / "tests" / source),
                       "-o", str(binary)]
            result = subprocess.run(command, capture_output=True, timeout=60)
            (cls.work / f"{name}-compile.log").write_bytes(result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError(result.stderr.decode(errors="replace"))
            cls.binaries[name] = binary

    def run_case(self, name, *args):
        result = subprocess.run([str(self.binaries[name]), *map(str, args)],
                                capture_output=True, timeout=90,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
        (self.work / f"{name}-{'-'.join(map(str, args)) or 'check'}.log").write_bytes(
            result.stdout + result.stderr
        )
        self.assertEqual(result.returncode, 0, result.stdout[:2000] + result.stderr)
        return result.stdout

    def test_record_bounds_corruption_and_publication(self):
        self.assertIn(b"PASS", self.run_case("journal", "check"))

    def test_wire_format_with_independent_crc(self):
        for mode, expected_count in enumerate((1, 1, 2, 481)):
            with self.subTest(mode=mode):
                data = self.run_case("journal", "emit", mode)
                self.assertEqual(data[:4], b"SJR1")
                length, count, device = struct.unpack_from(">III", data, 4)
                self.assertEqual((length, count, device),
                                 (len(data), expected_count, int(mode != 0)))
                self.assertEqual(data[16:32], b"TESTGAME00000001")
                self.assertEqual(struct.unpack_from(">II", data, 32),
                                 (zlib.crc32(data[512:]), zlib.crc32(data[:36])))

    def test_bounded_stream_and_replay(self):
        self.assertIn(b"PASS", self.run_case("stream"))
        self.assertIn(b"PASS", self.run_case("replay"))


if __name__ == "__main__":
    unittest.main()
