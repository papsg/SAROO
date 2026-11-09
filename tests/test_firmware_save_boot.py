"""Actual optical/CD-player caller gates, without a complete boot simulation."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from firmware_save_support import ROOT, BUILD


class FirmwareSaveBootTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_SATURN_SOURCE_DIR', ROOT / 'Firm_Saturn'))
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='boot-', dir=BUILD))
        cls.results = []
        inputs = {}
        for name, command, end_marker, fragment in (
            ('game_load.c', b'SS_CMD = SSCMD_LSAVE;', b'\n\t}', 'optical-gate.inc'),
            ('main.c', b'}else if(BUTTON_DOWN(ctrl, PAD_Z))', b'\n\t\t}else{', 'cdplayer-gate.inc'),
        ):
            data = (source / name).read_bytes(); inputs[name] = data
            start = data.index(b'\n', data.index(b'while(SS_CMD);', data.index(command))) + 1
            end = data.index(end_marker, start - 1)
            (cls.target / fragment).write_bytes(data[start:end])
        for name in ('firmware_save_boot.c', 'test_firmware_save_boot.py'):
            inputs[name] = (ROOT / 'tests' / name).read_bytes()
        for name, data in inputs.items():
            (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2)+'\n')
        cls.binary = cls.target / 'boot'
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-g', *flags,
            '-I', str(cls.target), str(cls.target / 'firmware_save_boot.c'), '-o', str(cls.binary)],
            capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def invoke(self, mode):
        for code in (0, 1, 2, 19, 65535):
            with self.subTest(code=code):
                result = subprocess.run([str(self.binary), mode, str(code)], capture_output=True,
                    timeout=5, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
                                   'ASAN_OPTIONS': 'detect_leaks=0'})
                self.results.append(dict(mode=mode, code=code, returncode=result.returncode,
                    stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace')))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_optical_boot_stops_on_save_load_error(self): self.invoke('optical')
    def test_cdplayer_reports_load_error_without_starting(self): self.invoke('cdplayer')


if __name__ == '__main__': unittest.main()
