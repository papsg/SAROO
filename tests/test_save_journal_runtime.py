"""Exercise the staged runtime with the public allocator and storage model."""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "tests/build/save-journal-runtime"


class SaveJournalRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix="case-", dir=BUILD)
        cls.addClassCleanup(cls.work.cleanup)
        target = Path(cls.work.name)
        source = (ROOT / "Firm_Saturn/game_save.c").read_bytes()

        def change(old, new):
            nonlocal source
            if source.count(old) != 1:
                raise AssertionError(f"allocator integration anchor changed: {old!r}")
            source = source.replace(old, new)

        change(b"static int bup_dev;", b"")
        change(b"static int mblk_start;", b"")
        change(b"static int mblk_end;", b"")
        change(b"static int mblk_bmp[2];", b"")
        change(b"static int msav_blk;", b"")
        change(b"static PendingSave pending_save[2];", b'#include "save_runtime.h"')
        change(b"static u8 resize_bitmap[1008];", b"")
        change(b"static u16 resize_blocks[510];", b"")
        change(b"static int checked_free, checked_saves;", b"")
        change(b"static int commit_save(int dev, int flags)\n{",
               b"static int commit_save(int dev, int flags)\n{\n"
               b"    if (journal_state != JOURNAL_LEGACY) return journal_commit(dev);")
        change(b"static int check_backup(int dev)\n{",
               b"static int check_backup(int dev)\n{\n"
               b"    if (journal_admit(dev)) return BUP_BROKEN;")
        change(b"int sro_bup_format(int dev)\n{",
               b"int sro_bup_format(int dev)\n{\n"
               b"    if (dev >= 0 && dev <= 1 && journal_admit(dev)) return BUP_BROKEN;")
        change(b"int sro_bup_write(int dev, BUPDIR *dir, u8 *data, int mode)\n{",
               b"int sro_bup_write(int dev, BUPDIR *dir, u8 *data, int mode)\n{\n"
               b"    if (dev >= 0 && dev <= 1 && journal_admit(dev)) return BUP_BROKEN;")
        change(b"static int same_pending_write(int dev, BUPDIR *dir, int mode)",
               b'#include "save_journal_plan.c"\n#include "save_runtime.c"\n'
               b'#include "save_launch.c"\n'
               b"static int same_pending_write(int dev, BUPDIR *dir, int mode)")
        change(b"    int result = access_data(header, data, 2);",
               b"    if (dev && journal_state != JOURNAL_LEGACY)\n"
               b"        return journal_mems_write(dir, data, bp);\n"
               b"    int result = access_data(header, data, 2);")
        target_source = BUILD / "target-source"
        target_source.mkdir(exist_ok=True)
        (target_source / "game_save.c").write_bytes(source)
        for name in ("save_runtime.c", "save_runtime.h", "save_journal_plan.c",
                     "save_journal.h", "save_launch.c"):
            shutil.copyfile(ROOT / "Firm_Saturn" / name, target_source / name)
        change(b"0x06000c20", b"((uintptr_t)test_game_id)")
        (target / "game_save.c").write_bytes(source)
        for name in ("save_runtime.c", "save_runtime.h", "save_journal_plan.c",
                     "save_journal.h", "save_launch.c"):
            shutil.copyfile(ROOT / "Firm_Saturn" / name, target / name)
        shutil.copyfile(ROOT / "Firm_Saturn/crc32.c", target / "crc32.c")
        (target / "crc").mkdir()
        (target / "crc/main.h").write_text("typedef unsigned int u32; typedef unsigned char u8;\n")
        for name in ("main.h", "smpc.h"):
            shutil.copyfile(ROOT / "tests/firmware-save-stubs" / name, target / name)
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
""")
        profile = os.environ.get("SAROO_SAVE_JOURNAL_PROFILE", "sanitizer")
        if profile not in ("sanitizer", "release"):
            raise ValueError("unknown profile")
        flags = (["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                 if profile == "sanitizer" else ["-O2"])
        cls.binary = target / "test"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-g",
                   "-fno-strict-aliasing", "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast",
                   *flags, "-I", str(target), "-I", str(target / "crc"),
                   str(ROOT / "tests/firmware_save_journal_runtime.c"),
                   str(ROOT / "Firm_Saturn/save_journal.c"), str(target / "crc32.c"),
                   "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (BUILD / (f"{profile}.compile.log")).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors="replace"))

    def invoke(self, dev, size, change, failure):
        result = subprocess.run([str(self.binary), str(dev), str(size), change, failure],
                                capture_output=True, text=True, timeout=90,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                     "ASAN_OPTIONS": "detect_leaks=0"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS", result.stdout)

    def test_normal_and_interrupted_transactions(self):
        for dev in (0, 1):
            sizes = (0, 1, 960, 961, 8192, 16384 if not dev else 32768)
            if dev:
                sizes += (65536, 131073, 479 * 1024)
            for change in ("new", "overwrite"):
                for size in sizes:
                    with self.subTest(dev=dev, size=size, change=change):
                        self.invoke(dev, size, change, "none")
            for change in ("delete", "format"):
                with self.subTest(dev=dev, change=change):
                    self.invoke(dev, 8192, change, "none")
            for failure in ("body", "header-reply", "apply", "partial", "corrupt"):
                with self.subTest(dev=dev, failure=failure):
                    self.invoke(dev, 8192, "overwrite", failure)


if __name__ == "__main__":
    unittest.main()
