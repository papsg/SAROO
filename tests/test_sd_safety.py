"""Compile actual SAROO SD command and initialization paths at a modeled boundary."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SDSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/sd-safety'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        source = Path(os.environ.get('SAROO_SD_SOURCE', ROOT / 'Firm_MCU/Main/sdio_h7.c'))
        data = source.read_bytes()
        definitions = data.index(b'#define SDCLK_LOW ')
        last_define = data.index(b'\n', data.index(b'#define DATA_ERROR '))
        config = Path(os.environ.get('SAROO_SD_RTX_CONFIG', ROOT / 'Firm_MCU/RTE/CMSIS/RTX_Config.h'))

        def function(signature):
            start = data.index(signature)
            end = data.index(b'\n}', start) + 2
            return data[start:end] + b'\n'

        recover = b''
        if b'static int sd_stop(void)' in data:
            recover = b''.join(function(signature) for signature in (
                b'static int sd_stop(', b'static void sd_reset_controller(',
                b'static __attribute__((noinline)) void sd_recover('))
        inputs = {
            'sdio_h7.c': data,
            'command-defines.inc': (b'#define TEST_SD_RECOVERY\n' if recover else b'')
                                   + data[definitions:last_define] + b'\n',
            'command.inc': recover + function(b'static int sd_cmd('),
            'identify.inc': b'\n'.join(function(signature) for signature in (
                b'static void sd_clk_set(', b'static void sd_bus_width(',
                b'static int sd_acmd(', b'int sdcard_identify(')),
            'RTX_Config.h': config.read_bytes(),
            'sd_model.h': (ROOT / 'tests/sd_model.h').read_bytes(),
            'sd_safety.c': (ROOT / 'tests/sd_safety.c').read_bytes(),
            'test_sd_safety.py': Path(__file__).read_bytes(),
        }
        for name, content in inputs.items():
            (cls.target / name).write_bytes(content)
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(content).hexdigest() for name, content in inputs.items()
        }, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_SD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'sd-safety'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags,
                   str(cls.target / 'sd_safety.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode(errors='replace'))

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, mode, shape, value=0):
        result = subprocess.run([str(self.binary), mode, str(shape), str(value)],
                                capture_output=True, text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(mode=mode, shape=shape, value=value,
                                 returncode=result.returncode, stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_only_successful_semaphore_waits_complete_commands(self):
        for shape in range(5):
            for status in range(-6, 0):
                with self.subTest(shape=shape, status=status):
                    self.invoke('wait', shape, status)

    def test_null_semaphore_cannot_publish_stale_response(self):
        self.invoke('null', 1)

    def test_command_response_shapes_and_no_crc_control(self):
        for shape in range(5):
            with self.subTest(shape=shape):
                self.invoke('success', shape)
        self.invoke('no-crc', 4)

    def test_controller_errors_follow_the_existing_cleanup_path(self):
        for shape, bits in ((1, 1), (1, 4), (2, 4), (3, 2), (3, 1 << 27), (4, 4)):
            with self.subTest(shape=shape, bits=bits):
                self.invoke('hardware', shape, bits)

    def test_ready_cards_follow_normal_legacy_and_high_capacity_startup(self):
        for legacy in (0, 1):
            for position in range(7):
                with self.subTest(legacy=legacy, position=position):
                    self.invoke('ready', legacy, position)

    def test_not_ready_retries_are_bounded_including_tick_wrap(self):
        for wrap in (0, 1):
            with self.subTest(wrap=wrap):
                self.invoke('not-ready', wrap)

    def test_ready_at_the_deadline_survives_tick_wrap(self):
        self.invoke('wrap-ready', 1)

    def test_card_ready_budget_accounts_for_command_latency(self):
        for latency in range(4):
            with self.subTest(latency=latency):
                self.invoke('latency', latency)

    def test_failed_prefix_or_request_exits_before_card_selection(self):
        for request in (0, 1):
            for attempt in (1, 2):
                with self.subTest(request=request, attempt=attempt):
                    self.invoke('identify-error', request, attempt)


if __name__ == '__main__':
    unittest.main()
