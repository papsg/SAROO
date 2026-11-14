"""October revision profiles: synthetic sectors and bounded instruction checks.

Adapted from paps-g's October 2 source reviews. No game executable is included;
source-file checks and historical playback limits are recorded separately.
"""
import json
from pathlib import Path
import tempfile
import unittest

import test_sector_overlay as overlay
import test_sector_overlay_later as later
from check_later_callbacks_sh2 import execute, region


class RevisionRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay.SectorOverlayTests.setUpClass()
        cls.fixture = overlay.SectorOverlayTests

    setUp = later.LaterRecipeTests.setUp
    source_fixture = later.LaterRecipeTests.source_fixture
    compile = later.LaterRecipeTests.compile
    check_delivery = later.LaterRecipeTests.check_delivery

    def diehard_fixture(self, name='us'):
        source = json.loads((overlay.ROOT / 'recipes' /
                             f'die-hard-trilogy-{name}-v1000-sector.json').read_text())
        self.assertTrue(source['replaces_callback'])
        self.assertNotIn('cart_mode', source)
        self.assertEqual(len(source['writes']), 18)
        self.assertTrue(all((w['before'], w['after']) == ('6011', 'e004')
                            for w in source['writes']))
        self.assertEqual(len({w['fad'] for w in source['writes']}), 8 if name == 'us' else 12)
        context = {}
        for w in source['writes']:
            context.setdefault(w['fad'], []).append((w['offset']+2, 'c904200889fb'))
        return self.source_fixture(source, b'V1.000', context)

    def test_diehard_us_all_six_executables_and_eighteen_waits(self):
        source = json.loads((overlay.ROOT / 'recipes/die-hard-trilogy-us-v1000-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'T-16103H  V1.000')
        self.assertEqual([(w['fad'], w['offset']) for w in source['writes']], [
            (327, 2042), (328, 898), (328, 1208),
            (423, 770), (423, 1674), (423, 1984),
            (13049, 770), (13049, 1674), (13049, 1984),
            (13846, 1350), (13847, 206), (13847, 516),
            (30023, 770), (30023, 1674), (30023, 1984),
            (34362, 278), (34362, 1170), (34362, 1476)])
        recipe, raws = self.diehard_fixture()
        self.check_delivery(recipe, raws)

    def test_diehard_rejects_wrong_revision_hash_and_instruction(self):
        for wrong in ('version', 'hash', 'instruction'):
            with self.subTest(wrong=wrong), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                recipe, _ = self.diehard_fixture()
                if wrong == 'version':
                    recipe['disc_id'] = (overlay.PRODUCT+b'V1.001').decode()
                elif wrong == 'hash':
                    recipe['tracks'][0]['sha256'] = '0'*64
                else:
                    recipe['writes'][-1]['before'] = '0009'
                result, pack = self.compile(recipe)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(pack.exists())

    def test_ready_wait_preserves_complete_modeled_cpu_state_but_eliminates_read(self):
        address, source_address = 0x06018000, 0x025e7ffe
        original = bytes.fromhex('6011c904200889fb')
        replacement = bytes.fromhex('e004c904200889fb')
        for high in range(256):
            for initial_t in (0, 1):
                memory = region(source_address, 2)
                memory[source_address], memory[source_address+1] = high, 255
                results = []
                for code in (original, replacement):
                    flags, trace = [initial_t], []
                    result = execute(code, address, address+8, memory, flags=flags,
                                     seed=high, registers={1: source_address}, trace=trace)
                    results.append((result, flags, trace))
                self.assertEqual(results[0][:2], results[1][:2])
                self.assertEqual(results[0][0][1][0], 4)
                self.assertEqual(results[0][1], [0])
                self.assertEqual(results[0][2], [('read', source_address, 2, (high << 8) | 255)])
                self.assertEqual(results[1][2], [])
        # A source that never asserts ready keeps waiting; do not hide that
        # the replacement implements a compatibility policy, not synchronization.
        with self.assertRaisesRegex(ValueError, 'instruction bound'):
            execute(original, address, address+8, {source_address: 0, source_address+1: 0},
                    registers={1: source_address})

    def test_croc_changes_only_eight_polygon_mode_bytes_and_preserves_gouraud(self):
        source = json.loads((overlay.ROOT / 'recipes/croc-us-v1000-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'T-5029H-50V1.000')
        self.assertNotIn('cart_mode', source)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']], [
            (196, 1722, '00ac', '00c4'), (197, 1078, '00a8', '00c0'),
            (197, 1356, '00ac', '00c4'), (198, 258, '00a8', '00c0'),
            (198, 548, '00ac', '00c4'), (199, 1390, '00ac', '00c4'),
            (200, 1054, '00ac', '00c4'), (329, 1206, '00a8', '00c0')])
        for w in source['writes']:
            before, after = int(w['before'], 16), int(w['after'], 16)
            self.assertEqual(before & 4, after & 4)  # Gouraud selection
            self.assertEqual(after & 0xc0, 0xc0)  # ECD and SPD
            self.assertEqual(after & 0x38, 0)  # nontextured color mode
            self.assertEqual((before ^ after) & ~0x78, 0)
        recipe, raws = self.source_fixture(source, b'V1.000', {})
        self.check_delivery(recipe, raws)

    def test_five_japanese_exact_revisions_and_native_edits(self):
        cases = [
            ('fighters-megamix-jp-v1001', 'GS-9126   V1.001', 0,
             [(227, 1028, '400b', '0009')]),
            ('fighting-vipers-jp-revc', 'GS-9101   V1.013', 0,
             [(240, 624, '400b', '0009')]),
            ('metal-slug-jp-v1002', 'T-3111G   V1.002', 1,
             [(337, 252, '2122', '0009')]),
            ('manx-tt-superbike-jp-v1001', 'GS-9102   V1.001', 0,
             [(206, 451, '0f', '03'), (206, 503, '0f', '03'),
              (203, 909, '4f', '43'), (203, 1037, '4f', '43'),
              (203, 1257, '4f', '43'), (203, 1577, '1f', '13')]),
            ('whizz-jp', 'T-36102G  V1.000', 0, [(152, 419, '09', '08')])]
        for name, identity, mode, expected in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                source = json.loads((overlay.ROOT / 'recipes' / f'{name}-sector.json').read_text())
                self.assertEqual(source['disc_id'], identity)
                self.assertEqual(source.get('cart_mode', 0), mode)
                self.assertTrue(source['replaces_callback'])
                self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                                  for w in source['writes']], expected)
                contexts = ({152: [(418, 'e1')]} if name == 'whizz-jp' else
                            {w['fad']: [(w['offset']+2, '0009')] for w in source['writes']}
                            if name.startswith(('fighters-', 'fighting-')) else {})
                recipe, raws = self.source_fixture(source, identity[10:].encode(), contexts)
                self.check_delivery(recipe, raws)

    def test_vipers_reva_no_edit_control_refuses_revc_source(self):
        source = json.loads((overlay.ROOT / 'recipes/fighting-vipers-jp-revc-sector.json').read_text())
        recipe, _ = self.source_fixture(source, b'V1.010', {240: [(624, 'd13b')]})
        # Even with synthetic matching FILE hashes the reviewed preimage is absent.
        result, pack = self.compile(recipe)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(pack.exists())

    def test_manx_register_copy_keeps_full_width_and_flags_for_every_register_pair(self):
        values = (0, 1, 0x7fff, 0x8000, 0xffff, 0x10000, 0x12348000, 0xffffffff)
        for n in range(16):
            for m in range(16):
                for value in values:
                    for incoming_t in (0, 1):
                        for low in (0xf, 3):
                            code = (0x6000 | n << 8 | m << 4 | low).to_bytes(2, 'big')
                            flags = [incoming_t]
                            before, after, pr, memory, writes, calls = execute(
                                code, 0x06018000, 0x06018002, {}, flags=flags,
                                registers={m: value})
                            expected = before.copy()
                            expected[n] = (value if low == 3 else
                                           ((value & 0x7fff) - (value & 0x8000)) & 0xffffffff)
                            self.assertEqual(after, expected)
                            self.assertEqual(flags, [incoming_t])
                            self.assertEqual((pr, memory, writes, calls), (0x06018002, {}, [], []))

    def test_whizz_changes_only_the_staged_clipping_command(self):
        for initial_t in (0, 1):
            for seed in (0, 127, 255):
                for command in (8, 9):
                    flags = [initial_t]
                    code = bytes([0xe1, command, 0x28, 0x11])
                    ram = region(0x06003400, 32)
                    before, after, pr, memory, writes, calls = execute(
                        code, 0x06003192, 0x06003196, ram, seed=seed,
                        registers={8: 0x06003408}, flags=flags)
                    before[1] = command
                    self.assertEqual(after, before)
                    expected = dict(ram)
                    expected.update({0x06003408: 0, 0x06003409: command})
                    self.assertEqual(memory, expected)
                    self.assertEqual(writes, [(0x06003408, 2, command)])
                    self.assertEqual((pr, flags, calls), (0x06003196, [initial_t], []))

    def test_six_direct_revision_profiles_use_separate_full_identity_keys(self):
        cases = [
            ('umk3-eu-v1007', 'T-25403H  V1.007',
             [(221, 1898, '480b', '0009'), (221, 1982, '480b', '0009')]),
            ('umk3-eu-reva-v2000', 'T-25403H  V2.000',
             [(221, 1914, '480b', '0009'), (221, 1998, '480b', '0009')]),
            ('hebereke-popoitto-jp', 'T-1504G   V1.002',
             [(198, 905, '01', '56'), (198, 773, '01', '56')]),
            ('super-real-mahjong-p7-special-jp', 'T-16510G  V1.003',
             [(176, 536, '2232', '0009')]),
            ('hissatsu-pachinko-jp-v1003', 'T-1503G   V1.003', [(174, 157, '01', '78')]),
            ('heart-of-darkness-jp-proto', '999999999 V1.000',
             [(172, 208, '0400', '0240'), (171, 1748, '2102', '0009')])]
        keys = set()
        for name, identity, expected in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                source = json.loads((overlay.ROOT / 'recipes' / f'{name}-sector.json').read_text())
                self.assertEqual(source['disc_id'], identity)
                self.assertTrue(source['replaces_callback'])
                self.assertNotIn('cart_mode', source)
                self.assertNotIn(identity, keys)
                keys.add(identity)
                self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                                  for w in source['writes']], expected)
                contexts = {}
                if name.startswith('umk3-'):
                    contexts = {221: [(w['offset']+2, 'e408') for w in source['writes']]}
                elif name.startswith(('hissatsu-', 'hebereke-')):
                    for w in source['writes']:
                        contexts.setdefault(w['fad'], []).append((w['offset']-1, '88'))
                elif name.startswith('heart-'):
                    contexts = {172: [(210, '0000')], 171: [(1750, 'e0001102')]}
                recipe, raws = self.source_fixture(source, identity[10:].encode(), contexts)
                self.check_delivery(recipe, raws)

    def test_status_compare_changes_only_t_for_all_status_bytes(self):
        for immediate in (1, 0x56, 0x78):
            for status in range(256):
                for incoming_t in (0, 1):
                    flags = [incoming_t]
                    before, after, pr, memory, writes, calls = execute(
                        bytes([0x88, immediate]), 0x06018000, 0x06018002, {},
                        registers={0: status}, flags=flags)
                    self.assertEqual(after, before)
                    self.assertEqual(flags, [int(status == immediate)])
                    self.assertEqual((pr, memory, writes, calls), (0x06018002, {}, [], []))

    def test_umk3_keeps_original_argument_instruction_without_calling_cdon_helper(self):
        address, helper, incoming_pr = 0x0601875a, 0x06017a3c, 0x06012340
        for t in (0, 1):
            flags = [t]
            before, after, pr, ram, writes, calls = execute(
                bytes.fromhex('0009e408'), address, address+4, {},
                registers={8: helper}, return_address=incoming_pr, flags=flags)
            before[4] = 8
            self.assertEqual(after, before)
            self.assertEqual((pr, ram, writes, calls, flags), (incoming_pr, {}, [], [], [t]))
            result = execute(bytes.fromhex('480be408'), address, address+4, {},
                             registers={8: helper}, external=helper, flags=[t])
            self.assertEqual(result[-1], [(8, result[0][5])])

    def test_heart_keeps_aref_clear_while_omitting_only_asr_store(self):
        for t in (0, 1):
            for value in (0, 0x1ff0, 0x23301ff0, 0xffffffff):
                for store in (0x2102, 9):
                    ram = region(0x25fe00b0, 16)
                    code = store.to_bytes(2, 'big') + bytes.fromhex('e0001102')
                    flags = [t]
                    before, after, pr, memory, writes, calls = execute(
                        code, 0x060106c4, 0x060106ca, ram, flags=flags,
                        registers={0: value, 1: 0x25fe00b0})
                    before[0] = 0
                    self.assertEqual(after, before)
                    expected = dict(ram)
                    if store != 9:
                        expected.update(enumerate(value.to_bytes(4, 'big'), 0x25fe00b0))
                    expected.update(enumerate(bytes(4), 0x25fe00b8))
                    self.assertEqual(memory, expected)
                    self.assertEqual(writes, ([(0x25fe00b0, 4, value)] if store != 9 else []) +
                                     [(0x25fe00b8, 4, 0)])
                    self.assertEqual((pr, flags, calls), (0x060106ca, [t], []))

    def test_manx_europe_and_existing_usa_coexist_with_distinct_version_keys(self):
        eu = json.loads((overlay.ROOT / 'recipes/manx-tt-superbike-eu-v1000-sector.json').read_text())
        us = json.loads((overlay.ROOT / 'recipes/manx-tt-superbike-us-sector.json').read_text())
        self.assertEqual(eu['disc_id'], 'MK-81210  V1.000')
        self.assertEqual(us['disc_id'], 'MK-81210  V1.001')
        self.assertNotEqual(eu['tracks'], us['tracks'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after']) for w in eu['writes']], [
            (206, 451, '0f', '03'), (206, 503, '0f', '03'),
            (203, 909, '4f', '43'), (203, 1037, '4f', '43'),
            (203, 1257, '4f', '43'), (203, 1577, '1f', '13')])
        for source in (eu, us):
            with self.subTest(identity=source['disc_id']), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                self.assertTrue(source['replaces_callback'])
                self.assertNotIn('cart_mode', source)
                recipe, raws = self.source_fixture(source, source['disc_id'][10:].encode(), {})
                self.check_delivery(recipe, raws)

    def test_diehard_japan_covers_all_eighteen_waits_and_twelfth_sector(self):
        source = json.loads((overlay.ROOT / 'recipes/die-hard-trilogy-jp-v1000-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'GS-9123   V1.000')
        self.assertEqual([(w['fad'], w['offset']) for w in source['writes']], [
            (330, 1786), (331, 654), (331, 964),
            (423, 1094), (423, 2010), (424, 272),
            (12907, 1094), (12907, 2010), (12908, 272),
            (13704, 1674), (13705, 542), (13705, 852),
            (29881, 1098), (29881, 2014), (29882, 276),
            (34220, 958), (34220, 1850), (34221, 108)])
        recipe, raws = self.diehard_fixture('jp')
        self.check_delivery(recipe, raws)


if __name__ == '__main__':
    unittest.main()
