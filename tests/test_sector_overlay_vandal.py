"""Exact DLCT native geometry and production-C lifetime using synthetic sectors.

No game/BIOS image is distributed. Source-native execution uses the separate
clean-source interpreter check documented in tools/README.md.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_sector_overlay as overlay
import test_sector_overlay_later as later
from check_vandal_stub_sh2 import STUB


class VandalNativeRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay.SectorOverlayTests.setUpClass()
        cls.fixture = overlay.SectorOverlayTests

    setUp = later.LaterRecipeTests.setUp
    source_fixture = later.LaterRecipeTests.source_fixture
    compile = later.LaterRecipeTests.compile

    def source(self):
        return json.loads((overlay.ROOT / 'recipes/vandal-hearts-cn-dlct-v20240517-sector.json').read_text())

    def fixture_source(self):
        return self.source_fixture(self.source(), b'V1.002',
                                   {150: [], 152: [], 292307: [], 292308: []})

    def test_exact_second_predicate_and_native_source_geometry(self):
        source = self.source()
        self.assertEqual(source['disc_id'], 'T-1219G   V1.002')
        self.assertEqual(source['product_id'], 'T-1219G')
        self.assertEqual(source['tracks'], [{'number': 1, 'sha256':
            '0e9361f5b1d130af6b513a8a80ad9f4d872b14669655159e77ff7ee339a21178'}])
        self.assertTrue(source['replaces_callback'])
        self.assertNotIn('cart_mode', source)
        self.assertEqual(source['expire_after_fad'], 292308)
        self.assertEqual([guard['fad'] for guard in source['guards']], [150, 152, 292307])
        aip, packed = source['writes']
        before, after = bytes.fromhex(aip['before']), bytes.fromhex(aip['after'])
        self.assertEqual((len(before), len(after)), (122, 122))
        self.assertEqual((aip['fad'], aip['offset']), (151, 1808))
        self.assertEqual(0x06002000 + (aip['fad'] - 150) * 2048 + aip['offset'] - 16,
                         0x06002f00)
        self.assertEqual(before[0x5e:0x60], bytes.fromhex('4601'))
        self.assertEqual(before[0x70:0x72], bytes.fromhex('4c2b'))
        self.assertNotIn(STUB, after)
        self.assertEqual((packed['fad'], packed['offset'], packed['before'], packed['after']),
                         (292306, 1766, '2202', '0009'))
        # Original reader geometry, independent of the recipe's source offsets.
        self.assertEqual(292214 + 95 - 1, source['expire_after_fad'])
        self.assertEqual(95 * 2048, 0x2f800)
        self.assertEqual(0x2e600 + 0xd6,
                         (packed['fad'] - 292214) * 2048 + packed['offset'] - 16)
        self.assertEqual(0x06002f00 + len(after), 0x06002f7a)

    def test_actual_fads_fragment_atomicity_retries_and_one_time_lifetime(self):
        recipe, raws = self.fixture_source()
        result, pack = self.compile(recipe)
        self.assertEqual(result.returncode, 0, result.stderr)
        encoded = pack.read_bytes()
        self.assertEqual(encoded[4:8], bytes([6, 8, 0, 0x80]))
        self.assertEqual(len(encoded), 2808)
        self.assertEqual([int.from_bytes(encoded[i:i + 4], 'big')
                          for i in range(24, len(encoded), 348)],
                         [150, 151, 151, 151, 152, 292306, 292307, 292308])
        paths = []
        for fad in (151, 292306):
            raw = raws[fad]
            post = bytearray(raw)
            for write in recipe['writes']:
                if write['fad'] == fad:
                    value = bytes.fromhex(write['after'])
                    post[write['offset']:write['offset'] + len(value)] = value
            for suffix, data in [('original', raw), ('post', overlay.canonical(post))]:
                path = self.path / f'{fad}-{suffix}'
                path.write_bytes(data)
                paths.append(path)
        marker = self.path / 'completion'
        marker.write_bytes(raws[292308])
        subprocess.run([self.fixture.decoder, 'finite', pack, *paths, marker],
                       check=True, capture_output=True, timeout=15)

    def test_wrong_revision_track_body_short_source_and_early_completion_refuse(self):
        for wrong in ('version', 'track', 'body', 'short', 'marker'):
            with self.subTest(wrong=wrong), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                recipe, raws = self.fixture_source()
                if wrong == 'version':
                    recipe['disc_id'] = (overlay.PRODUCT + b'V1.003').decode()
                elif wrong == 'track':
                    recipe['tracks'][0]['sha256'] = '0' * 64
                elif wrong == 'body':
                    # Same product/revision never suffices for an unsupported native body.
                    recipe['writes'][0]['before'] = '00' * 122
                elif wrong == 'short':
                    path = self.path / '292306.bin'
                    path.write_bytes(raws[292306][:-1])
                    number = list(raws).index(292306) + 1
                    pin = next(track for track in recipe['tracks'] if track['number'] == number)
                    pin['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
                else:
                    recipe['expire_after_fad'] = 152
                result, pack = self.compile(recipe)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(pack.exists())

    def test_untouched_guards_bind_ip_version_callees_and_complete_packed_source(self):
        recipe, raws = self.fixture_source()
        result, pack = self.compile(recipe)
        self.assertEqual(result.returncode, 0, result.stderr)
        for fad, record, damaged in [(150, 0, 63), (152, 4, 2063), (292307, 6, 2063)]:
            with self.subTest(fad=fad):
                path = self.path / f'guard-{fad}.bin'
                path.write_bytes(raws[fad])
                subprocess.run([self.fixture.decoder, 'guard', pack, path, path,
                                str(record), str(damaged)], check=True,
                               capture_output=True, timeout=15)

    def test_both_inherited_product_routes_remain_available(self):
        source = (overlay.ROOT / 'Firm_Saturn/game_patch.c').read_bytes()
        for product in (b'T-9526G', b'T-1219G'):
            line = next(line for line in source.splitlines() if b'{"' + product + b'"' in line)
            self.assertIn(b'VANDAL_HEARTS_CN_patch', line)
        self.assertIn('rationale', self.source())
