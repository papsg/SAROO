"""Finite native-source edits: delivery, completion and launch ownership."""
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

import test_sector_overlay as overlay

ROOT = Path(__file__).resolve().parents[1]


class FiniteOverlayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay.SectorOverlayTests.setUpClass()
        cls.fixture = overlay.SectorOverlayTests
        cls.previous = cls.fixture.output / 'previous-decoder'
        source = cls.fixture.output / 'previous.c'
        source.write_bytes(subprocess.check_output(
            ['git', '-C', ROOT, 'show', '8ee7dad:Firm_MCU/Saturn/sector_overlay.c']))
        flags = ['-O2'] if os.environ.get('SAROO_OVERLAY_PROFILE') == 'release' else [
            '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run([*shlex.split(os.environ.get('CC', 'cc')), '-std=c11',
                        '-Wall', '-Wextra', '-Werror', *flags, '-I', ROOT / 'Firm_MCU/Saturn',
                        ROOT / 'tests/sector_overlay.c', source, '-o', cls.previous],
                       check=True, capture_output=True, timeout=60)

    def setUp(self):
        folder = tempfile.TemporaryDirectory(dir=self.fixture.output)
        self.addCleanup(folder.cleanup)
        self.path = Path(folder.name)
        self.raws = [overlay.sector(fad, ip=fad == 150) for fad in range(150, 158)]
        track = self.path / 'source.bin'
        track.write_bytes(b''.join(self.raws))
        self.cue = self.path / 'source.cue'
        self.cue.write_text('FILE "source.bin" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n')
        self.recipe = {
            'abi': 2, 'product_id': 'T-TEST', 'origin': 'Public synthetic lifetime check',
            'title': 'Two source regions', 'summary': 'Keep both edits until completion',
            'rationale': 'Exercise split edits, a later source and persistent launch ownership',
            'replaces_callback': True, 'expire_after_fad': 155,
            'tracks': [{'number': 1, 'sha256': hashlib.sha256(track.read_bytes()).hexdigest()}],
            'guards': [{'fad': 153, 'reason': 'Preflight identity without required playback'}],
            'writes': [
                {'fad': 151, 'offset': 1808, 'before': '71' * 122, 'after': '42' * 122,
                 'effect': 'Replace a native routine', 'reason': 'Three atomic fragments'},
                {'fad': 154, 'offset': 64, 'before': '7171', 'after': '0009',
                 'effect': 'Change one later source word', 'reason': 'Completion must wait for it'},
            ],
        }

    def compile(self, recipe=None, success=True):
        recipe = self.recipe if recipe is None else recipe
        source = self.path / 'recipe.json'
        source.write_text(json.dumps(recipe))
        pack = self.path / f'{zlib.crc32(overlay.PRODUCT):08X}.SOV'
        if pack.exists():
            pack.unlink()
        result = subprocess.run([self.fixture.tool, 'compile', source, self.cue, pack],
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        if not success:
            self.assertFalse(pack.exists())
        return pack

    def invoke(self, *args, previous=False):
        result = subprocess.run([self.previous if previous else self.fixture.decoder, *args], capture_output=True,
                                text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_fragments_later_source_retry_early_marker_rearm_and_action_lifetime(self):
        for action in (0, 1, 4):
            with self.subTest(action=action):
                self.recipe['cart_mode'] = action
                pack = self.compile()
                encoded = pack.read_bytes()
                self.assertEqual(encoded[4:8], bytes([6, 6, 0, action | 0x80]))
                self.assertEqual(encoded[-overlay.RECORD_BYTES + 8:],
                                 b'\0\1' + bytes(overlay.RECORD_BYTES - 10))
                paths = []
                for fad in (151, 154):
                    original = self.raws[fad - 150]
                    post = bytearray(original)
                    for write in self.recipe['writes']:
                        if write['fad'] == fad:
                            value = bytes.fromhex(write['after'])
                            post[write['offset']:write['offset'] + len(value)] = value
                    for name, data in [('original', original), ('post', overlay.canonical(post))]:
                        path = self.path / f'{fad}-{name}'
                        path.write_bytes(data)
                        paths.append(path)
                marker = self.path / 'marker'
                marker.write_bytes(self.raws[5])
                self.invoke('finite', pack, *paths, marker)

    def test_compiler_refuses_invalid_boundaries_and_incompatible_services(self):
        for boundary in (149, 150, 151, 153, 154, 450000, -1, True, '155'):
            with self.subTest(boundary=boundary):
                recipe = copy.deepcopy(self.recipe)
                recipe['expire_after_fad'] = boundary
                self.compile(recipe, success=False)
        for extra in ({'replaces_callback': False},
                      {'first_read_guard': {'sha256': '0' * 64, 'reason': 'Must not combine'}},
                      {'rom_cart': {'phase': 'first_read', 'resume': 0x06002222,
                                    'path': '/SAROO/ISO/TEST.BIN', 'sha256': '0' * 64,
                                    'crc32': '00000000'}}):
            with self.subTest(extra=extra):
                recipe = copy.deepcopy(self.recipe)
                recipe.update(extra)
                self.compile(recipe, success=False)

    def test_previous_real_decoder_refuses_the_new_contract(self):
        self.invoke('reject', self.compile(), previous=True)

    def test_complete_source_stream_and_production_decoder_verification(self):
        self.verify_stream()

    def test_twelve_records_retire_together_and_thirteenth_record_refuses(self):
        self.raws.extend(overlay.sector(fad) for fad in range(158, 165))
        track = self.path / 'source.bin'
        track.write_bytes(b''.join(self.raws))
        self.recipe['tracks'][0]['sha256'] = hashlib.sha256(track.read_bytes()).hexdigest()
        self.recipe['expire_after_fad'] = 162
        for fad in range(156, 162):
            self.recipe['writes'].append({
                'fad': fad, 'offset': 32, 'before': '71', 'after': '42',
                'effect': 'Additional native source', 'reason': 'Exercise the bounded delivery mask'})
        self.assertEqual(self.compile().read_bytes()[5], 12)
        self.verify_stream()
        self.recipe['expire_after_fad'] = 164
        self.recipe['writes'].append({
            'fad': 163, 'offset': 32, 'before': '71', 'after': '42',
            'effect': 'One record too many', 'reason': 'Refuse instead of truncating the lifetime'})
        self.compile(success=False)

    def verify_stream(self):
        pack = self.compile()
        data = b''.join(raw[16:2064] for raw in self.raws)
        post = bytearray(data)
        for write in self.recipe['writes']:
            at = (write['fad'] - 150) * 2048 + write['offset'] - 16
            value = bytes.fromhex(write['after'])
            post[at:at + len(value)] = value
        original = self.path / 'logical.bin'
        original.write_bytes(data)
        def pin(path):
            return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
        manifest = {'recipe': pin(self.path / 'recipe.json'), 'pack': pin(pack),
                    'decoder': pin(self.fixture.decoder),
                    'programs': [{'source': pin(original), 'lba': 0,
                                  'postimage_sha256': hashlib.sha256(post).hexdigest()}]}
        path = self.path / 'manifest.json'
        path.write_text(json.dumps(manifest))
        output = self.path / 'verified'
        result = subprocess.run([self.fixture.tool, 'verify-source', path, output],
                                capture_output=True, text=True, timeout=40)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        receipt = json.loads((output / 'receipt.json').read_text())
        self.assertTrue(receipt['complete'] and receipt['all_target_sources_verified'])
        self.assertFalse(receipt['unverified_guard_fads'])

    def test_decoder_refuses_ambiguous_or_incomplete_retirement_contracts(self):
        encoded = self.compile().read_bytes()
        last = len(encoded) - overlay.RECORD_BYTES
        variants = []
        for offset, value in [(4, 3), (6, 1), (7, 0), (last + 9, 0),
                              (overlay.HEADER_BYTES + 9, 1), (last + 12, 1)]:
            bad = bytearray(encoded)
            bad[offset] = value
            variants.append(bad)
        bad = bytearray(encoded)
        bad[last:last + 4] = (154).to_bytes(4, 'big')
        variants.append(bad)
        guard = bytearray(encoded[24:24 + overlay.RECORD_BYTES])
        guard[8:] = bytes(overlay.RECORD_BYTES - 8)
        marker = encoded[last:]
        for records in ([guard, marker], [marker]):
            bad = bytearray(encoded[:24] + b''.join(records))
            bad[5] = len(records)
            bad[18:20] = len(bad).to_bytes(2, 'big')
            variants.append(bad)
        for index, bad in enumerate(variants):
            with self.subTest(index=index):
                overlay.seal(bad)
                path = self.path / f'refuse-{index}.SOV'
                path.write_bytes(bad)
                self.invoke('reject', path)


if __name__ == '__main__':
    unittest.main()
