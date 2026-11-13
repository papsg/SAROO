"""Exercise the actual launch caller and game patch dispatcher on host memory."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest
import sys
import zlib

from test_menu_cdplayer import ROOT, function


def crc_suffix(data, target):
    """Solve the 32 CRC bit equations for a synthetic four-byte suffix."""
    data[-4:] = bytes(4)
    base = zlib.crc32(data)
    basis = {}
    for bit in range(32):
        data[-4:] = (1 << bit).to_bytes(4, 'little')
        value, source = zlib.crc32(data) ^ base, 1 << bit
        while value:
            pivot = value.bit_length() - 1
            if pivot not in basis:
                basis[pivot] = value, source
                break
            value ^= basis[pivot][0]
            source ^= basis[pivot][1]
    value, suffix = target ^ base, 0
    while value:
        pivot = value.bit_length() - 1
        value ^= basis[pivot][0]
        suffix ^= basis[pivot][1]
    result = suffix.to_bytes(4, 'little')
    data[-4:] = result
    assert zlib.crc32(data) == target
    return result


class MenuVDP1OrderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_VDP1_SOURCE', ROOT / 'Firm_Saturn'))
        patch = (source / 'game_patch.c').read_bytes()
        load = (source / 'game_load.c').read_bytes()
        crc = (source / 'crc32.c').read_bytes().replace(b'#include "main.h"', b'')
        first = patch.index(b'typedef struct _game_db')
        table = patch[first:patch.index(b'void patch_game(', first)]
        names = set(re.findall(rb'"[^"\n]+",\s*([A-Za-z_0-9]+)\}', table))
        actual = {b'Croc_LOG_patch', b'ULTRAMAN_patch', b'kof95_patch',
                  b'DND2_patch', b'FRIENDS_patch'}
        assert actual <= names, 'Required game callbacks missing from actual table'
        if b'CNC_reset_patch' in names:
            actual.add(b'CNC_reset_patch')
        code = b'\n'.join(function(patch, name.decode()) if name in actual else
                          b'void ' + name + b'(void) { other_patch(); }'
                          for name in sorted(names))
        code += table + function(patch, 'patch_game')
        first = load.index(b'static u16 code_06b8[')
        code += load[first:load.index(b'\n};', first) + 4]
        code += function(load, 'read_1st')
        if b'SD_OVERLAY_REPLACEMENT' in code:
            header = (source / 'main.h').read_bytes()
            define = re.search(rb'^#define SD_OVERLAY_REPLACEMENT .+$', header, re.M)
            code = define[0] + b'\n#define TEST_OVERLAY_HANDOFF 1\n' + code
            if b'SD_OVERLAY_TAG' in code:
                code = re.search(rb'^#define SD_OVERLAY_TAG .+$', header, re.M)[0] + b'\n' + code

        # Keep the real dispatch and memory writes while mapping the bus
        # pointers and executable targets to the host fixture.
        code = code.replace(b'memset(', b'observed_memset(')
        code = code.replace(b'(void*) *(u32*)(0x06000284)',
                            b'loader_target(*(u32*)(0x06000284))')
        code = code.replace(b'(void*)0x200000', b'rom_target')
        code = code.replace(b'(u8*)0x26003800', b'(u8*)cnc_image_at(0x26003800)')
        code = code.replace(b'REG16(0x06076344)', b'(*cnc_instruction_at(0x06076344))')
        code = re.sub(rb'\((u8|u16|u32|char)\s*\*\)\s*\((0x[0-9a-fA-F]+(?:\+i)?|val0)\)',
                      rb'(\1*)memory_at(\2)', code)
        code = re.sub(rb'\((u8|u16|u32|char)\s*\*\)(0x[0-9a-fA-F]+)',
                      rb'(\1*)memory_at(\2)', code)
        for name in (b'bup_init', b'my_cdplayer', b'my_06b0', b'kof95_handle',
                     b'kof95_yzb_hack_handle'):
            code = code.replace(b'(u32)' + name, b'(u32)(uintptr_t)' + name)

        build = ROOT / 'tests/build/menu-vdp1-order'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        # Generated data has the reviewed CRC and words, without containing a
        # game executable. Wrong-word cases keep the CRC valid independently.
        fixtures = []
        for size, crc_value, sites in (
                (179656, 0x15e2d749, ((0x51a6, 0x2131), (0x51ae, 0x2301))),
                (244484, 0xf4bfcb42, ((0x328e8, 0x2122), (0x3298a, 0x2122)))):
            variants = []
            for changed in range(4):
                data = bytearray(size)
                for i, (offset, word) in enumerate(sites):
                    data[offset:offset + 2] = (word ^ bool(changed & (1 << i))).to_bytes(2, sys.byteorder)
                variants.append('{' + ','.join(map(str, crc_suffix(data, crc_value))) + '}')
            fixtures.append('{' + ','.join(variants) + '}')
        (cls.target / 'shared-fixtures.inc').write_text(
            'static const u8 sibling_suffix[2][4][4] = {' + ','.join(fixtures) + '};\n')
        (cls.target / 'actual.inc').write_bytes(code)
        (cls.target / 'crc32.inc').write_bytes(crc)
        cls.binary = cls.target / 'menu-vdp1-order'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_VDP1_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        result = subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall',
             '-Wextra', '-Werror', '-Wno-missing-field-initializers', '-fno-strict-aliasing',
             *flags, '-I', str(cls.target), str(ROOT / 'tests/menu_vdp1_order.c'),
             '-o', str(cls.binary)], capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def scenario(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True,
                                text=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'{name}.log').write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_builtin_croc_workaround_survives_launch(self):
        self.scenario('croc')

    def test_configuration_writes_survive_and_override_builtin(self):
        self.scenario('config')

    def test_unmatched_and_bypassed_games_keep_initialization(self):
        self.scenario('plain')

    def test_direct_rom_takeover_leaves_video_memory_intact(self):
        self.scenario('takeover')

    def test_returning_rom_path_initializes_once(self):
        self.scenario('rom-return')

    def test_kof_keeps_deferred_handoff_and_backup_selection(self):
        self.scenario('kof')

    def test_remaining_table_entries_dispatch_once(self):
        self.scenario('table')

    def test_cnc_reset_patch_requires_the_matching_game_and_instruction(self):
        self.scenario('cnc')

    def test_only_admitted_virtual_overlay_replaces_builtin_callback(self):
        self.scenario('overlay')

    def test_shared_id_callbacks_require_the_complete_correct_first_file(self):
        self.scenario('siblings')

    def test_recipe_cartridge_actions_precede_user_overrides_and_require_exact_tags(self):
        self.scenario('cart-action')


if __name__ == '__main__':
    unittest.main()
