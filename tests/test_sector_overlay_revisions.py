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


if __name__ == '__main__':
    unittest.main()
