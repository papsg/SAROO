"""Actual sector reader and worker admission with modeled FIFO preemption."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

TESTS = Path(__file__).resolve().parent
MCU = TESTS.parent / "Firm_MCU"


class CDSubrwTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(tmp.cleanup)
        out = Path(tmp.name)
        main = Path(os.environ.get("SAROO_CDC_SUBRW_SOURCE",
                                   MCU / "Saturn/saturn_main.c")).read_bytes()
        header = (MCU / "Saturn/cdc.h").read_bytes()
        (out / "types.h").write_bytes(header[header.index(b"#define REG8("):
                                             header.index(b"extern CDBLOCK cdb;")])

        def function(signature):
            start = main.index(signature)
            return main[start:main.index(b"\n}", start) + 2] + b"\n"

        producer = function(b"int get_sector(")
        if b"static void publish_subrw(" in main:
            producer += function(b"static void publish_subrw(")
        # Evaluate each selected load once, then let the command task run
        # immediately or at restore_irq, depending on the simulated mask.
        producer, sites = re.subn(
            rb"(cdb\.subrw_wp = |(?:new_wp|next)\s*==\s*)cdb\.subrw_rp\b",
            rb"\1read_cursor(cdb.subrw_rp)", producer,
        )
        assert sites == 2
        start = main.index(b"retv = get_sector(cdb.fad, &wblk);")
        end = main.index(b"\n\t\t}\n\n_play_end:", start)
        producer += (b"static int worker_step(void) { BLOCK wblk; int retv;\n"
                     + main[start:end] + b"\nreturn 1;\n"
                     + b"_restart_nowait: return 2;\n_restart_wait: return 0;\n"
                     + b"_delay_error: abort();\n_read_error: return -1;\n}\n")
        (out / "producer.inc").write_bytes(re.sub(
            rb"\bHIRQ = ([^;]+);", rb"signal_irq(\1);", producer))
        flags = (["-O2"] if os.environ.get("SAROO_CDC_PROFILE") == "release"
                 else ["-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])
        cls.bin = out / "subrw"
        subprocess.run(
            [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-g",
             "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", *flags,
             "-I", out, "-I", MCU / "FatFS", TESTS / "cdc_subrw.c", "-o", cls.bin],
            check=True, timeout=60,
        )

    def run_case(self, case):
        result = subprocess.run([self.bin, case], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_retried_sectors_publish_once(self):
        self.run_case("retry")

    def test_track_reset_cannot_reveal_old_packs(self):
        self.run_case("reset")

    def test_full_ring_preserves_order_across_consumer_preemption(self):
        self.run_case("full")

    def test_final_sector_renotifies_after_early_q(self):
        self.run_case("terminal")

    def test_directory_success_and_discard(self):
        self.run_case("directory")

    def test_normal_admission_and_decode_disabled(self):
        self.run_case("controls")
