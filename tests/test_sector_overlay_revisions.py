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


if __name__ == '__main__':
    unittest.main()
