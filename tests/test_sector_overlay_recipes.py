"""Compare compiled SD sector edits with the retained C callback's memory effects."""
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import function
from test_menu_vdp1_order import crc_suffix
from test_sector_overlay import ROOT, canonical, sector
import test_sector_overlay as overlay_fixture


# Reviewed first-file LBA, load address and original ISO length. IP edits use
# LBA zero and the IP load address. These values are independent of recipe offsets.
CASES = {
    'amagi-shien-jp': (28, 0x06010000, 244312, 'Amagi_Shien_patch'),
    'cotton-2-jp': (24, 0x06004000, 421952, 'cotton2_patch'),
    'cotton-boomerang-jp': (26, 0x06004000, 309072, 'cotton1_patch'),
    'dragons-dream-jp': (65, 0x06010000, 504120, 'Dragons_Dream_patch'),
    'dungeons-dragons-disc2-jp': (54, 0x06010000, 179656, 'DND2_patch'),
    'final-fight-revenge-jp': (26, 0x06010000, 483456, 'FINAL_FIGHT_REVENGE_patch'),
    'friends-game-disc-jp': (21, 0x06010000, 244484, 'FRIENDS_patch'),
    'game-no-tatsujin-2-jp': (22, 0x06010000, 120109, 'Game_no_Tatsujin2_patch'),
    'gotha-jp': (24, 0x06010000, 341572, 'Gotha_patch'),
    'gundam-zeta-zenpen-jp': (0, 0x06002000, 6144, 'Gundam_Zenpen_Zeta_no_Kodou_patch'),
    'hebereke-popoitto-eu': (21, 0x06010000, 439497, 'Hebereke_Popoitto_patch'),
    'hissatsu-pachinko-jp-reva': (23, 0x06010000, 558817, 'Hissatsu_Pachinko_Collection_patch'),
    'manx-tt-superbike-us': (21, 0x06010000, 499684, 'MANX_TT_SUPER_BIKE_patch'),
    'noel-3-disc-1-jp': (116594, 0x06010000, 290228, 'NOEL3_patch'),
    'noel-3-disc-2-jp': (96842, 0x06010000, 290228, 'NOEL3_patch'),
    'noel-3-disc-3-jp': (34123, 0x06010000, 290148, 'NOEL3_patch'),
    'pocket-fighter-jp': (26, 0x0600C800, 280932, 'POCKET_FIGHTER_patch'),
    'shichuu-suimei-pitagraph-jp': (24, 0x06010000, 471629, 'Shichuu_Suimei_Pitagraph_patch'),
    'street-fighter-zero-3-jp': (39, 0x06008000, 479260, 'SF_ZERO3_patch'),
    'super-real-mahjong-p7-jp': (24, 0x06010000, 541236, 'srmp7_patch'),
    'tamagotchi-park-jp': (25, 0x06010000, 214588, 'Tamagotchi_Park_patch'),
    'whizz-eu': (0, 0x06002000, 6144, 'WHIZZ_patch'),
}


def memory_operations(code):
    """Map explicit scalar RAM operations to checked big-endian fixture access."""
    code = re.sub(rb'REG(16|32)\(([^()]*)\)',
                  lambda m: b'MEM(' + str(int(m[1]) // 8).encode() + b',' + m[2] + b')', code)
    code = re.sub(rb'\*\s*\(\s*(u8|u16|u32)\s*\*\s*\)\s*\(([^()]*)\)',
                  lambda m: b'MEM(' + str(int(m[1][1:]) // 8).encode() + b',' + m[2] + b')', code)
    code = re.sub(rb'MEM\((\d+),([^()]*)\)\s*=\s*([^=;][^;]*);',
                  rb'write_be(\2, \3, \1);', code)
    return re.sub(rb'MEM\((\d+),([^()]*)\)', rb'read_be(\2, \1)', code)


class RecipeEffectsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixture = overlay_fixture.SectorOverlayTests
        fixture.setUpClass()
        cls.decoder, cls.tool = fixture.decoder, fixture.tool
        cls.output = fixture.output
        patch = (ROOT / 'Firm_Saturn/game_patch.c').read_bytes()
        functions = sorted({row[3] for row in CASES.values()})
        cls.callback_index = {name: i for i, name in enumerate(functions)}
        code = b'\n'.join(memory_operations(function(patch, name)) for name in functions)
        code += b'\nstatic void (*const callbacks[])(void) = {' + b','.join(
            name.encode() for name in functions) + b'};\n'
        (cls.output / 'callbacks.inc').write_bytes(code)
        (cls.output / 'crc32.inc').write_bytes(
            (ROOT / 'Firm_Saturn/crc32.c').read_bytes().replace(b'#include "main.h"', b''))
        cls.runner = cls.output / 'recipe-effects'
        result = subprocess.run([
            *shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-O1', '-g',
            '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
            '-fno-omit-frame-pointer', '-I', cls.output, ROOT / 'tests/sector_recipe.c',
            '-o', cls.runner], capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def check_recipe(self, name, descriptor):
        lba, load, length, callback = descriptor
        with tempfile.TemporaryDirectory(dir=self.output) as directory:
            path = Path(directory)
            recipes = Path(os.environ.get('SAROO_RECIPE_ROOT', ROOT / 'recipes'))
            source = json.loads((recipes / (name + '-sector.json')).read_text())
            before = bytearray([0xcc]) * 0x100000

            def offset(fad, raw_offset):
                value = load - 0x06000000 + (fad - 150 - lba) * 2048 + raw_offset - 16
                self.assertGreaterEqual(value, load - 0x06000000)
                self.assertLess(value, load - 0x06000000 + length)
                return value

            for write in source['writes']:
                start = offset(write['fad'], write['offset'])
                original = bytes.fromhex(write['before'])
                before[start:start + len(original)] = original
            # Seed the documented branch guards which are outside the changed bytes.
            if name == 'game-no-tatsujin-2-jp':
                before[0x11024:0x11026] = bytes.fromhex('8801')
            if name == 'manx-tt-superbike-us':
                before[0x21a11] = 1
            if name == 'shichuu-suimei-pitagraph-jp':
                before[0x1cfd4:0x1cfd7] = bytes.fromhex('600c88')
            if name == 'pocket-fighter-jp':
                before[0x14074:0x14078] = bytes.fromhex('060383c0')
            if name in ('dungeons-dragons-disc2-jp', 'friends-game-disc-jp'):
                crc = 0x15e2d749 if name.startswith('dungeons') else 0xf4bfcb42
                image = before[0x10000:0x10000 + length]
                before[0x10000 + length - 4:0x10000 + length] = crc_suffix(image, crc)
            expected = bytearray(before)
            for write in source['writes']:
                start = offset(write['fad'], write['offset'])
                replacement = bytes.fromhex(write['after'])
                expected[start:start + len(replacement)] = replacement
            (path / 'before').write_bytes(before)
            (path / 'expected').write_bytes(expected)
            subprocess.run([self.runner, str(self.callback_index[callback]), path / 'before',
                            path / 'expected', str(source.get('cart_mode', 0))],
                           check=True, capture_output=True, timeout=15)

            # Build a compact synthetic CUE with generated gaps at the original FADs.
            # Only the test's source identity changes. All reviewed edits remain exact.
            recipe = copy.deepcopy(source)
            recipe['product_id'] = 'T-TEST'
            recipe.pop('disc_id', None)
            recipe['tracks'] = []
            (path / 'ip.bin').write_bytes(sector(150, ip=True))
            cue = ['FILE "ip.bin" BINARY', '  TRACK 01 MODE1/2352', '    INDEX 01 00:00:00']
            fads = sorted({w['fad'] for w in source['writes']} |
                          {g['fad'] for g in source.get('guards', [])})
            originals = {}
            previous = 150
            for number, fad in enumerate(fads, 2):
                if fad != previous + 1:
                    gap = fad - previous - 1
                    cue.append(f'    POSTGAP {gap // 4500:02}:{gap // 75 % 60:02}:{gap % 75:02}')
                filename = f'{fad}.bin'
                cue += [f'FILE "{filename}" BINARY', f'  TRACK {number:02} MODE1/2352',
                        '    INDEX 01 00:00:00']
                raw = bytearray(sector(fad))
                start = offset(fad, 16)
                raw[16:2064] = before[start:start + 2048]
                original = canonical(raw)
                originals[fad] = original
                (path / filename).write_bytes(original)
                recipe['tracks'].append({'number': number,
                                         'sha256': hashlib.sha256(original).hexdigest()})
                previous = fad
            (path / 'source.cue').write_text('\n'.join(cue) + '\n')
            (path / 'recipe.json').write_text(json.dumps(recipe))
            pack = path / '968872B4.SOV'
            subprocess.run([self.tool, 'compile', path / 'recipe.json', path / 'source.cue', pack],
                           check=True, capture_output=True, timeout=20)
            for index, fad in enumerate(fads):
                start = offset(fad, 16)
                raw = bytearray(originals[fad])
                raw[16:2064] = expected[start:start + 2048]
                (path / 'raw').write_bytes(originals[fad])
                (path / 'post').write_bytes(canonical(raw))
                edited = any(write['fad'] == fad for write in source['writes'])
                subprocess.run([self.decoder, 'apply' if edited else 'guard', pack,
                                path / 'raw', path / 'post', str(index)],
                               check=True, capture_output=True, timeout=15)

    def test_each_recipe_has_all_retained_callback_memory_effects(self):
        for name, descriptor in CASES.items():
            with self.subTest(recipe=name):
                self.check_recipe(name, descriptor)
