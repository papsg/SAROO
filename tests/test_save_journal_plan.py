"""Compare journal plans with the public firmware's complete saved image."""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/save-journal-plan"


class SaveJournalPlanTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix="case-", dir=BUILD)
        cls.addClassCleanup(cls.work.cleanup)
        target = Path(cls.work.name)
        source = (ROOT / "Firm_Saturn/game_save.c").read_bytes()
        root_hook = b"static int commit_save(int dev, int flags)\n{"
        write_hook = b"    int result = access_data(header, data, 2);"
        if source.count(root_hook) != 1 or source.count(write_hook) != 1:
            raise AssertionError("save observation boundaries changed")
        source = source.replace(root_hook,
            b"static void probe_plan_write(int, BUPDIR *, u8 *, u8 *);\n"
            b"static void probe_plan_root(int);\n" + root_hook + b"\n    probe_plan_root(dev);")
        source = source.replace(write_hook,
            b"    probe_plan_write(dev, dir, data, bp);\n" + write_hook)
        (target / "game_save.c").write_bytes(source)
        shutil.copyfile(ROOT / "Firm_Saturn/save_journal_plan.c", target / "save_journal_plan.c")
        shutil.copyfile(ROOT / "Firm_Saturn/save_journal.h", target / "save_journal.h")
        shutil.copyfile(ROOT / "Firm_Saturn/crc32.c", target / "crc32.c")
        (target / "crc/main.h").parent.mkdir()
        (target / "crc/main.h").write_text("typedef unsigned int u32; typedef unsigned char u8;\n")
        for name in ("main.h", "smpc.h"):
            shutil.copyfile(ROOT / "tests/firmware-save-stubs" / name, target / name)
        profile = os.environ.get("SAROO_SAVE_JOURNAL_PROFILE", "sanitizer")
        if profile not in ("sanitizer", "release"):
            raise ValueError("unknown profile")
        flags = (["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                 if profile == "sanitizer" else ["-O2"])
        cls.binary = target / "test"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-g",
                   "-fno-strict-aliasing", "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
                   *flags, "-I", str(target), "-I", str(target / "crc"),
                   str(ROOT / "tests/firmware_save_journal_plan.c"),
                   str(ROOT / "Firm_Saturn/save_journal.c"), str(target / "crc32.c"),
                   "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (BUILD / (f"{profile}.compile.log")).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))

    def test_write_and_root_plans_match_full_persisted_archives(self):
        cases = 0
        for dev in (0, 1):
            sizes = (0, 1, 127, 128, 129, 960, 961, 8192,
                     16384 if not dev else 32768)
            if dev:
                sizes += (65536, 131073, 479 * 1024)
            for change in ("new", "overwrite"):
                for size in sizes:
                    with self.subTest(dev=dev, size=size, change=change):
                        self.invoke(dev, size, change)
                        cases += 1
            with self.subTest(dev=dev, change="delete"):
                self.invoke(dev, 8192, "delete")
                cases += 1
        self.assertEqual(cases, 44)

    def invoke(self, *args):
        result = subprocess.run([str(self.binary), *map(str, args)], capture_output=True,
                                text=True, timeout=60,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                     "ASAN_OPTIONS": "detect_leaks=0"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
