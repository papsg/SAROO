"""Full public header and independent release-event oracle (SSR-066).

Adapted from paps-g fac81eb1; no private preparer or device operations.
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


class MenuButtonUpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT/'tests/build/menu-button-up'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        header = Path(os.environ.get('SAROO_MENU_BUTTON_HEADER', ROOT/'Firm_Saturn/main.h'))
        inputs = {'main.h': header.read_bytes(),
                  'fixture.c': (ROOT/'tests/menu_button_up.c').read_bytes(),
                  'driver.py': Path(__file__).read_bytes()}
        for name, raw in inputs.items():
            (cls.target/name).write_bytes(raw)
        (cls.target/'inputs.json').write_text(json.dumps({n: hashlib.sha256(v).hexdigest()
            for n, v in inputs.items()}, indent=2) + '\n')

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def helper(self, release):
        name = 'release' if release else 'control'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        binary = self.target/name
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g', '-ffreestanding',
                   '-fno-builtin', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags,
                   '-DTEST_UP='+str(int(release)), '-I', str(self.target),
                   str(self.target/'fixture.c'), '-o', str(binary)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (self.target/(name+'-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        (self.target/(name+'-compile.log')).write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=20,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=name, returncode=result.returncode,
                                 stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('393224 checks, 0 failures' if release else '4 checks, 0 failures', result.stdout)

    def test_existing_press_helper_and_independent_release_control(self): self.helper(False)
    def test_release_fields_right_boundary_and_single_argument_evaluation(self): self.helper(True)


if __name__ == '__main__': unittest.main()
