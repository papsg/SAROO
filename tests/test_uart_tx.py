"""Compile and exercise the actual MCU UART transmit setup."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class UARTTransmitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get("SAROO_UART_SOURCE", ROOT / "Firm_MCU/Main/uart4.c"))
        raw = source.read_bytes()
        start = raw.index(b"static void uart4_dma_send(")
        end = raw.index(b"\n}", start) + 2
        # The MCU writes 32-bit addresses. Use uintptr_t for the host cast.
        code = raw[start:end].replace(b"(u32)&", b"(u32)(uintptr_t)&")
        code = code.replace(b"(u32)buf", b"(u32)(uintptr_t)buf")
        code = code.replace(b"DMA1_Stream0->CR, DMA1->LIFCR)",
                            b"(u32)DMA1_Stream0->CR, DMA1->LIFCR)")

        build = ROOT / "tests/build/uart-tx"
        build.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix="case-", dir=build))
        (output / "uart-dma.inc").write_bytes(code)
        cls.binary = output / "uart-tx"
        args = [*shlex.split(os.environ.get("CXX", "c++")), "-std=c++17",
                "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-I", str(output), str(ROOT / "tests/uart_tx.cpp"), "-o", str(cls.binary)]
        result = subprocess.run(args, capture_output=True, timeout=60)
        (output / "compile.log").write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True, timeout=5,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_uart_dma_requires_bufferable_mode(self):
        self.run_case("start")

    def test_uart_dma_retry_retains_bufferable_mode(self):
        self.run_case("retry")


if __name__ == "__main__":
    unittest.main()
