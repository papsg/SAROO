"""Synthetic, sparse FAT32 media for actual firmware/FatFs tests only."""
# Adapted from paps-g's aa74eb3e host fixtures; no SD device operations.
import hashlib
import json
import os
import re
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
        marker = b'static FIL cover_fp;' if cls.fixture in ('runtime', 'extents', 'api', 'disc') else b'int load_savefile(char *gameid)'
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
        if cls.fixture in ('runtime', 'extents', 'disc', 'cue', 'header'):
            header = (source / 'cdc.h').read_bytes(); inputs['Saturn/cdc.h'] = header
            start = header.rfind(b'typedef struct', 0, header.index(b'}TRACK_INFO;'))
            end = header.index(b'typedef struct', header.index(b'}TRACK_INFO;'))
            (cls.target / 'track-layout.h').write_bytes(header[start:end])
            (cls.target / 'track-limit.h').write_bytes(b'\n'.join(
                line for line in header.splitlines() if line.startswith((b'#define MAX_TRACKS', b'#define PLAYTYPE_')))+b'\n')
            harnesses += ['track_fixture.h']
        if cls.fixture in ('runtime', 'extents'):
            disc = (source / 'cdimg.c').read_bytes(); inputs['Saturn/cdimg.c'] = disc
            begin = disc.index(b'int get_gameid(')
            selected = disc[begin:disc.index(b'\n}', begin)+2]
            if b'static int read_ip(' in disc:
                selected = disc[disc.index(b'static int read_ip('):begin] + selected
            (cls.target / 'game-id.inc').write_bytes(selected.replace(b'0x24002000', b'TEST_SCRATCH_ADDR'))
            call = disc.index(b'load_savefile(gameid);')
            (cls.target / 'disc-finish.inc').write_bytes(
                disc[disc.rfind(b'\n', 0, call)+1:disc.index(b'int get_disc_ip(', call)])
            harnesses += ['mcu_save_runtime.c', 'test_mcu_save_runtime.py']
        if cls.fixture == 'extents':
            harnesses += ['mcu_save_extents.c', 'test_mcu_save_extents.py']
        if cls.fixture in ('api', 'disc'):
            harnesses += ['mcu_save_api.c', 'test_mcu_save_api.py']
            path = Path(os.environ.get('SAROO_GAME_SAVE_SOURCE', ROOT / 'Firm_Saturn/game_save.c'))
            data = path.read_bytes(); inputs['Firm_Saturn/game_save.c'] = data
            (cls.target / 'game_save.c').write_bytes(data.replace(b'0x06000c20', b'TEST_IP_ADDR'))
            for name in ('main.h', 'smpc.h'):
                data = (ROOT / 'tests/firmware-save-stubs' / name).read_bytes()
                (cls.target / name).write_bytes(data); inputs['tests/firmware-save-stubs/' + name] = data
        if cls.fixture == 'disc':
            harnesses += ['mcu_save_disc.c', 'test_mcu_save_disc.py']
            start = module.index(b'static char *gameid_split(')
            config = module[start:module.index(b'/******************************************************************************/', start)]
            start = module.index(b'char *get_line(')
            config += module[start:module.index(b'#define SSAVE_FILE', start)]
            (cls.target / 'config-disc.inc').write_bytes(config.replace(b'0x24002000', b'TEST_SCRATCH_ADDR'))
            coupled = (ROOT / 'tests/mcu_save_api.c').read_bytes()
            (cls.target / 'coupled-harness.inc').write_bytes(coupled[:coupled.index(b'int main(')])
            disc = (source / 'cdimg.c').read_bytes(); inputs['Saturn/cdimg.c'] = disc
            begin = disc.index(b'int parse_iso(')
            close = disc.index(b'static int close_tracks(int count)\n{') if b'static int close_tracks(int count)\n{' in disc else disc.index(b'int unload_disc(')
            parts = [disc[disc.index(b'int get_gameid('):disc.index(b'int load_pcm(')],
                     disc[begin:disc.index(b'\n}', begin)+2],
                     disc[close:disc.index(b'int find_cue_iso(')]]
            if b'static int join_path(' in disc:
                parts.insert(0, disc[disc.index(b'static int join_path('):disc.index(b'int get_gameid(')])
                parts.append(disc[disc.index(b'static int game_dir('):disc.index(b'int load_disc(')])
            (cls.target / 'disc-core.inc').write_bytes(b''.join(parts).replace(b'0x24002000', b'TEST_SCRATCH_ADDR'))
            (cls.target / 'disc-load.inc').write_bytes(disc[disc.index(b'int load_disc('):])
            signature = b'int load_disc(int index, int start_save)' in disc
            (cls.target / 'disc-api.h').write_text('#define DISC_STARTS_SAVE %d\n' % signature)
            if b'int pause_disc(void)' in main:
                begin = main.index(b'int pause_drive(void)') if b'int pause_drive(void)' in main else main.index(b'int pause_disc(void)')
                pause = main[begin:main.index(b'void cdc_delay(', begin)]
            else:
                pause = b'static int pause_disc(void) { return 0; }\n'
            (cls.target / 'pause-disc.inc').write_bytes(pause)
            ack = b''
            if b'static void acknowledge_disc_pause(void)' in main:
                begin = main.index(b'static void acknowledge_disc_pause(void)')
                ack = main[begin:main.index(b'\n}', begin)+2]
                cdc = (source/'saturn_cdc.c').read_bytes(); inputs['Saturn/saturn_cdc.c']=cdc
                if b'void invalidate_directory(' in cdc:
                    begin=cdc.index(b'void invalidate_directory(')
                    ack=cdc[begin:cdc.index(b'\n}',begin)+2]+b'\n'+ack
            begin = main.index(b'_restart_nowait:')
            begin = main.index(b'\t\tif(cdb.pause_request)',begin)
            end = main.index(b'\n\t\t}',begin)+4
            restart = main[begin:end]
            begin = main.index(b'\t\t\tif(cdb.pause_request)',end)
            end = main.index(b'\n\t\t\t}',begin)+5
            active = main[begin:end]
            # Test-only preemption checkpoint after actual idle publication.
            for name, code in (('pause-ack.inc', ack), ('pause-worker.inc', restart), ('pause-active-worker.inc', active)):
                code = re.sub(rb'(cdb\.play_type\s*=\s*0;)', rb'\1 publication_checkpoint();', code)
                (cls.target / name).write_bytes(code)
            begin = main.index(b'\t\t\tif(next_disc>=0)')
            end = main.index(b'\n\t\t\tHIRQ = HIRQ_SCDQ;', begin)
            (cls.target / 'swap.inc').write_bytes(main[begin:end])
            begin = main.index(b'case SSCMD_LOADDISC:')
            end = main.index(b'\n\t\tbreak;', begin)
            (cls.target / 'menu-load.inc').write_bytes(main[main.index(b'\n', begin)+1:end])
            data = (source.parent / 'Main/sdio_h7.c').read_bytes(); inputs['Main/sdio_h7.c'] = data
            (cls.target / 'sd-reset.inc').write_bytes(data[data.index(b'int sdio_reset(void)'):])
        if cls.fixture in ('discovery', 'cover'):
            harnesses += ['mcu_save_discovery.c', 'test_mcu_save_discovery.py']
            data = (source / 'cdimg.c').read_bytes(); inputs['Saturn/cdimg.c'] = data
            helper = data.index(b'static int join_path(') if b'static int join_path(' in data else -1
            helpers = data[helper:data.index(b'\n}', helper)+2] if helper >= 0 else b''
            suffix = data.find(b'static int image_suffix(')
            if suffix >= 0: helpers += data[suffix:data.index(b'\n}', suffix)+2]
            close = data.index(b'static int close_tracks(int count)\n{') if b'static int close_tracks(int count)\n{' in data else data.index(b'int unload_disc(')
            catalog = data[data.index(b'int category_current'):close]
            scanner = data[data.index(b'int find_cue_iso('):data.index(b'int load_disc(')]
            (cls.target / 'discovery.inc').write_bytes(helpers + catalog + scanner)
            if b'static int game_dir(' in data:
                (cls.target / 'path.inc').write_text('return game_dir(index, out);\n')
            else:
                begin = data.index(b'\n\tif(', data.index(b'int load_disc('))
                end = data.index(b'\tfname = malloc(256);', begin)
                (cls.target / 'path.inc').write_bytes(b'char cat_dir[256];\n' + data[begin:end] + b'memcpy(out,cat_dir,256); return 0;\n')
            begin = module.index(b'static int max_depth;'); end = module.index(b'char *get_token(', begin)
            (cls.target / 'sort.inc').write_bytes(module[begin:end].replace(b'0x2400a000', b'TEST_SORT_ADDR'))
            path = Path(os.environ.get('SAROO_MENU_BINARY_SOURCE', ROOT / 'Firm_Saturn/main.c'))
            data = path.read_bytes(); inputs['Firm_Saturn/main.c'] = data
            begin = data.index(b'int run_binary('); end = data.index(b'static int sel_handle(', begin)
            binary = data[begin:end]
            if b'static u32 file_capacity(' in data:
                helper = data.index(b'static u32 file_capacity(')
                binary = data[helper:data.index(b'\n}', helper)+2] + b'\n' + binary
            (cls.target / 'binary.inc').write_bytes(binary)
        if cls.fixture == 'cover':
            harnesses += ['mcu_save_cover.c', 'test_mcu_save_cover.py']
            header = (source / 'cdc.h').read_bytes(); inputs['Saturn/cdc.h'] = header
            layout = b'\n'.join(line for line in header.splitlines() if line.startswith(
                (b'#define COVER_INDEX_OFFSET ', b'#define COVER_CACHE_OFFSET ')))
            begin = module.index(b'u32 adler32(')
            adler = module[begin:module.index(b'/******************************************************************************/', begin)]
            code = module[module.index(b'static FIL cover_fp;'):]
            # Keep the old 32-bit physical cache representation on a 64-bit host.
            # This translation changes only addresses, not cache capacity/fields.
            code = re.sub(rb'0x614([0-9a-fA-F]{5})',
                          lambda m: b'(TEST_GFX_BASE + 0x' + m[1] + b')', code)
            code = code.replace(b'return (int)hdr;',
                                b'return (int)(0x614f0000 + (hdr - cover_buf));')
            code = re.sub(rb'\((u16|int)\*\)\(chdr\+(0x[0-9a-f]+)\)',
                          rb'(\1*)(TEST_GFX_BASE + (u32)chdr - 0x61400000 + \2)', code)
            if b'void cover_reset(void)' not in code:
                code += b'\nvoid cover_reset(void) {}\n'
            helper = module[module.index(b'static int save_io('):module.index(b'int load_savefile(')]
            (cls.target / 'cover.inc').write_bytes(layout + b'\n' + adler + helper + code)
            path = Path(os.environ.get('SAROO_COVER_MENU_SOURCE', ROOT / 'Firm_Saturn/main.c'))
            data = path.read_bytes(); inputs['Firm_Saturn/main.c'] = data
            begin = data.index(b'static int last_w, last_h, last_f;')
            code = data[begin:data.index(b'static void change_cover_layer(', begin)]
            code = re.sub(rb'0x224([0-9a-fA-F]{5})',
                          lambda m: b'(TEST_GFX_BASE + 0x' + m[1] + b')', code)
            (cls.target / 'cover-display.inc').write_bytes(code)
        if cls.fixture in ('cue', 'header'):
            harnesses += ['mcu_save_cue.c', 'test_mcu_save_cue.py']
            if cls.fixture == 'header':
                harnesses += ['mcu_save_header.c', 'test_mcu_save_header.py']
            data = (source / 'cdimg.c').read_bytes(); inputs['Saturn/cdimg.c'] = data
            begin = data.index(b'static int join_path(')
            code = data[begin:data.index(b'int category_current')]
            close = data.index(b'static int close_tracks(int count)\n{') if b'static int close_tracks(int count)\n{' in data else data.index(b'int unload_disc(')
            code += data[close:data.index(b'int find_cue_iso(')]
            for old, new in ((b'0x24002000', b'TEST_SCRATCH_ADDR'), (b'0x2400b000', b'TEST_CUE_ADDR'),
                             (b'0x2400a200', b'TEST_FULL_PATH'), (b'0x2400a000', b'TEST_DIR_PATH')):
                code = code.replace(old, new)
            (cls.target / 'cue.inc').write_bytes(code)
            begin = data.index(b'static int game_dir(')
            (cls.target / 'load.inc').write_bytes(data[begin:data.index(b'int get_disc_ip(', begin)])
            (cls.target / 'preview.inc').write_bytes(data[data.index(b'int get_disc_ip('):])
            begin = main.index(b'u32 bswap32('); end = main.index(b'u32 track_to_fad(', begin)
            (cls.target / 'toc.inc').write_bytes(main[begin:end])
            begin = module.index(b'char *get_token('); end = module.index(b'\n}', module.index(b'char *get_line(', begin)) + 3
            (cls.target / 'token.inc').write_bytes(module[begin:end])
            if getattr(cls, 'reader_fixture', False):
                def function(signature):
                    start = main.index(signature)
                    return main[start:main.index(b'\n}', start)+2]+b'\n'
                code = b''.join(function(signature) for signature in (
                    b'TRACK_INFO *get_track_info(', b'u32 track_to_fad(', b'int fad_to_track('))
                start = main.index(b'static u8 reorder[')
                code += main[start:main.index(b'int get_sector(', start)]
                code += function(b'int get_sector(')
                (cls.target / 'reader.inc').write_bytes(code)
                harnesses += [cls.c_source, getattr(cls, 'test_source', 'test_cue_geometry.py')]
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
        if cls.fixture in ('api', 'disc', 'discovery', 'cover'):
            flags += ['-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast', '-fno-strict-aliasing']
        args = [os.environ.get('CC', 'cc'), '-std=gnu11', '-g', *flags, '-pthread',
                '-I', str(cls.target), '-I', str(fatfs), str(cls.target / getattr(cls, 'c_source', 'mcu_save_'+cls.fixture+'.c')),
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
                                    capture_output=True, timeout=getattr(self, 'timeout', 20),
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
