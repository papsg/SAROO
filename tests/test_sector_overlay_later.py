"""Later-load recipes: generated source sectors and actual production decoder.

Sparse original instruction words describe the reviewed edits; these fixtures
contain no game image. The SH-2 reference bytes are SAROO's own callback.
"""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zlib

from check_later_callbacks_sh2 import verify_cyber_handler
from test_sector_overlay import ROOT, PRODUCT, canonical, sector
import test_sector_overlay as overlay_fixture


CYBER_HANDLER = bytes.fromhex(
    'd103e2092121d103412b0009000b00090601695a0600d000')


class LaterRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay_fixture.SectorOverlayTests.setUpClass()
        cls.fixture = overlay_fixture.SectorOverlayTests

    def setUp(self):
        folder = tempfile.TemporaryDirectory(dir=self.fixture.output)
        self.addCleanup(folder.cleanup)
        self.path = Path(folder.name)

    def cyber_fixture(self):
        source = json.loads((ROOT / 'recipes/cyberbots-jp-v1002-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'T-1217G   V1.002')
        self.assertEqual(source['cart_mode'], 1)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']], [(333, 362, '2122', '0009')])
        self.assertEqual([g['fad'] for g in source['guards']], [303])
        recipe = copy.deepcopy(source)
        recipe['product_id'] = PRODUCT.decode().strip()
        recipe['disc_id'] = (PRODUCT + b'V1.002').decode()
        recipe['tracks'] = []
        raws = {150: sector(150, ip=True, version=b'V1.002'),
                303: sector(303), 333: sector(333)}
        guard = bytearray(raws[303])
        guard[104:108] = bytes.fromhex('0600d000')
        raws[303] = canonical(guard)
        edited = bytearray(raws[333])
        edited[362:364] = bytes.fromhex('2122')
        raws[333] = canonical(edited)
        cue, previous = [], None
        for track, (fad, raw) in enumerate(raws.items(), 1):
            if previous is not None and fad != previous + 1:
                n = fad - previous - 1
                cue.append(f'    POSTGAP {n // 4500:02}:{n // 75 % 60:02}:{n % 75:02}')
            leaf = f'{fad}.bin'
            (self.path / leaf).write_bytes(raw)
            cue += [f'FILE "{leaf}" BINARY', f'  TRACK {track:02} MODE1/2352',
                    '    INDEX 01 00:00:00']
            if track > 1:
                recipe['tracks'].append({'number': track,
                                         'sha256': hashlib.sha256(raw).hexdigest()})
            previous = fad
        (self.path / 'fixture.cue').write_text('\n'.join(cue) + '\n')
        return recipe, raws

    def compile(self, recipe):
        source = self.path / 'recipe.json'
        source.write_text(json.dumps(recipe))
        output = self.path / f'{zlib.crc32(recipe["disc_id"].encode()):08X}.SOV'
        result = subprocess.run([self.fixture.tool, 'compile', source,
                                 self.path / 'fixture.cue', output],
                                capture_output=True, text=True, timeout=30)
        return result, output

    def test_cyber_delivers_only_the_later_nop_and_keeps_original_dispatch(self):
        recipe, raws = self.cyber_fixture()
        result, pack = self.compile(recipe)
        self.assertEqual(result.returncode, 0, result.stderr)
        header = pack.read_bytes()[:24]
        self.assertEqual(header[4:8], bytes([3, 2, 0, 0x81]))
        for index, fad in enumerate((303, 333)):
            before = raws[fad]
            after = bytearray(before)
            if fad == 333:
                after[362:364] = bytes.fromhex('0009')
            expected = canonical(after)
            self.assertEqual(before[104:108], expected[104:108])
            (self.path / 'raw').write_bytes(before)
            (self.path / 'post').write_bytes(expected)
            subprocess.run([self.fixture.decoder, 'guard' if fad == 303 else 'apply',
                            pack, self.path / 'raw', self.path / 'post', str(index),
                            '104' if fad == 303 else '362'],
                           check=True, capture_output=True, timeout=15)

    def test_cyber_refuses_a_different_revision_or_original_instruction(self):
        original, _ = self.cyber_fixture()
        for wrong in ('revision', 'instruction', 'source'):
            recipe = copy.deepcopy(original)
            if wrong == 'revision':
                recipe['disc_id'] = (PRODUCT + b'V1.001').decode()
            elif wrong == 'instruction':
                recipe['writes'][0]['before'] = '2121'
            else:
                recipe['tracks'][0]['sha256'] = '0' * 64
            result, pack = self.compile(recipe)
            with self.subTest(wrong=wrong):
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(pack.exists())


class CyberCallbackModelTests(unittest.TestCase):
    def test_original_callback_registers_write_width_and_fixed_continuation(self):
        verify_cyber_handler(0x02008268, CYBER_HANDLER)

    def test_wrong_store_width_delay_slot_target_or_return_are_detected(self):
        for offset, replacement in ((4, '2120'), (10, 'e109'),
                                    (22, 'd004'), (8, '000b')):
            code = bytearray(CYBER_HANDLER)
            code[offset:offset+2] = bytes.fromhex(replacement)
            with self.subTest(offset=offset), self.assertRaises((AssertionError, ValueError)):
                verify_cyber_handler(0x02008268, code)


if __name__ == '__main__':
    unittest.main()
