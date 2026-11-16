"""Actual optional getcwd; public providers, TCHAR units and exact-fit prefixes."""
# Adapted from paps-g 097ad56b/6e9c9e63 (SSR-115). No diagnostic provider.
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FatFsGetcwdTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/fatfs-getcwd'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.variants = []; cls.results = []; pins = {}
        provider = Path(os.environ.get('SAROO_FATFS_DIR', ROOT / 'Firm_MCU/FatFS'))
        source = {name: (provider / name).read_bytes() for name in ('ff.c', 'ff.h', 'ffconf.h', 'ffunicode.c', 'diskio.h')}
        scaffold = (ROOT / 'tests/fatfs_seek.c').read_bytes()
        scaffold = scaffold[:scaffold.index(b'static int selected(')]
        fixture = (ROOT / 'tests/fatfs_getcwd.c').read_bytes()
        for name, data in {**source, 'fixture.c': fixture, 'scaffold.c': scaffold,
                           'driver.py': Path(__file__).read_bytes()}.items():
            (cls.target / name).write_bytes(data)
        profiles = [(p, enc, prefix, 2) for p in ('fat32', 'generic') for enc in range(4)
                    for prefix in ('single', 'numeric', 'dos', 'unix')]
        profiles += [('fat32', 2, 'single', 1)]  # Enable only RPATH in the selected static-LFN API.
        requested = json.loads(os.environ.get('SAROO_GETCWD_VARIANTS', 'null'))
        names = {f'{p}-u{e}-{prefix}' + ('-static' if lfn == 1 else '') for p, e, prefix, lfn in profiles}
        if requested is not None:
            assert isinstance(requested, list) and requested and len(set(requested)) == len(requested)
            assert set(requested) <= names
        for p, encoding, prefix, lfn in profiles:
            name = f'{p}-u{encoding}-{prefix}' + ('-static' if lfn == 1 else '')
            if requested is not None and name not in requested: continue
            folder = cls.target / name; folder.mkdir()
            config = source['ffconf.h'].decode()
            settings = dict(FF_USE_MKFS=1, FF_FS_RPATH=2, FF_USE_LFN=lfn,
                            FF_LFN_UNICODE=encoding, FF_VOLUMES=1 if prefix == 'single' else 2,
                            FF_STR_VOLUME_ID={'single': 0, 'numeric': 0, 'dos': 1, 'unix': 2}[prefix])
            for key, value in settings.items():
                config, count = re.subn(r'(?m)^(#define\s+' + key + r'\s+)\S+',
                                       lambda m: m[1] + str(value), config)
                assert count == 1, key
            config, count = re.subn(r'(?m)^#define\s+FF_VOLUME_STRS[^\n]*$',
                                   '#define FF_VOLUME_STRS ' + ('"RAM"' if prefix == 'single' else '"RAM","NAND"'), config)
            assert count == 1
            staged = {**source, 'ffconf.h': config.encode(), 'disk.inc': scaffold, 'fixture.c': fixture}
            for filename, data in staged.items(): (folder / filename).write_bytes(data)
            pins[name] = {n: hashlib.sha256(v).hexdigest() for n, v in staged.items()}
            flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
                '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
            if p == 'generic': flags += ['-DSAROO_FAT32_ONLY=0']
            command = [os.environ.get('CC', 'cc'), '-std=gnu11', '-g', '-Wall', '-Wextra', '-Werror',
                       '-Wno-unused-function', *flags, '-pthread', '-I', str(folder),
                       str(folder / 'fixture.c'), str(folder / 'ff.c'), str(folder / 'ffunicode.c'),
                       '-o', str(folder / 'check')]
            (folder / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
            result = subprocess.run(command, capture_output=True, timeout=60)
            (folder / 'compile.log').write_bytes(result.stdout + result.stderr); result.check_returncode()
            cls.variants.append((name, folder, p == 'generic'))
        pins['base'] = {n: hashlib.sha256(v).hexdigest() for n, v in source.items()}
        pins['fixture'] = hashlib.sha256(fixture).hexdigest()
        pins['driver'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        (cls.target / 'inputs.json').write_text(json.dumps(pins, indent=2) + '\n')

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def case(self, mode, generic_only=False):
        for name, folder, generic in self.variants:
            if generic_only and not generic: continue
            with self.subTest(variant=name, mode=mode):
                image = folder / (mode + '.img')
                with image.open('wb') as stream: stream.truncate(64 * 1024**2)
                command = [str(folder / 'check'), str(image), mode, name]
                result = subprocess.run(command, capture_output=True, timeout=30,
                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
                         'ASAN_OPTIONS': 'halt_on_error=1:detect_leaks=0'})
                (folder / (mode + '.stdout')).write_bytes(result.stdout)
                (folder / (mode + '.stderr')).write_bytes(result.stderr)
                self.results.append(dict(variant=name, mode=mode, command=command,
                    exit=result.returncode, stdout=result.stdout.decode(errors='replace'),
                    stderr=result.stderr.decode(errors='replace')))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_larger_buffers_keep_path_encoding(self): self.case('valid')
    def test_exact_fit_includes_terminator_and_prefix(self): self.case('exact')
    def test_path_only_capacity_and_guards(self): self.case('short')
    def test_zero_and_one_TCHAR_capacity(self): self.case('minimum')
    def test_short_actual_heap_allocations(self): self.case('heap')
    def test_documented_exfat_root_result(self): self.case('exfat', generic_only=True)

    def test_optional_arm_objects(self):
        compiler = os.environ.get('SAROO_ARM_CC')
        if not compiler: self.skipTest('set SAROO_ARM_CC for optional native objects')
        wanted = {'fat32-u0-single', 'fat32-u1-single', 'fat32-u2-single',
                  'fat32-u3-single', 'generic-u2-single', 'fat32-u2-single-static'}
        for name, folder, generic in self.variants:
            if name not in wanted: continue
            command = [compiler, '-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16', '-mfloat-abi=softfp',
                       '-Os', '-std=c99', '-ffunction-sections', '-fdata-sections', '-fstack-usage',
                       '-I', str(folder), *(['-DSAROO_FAT32_ONLY=0'] if generic else []),
                       '-c', str(folder / 'ff.c'), '-o', str(folder / 'optional.o')]
            result = subprocess.run(command, capture_output=True, timeout=60)
            (folder / 'arm-command.json').write_text(json.dumps(command, indent=2) + '\n')
            (folder / 'arm-compile.log').write_bytes(result.stdout + result.stderr)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            elf = (folder / 'optional.o').read_bytes()
            self.assertEqual(elf[:6], b'\x7fELF\x01\x01')
            self.assertEqual(elf[16:20], b'\x01\x00\x28\x00')
            rows = [r for r in (folder / 'optional.su').read_text().splitlines() if ':f_getcwd\t' in r]
            self.assertEqual(len(rows), 1)
            self.results.append(dict(variant=name, mode='arm', exit=0,
                                     object_sha256=hashlib.sha256(elf).hexdigest(), frame=rows[0]))


if __name__ == '__main__': unittest.main()
