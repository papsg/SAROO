"""Actual retained floating formatter, public files and contained finite probes."""
# Adapted from paps-g 1326a62c/31151513 (SSR-116); no diagnostic provider.
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FatFsFloatTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/fatfs-float'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.variants = []; cls.results = []; pins = {}
        provider = Path(os.environ.get('SAROO_FATFS_DIR', ROOT / 'Firm_MCU/FatFS'))
        source = {n: (provider / n).read_bytes() for n in ('ff.c', 'ff.h', 'ffconf.h', 'ffunicode.c', 'diskio.h')}
        disk = (ROOT / 'tests/fatfs_seek.c').read_bytes()
        disk = disk[:disk.index(b'static int selected(')]
        fixture = (ROOT / 'tests/fatfs_float.c').read_bytes()
        profiles = [(p, floating, strings) for p in ('fat32', 'generic')
                    for floating in (1, 2) for strings in (1, 2)]
        requested = json.loads(os.environ.get('SAROO_FLOAT_VARIANTS', 'null'))
        names = {f'{p}-float{f}-str{s}' for p, f, s in profiles}
        if requested is not None:
            assert isinstance(requested, list) and requested and len(set(requested)) == len(requested)
            assert set(requested) <= names
        for p, floating, strings in profiles:
            name = f'{p}-float{floating}-str{strings}'
            if requested is not None and name not in requested: continue
            folder = cls.target / name; folder.mkdir()
            config = source['ffconf.h'].decode()
            for key, value in dict(FF_USE_MKFS=1, FF_USE_STRFUNC=strings, FF_PRINT_FLOAT=floating).items():
                config, count = re.subn(r'(?m)^(#define\s+' + key + r'\s+)\S+',
                                       lambda m: m[1] + str(value), config)
                assert count == 1, key
            staged = {**source, 'ffconf.h': config.encode(), 'disk.inc': disk, 'fixture.c': fixture,
                      'driver.py': Path(__file__).read_bytes()}
            for filename, data in staged.items(): (folder / filename).write_bytes(data)
            pins[name] = {n: hashlib.sha256(v).hexdigest() for n, v in staged.items()}
            flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
                '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
            if p == 'generic': flags += ['-DSAROO_FAT32_ONLY=0']
            command = [os.environ.get('CC', 'cc'), '-std=gnu11', '-g', '-Wall', '-Wextra', '-Werror',
                       '-Wno-unused-function', *flags, '-pthread', '-I', str(folder),
                       str(folder / 'fixture.c'), str(folder / 'ffunicode.c'), '-lm', '-o', str(folder / 'check')]
            (folder / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
            r = subprocess.run(command, capture_output=True, timeout=60)
            (folder / 'compile.log').write_bytes(r.stdout + r.stderr); r.check_returncode()
            cls.variants.append((name, folder, p == 'generic'))
        (cls.target / 'inputs.json').write_text(json.dumps(pins, indent=2) + '\n')

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def case(self, mode):
        for name, folder, _ in self.variants:
            with self.subTest(variant=name, mode=mode):
                image = folder / (mode + '.img')
                if mode not in ('direct', 'extreme'):
                    with image.open('wb') as stream: stream.truncate(64 * 1024**2)
                command = [str(folder / 'check'), str(image), mode]
                started = time.monotonic()
                child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                    start_new_session=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
                        'ASAN_OPTIONS': 'halt_on_error=1:detect_leaks=0'})
                timed_out = False
                try:
                    stdout, stderr = child.communicate(timeout=2 if mode == 'extreme' else 45)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    try: os.killpg(child.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
                    stdout, stderr = child.communicate(timeout=5)
                assert child.returncode is not None  # communicate has reaped this exact child.
                (folder / (mode + '.stdout')).write_bytes(stdout)
                (folder / (mode + '.stderr')).write_bytes(stderr)
                row = dict(variant=name, mode=mode, command=command, pid=child.pid,
                           exit=child.returncode, timed_out=timed_out, reaped=True,
                           elapsed_seconds=time.monotonic()-started,
                           stdout=stdout.decode(errors='replace'), stderr=stderr.decode(errors='replace'))
                self.results.append(row)
                self.assertFalse(timed_out, row)
                self.assertEqual(child.returncode, 0, stdout + stderr)
                self.assertEqual(stderr, b'')

    def test_numeric_guards_all_precisions(self): self.case('direct')
    def test_public_printf_roundtrip_all_precisions(self): self.case('public')
    def test_sign_and_notation_exact_31_character_fits(self): self.case('fits')
    def test_finite_exponent_retired_before_rounding(self): self.case('extreme')
    def test_rounding_special_padding_default_newline_controls(self): self.case('controls')
    def test_lowercase_zero_first_overflow_precision(self): self.case('lower26')
    def test_lowercase_zero_maximum_precision(self): self.case('lower99')
    def test_uppercase_zero_maximum_precision(self): self.case('upper99')

    def test_optional_arm_objects(self):
        compiler = os.environ.get('SAROO_ARM_CC')
        if not compiler: self.skipTest('set SAROO_ARM_CC for optional native objects')
        for name, folder, generic in self.variants:
            command = [compiler, '-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16', '-mfloat-abi=softfp',
                       '-Os', '-std=c99', '-ffunction-sections', '-fdata-sections', '-fstack-usage',
                       '-I', str(folder), *(['-DSAROO_FAT32_ONLY=0'] if generic else []),
                       '-c', str(folder / 'ff.c'), '-o', str(folder / 'optional.o')]
            r = subprocess.run(command, capture_output=True, timeout=60)
            (folder / 'arm-command.json').write_text(json.dumps(command, indent=2) + '\n')
            (folder / 'arm-compile.log').write_bytes(r.stdout + r.stderr)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            elf = (folder / 'optional.o').read_bytes()
            self.assertEqual(elf[:6], b'\x7fELF\x01\x01')
            self.assertEqual(elf[16:20], b'\x01\x00\x28\x00')
            rows = [r for r in (folder / 'optional.su').read_text().splitlines() if ':f_printf\t' in r]
            self.assertEqual(len(rows), 1)
            self.results.append(dict(variant=name, mode='arm', exit=0,
                                     object_sha256=hashlib.sha256(elf).hexdigest(), frame=rows[0]))


if __name__ == '__main__': unittest.main()
