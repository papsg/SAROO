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

from check_later_callbacks_sh2 import (execute, region, verify_cyber_handler,
                                       verify_groove_handler, verify_habitat_handler)
from test_sector_overlay import ROOT, PRODUCT, canonical, sector
import test_sector_overlay as overlay_fixture


CYBER_HANDLER = bytes.fromhex(
    'd103e2092121d103412b0009000b00090601695a0600d000')
GROOVE_HANDLER = bytes.fromhex(
    'd205e1092211d2052211d105d2052212412b0009000b0009'
    '0600833806099a840600400000280008')
HABITAT_HANDLER = bytes.fromhex(
    'd108601088118f037101e209a0052121d105601088118df87101d104412b0009'
    '000b00090602a15d0602b52d06020000')


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
        return self.source_fixture(source, b'V1.002', {303: [(104, '0600d000')]})

    def groove_fixture(self):
        source = json.loads((ROOT / 'recipes/groove-on-fight-jp-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'T-14411G  V1.001')
        self.assertEqual(source.get('cart_mode', 0), 0)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']],
                         [(175, 596, '2012', '0009'), (179, 1736, '2322', '0009'),
                          (310, 840, '2012', '0009'), (601, 660, '2322', '0009')])
        self.assertEqual([g['fad'] for g in source['guards']], [174])
        # Independent original-file extents bind source offsets to each real
        # callback's destinations, including low Work RAM and a later file.
        locations = []
        for w in source['writes']:
            lba, load = (24, 0x00200000) if w['fad'] < 302 else (152, 0x06004000)
            locations.append(load + (w['fad'] - 150 - lba) * 2048 + w['offset'] - 16)
        self.assertEqual(locations, [0x00200a44, 0x00202eb8, 0x06008338, 0x06099a84])
        return self.source_fixture(source, b'V1.001', {174: [(1136, '06004000')]})

    def source_fixture(self, source, version, guards):
        recipe = copy.deepcopy(source)
        recipe['product_id'] = PRODUCT.decode().strip()
        recipe['disc_id'] = (PRODUCT + version).decode()
        recipe['tracks'] = []
        fads = {w['fad'] for w in source['writes']} | set(guards)
        raws = {150: sector(150, ip=True, version=version)}
        for fad in sorted(fads):
            raw = bytearray(sector(fad))
            edits = [(w['offset'], w['before']) for w in source['writes'] if w['fad'] == fad]
            for offset, value in edits + guards.get(fad, []):
                data = bytes.fromhex(value)
                raw[offset:offset+len(data)] = data
            raws[fad] = canonical(raw)
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

    def habitat_fixture(self):
        source = json.loads((ROOT / 'recipes/habitat-ii-jp-v1004-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'GS-7105   V1.004')
        self.assertEqual(source.get('cart_mode', 0), 0)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']], [(214, 1342, '8b02', '0009')])
        self.assertEqual([g['fad'] for g in source['guards']], [173, 212])
        w = source['writes'][0]
        self.assertEqual(0x06020000 + (w['fad'] - 150 - 42)*2048 + w['offset'] - 16,
                         0x0602b52e)
        return self.source_fixture(source, b'V1.004',
                                   {173: [(1446, '430b0009'), (1476, '06020000')],
                                    212: [(365, 'f4')]})

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
        self.check_delivery(recipe, raws)

    def test_groove_delivers_both_program_edits_without_redirecting_the_call(self):
        recipe, raws = self.groove_fixture()
        self.check_delivery(recipe, raws)

    def test_habitat_binds_the_original_entry_and_other_shared_predicate(self):
        recipe, raws = self.habitat_fixture()
        self.check_delivery(recipe, raws)

    def test_streamed_program_keeps_guard_and_unrelated_sectors_unchanged(self):
        recipe, raws = self.groove_fixture()
        result, pack = self.compile(recipe)
        self.assertEqual(result.returncode, 0, result.stderr)
        original, expected = bytearray(), bytearray()
        for fad in range(174, 182):
            raw = raws[fad] if fad in raws else sector(fad)
            original.extend(raw)
            post = bytearray(raw)
            for w in recipe['writes']:
                if w['fad'] == fad:
                    value = bytes.fromhex(w['after'])
                    post[w['offset']:w['offset']+len(value)] = value
            expected.extend(canonical(post))
        (self.path / 'original.raw').write_bytes(original)
        (self.path / 'expected.raw').write_bytes(expected)
        subprocess.run([self.fixture.decoder, 'stream', pack, self.path / 'original.raw',
                        self.path / 'expected.raw', '174', '2'],
                       check=True, capture_output=True, timeout=15)
        # A bad byte in the final unrelated sector must also fail the oracle.
        expected[-1] ^= 1
        (self.path / 'expected.raw').write_bytes(expected)
        result = subprocess.run([self.fixture.decoder, 'stream', pack,
                                 self.path / 'original.raw', self.path / 'expected.raw',
                                 '174', '2'], capture_output=True, timeout=15)
        self.assertNotEqual(result.returncode, 0)

    def check_delivery(self, recipe, raws):
        result, pack = self.compile(recipe)
        self.assertEqual(result.returncode, 0, result.stderr)
        header = pack.read_bytes()[:24]
        self.assertEqual(header[4:8], bytes([3, len(raws) - 1, 0,
                                          0x80 | recipe.get('cart_mode', 0)]))
        for index, fad in enumerate(sorted(set(raws) - {150})):
            before = raws[fad]
            after = bytearray(before)
            edits = [w for w in recipe['writes'] if w['fad'] == fad]
            for w in edits:
                data = bytes.fromhex(w['after'])
                after[w['offset']:w['offset']+len(data)] = data
            expected = canonical(after)
            self.assertEqual(before[104:108], expected[104:108])
            (self.path / 'raw').write_bytes(before)
            (self.path / 'post').write_bytes(expected)
            subprocess.run([self.fixture.decoder, 'apply' if edits else 'guard',
                            pack, self.path / 'raw', self.path / 'post', str(index),
                            str(edits[0]['offset'] if edits else 104)],
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


class GrooveCallbackModelTests(unittest.TestCase):
    def test_original_loader_delay_slot_stores_the_selected_entry(self):
        # Reviewed 1ST.BIN call sequence; literal values are supplied here.
        # It explains why the pointer repair is needed by the old redirection
        # and is already correct with the original, unredirected game target.
        for target in (0x06004000, 0x020080e4):
            code = bytearray(0x62)
            code[:10] = bytes.fromhex('d316d2166133410b2232')
            code[0x5a:0x5e] = target.to_bytes(4, 'big')
            code[0x5e:0x62] = bytes.fromhex('00280008')
            ram = region(0x00280000, 32)
            _, regs, pr, _, writes, calls = execute(
                code, 0x00200406, target, ram, return_address=0x06054320)
            self.assertEqual(writes, [(0x00280008, 4, target)])
            self.assertEqual(regs[1:4], [target, 0x00280008, target])
            self.assertEqual(pr, 0x00200410)
            self.assertFalse(calls)

    def test_original_handler_restores_the_native_delay_slot_value(self):
        verify_groove_handler(0x020080e4, GROOVE_HANDLER)

    def test_wrong_low_ram_repair_or_continuation_is_rejected(self):
        for offset in (35, 39):
            code = bytearray(GROOVE_HANDLER)
            code[offset] ^= 4
            with self.subTest(offset=offset), self.assertRaises((AssertionError, ValueError)):
                verify_groove_handler(0x020080e4, code)


class HabitatCallbackModelTests(unittest.TestCase):
    def test_shared_handler_keeps_predicate_priority_writes_and_flags(self):
        verify_habitat_handler(0x020082d4, HABITAT_HANDLER)

    def test_changed_predicate_delay_slot_or_continuation_is_rejected(self):
        for offset in (5, 21, 25, 47):
            code = bytearray(HABITAT_HANDLER)
            code[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises((AssertionError, ValueError)):
                verify_habitat_handler(0x020082d4, code)

    def test_native_check_keeps_modem_read_comparison_and_return_flags(self):
        original = bytearray(46)
        original[:22] = bytes.fromhex('d00a6000600c88118b02e001000b0009e000000b0009')
        original[42:46] = bytes.fromhex('25885029')
        patched = bytearray(original)
        patched[8:10] = bytes.fromhex('0009')
        for modem in range(256):
            for initial_t in (0, 1):
                for code, expected in ((original, int(modem == 0x11)), (patched, 1)):
                    flags = [initial_t]
                    before, regs, pr, after, writes, calls = execute(
                        code, 0x0602b526, 0x06054320, {0x25885029: modem}, flags=flags)
                    self.assertEqual(regs[0], expected)
                    self.assertEqual(regs[1:], before[1:])
                    self.assertEqual(flags, [int(modem == 0x11)])
                    self.assertEqual(pr, 0x06054320)
                    self.assertEqual(after, {0x25885029: modem})
                    self.assertFalse(writes or calls)


if __name__ == '__main__':
    unittest.main()
