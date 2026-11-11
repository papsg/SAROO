"""Execute the production controller subsystem with bounded register fixtures."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def pad_timing(main):
    if b"const int HZ = 1000000;" in main and b"SS_TIMER_LO" in main:
        rate, admission, timeout = 1000000, 432, 100000
    elif b"const int HZ = 208496;" in main:
        rate, admission, timeout = 208496, 90, 20849
    else:
        raise AssertionError("Unknown menu clock")
    return (f"#define HZ {rate}\n#define EXPECTED_PAD_ADMISSION {admission}\n"
            f"#define EXPECTED_PAD_TIMEOUT {timeout}\n").encode()


class MenuPeripheralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-peripherals-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        override = os.environ.get("SAROO_TEST_MENU_PERIPHERALS")
        main = Path(override).read_bytes() if override else (ROOT / "Firm_Saturn/main.c").read_bytes()
        marker = b"static volatile int pad_state;" if b"static volatile int pad_state;" in main else b"static int pad_state;"
        start = main.index(marker)
        end = main.index(b"\n}", main.index(b"void pad_init(", start))+3
        body = main[start:end]
        cls.legacy = b"(u8*)0x20100021" in body
        body = body.replace(b"(u8*)0x20100021", b"(u8*)test_oreg")
        paired_header = Path(override).with_name("main.h") if override else None
        header = (paired_header if paired_header and paired_header.is_file()
                  else ROOT / "Firm_Saturn/main.h").read_bytes()
        helper = re.search(rb"static inline int timer_reached[^\n]*\n\{.*?\n\}", header, re.S)
        macros = b"\n".join(line for line in header.splitlines() if line.startswith(b"#define MS2TICK"))
        prefix = f"#define PAD_TEST_LEGACY {int(cls.legacy)}\n".encode() + pad_timing(main)
        code = prefix + macros + b"\n" + (helper[0] if helper else b"") + b"\n" + body
        (directory / "pad.inc").write_bytes(code)
        compiler = os.environ.get("CC", "cc")
        flags = ["-std=c99", "-O2", "-g", "-Wall", "-Wextra", "-fno-strict-aliasing",
                 "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(directory)]
        cls.binary = directory / "peripherals"
        subprocess.run([compiler, *flags, str(ROOT / "tests/test_menu_peripherals.c"),
                        "-o", str(cls.binary)], check=True, timeout=30)
        if not cls.legacy:
            independent, count = re.subn(rb"\bIREG0 = ([^;\n]+);", rb"pad_test_ireg0_write(\1);", code)
            if count != 5:
                raise AssertionError("changed IREG0 write seams")
            (directory / "pad-writeonly.inc").write_bytes(independent)
            cls.writeonly = directory / "peripherals-writeonly"
            subprocess.run([compiler, *flags, "-Werror", str(ROOT / "tests/test_menu_peripherals_writeonly.c"),
                            "-o", str(cls.writeonly)], check=True, timeout=30)

    def invoke(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True, text=True,
                                env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertIn(case+": pass", result.stdout)
        print(result.stdout.strip())

    def test_direct_ports_and_lengths(self): self.invoke("direct")
    def test_mouse_with_pad_in_either_port(self): self.invoke("mouse")
    def test_all_two_multitap_socket_occupancies(self): self.invoke("taps")
    def test_mixed_extended_and_page_boundaries(self): self.invoke("lengths")
    def test_invalid_and_truncated_records(self): self.invoke("invalid")
    def test_busy_smpc_yields(self): self.invoke("busy")
    def test_interrupt_readiness_timeout_and_recovery(self): self.invoke("transport")
    def test_vblank_admission_and_stale_field(self): self.invoke("timing")
    def test_interrupt_lifecycle(self): self.invoke("lifecycle")
    def test_existing_export_after_binary_handoff(self): self.invoke("guest")

    def test_write_only_cont_break_and_handoffs(self):
        if self.legacy:
            self.skipTest("legacy transport has no continued interrupt collector")
        for case in ("complete", "invalid", "timeout", "handoff"):
            with self.subTest(case=case):
                result = subprocess.run([str(self.writeonly), case], capture_output=True, text=True,
                                        env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=15)
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                self.assertIn("writeonly: pass", result.stdout)
                print(result.stdout.strip())


if __name__ == "__main__":
    unittest.main()
