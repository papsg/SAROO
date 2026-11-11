"""Exercise the production UART unit against independent DMA and FIFO events."""
# Adapted from paps-g acb824af. The MCU code is compiled from this checkout.
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
        bounded = b"void TIM4_IRQHandler(" in raw
        assert raw.count(b'#include "main.h"') == raw.count(b"(u8*)0x30000000") == 1
        code = raw.replace(b'#include "main.h"', b"").replace(b"(u8*)0x30000000", b"memory.data()")
        code = code.replace(b"_Static_assert", b"static_assert")
        # Only addresses and arguments passed through C varargs need host casts.
        code = code.replace(b"(u32)&", b"(uintptr_t)&").replace(b"(u32)buf", b"(uintptr_t)buf")
        code = code.replace(b"(u32)(tx_buffer + read_pos)", b"(uintptr_t)(tx_buffer + read_pos)")
        code = code.replace(b"DMA1_Stream0->CR, DMA1->LIFCR)",
                            b"(u32)DMA1_Stream0->CR, (u32)DMA1->LIFCR)")

        build = ROOT / "tests/build/uart-tx"
        build.mkdir(parents=True, exist_ok=True)
        cls.output = Path(tempfile.mkdtemp(prefix="case-", dir=build))
        (cls.output / "uart.inc").write_bytes(code)
        profiles = ["-O2"] if os.environ.get("SAROO_UART_PROFILE") == "release" else [
            "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        for name, defines in (("app", []), ("boot", ["-DBOOT"])):
            binary = cls.output / name
            args = [*shlex.split(os.environ.get("CXX", "c++")), "-std=c++17",
                    "-g", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    *profiles, *defines, f"-DUART_BOUNDED_TX={int(bounded)}",
                    "-I", str(cls.output), str(ROOT / "tests/uart_tx.cpp"), "-o", str(binary)]
            result = subprocess.run(args, capture_output=True, timeout=60)
            (cls.output / f"{name}-compile.log").write_bytes(result.stdout + result.stderr)
            if result.returncode:
                raise AssertionError(result.stdout + result.stderr)

    def run_case(self, case, target="app"):
        result = subprocess.run([str(self.output / target), case], capture_output=True, timeout=10,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
        (self.output / f"{case}.log").write_bytes(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b"pass", result.stdout)

    def test_healthy_stream_crosses_ring_end_in_order(self):
        self.run_case("healthy")

    def test_uart_dma_requires_bufferable_mode(self):
        self.run_case("mode")

    def test_uart_tc_is_not_dma_completion(self):
        self.run_case("uart_tc")

    def test_full_and_oversize_messages_do_not_wait(self):
        self.run_case("full")

    def test_dma_completion_during_wrapped_copy(self):
        self.run_case("wrap")

    def test_nested_producer_publishes_in_reservation_order(self):
        self.run_case("nested")

    def test_unpublished_reservation_consumes_capacity(self):
        self.run_case("reserve")

    def test_every_partial_dma_count_preserves_fifo_and_suffix(self):
        self.run_case("partial")

    def test_partial_dma_ending_at_ring_boundary_resumes_exactly(self):
        self.run_case("wrap_residual")

    def test_repeated_transfer_errors_back_off(self):
        self.run_case("fault_backoff")

    def test_refused_enables_use_bounded_retry_and_required_mode(self):
        self.run_case("refused")

    def test_initial_stop_acknowledgment_is_deferred(self):
        self.run_case("initial_stop")

    def test_existing_interrupt_mask_is_restored(self):
        self.run_case("masked")

    def test_direct_mode_fifo_warning_preserves_running_transfer(self):
        self.run_case("feif")

    def test_coalesced_error_and_completion_preserve_order(self):
        self.run_case("coalesced")

    def test_producer_cannot_bypass_pending_retry_irq(self):
        self.run_case("pending_timer")

    def test_late_completion_survives_irq_snapshot(self):
        self.run_case("late_irq")

    def test_stale_timer_irq_does_not_restart_dma(self):
        self.run_case("stale_timer")

    def test_initial_configuration_and_empty_message(self):
        self.run_case("init_and_empty")

    def test_boot_polling_serial_control(self):
        self.run_case("boot", "boot")


if __name__ == "__main__":
    unittest.main()
