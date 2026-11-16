"""Optional FatFs volume labels on complete selected/generic public providers."""
# Adapted from paps-g d063f6db (SSR-117); no diagnostic provider.
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class FatFsLabelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/fatfs-label'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.variants = []
        cls.results = []
        provider = Path(os.environ.get('SAROO_FATFS_DIR', ROOT / 'Firm_MCU/FatFS'))
        source = {n: (provider / n).read_bytes()
                  for n in ('ff.c', 'ff.h', 'ffconf.h', 'ffunicode.c', 'diskio.h')}
        scaffold = (ROOT / 'tests/fatfs_seek.c').read_bytes()
        scaffold = scaffold[:scaffold.index(b'static int selected(')]
        fixture = (ROOT / 'tests/fatfs_label.c').read_bytes()
        driver = Path(__file__).read_bytes()
        requested = json.loads(os.environ.get('SAROO_LABEL_VARIANTS', 'null'))
        names = {f'{profile}-u{encoding}' for profile in ('fat32', 'generic')
                 for encoding in range(4)}
        if requested is not None:
            assert isinstance(requested, list) and requested
            assert len(set(requested)) == len(requested) and set(requested) <= names
        pins = {'base': {n: hashlib.sha256(v).hexdigest() for n, v in source.items()},
                'fixture': hashlib.sha256(fixture).hexdigest(),
                'driver': hashlib.sha256(driver).hexdigest()}
        for profile in ('fat32', 'generic'):
            for encoding in range(4):
                name = f'{profile}-u{encoding}'
                if requested is not None and name not in requested:
                    continue
                folder = cls.target / name
                folder.mkdir()
                config = source['ffconf.h'].decode()
                settings = dict(FF_USE_LABEL=1, FF_USE_MKFS=1, FF_USE_LFN=2,
                                FF_LFN_UNICODE=encoding,
                                FF_FS_REENTRANT=int(profile == 'generic'),
                                FF_CODE_PAGE=437)
                for key, value in settings.items():
                    config, count = re.subn(r'(?m)^(#define\s+' + key + r'\s+)\S+',
                                           lambda match: match[1] + str(value), config)
                    assert count == 1, key
                staged = {**source, 'ffconf.h': config.encode(),
                          'disk.inc': scaffold, 'fixture.c': fixture, 'driver.py': driver}
                for filename, data in staged.items():
                    (folder / filename).write_bytes(data)
                pins[name] = {n: hashlib.sha256(v).hexdigest() for n, v in staged.items()}
                flags = (['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release'
                         else ['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])
                if profile == 'generic':
                    flags += ['-DSAROO_FAT32_ONLY=0']
                command = [os.environ.get('CC', 'cc'), '-std=gnu11', '-g', '-Wall', '-Wextra',
                           '-Werror', '-Wno-unused-function', *flags, '-pthread',
                           '-I', str(folder), str(folder / 'fixture.c'),
                           str(folder / 'ff.c'), str(folder / 'ffunicode.c'),
                           '-o', str(folder / 'check')]
                (folder / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
                result = subprocess.run(command, capture_output=True, timeout=60)
                (folder / 'compile.log').write_bytes(result.stdout + result.stderr)
                result.check_returncode()
                cls.variants.append((name, folder, profile == 'generic'))
        (cls.target / 'inputs.json').write_text(json.dumps(pins, indent=2) + '\n')

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def case(self, mode, generic_only=False):
        for name, folder, generic in self.variants:
            if generic_only and not generic:
                continue
            with self.subTest(variant=name, mode=mode):
                image = folder / (mode + '.img')
                with image.open('wb') as stream:
                    stream.truncate(64 * 1024**2)
                command = [str(folder / 'check'), str(image), mode]
                result = subprocess.run(
                    command, capture_output=True, timeout=120,
                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
                         'ASAN_OPTIONS': 'halt_on_error=1:detect_leaks=0'})
                (folder / (mode + '.stdout')).write_bytes(result.stdout)
                (folder / (mode + '.stderr')).write_bytes(result.stderr)
                self.results.append(dict(variant=name, mode=mode, command=command,
                                         exit=result.returncode,
                                         stdout=result.stdout.decode(errors='replace'),
                                         stderr=result.stderr.decode(errors='replace')))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(result.stderr, b'')

    def test_absent_label_and_null_outputs(self): self.case('absent', generic_only=True)
    def test_first_invalid_lengths_with_guards(self): self.case('bounds', generic_only=True)
    def test_every_invalid_byte_count_first_and_last_slot(self): self.case('counts', generic_only=True)
    def test_fat32_labels_serial_and_remount(self): self.case('fat32')
    def test_exact_documented_heap_capacity(self): self.case('heap', generic_only=True)
    def test_serial_only_and_malformed_label_repair(self): self.case('repair', generic_only=True)
    def test_valid_lengths_encodings_and_end_slots(self): self.case('valid', generic_only=True)

    def test_optional_arm_objects(self):
        compiler = os.environ.get('SAROO_ARM_CC')
        if not compiler:
            self.skipTest('set SAROO_ARM_CC for optional native objects')
        for name, folder, generic in self.variants:
            command = [compiler, '-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16',
                       '-mfloat-abi=softfp', '-Os', '-std=c99', '-ffunction-sections',
                       '-fdata-sections', '-fstack-usage', '-I', str(folder),
                       *(['-DSAROO_FAT32_ONLY=0'] if generic else []),
                       '-c', str(folder / 'ff.c'), '-o', str(folder / 'optional.o')]
            result = subprocess.run(command, capture_output=True, timeout=60)
            (folder / 'arm-command.json').write_text(json.dumps(command, indent=2) + '\n')
            (folder / 'arm-compile.log').write_bytes(result.stdout + result.stderr)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            elf = (folder / 'optional.o').read_bytes()
            self.assertEqual(elf[:6], b'\x7fELF\x01\x01')
            self.assertEqual(elf[16:20], b'\x01\x00\x28\x00')
            rows = [row for row in (folder / 'optional.su').read_text().splitlines()
                    if ':f_getlabel\t' in row]
            self.assertEqual(len(rows), 1)
            self.results.append(dict(variant=name, mode='arm', exit=0,
                                     object_sha256=hashlib.sha256(elf).hexdigest(),
                                     frame=rows[0]))


if __name__ == '__main__':
    unittest.main()
