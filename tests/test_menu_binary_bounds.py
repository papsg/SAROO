"""Actual menu path/index selection; no payload loading or execution."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MenuBinaryBoundsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_BINARY_SOURCE', ROOT / 'Firm_Saturn/main.c'))
        build = ROOT / 'tests/build/menu-binary'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build)); cls.results = []
        data = source.read_bytes()
        begin = data.index(b'int run_binary('); end = data.index(b'static int sel_handle(', begin)
        (cls.target / 'binary.inc').write_bytes(data[begin:end])
        inputs = {'main.c': data, 'menu_binary_bounds.c': (ROOT / 'tests/menu_binary_bounds.c').read_bytes(),
                  'test_menu_binary_bounds.py': Path(__file__).read_bytes()}
        for name, data in inputs.items(): (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2)+'\n')
        cls.binary = cls.target / 'binary'
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-g', *flags,
            '-Wno-int-to-pointer-cast', '-I', str(cls.target), str(cls.target / 'menu_binary_bounds.c'),
            '-o', str(cls.binary)], capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def test_indices_and_names_are_checked_before_io(self):
        for value in range(9):
            with self.subTest(value=value):
                result = subprocess.run([str(self.binary), str(value)], capture_output=True, timeout=5,
                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'})
                self.results.append(dict(value=value, returncode=result.returncode,
                    stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace')))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__': unittest.main()
