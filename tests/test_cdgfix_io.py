"""Exercise the public repair's file publication with host-only I/O faults."""
# Adapted from paps-g fee3293f.
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import test_cdgfix as cdg


@unittest.skipIf(os.name == "nt", "POSIX syscall fault fixture")
class CDGFixIOTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix="saroo-cdgfix-io-")
        cls.addClassCleanup(build.cleanup)
        root = Path(build.name)
        cls.binary = root / "cdgfix-io"
        obj = root / "cdgfix.o"
        cls.batch = int(os.environ.get("SAROO_CDG_BATCH_FRAMES", "64"))
        directory = cdg.ROOT / "tests"
        compiler = cdg.compiler()
        for command in ([*compiler, "-include", str(directory / "cdgfix_faults.h"),
                         "-c", str(cdg.SOURCE), "-o", str(obj)],
                        [*compiler, str(obj), str(directory / "cdgfix_faults.c"), "-o", str(cls.binary)]):
            subprocess.run(command, check=True, timeout=45)

    def run_case(self, fault="", first=False, clean=False, after_replace=False, edit=None, succeeds=False):
        with tempfile.TemporaryDirectory(prefix="saroo-cdgfix-case-") as temp:
            root = Path(temp)
            path = root / "synthetic.cdg"
            expected = cdg.packet() * 1024
            broken = bytearray(expected)
            if not clean:
                broken[8 if first else len(broken) - 16] ^= 1
            path.write_bytes(broken)
            stats_path = root / "stats.json"
            run = subprocess.run([str(self.binary), str(path)], capture_output=True, timeout=10,
                                 env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                      "SAROO_CDG_FAULT": fault, "SAROO_CDG_INPUT": str(path),
                                      "SAROO_CDG_STATS": str(stats_path)})
            self.assertNotIn(b"AddressSanitizer", run.stderr)
            self.assertNotIn(b"runtime error:", run.stderr)
            self.assertEqual(run.returncode, 0 if succeeds else 1, run.stdout + run.stderr)
            stats = json.loads(stats_path.read_text())
            self.assertEqual(stats["triggered"], int(bool(fault)), run.stdout + run.stderr)
            wanted = edit(broken) if edit else expected if succeeds or after_replace else broken
            self.assertEqual(path.read_bytes(), wanted)
            self.assertFalse(list(root.glob(".saroo-cdg-*")))
            if after_replace:
                self.assertIn(b"File replaced. Finalization failed.", run.stderr)
            elif not succeeds:
                self.assertIn(b"Input path was not replaced.", run.stderr)
            return stats

    def test_prepublication_failures_preserve_original(self):
        faults = ["open-input", "stat", "open-writable", "close-writable", "alloc", "open-directory",
                  "create", "stat:2", "stat:3", "stat:4", "stream-zero", "stream-short",
                  "stream-zero:4", "prefix-zero", "prefix-error", "write-zero", "write-error",
                  "write-error:2", "close-input", "sync-file", "close-temp", "rename"]
        if sys.platform == "darwin":
            faults.append("metadata")
        for fault in faults:
            with self.subTest(fault=fault):
                self.run_case(fault)

    def test_short_and_interrupted_calls_finish_the_write(self):
        for fault in ("prefix-short", "prefix-intr", "write-short", "write-intr"):
            with self.subTest(fault=fault):
                self.run_case(fault, succeeds=True)

    def test_directory_failures_report_the_completed_replacement(self):
        for fault in ("sync-directory", "close-directory"):
            with self.subTest(fault=fault):
                self.run_case(fault, after_replace=True)

    def test_observed_concurrent_edit_is_left_intact(self):
        for fault, edit in (("append", lambda data: data + b"x"),
                            ("modify", lambda data: b"x" + data[1:]),
                            ("truncate", lambda data: data[:24])):
            with self.subTest(fault=fault):
                self.run_case(fault, first=True, edit=edit)

    def test_clean_and_late_repair_io_counts(self):
        for first, clean in ((True, False), (False, False), (False, True)):
            with self.subTest(first=first, clean=clean):
                stats = self.run_case(first=first, clean=clean, succeeds=True)
                self.assertEqual(stats["read_bytes"], 24576)
                self.assertEqual(stats["write_bytes"], 0 if clean else 24576)
                self.assertEqual(stats["creations"], 0 if clean else 1)
                window = self.batch * 96 + 192
                self.assertEqual(stats["prefix_bytes"], 0 if clean or first else 24575 // window * window)


@unittest.skipUnless(os.name == "nt", "Windows replacement failure fixture")
class CDGFixWindowsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix="saroo-cdgfix-win-")
        cls.addClassCleanup(build.cleanup)
        cls.binary = Path(build.name) / "cdgfix.exe"
        source = cdg.ROOT / "tests/cdgfix_windows_fault.c"
        subprocess.run([*cdg.compiler(), str(source), "-o", str(cls.binary)],
                       check=True, timeout=45)

    def test_partial_publication_restores_or_retains_the_original(self):
        expected = bytearray(cdg.packet())
        expected[8] ^= 1
        for blocked in (False, True):
            with self.subTest(rollback_blocked=blocked), tempfile.TemporaryDirectory() as temp:
                path = Path(temp) / "synthetic.cdg"
                path.write_bytes(expected)
                env = os.environ.copy()
                env.pop("SAROO_CDG_FAIL_ROLLBACK", None)
                if blocked:
                    env["SAROO_CDG_FAIL_ROLLBACK"] = "1"
                run = subprocess.run([str(self.binary), str(path)], env=env, capture_output=True,
                                     stdin=subprocess.DEVNULL, timeout=10)
                self.assertEqual(run.returncode, 1, run.stdout + run.stderr)
                staging = list(path.parent.glob(".saroo-cdg-*"))
                if blocked:
                    self.assertFalse(path.exists())
                    self.assertEqual(len(staging), 1)
                    self.assertEqual((staging[0] / "original").read_bytes(), expected)
                    self.assertIn(b"Recovery copy", run.stderr)
                else:
                    self.assertEqual(path.read_bytes(), expected)
                    self.assertFalse(staging)
                    self.assertIn(b"Original file restored.", run.stderr)


if __name__ == "__main__":
    unittest.main()
