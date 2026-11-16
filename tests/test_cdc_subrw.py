"""Actual sector reader and worker admission with modeled FIFO preemption."""
import os
import hashlib
import json
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
        build = TESTS / 'build/cdc-subrw'; build.mkdir(parents=True, exist_ok=True)
        out = cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        main = Path(os.environ.get("SAROO_CDC_SUBRW_SOURCE",
                                   MCU / "Saturn/saturn_main.c")).read_bytes()
        header = Path(os.environ.get("SAROO_CDC_SUBRW_HEADER", MCU / "Saturn/cdc.h")).read_bytes()
        (out / "types.h").write_bytes(header[header.index(b"#define REG8("):
                                             header.index(b"extern CDBLOCK cdb;")])

        def function(signature):
            start = main.index(signature)
            return main[start:main.index(b"\n}", start) + 2] + b"\n"

        producer = function(b"int get_sector(")
        if b"void invalidate_sector_cache(" in main:
            producer += function(b"void invalidate_sector_cache(")
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
        if re.search(rb'\bu8\s+subrw_active\b', header): flags.append('-DHAS_SUBRW_SESSION=1')
        for name,data in [('saturn_main.c',main),('cdc.h',header),('cdc_subrw.c',(TESTS/'cdc_subrw.c').read_bytes()),('test_cdc_subrw.py',Path(__file__).read_bytes())]:
            (out/name).write_bytes(data)
        (out/'inputs.json').write_text(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.is_file()},indent=2)+'\n')
        cls.bin = out / "subrw"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11", "-g",
             "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", *flags,
             "-I", out, "-I", MCU / "FatFS", out / "cdc_subrw.c", "-o", cls.bin]
        result=subprocess.run(command,capture_output=True,timeout=60)
        (out/'command.json').write_text(json.dumps(list(map(str,command)),indent=2)+'\n')
        (out/'compile.log').write_bytes(result.stdout+result.stderr); result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self, case):
        result = subprocess.run([self.bin, case], capture_output=True, text=True, timeout=10)
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_retried_sectors_publish_once(self):
        self.run_case("retry")

    def test_metadata_lookup_retains_queued_packets_and_loss(self):
        self.run_case("reset")

    def test_full_ring_preserves_order_across_consumer_preemption(self):
        self.run_case("full")

    def test_final_sector_renotifies_after_early_q(self):
        self.run_case("terminal")

    def test_directory_success_and_discard(self):
        self.run_case("directory")

    def test_normal_admission_and_decode_disabled(self):
        self.run_case("controls")

    def test_decode_eligibility_for_audio_data_file_directory_scan_and_lazy_start(self):
        self.run_case("eligibility")

    def test_new_decode_retires_old_queue_under_both_irq_masks(self):
        self.run_case("session-start")
