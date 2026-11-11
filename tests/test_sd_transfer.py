"""Run actual SD transfer/recovery code against modeled MMIO and IRQ events."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SDTransferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        output = ROOT / 'tests/build/sd-transfer'
        output.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=output))
        cls.results = []
        source = Path(os.environ.get('SAROO_SD_SOURCE', ROOT / 'Firm_MCU/Main/sdio_h7.c'))
        raw = source.read_bytes()
        handler = raw.index(b'void SDIO_IRQHandler(')
        identify = raw.index(b'int sdcard_identify(')
        defines = raw[raw.index(b'#define SDCLK_LOW '):raw.index(b'static u32 cmd_resp')]
        # Preserve the MCU's 32-bit DMA addresses when compiling on a wider host.
        transfers = raw[handler:identify].replace(b'(u32)buf', b'(u32)(uintptr_t)buf')
        inputs = {
            'sdio_h7.c': raw,
            'driver.inc': defines + transfers,
            'support.inc': raw[raw.index(b'static void sd_clk_set('):handler],
            'init.inc': raw[raw.index(b'int sdio_init('):],
            'RTX_Config.h': (ROOT / 'Firm_MCU/RTE/CMSIS/RTX_Config.h').read_bytes(),
            'sd_model.h': (ROOT / 'tests/sd_model.h').read_bytes(),
            'sd_transfer.cpp': (ROOT / 'tests/sd_transfer.cpp').read_bytes(),
            'test_sd_transfer.py': Path(__file__).read_bytes(),
        }
        for name, content in inputs.items():
            (cls.target / name).write_bytes(content)
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(content).hexdigest() for name, content in inputs.items()
        }, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_SD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'sd-transfer'
        command = [*shlex.split(os.environ.get('CXX', 'c++')), '-std=gnu++17', '-g',
                   '-Wall', '-Wextra', '-Werror', '-Wno-unused-but-set-variable',
                   '-Wno-unused-function', *flags, str(cls.target / 'sd_transfer.cpp'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode(errors='replace'))

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, scenario):
        result = subprocess.run([str(self.binary), str(scenario)], capture_output=True,
                                text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(scenario=scenario, returncode=result.returncode,
                                 stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_healthy_commands_and_transfers(self):
        for scenario in (0, 14, 18):
            with self.subTest(scenario=scenario):
                self.invoke(scenario)

    def test_transfer_stop_retains_dma_until_command_data_and_busy_finish(self):
        for scenario in (1, 2, 11, 17, 19, 20, 22):
            with self.subTest(scenario=scenario):
                self.invoke(scenario)

    def test_controller_reset_and_invalidation_are_bounded(self):
        for scenario in (3, 4, 5, 12, 13, 21):
            with self.subTest(scenario=scenario):
                self.invoke(scenario)

    def test_late_masked_unrelated_and_busy_error_irqs(self):
        for scenario in (6, 7, 8, 10):
            with self.subTest(scenario=scenario):
                self.invoke(scenario)

    def test_preerase_startup_and_reinsertion_ownership(self):
        for scenario in (9, 15, 16):
            with self.subTest(scenario=scenario):
                self.invoke(scenario)


if __name__ == '__main__':
    unittest.main()
