"""Optional special services, with synthetic sectors and explicit SH-2 I/O models.

Adapted from paps-g e2029f04. No game binary or device is required.
"""
import json
from pathlib import Path
import tempfile
import unittest

import test_sector_overlay as overlay
import test_sector_overlay_later as later
from check_later_callbacks_sh2 import execute, region


class SpecialRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay.SectorOverlayTests.setUpClass()
        cls.fixture = overlay.SectorOverlayTests

    setUp = later.LaterRecipeTests.setUp
    source_fixture = later.LaterRecipeTests.source_fixture
    compile = later.LaterRecipeTests.compile
    check_delivery = later.LaterRecipeTests.check_delivery

    def source(self):
        return json.loads((overlay.ROOT / 'recipes/in-the-hunt-us-v1000-sector.json').read_text())

    def test_us_music_source_span_pointer_and_atomic_fragments(self):
        source = self.source()
        self.assertEqual(source['disc_id'], 'T-10001G  V1.000')
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset']) for w in source['writes']], [(185, 968), (173, 1264)])
        body, pointer = source['writes']
        self.assertEqual((len(bytes.fromhex(body['before'])), len(bytes.fromhex(body['after']))), (108, 108))
        self.assertEqual((pointer['before'], pointer['after']), ('0600a44c', '0600a3f4'))
        recipe, raws = self.source_fixture(source, b'V1.000', {})
        self.check_delivery(recipe, raws)

    def test_us_music_wrong_source_version_hash_and_body_refuse(self):
        for wrong in ('version', 'hash', 'body'):
            with self.subTest(wrong=wrong), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                recipe, _ = self.source_fixture(self.source(), b'V1.000', {})
                if wrong == 'version':
                    recipe['disc_id'] = (overlay.PRODUCT + b'V1.001').decode()
                elif wrong == 'hash':
                    recipe['tracks'][0]['sha256'] = '0' * 64
                else:
                    recipe['writes'][0]['before'] = '00' * 108
                result, pack = self.compile(recipe)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(pack.exists())

    def check_music(self, source, base, flag):
        code = bytes.fromhex(source['writes'][0]['after'])
        size, buffer, destination, reader = 0x54f00, 0x22400000, 0x002aa000, 0x06090000
        stop, sp = 0x06012340, 0x060ff000
        memory = region(buffer-8, size+16) | region(destination-8, size+16) | region(sp-16, 32)
        memory.update({buffer+i: ((i*17) ^ (i>>8) ^ 0x79) & 255 for i in range(size)})
        memory.update({destination+i: 0xa5 for i in range(size)})
        memory[flag] = 0
        memory.update(enumerate(reader.to_bytes(4, 'big'), 0x02000f04))
        # Without a complete preload, even an arbitrary stale buffer is not copied.
        before, regs, pr, after, writes, calls = execute(
            code[60:], base+60, stop, memory, registers={15: sp})
        self.assertEqual((after, writes, calls), (memory, [], []))
        self.assertEqual((regs[8:], pr), (before[8:], stop))
        # Independent contract: the reader sees the fixed FAD/size/destination;
        # its return count, not payload content, controls eligibility.
        for count in (size, size-1, 0, -1, size+1, size):
            def read_return(regs, ram):
                self.assertEqual(ram[flag], 0, 'clear previous success before every call')
                self.assertEqual(regs[4:7], [2612, size, buffer])
                for number in range(8):
                    regs[number] = 0xdead0000 + number
                regs[0] = count & 0xffffffff

            previous = dict(memory)
            initial, regs, pr, memory, writes, calls = execute(
                code, base, stop, memory, registers={15: sp}, external=reader,
                external_handler=read_return)
            self.assertEqual(calls, [(2612, size)])
            self.assertEqual((regs[8:], pr), (initial[8:], stop))
            self.assertEqual(memory[flag], int(count == size))
            self.assertEqual(writes, [(sp-4, 4, stop), (flag, 1, 0),
                                     (flag, 1, int(count == size))])
            expected = dict(previous)
            expected.update(enumerate(stop.to_bytes(4, 'big'), sp-4))
            expected[flag] = int(count == size)
            self.assertEqual(memory, expected)
            # The original source supplies R6 before the explicit external call;
            # its value is checked independently from the instruction literal.
            self.assertEqual(int.from_bytes(code[56:60], 'big'), buffer)
            previous = dict(memory)
            initial, regs, pr, memory, writes, calls = execute(
                code[60:], base+60, stop, memory, registers={15: sp}, max_steps=500000)
            expected = dict(previous)
            if count == size:
                expected.update({destination+i: previous[buffer+i] for i in range(size)})
            self.assertEqual(memory, expected)
            self.assertEqual((regs[8:], pr, calls), (initial[8:], stop, []))
            self.assertEqual(len(writes), size//4 if count == size else 0)

    def test_us_music_complete_short_failed_repeated_preload_and_copy_contract(self):
        self.check_music(self.source(), 0x0600a3b8, 0x2600a420)

    def test_fenrir_exact_virtual_disc_changes_one_pointer_literal(self):
        source = json.loads((overlay.ROOT / 'recipes/fenrir-loader-20200722-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'CD0000001 V1.000')
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after']) for w in source['writes']],
                         [(193, 1320, '0600029c', '02000f30')])
        self.assertTrue(source['replaces_callback'])
        recipe, raws = self.source_fixture(source, b'V1.000', {})
        self.check_delivery(recipe, raws)

    def test_fenrir_indirect_call_matches_retained_adapter_at_next_bios_call(self):
        base, stop, callee = 0x0600dcc6, 0x0600dcd4, 0x06090000
        # Authored isolated call/literal window from the reviewed disassembly.
        # No loader executable is included; the external launch call is modeled.
        for seed in (0, 127, 255):
            results = []
            for legacy in (False, True):
                code = bytearray(94)
                code[:14] = bytes.fromhex('d114d8146012400b0009d1136012')
                code[82:86] = (callee if legacy else 0x02000f30).to_bytes(4, 'big')
                code[86:90] = (0x06000288).to_bytes(4, 'big')
                code[90:94] = (0x060002cc).to_bytes(4, 'big')
                if legacy:
                    code[4:6] = bytes.fromhex('0009')
                    code[6] = 0x41
                ram = region(0x02000f30, 4) | region(0x060002cc, 4)
                ram.update(enumerate(callee.to_bytes(4, 'big'), 0x02000f30))
                results.append(execute(code, base, stop, ram, external=callee, seed=seed))
            self.assertEqual(results[0], results[1])
            self.assertEqual(results[0][1][0], int.from_bytes(bytes(ram[0x060002cc+i] for i in range(4)), 'big'))
            self.assertEqual(results[0][1][1], 0x060002cc)
            self.assertEqual(results[0][4], [])

    def jp_source(self):
        return json.loads((overlay.ROOT / 'recipes/kaitei-daisensou-jp-v1004-sector.json').read_text())

    def test_jp_music_exact_source_and_region_literals(self):
        source = self.jp_source()
        self.assertEqual(source['disc_id'], 'T-15006G  V1.004')
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset']) for w in source['writes']], [(185, 740), (173, 1264)])
        self.assertEqual((source['writes'][1]['before'], source['writes'][1]['after']),
                         ('0600a374', '0600a310'))
        us = bytes.fromhex(self.source()['writes'][0]['after'])
        jp = bytes.fromhex(source['writes'][0]['after'])
        expected = bytearray(us)
        expected[40:44] = expected[88:92] = bytes.fromhex('2600a33c')
        self.assertEqual(jp, expected)
        recipe, raws = self.source_fixture(source, b'V1.004', {})
        self.check_delivery(recipe, raws)

    def test_jp_music_complete_short_failed_repeated_preload_and_copy_contract(self):
        self.check_music(self.jp_source(), 0x0600a2d4, 0x2600a33c)

    def test_jp_music_refuses_us_layout_even_with_matching_header(self):
        recipe, _ = self.source_fixture(self.source(), b'V1.004', {})
        jp = self.jp_source()
        recipe['disc_id'] = (overlay.PRODUCT + b'V1.004').decode()
        recipe['writes'] = jp['writes']
        result, pack = self.compile(recipe)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(pack.exists())


if __name__ == '__main__':
    unittest.main()
