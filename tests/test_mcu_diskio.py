"""Actual public diskio.c; checked CMSIS/SD stubs, never device operations."""
# Adapted from paps-g 2ba4e96b; no private preparer or runtime diagnostic.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MCUDiskIOTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/diskio'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build)); cls.results = []
        source = Path(os.environ.get('SAROO_DISKIO_SOURCE', ROOT / 'Firm_MCU/FatFS/diskio.c'))
        inputs = {'diskio.c': source, 'mcu_diskio.c': ROOT / 'tests/mcu_diskio.c',
                  'test_mcu_diskio.py': Path(__file__), 'main.h': ROOT / 'tests/diskio-stubs/main.h'}
        for name in ('ff.h', 'ffconf.h', 'diskio.h'):inputs[name] = ROOT / 'Firm_MCU/FatFS' / name
        for name in ('cmsis_os2.h', 'LICENSE.txt'):inputs[name] = ROOT / 'tests/diskio-stubs/cmsis' / name
        hashes = {}
        for name, path in inputs.items():
            data = path.read_bytes(); (cls.target / name).write_bytes(data)
            hashes[name] = hashlib.sha256(data).hexdigest()
        (cls.target / 'inputs.json').write_text(json.dumps(hashes, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'diskio'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', *flags, '-I', str(cls.target),
                   str(cls.target / 'mcu_diskio.c'), str(cls.target / 'diskio.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr); result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, mode, value=0):
        result = subprocess.run([str(self.binary), mode, str(value)], capture_output=True, timeout=5,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'})
        self.results.append(dict(mode=mode, value=value, returncode=result.returncode,
            stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace')))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn(b'runtime error:', result.stderr)

    def test_initialization_failure_retry_status_and_reuse(self):
        for mode in ('status', 'create-failure', 'repeat'):
            with self.subTest(mode=mode):self.invoke(mode)

    def test_uninitialized_io_never_calls_the_sd_driver(self):
        self.invoke('uninitialized')

    def test_non_success_cmsis_statuses_prevent_io_and_release(self):
        # All six defined osStatus_t error values; not a reachability claim for each.
        for value in (-1, -2, -3, -4, -5, -6):
            with self.subTest(value=value):self.invoke('acquire-error', value)

    def test_transfer_retries_and_exactly_one_owned_release(self):
        for mode in ('read', 'write'):
            for failures in (0, 1, 3, 4):
                with self.subTest(mode=mode, failures=failures):self.invoke(mode, failures)

    def test_reidentification_requires_the_disk_lock_and_returns_the_driver_result(self):
        for value in range(5):
            with self.subTest(value=value): self.invoke('reidentify',value)

    def test_reidentification_cannot_reset_an_in_flight_read_or_write(self):
        for write in (0,1):
            with self.subTest(write=write): self.invoke('reidentify-overlap',write)


if __name__ == '__main__': unittest.main()
