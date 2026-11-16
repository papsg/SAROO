"""Actual R-W publisher/command/FIFO/DataEnd/reset, with explicit MMIO/IRQ seams.

Adapted from paps-g 329466e4 / SSR-064 to the public bounds fixture.
"""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import test_cdc_bounds as bounds

ROOT = Path(__file__).resolve().parents[1]


def function(raw, signature):
    start = raw.index(signature)
    return raw[start:raw.index(b'\n}', start) + 2] + b'\n'


class CDSubrwOverrunTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        command = Path(os.environ.get('SAROO_CDC_SUBRW_COMMAND_SOURCE', ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        main = Path(os.environ.get('SAROO_CDC_SUBRW_SOURCE', ROOT/'Firm_MCU/Saturn/saturn_main.c'))
        with patch.dict(os.environ, {'SAROO_CDC_BOUNDS_SOURCE': str(command)}):
            bounds.CDBoundsTests.setUpClass()
        build = ROOT/'tests/build/cdc-subrw-overrun'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        for source in bounds.CDBoundsTests.target.iterdir():
            if source.suffix in ('.c', '.h', '.inc'):
                shutil.copyfile(source, cls.target/source.name)
        harness = (ROOT/'tests/cdc_bounds.c').read_bytes()
        assert b'#ifndef CDC_BOUNDS_MAIN' in harness
        (cls.target/'bounds-harness.inc').write_bytes(
            b'#define CDC_BOUNDS_MAIN original_bounds_main\n' + harness)
        raw, producer = command.read_bytes(), main.read_bytes()
        (cls.target/'subrw-joined.inc').write_bytes(
            function(producer, b'static void publish_subrw(') + function(raw, b'int get_subcode('))
        (cls.target/'saturn_main.c').write_bytes(producer)
        for name in ('cdc_subrw_overrun.c', 'test_cdc_subrw_overrun.py'):
            shutil.copyfile(ROOT/'tests'/name, cls.target/name)
        (cls.target/'inputs.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in cls.target.iterdir() if p.is_file()}, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_CDC_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target/'overrun'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
                   '-Wno-parentheses', *flags, '-I', str(cls.target), str(cls.target/'cdc_subrw_overrun.c'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target/'command.json').write_text(json.dumps(command, indent=2) + '\n')
        (cls.target/'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        bounds.CDBoundsTests.tearDownClass()
        (cls.target/'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def scenario(self, case):
        result = subprocess.run([str(self.binary), str(case)], capture_output=True, text=True,
            timeout=30, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=case, returncode=result.returncode, stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_overrun_and_loss_free_controls_at_every_cursor(self): self.scenario(0)
    def test_flags_survive_q_refusals_new_loss_and_nonreset_init(self): self.scenario(1)


if __name__ == '__main__': unittest.main()
