"""Later-load recipes: generated source sectors and actual production decoder.

Sparse original instruction words describe the reviewed edits; these fixtures
contain no game image. The SH-2 reference bytes are SAROO's own callback.
"""
import copy
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
import zlib

from check_later_callbacks_sh2 import (execute, region, verify_cyber_handler,
                                       verify_groove_handler, verify_habitat_handler,
                                       verify_marvel_handler, verify_phantasy_handler,
                                       verify_vampire_handler)
from test_sector_overlay import ROOT, PRODUCT, canonical, sector, seal
import test_sector_overlay as overlay_fixture


CYBER_HANDLER = bytes.fromhex(
    'd103e2092121d103412b0009000b00090601695a0600d000')
GROOVE_HANDLER = bytes.fromhex(
    'd205e1092211d2052211d105d2052212412b0009000b0009'
    '0600833806099a840600400000280008')
HABITAT_HANDLER = bytes.fromhex(
    'd108601088118f037101e209a0052121d105601088118df87101d104412b0009'
    '000b00090602a15d0602b52d06020000')
MARVEL_HANDLER = bytes.fromhex(
    'd20b93126121611d31308f01e1092211d208930b6121611d31308f01e1092211'
    'd105412b0009000b00092012210200090600286e0600205a06002000')
PHANTASY_HANDLER = bytes.fromhex(
    'd104d205631233208b01d2042122d104412b00090602345c06021b580600083c06020000')
VAMPIRE_HANDLER = bytes.fromhex(
    'd103e2092121d103412b0009000b000906014a520600d000')


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
            raw = bytearray(raws[fad] if fad in raws else sector(fad))
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
            if track > 1 or fad in fads:
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

    def marvel_fixture(self):
        source = json.loads((ROOT / 'recipes/marvel-vs-street-fighter-jp-v1000-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'T-1238G   V1.000')
        self.assertEqual(source.get('cart_mode', 0), 0)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']],
                         [(184, 106, '2102', '0009'), (1256, 126, '2012', '0009')])
        self.assertEqual([g['fad'] for g in source['guards']], [947, 1255])
        for w, lba, target in zip(source['writes'], [34, 1105], [0x0600205a, 0x0600286e]):
            self.assertEqual(0x06002000 + (w['fad']-150-lba)*2048 + w['offset']-16, target)
        return self.source_fixture(source, b'V1.000',
                                   {947: [(26, 'd120410b0009'), (44, 'd11b412b0009'),
                                          (156, '06002000'), (308, '00000000')],
                                    1255: [(106, '2fc6')]})

    def compile(self, recipe):
        source = self.path / 'recipe.json'
        source.write_text(json.dumps(recipe))
        output = self.path / f'{zlib.crc32(recipe["disc_id"].encode()):08X}.SOV'
        result = subprocess.run([self.fixture.tool, 'compile', source,
                                 self.path / 'fixture.cue', output],
                                capture_output=True, text=True, timeout=30)
        return result, output

    def phantasy_fixture(self):
        source = json.loads((ROOT / 'recipes/phantasy-star-collection-jp-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'GS-9186   V1.003')
        self.assertEqual(source.get('cart_mode', 0), 0)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']], [(501, 1076, '410b', '0009')])
        self.assertEqual([g['fad'] for g in source['guards']], [177])
        self.assertEqual(0x06020000 + (501-150-345)*2048 + 1076-16, 0x06023424)
        return self.source_fixture(source, b'V1.003',
                                   {177: [(904, 'd118410b0009'), (1004, '06020000')],
                                    501: [(1074, 'd10e410b00094f266af669f6000b68f60009'),
                                          (1132, '06021b58')]})

    def vampire_fixture(self):
        source = json.loads((ROOT / 'recipes/vampire-savior-jp-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'T-1229G   V1.000')
        self.assertEqual(source.get('cart_mode', 0), 0)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']],
                         [(18022, 610, '2012', '0009'), (180, 738, '6232', '6233'),
                          (180, 968, '060ddb30', '0600d000'), (188, 1082, '6232', '6233'),
                          (188, 1164, '060ddb68', '0600d000')])
        self.assertEqual([g['fad'] for g in source['guards']], [231, 303])
        for w, lba, base, target in zip(source['writes'], [17857, 30, 30, 30, 30],
                                       [0x0600d000, *([0x060a0000]*4)],
                                       [0x06014a52, 0x060a02d2, 0x060a03b8,
                                        0x060a442a, 0x060a447c]):
            self.assertEqual(base + (w['fad']-150-lba)*2048 + w['offset']-16, target)
        return self.source_fixture(source, b'V1.000',
                                   {180: [(734, 'd33a4f266232422b6ef6')],
                                    188: [(1078, 'd3154f266232422b6ef6')],
                                    231: [(2028, '562e42494e00')],
                                    303: [(832, '0600d000'), (888, '0600d000')]})

    def test_cyber_delivers_only_the_later_nop_and_keeps_original_dispatch(self):
        recipe, raws = self.cyber_fixture()
        self.check_delivery(recipe, raws)

    def test_groove_delivers_both_program_edits_without_redirecting_the_call(self):
        recipe, raws = self.groove_fixture()
        self.check_delivery(recipe, raws)

    def test_habitat_binds_the_original_entry_and_other_shared_predicate(self):
        recipe, raws = self.habitat_fixture()
        self.check_delivery(recipe, raws)

    def test_marvel_covers_both_loaded_programs_and_preserves_both_handoffs(self):
        recipe, raws = self.marvel_fixture()
        self.check_delivery(recipe, raws)

    def test_phantasy_changes_only_the_call_and_keeps_native_pointer_and_epilogue(self):
        recipe, raws = self.phantasy_fixture()
        self.check_delivery(recipe, raws)

    def test_vampire_delivers_both_fixed_handoffs_and_the_later_nop(self):
        recipe, raws = self.vampire_fixture()
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

    def test_source_verifier_checks_complete_files_and_reports_missing_guards(self):
        recipe, raws = self.groove_fixture()
        result, pack = self.compile(recipe)
        self.assertEqual(result.returncode, 0, result.stderr)

        def pin(path):
            return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

        manifest = {'recipe': pin(self.path / 'recipe.json'), 'pack': pin(pack),
                    'decoder': pin(self.fixture.decoder), 'programs': []}
        images = []
        for index, (lba, count) in enumerate(((24, 8), (152, 300))):
            original = bytearray([0x71]) * (2048 * count)
            for fad, raw in raws.items():
                offset = (fad - 150 - lba) * 2048
                if 0 <= offset < len(original):
                    original[offset:offset+2048] = raw[16:2064]
            post = bytearray(original)
            for w in recipe['writes']:
                offset = (w['fad'] - 150 - lba) * 2048 + w['offset'] - 16
                if 0 <= offset < len(post):
                    value = bytes.fromhex(w['after'])
                    post[offset:offset+len(value)] = value
            source = self.path / f'source-{index}.bin'
            source.write_bytes(original)
            manifest['programs'].append({'source': pin(source), 'lba': lba,
                                         'postimage_sha256': hashlib.sha256(post).hexdigest()})
            images.append((original, post))

        def verify(value, destination):
            source = self.path / 'manifest.json'
            source.write_text(json.dumps(value))
            return subprocess.run([self.fixture.tool, 'verify-source', source, destination],
                                  capture_output=True, text=True, timeout=60)

        output = self.path / 'verified'
        result = verify(manifest, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads((output / 'receipt.json').read_text())
        self.assertTrue(receipt['complete'] and receipt['all_target_sources_verified'])
        self.assertFalse(receipt['full_track_identity_verified'])
        self.assertEqual(receipt['unverified_guard_fads'], [])
        self.assertEqual([p['edited_sectors'] for p in receipt['programs']], [2, 2])
        saved = (output / 'receipt.json').read_bytes()
        self.assertNotEqual(verify(manifest, output).returncode, 0)
        self.assertEqual((output / 'receipt.json').read_bytes(), saved)

        partial = copy.deepcopy(manifest)
        source = self.path / 'partial.bin'
        source.write_bytes(images[0][0][2048:])
        partial['programs'][0] = {'source': pin(source), 'lba': 25,
                                  'postimage_sha256': hashlib.sha256(images[0][1][2048:]).hexdigest()}
        partial_out = self.path / 'partial'
        result = verify(partial, partial_out)
        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads((partial_out / 'receipt.json').read_text())
        self.assertTrue(receipt['complete'])
        self.assertFalse(receipt['all_target_sources_verified'])
        self.assertEqual(receipt['unverified_guard_fads'], [174])

        for wrong in ('source', 'postimage', 'overlap', 'missing', 'overflow', 'unrelated',
                      'decoder', 'pack', 'policy', 'disc-id'):
            value = copy.deepcopy(manifest)
            if wrong == 'source':
                value['programs'][0]['source']['sha256'] = '0' * 64
            elif wrong == 'postimage':
                value['programs'][0]['postimage_sha256'] = '0' * 64
            elif wrong == 'overlap':
                value['programs'][1]['lba'] = 24
            elif wrong == 'missing':
                value['programs'].pop()
            elif wrong == 'overflow':
                value['programs'][0]['lba'] = 0xffffffff
            elif wrong == 'decoder':
                value['decoder']['sha256'] = '0' * 64
            elif wrong == 'pack':
                data = bytearray(pack.read_bytes())
                data[24 + 348 + 20] ^= 1
                seal(data)
                path = self.path / 'wrong.SOV'
                path.write_bytes(data)
                value['pack'] = pin(path)
            elif wrong in ('policy', 'disc-id'):
                data = copy.deepcopy(recipe)
                if wrong == 'policy':
                    data['replaces_callback'] = False
                else:
                    data['disc_id'] = 'wrong'
                path = self.path / ('wrong-' + wrong + '.json')
                path.write_text(json.dumps(data))
                value['recipe'] = pin(path)
            else:
                data = bytearray(images[0][0])
                data[-1] ^= 1
                path = self.path / 'corrupt.bin'
                path.write_bytes(data)
                value['programs'][0]['source'] = pin(path)
            target = self.path / ('refused-' + wrong)
            with self.subTest(wrong=wrong):
                self.assertNotEqual(verify(value, target).returncode, 0)
                self.assertFalse(target.exists())

        # An explicit failing child is a reporter control, not a source decoder.
        c = self.path / 'reject.c'
        c.write_text('int main(void) { return 1; }\n')
        binary = self.path / ('reject.exe' if os.name == 'nt' else 'reject')
        subprocess.run([*shlex.split(os.environ.get('CC', 'cc')), c, '-o', binary],
                       check=True, capture_output=True, timeout=60)
        failing = copy.deepcopy(manifest)
        failing['decoder'] = pin(binary)
        failed_out = self.path / 'failed-child'
        self.assertNotEqual(verify(failing, failed_out).returncode, 0)
        self.assertFalse(json.loads((failed_out / 'receipt.json').read_text())['complete'])

    def check_delivery(self, recipe, raws):
        result, pack = self.compile(recipe)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = pack.read_bytes()
        header = data[:24]
        targets = sorted({w['fad'] for w in recipe['writes']} |
                         {g['fad'] for g in recipe.get('guards', [])})
        expected_fads = [fad for fad in targets for _ in range(max(
            [1] + [(len(w['before'])//2+43)//44 for w in recipe['writes'] if w['fad'] == fad]))]
        record_fads = [int.from_bytes(data[at:at+4], 'big') for at in range(24, len(data), 348)]
        self.assertEqual(record_fads, expected_fads)
        self.assertEqual(header[4:8], bytes([3, len(expected_fads), 0,
                                          0x80 | recipe.get('cart_mode', 0)]))
        for fad in targets:
            index = record_fads.index(fad)
            before = raws[fad]
            after = bytearray(before)
            edits = [w for w in recipe['writes'] if w['fad'] == fad]
            for w in edits:
                data = bytes.fromhex(w['after'])
                after[w['offset']:w['offset']+len(data)] = data
            expected = canonical(after)
            if not edits:
                self.assertEqual(before, expected)
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


class MarvelCallbackModelTests(unittest.TestCase):
    def test_both_original_guarded_writes_are_independent(self):
        verify_marvel_handler(0x020081e4, MARVEL_HANDLER)

    def test_losing_either_write_comparison_or_continuation_is_detected(self):
        for offset, value in ((14, '0009'), (30, '0009'), (24, '0009'), (58, '2004')):
            code = bytearray(MARVEL_HANDLER)
            code[offset:offset+2] = bytes.fromhex(value)
            with self.subTest(offset=offset), self.assertRaises((AssertionError, ValueError)):
                verify_marvel_handler(0x020081e4, code)


    def test_original_loader_retains_call_and_jump_delay_slots(self):
        for start, instructions, literal, expected_pr in (
                (0x060f000a, 'd120410b0009', 0x82, 0x060f0010),
                (0x060f001c, 'd11b412b0009', 0x70, 0x06054320)):
            code = bytearray(literal + 4)
            code[:6] = bytes.fromhex(instructions)
            code[literal:literal+4] = bytes.fromhex('06002000')
            _, regs, pr, _, writes, calls = execute(
                code, start, 0x06002000, {}, return_address=0x06054320)
            self.assertEqual(regs[1], 0x06002000)
            self.assertEqual(pr, expected_pr)
            self.assertFalse(writes or calls)


class PhantasyCallbackModelTests(unittest.TestCase):
    def test_original_pointer_write_is_conditional_and_not_a_resident_stub_call(self):
        verify_phantasy_handler(0x02008588, PHANTASY_HANDLER)

    def test_wrong_guard_write_or_continuation_fails(self):
        for offset, value in ((6, '0009'), (12, '0009'), (26, '1b5c'), (34, '0004')):
            code = bytearray(PHANTASY_HANDLER)
            code[offset:offset+2] = bytes.fromhex(value)
            with self.subTest(offset=offset), self.assertRaises((AssertionError, ValueError)):
                verify_phantasy_handler(0x02008588, code)


class VampireCallbackModelTests(unittest.TestCase):
    def test_original_handler_keeps_the_fixed_entry_and_only_its_inherited_nop(self):
        verify_vampire_handler(0x02008130, VAMPIRE_HANDLER)

    def test_wrong_nop_width_literal_or_entry_is_detected(self):
        for offset, value in ((4, '2120'), (18, '4a56'), (22, 'd004')):
            code = bytearray(VAMPIRE_HANDLER)
            code[offset:offset+2] = bytes.fromhex(value)
            with self.subTest(offset=offset), self.assertRaises((AssertionError, ValueError)):
                verify_vampire_handler(0x02008130, code)

    def test_both_dispatches_restore_pr_and_r14_and_ignore_mutable_pointer_values(self):
        handler_at = 0x02008130
        for start, literal, load, pointer in ((0x060a02ce, 0xea, 'd33a', 0x060ddb30),
                                              (0x060a4426, 0x56, 'd315', 0x060ddb68)):
            for table in (0, 0x0600d000, 0x06012340, 0xffffffff):
                for candidate in (False, True):
                    code = bytearray(literal + 4)
                    # The old menu changes JMP @R2 to JMP @R3 and its literal
                    # to the actual handler. The candidate supplies the fixed
                    # target directly, retaining JMP @R2 and the delay slot.
                    code[:10] = bytes.fromhex(load + '4f26' +
                                             ('6233422b' if candidate else '6232432b') + '6ef6')
                    code[literal:literal+4] = (0x0600d000 if candidate else handler_at).to_bytes(4, 'big')
                    ram = (region(0x06014a40, 32) | dict(enumerate(VAMPIRE_HANDLER, handler_at)) |
                           dict(enumerate(bytes.fromhex('06054320060bcafe'), 0x060ff000)))
                    ram.update(enumerate(table.to_bytes(4, 'big'), pointer))
                    if candidate:
                        ram.update({0x06014a52: 0, 0x06014a53: 9})
                    flags = [1]
                    before, regs, pr, after, writes, calls = execute(
                        code, start, 0x0600d000, ram, flags=flags)
                    self.assertEqual(pr, 0x06054320)
                    self.assertEqual(regs[14:], [0x060bcafe, 0x060ff008])
                    self.assertEqual(regs[3], 0x0600d000 if candidate else handler_at)
                    self.assertEqual(regs[2], 0x0600d000 if candidate else 9)
                    self.assertEqual(regs[1], before[1] if candidate else 0x0600d000)
                    self.assertEqual(regs[:1] + regs[4:14], before[:1] + before[4:14])
                    self.assertEqual(flags, [1])
                    self.assertEqual(writes, [] if candidate else [(0x06014a52, 2, 9)])
                    expected = dict(ram)
                    expected.update({0x06014a52: 0, 0x06014a53: 9})
                    self.assertEqual(after, expected)
                    self.assertFalse(calls)


if __name__ == '__main__':
    unittest.main()
