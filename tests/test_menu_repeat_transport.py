"""Compose the public SMPC collector with actual menu debounce/repeat source."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MenuRepeatTransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_TIMER_DIR', ROOT / 'Firm_Saturn'))
        inputs = {name: (source / name).read_bytes()
                  for name in ('main.c', 'main.h', 'conio.c')}
        main, header, conio = (inputs[name] for name in ('main.c', 'main.h', 'conio.c'))
        if b'const int HZ = 208496;' not in main:
            raise AssertionError('reassess the inherited FRT tick model')
        start = main.index(b'static volatile int pad_state;')
        end = main.index(b'\n}', main.index(b'void pad_init(', start)) + 3
        helper = header.index(b'static inline int timer_reached')
        helper_end = header.index(b'\n}', helper) + 2
        macros = b'\n'.join(line for line in header.splitlines()
                            if line.startswith(b'#define MS2TICK'))
        prefix = (b'#define PAD_TEST_LEGACY 0\n#define EXPECTED_PAD_TIMEOUT 20849\n'
                  b'#define EXPECTED_PAD_ADMISSION 90\n#define HZ 208496\n')
        build = ROOT / 'tests/build/menu-repeat-transport'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        (cls.target / 'pad.inc').write_bytes(prefix + macros + b'\n' +
                                           header[helper:helper_end] + b'\n' + main[start:end])
        start = conio.index(b'#define PAD_REPEAT_TIME')
        end = conio.index(b'/******************************************************************************/', start)
        (cls.target / 'repeat.inc').write_bytes(conio[start:end])
        for name in ('menu_repeat_transport.c', 'test_menu_repeat_transport.py',
                     'test_menu_peripherals.c'):
            inputs[name] = (ROOT / 'tests' / name).read_bytes()
        for name, data in inputs.items():
            (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'menu-repeat-transport'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-fno-strict-aliasing', *flags, '-I', str(cls.target),
                   str(cls.target / 'menu_repeat_transport.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def test_controller_packets_debounce_repeat_and_release_across_wrap(self):
        for origin in range(5):
            for late in range(2):
                with self.subTest(origin=origin, late=late):
                    result = subprocess.run([str(self.binary), str(origin), str(late)],
                        capture_output=True, text=True, timeout=10,
                        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
                    self.results.append(dict(origin=origin, late=late,
                        returncode=result.returncode, stdout=result.stdout, stderr=result.stderr))
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('transport-repeat: pass', result.stdout)


if __name__ == '__main__':
    unittest.main()
