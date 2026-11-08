"""Execute the production controller subsystem with bounded register fixtures."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


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
        header = (ROOT / "Firm_Saturn/main.h").read_bytes()
        helper = re.search(rb"static inline int timer_reached[^\n]*\n\{.*?\n\}", header, re.S)
        macros = b"\n".join(line for line in header.splitlines() if line.startswith(b"#define MS2TICK"))
        prefix = (b"#define PAD_TEST_LEGACY " + str(int(cls.legacy)).encode() +
                  b"\n#define EXPECTED_PAD_TIMEOUT 20849\n#define EXPECTED_PAD_ADMISSION 90\n#define HZ 208496\n")
        code = prefix+macros+b"\n"+(helper[0] if helper else b"")+b"\n"+body
        (directory / "pad.inc").write_bytes(code)
        compiler = os.environ.get("CC", "cc")
        if not re.search(rb"const int HZ = 208496;", main):
            raise AssertionError("reassess the fixture's inherited FRT clock model")
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
        declaration = re.search(rb"static (?:u32|int) timer_base;", main)
        clock = re.search(rb"u32 get_timer\(void\)\n\{.*?\n\}", main[declaration.start():], re.S)
        timer = (b"#include <assert.h>\n#include <stdint.h>\n#include <stdio.h>\n"
                 b"typedef uint32_t u32; static volatile unsigned char FTCSR,FRCH,FRCL;\n"+
                 declaration[0]+b"\n"+clock[0]+b"\nint main(void) {\n"
                 b"timer_base=0x7fff0000u; FTCSR=2; FRCH=0x12; FRCL=0x34;\n"
                 b"assert(get_timer()==0x80001234u && FTCSR==0);\n"
                 b"timer_base=0xffff0000u; FTCSR=2;\n"
                 b"assert(get_timer()==0x1234u && FTCSR==0);\n"
                 b"puts(\"counter: pass\"); return 0; }\n")
        (directory / "timer.c").write_bytes(timer)
        cls.counter = directory / "counter"
        subprocess.run([compiler, *flags, "-Werror", str(directory / "timer.c"),
                        "-o", str(cls.counter)], check=True, timeout=30)

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

    def test_actual_counter_signed_boundary_and_wrap(self):
        result = subprocess.run([str(self.counter)], capture_output=True, text=True,
                                env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=5)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertIn("counter: pass", result.stdout)

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
