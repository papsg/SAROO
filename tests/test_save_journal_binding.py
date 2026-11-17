"""Run the actual standard game_save.c binding against a host storage model."""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/save-journal-binding"


class SaveJournalBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix="case-", dir=BUILD)
        cls.addClassCleanup(cls.work.cleanup)
        target = Path(cls.work.name)
        source = (ROOT / "Firm_Saturn/game_save.c").read_bytes()
        # This address is an SH-2 hardware register in the production source.
        # The host fixture maps only that hardware read to its test game ID.
        assert source.count(b"0x06000c20") == 1
        source = source.replace(b"0x06000c20", b"((uintptr_t)test_game_id)")
        (target / "game_save.c").write_bytes(source)
        for name in ("save_runtime.c", "save_runtime.h", "save_journal_plan.c",
                     "save_journal.h", "save_launch.c", "crc32.c"):
            shutil.copyfile(ROOT / "Firm_Saturn" / name, target / name)
        (target / "crc").mkdir()
        (target / "crc/main.h").write_text(
            "typedef unsigned int u32; typedef unsigned char u8;\n")
        for name in ("main.h", "smpc.h"):
            shutil.copyfile(ROOT / "tests/firmware-save-stubs" / name,
                            target / name)
        stub = (target / "main.h").read_text()
        assert stub.count("#define printk(...) ((void)0)") == 1
        (target / "main.h").write_text(stub.replace(
            "#define printk(...) ((void)0)", "int printk(char *, ...);"))
        with (target / "main.h").open("a") as header:
            header.write("""
extern const u8 test_game_id[16];
extern u8 test_work[];
extern int use_sys_bup;
#define TMPBUFF_ADDR ((uintptr_t)test_work)
#define JOURNAL_INPUT_ADDRESS(pointer) ((uintptr_t)(pointer))
int read_file_bounded(char *, int, int, void *, u32);
int write_file(char *, int, int, void *);
int archive_command(unsigned, u32, void *, int);
int archive_find_slot(const u8 *, void *, u32);
int read_file_bounded_owned(char *, int, int, void *, u32);
int write_file_owned(char *, int, int, void *);
int archive_command_owned(unsigned, u32, void *, int);
int archive_find_slot_owned(const u8 *, void *, u32);
int command_printk(char *, ...);
int command_memcmp(void *, void *, int);
""")
        profile = os.environ.get("SAROO_SAVE_JOURNAL_PROFILE", "sanitizer")
        if profile not in ("sanitizer", "release"):
            raise ValueError("unknown profile")
        flags = (["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                 if profile == "sanitizer" else ["-O2"])
        cls.native = os.environ.get("SAROO_NATIVE_INTERNAL_BACKUP_TEST", "1") == "1"
        cls.binary = target / "test"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-g",
                   "-fno-strict-aliasing", "-Wno-int-to-pointer-cast",
                   "-Wno-pointer-to-int-cast", "-DSAROO_SAVE_GENERATION",
                   *(["-DSAROO_NATIVE_INTERNAL_BACKUP"] if cls.native else []),
                   *flags, "-I", str(target), "-I", str(target / "crc"),
                   "-I", str(ROOT / "Firm_Saturn"),
                   str(ROOT / "tests/firmware_save_journal_runtime.c"),
                   str(ROOT / "Firm_Saturn/save_journal.c"),
                   str(target / "crc32.c"), "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (BUILD / f"{profile}.compile.log").write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))

    def test_public_backup_entries_and_journal_recovery(self):
        for dev in (0, 1):
            for change in ("new", "overwrite", "delete", "format"):
                for failure in ("none", "body", "header-reply", "apply",
                                "partial", "corrupt"):
                    if change in ("delete", "format") and failure != "none":
                        continue
                    with self.subTest(dev=dev, change=change, failure=failure):
                        result = subprocess.run(
                            [str(self.binary), str(dev), "8192", change, failure],
                            capture_output=True, text=True, timeout=90,
                            env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                 "ASAN_OPTIONS": "detect_leaks=0"})
                        self.assertEqual(result.returncode, 0,
                                         result.stdout + result.stderr)
                        self.assertIn("PASS", result.stdout)
            sizes = ((0, 960, 961, 32768, 479 * 1024) if dev else
                     (0, 1, 28, 29, 86, 87, 960, 961,
                      16384, 29434 if self.native else 32768))
            for size in sizes:
                with self.subTest(dev=dev, size=size):
                    result = subprocess.run(
                        [str(self.binary), str(dev), str(size), "new", "none"],
                        capture_output=True, text=True, timeout=90,
                        env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                             "ASAN_OPTIONS": "detect_leaks=0"})
                    self.assertEqual(result.returncode, 0,
                                     result.stdout + result.stderr)
                    self.assertIn("PASS", result.stdout)

    def test_native_growth_refuses_overcapacity_without_archive_or_journal_writes(self):
        if not self.native:
            self.skipTest("Native device-0 profile only")
        for size in (29435, 29551, 65024):
            with self.subTest(size=size):
                result = subprocess.run(
                    [str(self.binary), "0", str(size), "refuse", "none"],
                    capture_output=True, text=True, timeout=90,
                    env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                         "ASAN_OPTIONS": "detect_leaks=0"})
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("PASS native refusal", result.stdout)


if __name__ == "__main__":
    unittest.main()
