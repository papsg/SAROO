"""Actual startup/caller fragments and public FatFs on synthetic FAT32 media."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from mcu_save_support import make_fat32

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/build/mcu-save'

class MCUStartupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MCU_SAVE_DIR', ROOT / 'Firm_MCU/Saturn')).resolve()
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='startup-', dir=BUILD))
        module = (source / 'saturn_utils.c').read_bytes()
        begin = module.index(b'#define SSAVE_FILE')
        end = module.index(b'int load_savefile(char *gameid)', begin)
        (cls.target / 'save-startup.inc').write_bytes(
            module[begin:end].replace(b'0x24002000', b'TEST_SCRATCH_ADDR'))
        main = (source / 'saturn_main.c').read_bytes()
        call = main.index(b'open_savefile();')
        begin = main.rfind(b'\n', 0, call) + 1
        end = main.index(b'\t*(u32*)(SYSINFO_ADDR+0x00)', call)
        (cls.target / 'save-startup-gate.inc').write_bytes(main[begin:end])
        fatfs = cls.target / 'FatFS'; fatfs.mkdir()
        inputs = {'saturn_utils.c': module, 'saturn_main.c': main}
        for name in ('mcu_save_startup.c', 'mcu_save_support.py', 'test_mcu_save_startup.py'):
            data = (ROOT / 'tests' / name).read_bytes()
            (cls.target / name).write_bytes(data); inputs['tests/' + name] = data
        for name in ('ff.c', 'ff.h', 'ffconf.h', 'ffunicode.c', 'diskio.h'):
            data = (ROOT / 'Firm_MCU/FatFS' / name).read_bytes()
            (fatfs / name).write_bytes(data); inputs['FatFS/' + name] = data
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2)+'\n')
        cls.binary = cls.target / 'startup'
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        args = [os.environ.get('CC', 'cc'), '-std=gnu11', '-g', *flags, '-pthread',
                '-I', str(cls.target), '-I', str(fatfs), str(cls.target / 'mcu_save_startup.c'),
                str(fatfs / 'ff.c'), str(fatfs / 'ffunicode.c'), '-o', str(cls.binary)]
        result = subprocess.run(args, capture_output=True)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    def invoke(self, mode, target=0, value=0, gate=0):
        with tempfile.TemporaryDirectory(dir=self.target) as tmp:
            card = Path(tmp) / 'card.img'; make_fat32(card)
            result = subprocess.run([str(self.binary), str(card), mode, str(target), str(value), str(gate)],
                                    capture_output=True, timeout=15,
                                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
                                         'ASAN_OPTIONS': 'detect_leaks=0'})
            with (self.target / 'cases.log').open('ab') as log:
                log.write(f'{mode} target={target} value={value} gate={gate} rc={result.returncode}\n'.encode())
                log.write(result.stdout + result.stderr)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotIn(b'runtime error:', result.stderr)

    def test_valid_missing_and_compatible_extents(self):
        for mode, target in (('healthy', 0), ('new-both', 0), ('missing', 0), ('missing', 1)):
            with self.subTest(mode=mode, target=target): self.invoke(mode, target)
        for target, size in ((0, 65537), (0, 131071), (1, 8388609), (1, 8392705)):
            with self.subTest(target=target, size=size): self.invoke('compatible-size', target, size)

    def test_existing_archive_errors_never_recreate(self):
        for target in (0, 1):
            for code in range(1, 20):
                with self.subTest(target=target, code=code): self.invoke('open-error', target, code)
            for mode in ('read-error', 'read-short', 'seek-error'):
                for value in ((0, 1) if mode == 'read-short' else (0,)):
                    with self.subTest(target=target, mode=mode, value=value): self.invoke(mode, target, value)
        self.invoke('cleanup-error', 1)

    def test_invalid_extents_and_signatures_are_preserved(self):
        for target, sizes in ((0, (0, 1, 63, 64, 65535)), (1, (0, 8191, 8192, 8388607))):
            for size in sizes:
                with self.subTest(target=target, size=size): self.invoke('size', target, size)
            for damage in (1, 2):
                with self.subTest(target=target, damage=damage): self.invoke('signature', target, damage)
        self.invoke('signature', 0, 3)

    def test_failed_creation_and_initialization_stop_service(self):
        for target in (0, 1):
            for code in (1, 3, 7, 8, 9, 10):
                with self.subTest(target=target, code=code): self.invoke('create-error', target, code)
            for mode in ('write-error', 'write-short', 'sync-error'):
                with self.subTest(target=target, mode=mode): self.invoke(mode, target)
        for mode in ('extend-error', 'extend-short'):
            with self.subTest(mode=mode): self.invoke(mode, 1)

    def test_actual_caller_stops_on_error(self):
        for mode, target, value in (('healthy', 0, 0), ('new-both', 0, 0), ('signature', 0, 1),
                                    ('signature', 1, 1), ('read-error', 1, 0), ('create-error', 1, 7)):
            with self.subTest(mode=mode, target=target): self.invoke(mode, target, value, 1)

if __name__ == '__main__':
    unittest.main()
