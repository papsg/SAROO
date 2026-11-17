"""Exercise the actual staged public transport wrappers under a host TAS model."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/build/command-transport-binding'


class CommandTransportBindingTests(unittest.TestCase):
    def test_public_entry_points_hold_one_lease_and_retain_faults(self):
        source = (ROOT / 'Firm_Saturn/main.c').read_bytes()
        start = source.index(b'/* Public transport entry points')
        end = source.index(b'#undef OWNED_TRANSPORT', start)
        code = b'#ifdef SAROO_SAVE_GENERATION\n' + source[start:end]
        compare = (ROOT / 'Firm_Saturn/string.c').read_bytes()
        compare = compare[compare.index(b'#ifdef SAROO_SAVE_GENERATION'):
                          compare.index(b'#else', compare.index(b'#ifdef SAROO_SAVE_GENERATION'))]
        compare += b'#endif\n'
        self.assertIn(b'int archive_command(', code)
        self.assertIn(b'int archive_find_slot(', code)
        self.assertIn(b'int read_file_bounded(', code)
        self.assertIn(b'int write_file(', code)
        BUILD.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='case-', dir=BUILD) as folder:
            work = Path(folder)
            (work / 'transport-public.inc').write_bytes(code)
            (work / 'transport-compare.inc').write_bytes(compare)
            profile = os.environ.get('SAROO_COMMAND_TRANSPORT_PROFILE', 'sanitizer')
            if profile not in ('sanitizer', 'release'):
                raise ValueError('unknown profile')
            flags = (['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
                     if profile == 'sanitizer' else ['-O2'])
            binary = work / 'test'
            command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g',
                       '-Wall', '-Wextra', '-Werror', '-fno-builtin', *flags,
                       '-DSAROO_SAVE_GENERATION',
                       '-I', str(work), '-I', str(ROOT / 'Firm_Saturn'),
                       str(ROOT / 'tests/command_transport_binding.c'), '-o', str(binary)]
            result = subprocess.run(command, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10,
                                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
                                         'ASAN_OPTIONS': 'detect_leaks=0'})
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PASS public command ownership', result.stdout)


if __name__ == '__main__':
    unittest.main()
