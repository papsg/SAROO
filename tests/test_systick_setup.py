"""Compile the selected Arm OS Tick C with observable host register objects.

Adapted by papsg from paps-g a90f6e3c/SSR-130. Requires an explicitly chosen
version-matched CMSIS RTOS2 SDK; never starts firmware or accesses a device.
"""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SysTickSetupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sdk_name = os.environ.get('SAROO_TICK_SDK')
        if not sdk_name:
            raise unittest.SkipTest('set SAROO_TICK_SDK to the selected CMSIS RTOS2 SDK')
        sdk = Path(sdk_name).resolve(strict=True)
        source = Path(os.environ.get('SAROO_TICK_SOURCE',
            ROOT / 'Firm_MCU/RTE/CMSIS/OS_Tick/os_systick.c')).resolve(strict=True)
        build = ROOT / 'tests/build'
        build.mkdir(parents=True, exist_ok=True)
        name = os.environ.get('SAROO_TICK_OUTPUT')
        cls.target = Path(name).resolve() if name else Path(tempfile.mkdtemp(prefix='systick-', dir=build))
        cls.target.mkdir(parents=True, exist_ok=not name)
        selected = cls.target / 'selected.c'
        selected.write_bytes(source.read_bytes())
        cls.binary = cls.target / 'test-systick'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-O1', '-g',
                   '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                   '-fno-sanitize-recover=all', '-I'+str(ROOT / 'tests/systick-stubs'),
                   '-I'+str(sdk / 'Include'), str(selected),
                   str(ROOT / 'tests/test_os_tick_setup.c'), '-o', str(cls.binary)]
        (cls.target / 'compile-command.json').write_text(json.dumps(command, indent=2)+'\n')
        inputs = [source, sdk / 'Include/os_tick.h', ROOT / 'tests/test_os_tick_setup.c',
                  ROOT / 'tests/systick-stubs/RTE_Components.h',
                  ROOT / 'tests/systick-stubs/systick_device.h']
        (cls.target / 'inputs.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in inputs}, indent=2)+'\n')
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout+result.stderr)
        result.check_returncode()
        cls.results = []

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def case(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True,
                                text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=name, exit=result.returncode,
                                 stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        report = json.loads(result.stdout)
        if name == 'matrix':
            self.assertEqual(report, dict(cases=10208, failed=0, zero_reload_admitted=0))
        return report

    def test_periodic_reload_boundaries(self): self.case('matrix')

    def test_invalid_zero_reload_keeps_pending_tick(self): self.case('pending-zero')

    def test_other_invalid_requests_keep_state(self):
        for name in ('pending-frequency-zero', 'pending-overflow', 'pending-beyond-clock'):
            with self.subTest(name=name): self.case(name)

    def test_valid_reconfiguration_keeps_reset_policy(self): self.case('pending-valid')

    def test_getters_and_acknowledge(self): self.case('getters')


if __name__ == '__main__':
    unittest.main()
