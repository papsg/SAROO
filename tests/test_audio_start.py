"""Actual I2S startup against an independent retained-status register model."""
# Adapted from paps-g e019e57c; no private preparation or device access.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AudioStartTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_AUDIO_SOURCE', ROOT / 'Firm_MCU/Saturn/cdda_i2s.c'))
        raw = source.read_bytes()
        end = raw.index(b'\n}', raw.index(b'int fill_audio_buffer(')) + 2
        code = raw[raw.index(b'#define ABUF_NUM'):end]
        for signature in (b'void spi2_set_notify(', b'void spi2_trans('):
            start = code.index(signature)
            end = code.index(b'\n}', start) + 2
            code = code[:start] + code[end:]
        # Widen only the five DMA MMIO address casts and the fixed ring address.
        assert code.count(b'(u32)') == 5
        assert code.count(b'(u8*)(0x30002000+i*2352)') == 1
        code = code.replace(b'(u32)', b'(uintptr_t)')
        code = code.replace(b'(u8*)(0x30002000+i*2352)', b'(u8*)(uintptr_t)(0x30002000+i*2352)')
        build = ROOT / 'tests/build/audio-start'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'audio.inc').write_bytes(code)
        inputs = {'cdda_i2s.c': raw, 'audio_start.cpp': (ROOT / 'tests/audio_start.cpp').read_bytes(),
                  'test_audio_start.py': Path(__file__).read_bytes()}
        for name, data in inputs.items():
            (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_AUDIO_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'audio-start'
        args = [*shlex.split(os.environ.get('CXX', 'c++')), '-std=gnu++17', '-g',
                '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags,
                '-I', str(cls.target), str(cls.target / 'audio_start.cpp'), '-o', str(cls.binary)]
        result = subprocess.run(args, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(args, indent=2) + '\n')
        result.check_returncode()
        cls.results = []

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def run_case(self, case):
        result = subprocess.run([str(self.binary), str(case)], capture_output=True,
                                timeout=20, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'case-{case}.log').write_bytes(result.stdout + result.stderr)
        self.results.append({'case': case, 'returncode': result.returncode})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b'pass', result.stdout)

    def test_init_acknowledges_all_16_retained_flag_combinations(self): self.run_case(0)
    def test_dma_start_acknowledges_before_enable(self): self.run_case(1)
    def test_all_32_sectors_and_backpressure(self): self.run_case(2)
    def test_each_restart_acknowledges_retained_flags(self): self.run_case(3)
    def test_baseline_valid_payload_capacity_and_format(self): self.run_case(5)


if __name__ == '__main__':
    unittest.main()
