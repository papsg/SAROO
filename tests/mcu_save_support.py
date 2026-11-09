"""Synthetic, sparse FAT32 media for actual firmware/FatFs tests only."""
# Adapted from paps-g's aa74eb3e host fixtures; no SD device operations.
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'tests/build/mcu-save'

def make_fat32(path, sectors=131072):
    """Small sparse FAT32 fixture with two FATs and a single root cluster."""
    fat_sectors = (sectors + 127) // 128
    boot = bytearray(512)
    boot[:11] = b'\xeb\x58\x90MSDOS5.0'
    struct.pack_into('<HBHBHHBHHHII', boot, 11,
                     512, 1, 32, 2, 0, 0, 0xf8, 0, 32, 16, 0, sectors)
    struct.pack_into('<IHHIHH', boot, 36, fat_sectors, 0, 0, 2, 1, 6)
    boot[64] = 0x80; boot[66] = 0x29
    boot[71:82] = b'SAVE TEST  '
    boot[82:90] = b'FAT32   '; boot[510:512] = b'\x55\xaa'
    info = bytearray(512)
    struct.pack_into('<I', info, 0, 0x41615252)
    struct.pack_into('<III', info, 484, 0x61417272, 0xffffffff, 0xffffffff)
    info[510:512] = b'\x55\xaa'
    with path.open('wb') as f:
        f.truncate(sectors * 512)
        for sector in (0, 6): f.seek(sector * 512); f.write(boot)
        for sector in (1, 7): f.seek(sector * 512); f.write(info)
        for sector in (32, 32 + fat_sectors):
            f.seek(sector * 512); f.write(struct.pack('<III', 0x0ffffff8, 0x0fffffff, 0x0fffffff))


class MCUSaveTest(unittest.TestCase):
    fixture = 'startup'

    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MCU_SAVE_DIR', ROOT / 'Firm_MCU/Saturn')).resolve()
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix=cls.fixture+'-', dir=BUILD))
        cls.results = []
        module = (source / 'saturn_utils.c').read_bytes()
        begin = module.index(b'#define SSAVE_FILE')
        marker = b'static FIL cover_fp;' if cls.fixture in ('runtime', 'extents', 'api') else b'int load_savefile(char *gameid)'
        end = module.index(marker, begin)
        (cls.target / 'save-startup.inc').write_bytes(
            module[begin:end].replace(b'0x24002000', b'TEST_SCRATCH_ADDR'))
        main = (source / 'saturn_main.c').read_bytes()
        call = main.index(b'open_savefile();')
        begin = main.rfind(b'\n', 0, call) + 1
        end = main.index(b'\t*(u32*)(SYSINFO_ADDR+0x00)', call)
        (cls.target / 'save-startup-gate.inc').write_bytes(main[begin:end])
        inputs = {'Saturn/saturn_utils.c': module, 'Saturn/saturn_main.c': main}
        harnesses = ['mcu_save_startup.c', 'mcu_save_support.py', 'test_mcu_save_startup.py']
        if cls.fixture in ('runtime', 'extents'):
            disc = (source / 'cdimg.c').read_bytes(); inputs['Saturn/cdimg.c'] = disc
            selected = disc[disc.index(b'int get_gameid('):disc.index(b'int load_pcm(')]
            (cls.target / 'game-id.inc').write_bytes(selected.replace(b'0x24002000', b'TEST_SCRATCH_ADDR'))
            call = disc.index(b'load_savefile(gameid);')
            (cls.target / 'disc-finish.inc').write_bytes(
                disc[disc.rfind(b'\n', 0, call)+1:disc.index(b'int get_disc_ip(', call)])
            harnesses += ['mcu_save_runtime.c', 'test_mcu_save_runtime.py']
        if cls.fixture == 'extents':
            harnesses += ['mcu_save_extents.c', 'test_mcu_save_extents.py']
        if cls.fixture == 'api':
            harnesses += ['mcu_save_api.c', 'test_mcu_save_api.py']
            path = Path(os.environ.get('SAROO_GAME_SAVE_SOURCE', ROOT / 'Firm_Saturn/game_save.c'))
            data = path.read_bytes(); inputs['Firm_Saturn/game_save.c'] = data
            (cls.target / 'game_save.c').write_bytes(data.replace(b'0x06000c20', b'TEST_IP_ADDR'))
            for name in ('main.h', 'smpc.h'):
                data = (ROOT / 'tests/firmware-save-stubs' / name).read_bytes()
                (cls.target / name).write_bytes(data); inputs['tests/firmware-save-stubs/' + name] = data
        for name in harnesses:
            data = (ROOT / 'tests' / name).read_bytes()
            (cls.target / name).write_bytes(data); inputs['tests/' + name] = data
        fatfs = cls.target / 'FatFS'; fatfs.mkdir()
        for name in ('ff.c', 'ff.h', 'ffconf.h', 'ffunicode.c', 'diskio.h'):
            data = (ROOT / 'Firm_MCU/FatFS' / name).read_bytes()
            if name == 'ffconf.h' and cls.fixture == 'extents':
                inputs['FatFS-production/ffconf.h'] = data
                before = b'#define FF_USE_MKFS\t\t0'
                assert data.count(before) == 1
                data = data.replace(before, b'#define FF_USE_MKFS\t\t1')
            (fatfs / name).write_bytes(data); inputs['FatFS/' + name] = data
        for name,data in inputs.items():
            target = cls.target / 'inputs' / name; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2)+'\n')
        cls.binary = cls.target / cls.fixture
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        if cls.fixture == 'api':
            flags += ['-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast', '-fno-strict-aliasing']
        args = [os.environ.get('CC', 'cc'), '-std=gnu11', '-g', *flags, '-pthread',
                '-I', str(cls.target), '-I', str(fatfs), str(cls.target / ('mcu_save_'+cls.fixture+'.c')),
                str(fatfs / 'ff.c'), str(fatfs / 'ffunicode.c'), '-o', str(cls.binary)]
        result = subprocess.run(args, capture_output=True)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def run_case(self, arguments, sectors=131072):
        with tempfile.TemporaryDirectory(dir=self.target) as tmp:
            card = Path(tmp) / 'card.img'; make_fat32(card, sectors)
            result = subprocess.run([str(self.binary), str(card), *map(str, arguments)],
                                    capture_output=True, timeout=20,
                                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
                                         'ASAN_OPTIONS': 'detect_leaks=0'})
            record = dict(arguments=arguments, returncode=result.returncode,
                          stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace'))
            self.results.append(record)
            with (self.target / 'cases.log').open('ab') as log:
                log.write((json.dumps(record)+'\n').encode())
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotIn(b'runtime error:', result.stderr)
            return result.stdout.decode(errors='replace')
