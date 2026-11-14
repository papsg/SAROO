"""Remaining stock profiles against synthetic sources and production C code."""
import json
from pathlib import Path
import tempfile
import unittest

import test_sector_overlay as overlay
import test_sector_overlay_later as later
from check_later_callbacks_sh2 import execute, region
from check_remaining_callbacks_sh2 import (BATMAN_HANDLER, MSH_HANDLER, SSCTRL_HELPER,
                                            PIA_HANDLER, verify_batman_handler,
                                            verify_msh_handler, verify_pia_handler)


class RemainingRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay.SectorOverlayTests.setUpClass()
        cls.fixture = overlay.SectorOverlayTests

    setUp = later.LaterRecipeTests.setUp
    source_fixture = later.LaterRecipeTests.source_fixture
    compile = later.LaterRecipeTests.compile
    check_delivery = later.LaterRecipeTests.check_delivery

    def batman_fixture(self, region):
        ids = {'us': 'T-8140H   V1.000', 'jp': 'T-8118G   V1.000',
               'eu-50': 'T-8140H-50V1.000'}
        first = 178 if region == 'eu-50' else 181
        source = json.loads((overlay.ROOT / 'recipes' /
                            f'batman-forever-arcade-{region}-v1000-sector.json').read_text())
        self.assertEqual(source['disc_id'], ids[region])
        self.assertEqual(source.get('cart_mode', 0), 0)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                          for w in source['writes']],
                         [(first, 724, '0600af9c', '0600d000'),
                          (664, 1620, 'd439490b6583d239612221188ffceb00',
                           'd23a612221188bfcd437490b6583eb00')])
        self.assertEqual([g['fad'] for g in source['guards']], [150, first+13])
        self.assertEqual(0x06004100 + 724-16, 0x060043c4)
        self.assertEqual(0x0600d000 + (664-150-469)*2048 + 1620-16, 0x06023e44)
        return self.source_fixture(source, b'V1.000',
                                   {150: [(240, '00000f0c')],
                                    first+13: [(1708, 'd0034f22400b00094f26000b000900000600d000')]})

    def test_batman_all_regions_deliver_only_native_edits_and_original_ip_and_wrapper(self):
        for region in ('us', 'jp', 'eu-50'):
            with self.subTest(region=region), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                recipe, raws = self.batman_fixture(region)
                self.check_delivery(recipe, raws)

    def test_pia_exact_a100_edits_keep_all_loader_and_refresh_context(self):
        source = json.loads((overlay.ROOT / 'recipes/pia-carrot-2-jp-v1002-sector.json').read_text())
        self.assertEqual(source['disc_id'], 'T-20114G  V1.002')
        self.assertNotIn('cart_mode', source)
        self.assertTrue(source['replaces_callback'])
        self.assertEqual([(w['fad'], w['offset'], w['before'], w['after']) for w in source['writes']],
                         [(476, 1868, '2122', '0009'), (476, 1884, '490b', '0009'),
                          (476, 2030, '2122', '0009')])
        self.assertEqual([g['fad'] for g in source['guards']], [174, 248, 477])
        self.assertEqual([0x06010000+(w['fad']-150-218)*2048+w['offset']-16
                          for w in source['writes']], [0x0604673c, 0x0604674c, 0x060467de])
        recipe, raws = self.source_fixture(source, b'V1.002', {
            174: [(1692, '00210000'), (1728, '06021fe4')],
            248: [(186, '490b0009'), (208, '06010000')],
            476: [(1870, 'd110e2132122'), (1886, '000940118902e0fda0030009e000a0000009'),
                  (1932, '23301ff025fe00b8060468f8'), (2032, 'd10ae2132122')],
            477: [(24, '23301ff025fe00b8')],
        })
        self.check_delivery(recipe, raws)

    def test_batman_rejects_wrong_revision_source_and_instruction(self):
        for wrong in ('revision', 'source', 'instruction'):
            recipe, _ = self.batman_fixture('us')
            if wrong == 'revision':
                recipe['disc_id'] = (overlay.PRODUCT + b'V1.001').decode()
            elif wrong == 'source':
                recipe['tracks'][0]['sha256'] = '0'*64
            else:
                recipe['writes'][1]['before'] = '0009'*8
            result, pack = self.compile(recipe)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(pack.exists())

    def test_msh_all_four_exact_sources_keep_native_handoffs_and_select_one_mb(self):
        rows = [('us-v1001', 'T-1214H   V1.001', 1250, 202, 1032, 188, 484, 268, 744, 1082),
                ('jp-v1003', 'T-1215G   V1.003', 1250, 202, 1032, 188, 484, 268, 744, 1082),
                ('eu-50-v1000', 'T-7032H-50V1.000', 3469, 202, 1468, 188, 484, 268, 744, 3301),
                ('jp-demo-v1000', '6106664   V1.000', 260, 191, 1076, 1356, 1658, 1900, 1436, 103)]
        for name, identity, fad, guard, pointer, first, second, ga, gb, lba in rows:
            with self.subTest(name=name), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path = Path(folder)
                source = json.loads((overlay.ROOT / 'recipes' /
                                     f'marvel-super-heroes-{name}-sector.json').read_text())
                self.assertEqual(source['disc_id'], identity)
                self.assertEqual(source['cart_mode'], 1)
                self.assertTrue(source['replaces_callback'])
                self.assertEqual([(w['fad'], w['offset'], w['before'], w['after'])
                                  for w in source['writes']],
                                 [(fad, first, '2212', '0009'), (fad, second, '2212', '0009')])
                self.assertEqual([g['fad'] for g in source['guards']], [guard])
                addresses = [0x06002000 + (fad-150-lba)*2048 + off-16 for off in [first, second]]
                self.assertEqual(addresses, [0x06005d3c, 0x06005e6a] if name.startswith('jp-demo')
                                 else [0x0600b0ac, 0x0600b1d4])
                recipe, raws = self.source_fixture(source, identity[10:].encode(),
                                                    {guard: [(pointer, '060d0ff4')],
                                                     fad: [(ga, '23301ff0'), (gb, '23301ff0')]})
                self.check_delivery(recipe, raws)


class BatmanInstructionTests(unittest.TestCase):
    def test_complete_original_conditional_handler_effects(self):
        verify_batman_handler(0x02008508, BATMAN_HANDLER)

    def test_bad_guard_partial_copy_or_entry_fails(self):
        for at, value in ((6, '0009'), (20, '0009'), (62, 'd004')):
            code = bytearray(BATMAN_HANDLER)
            code[at:at+2] = bytes.fromhex(value)
            with self.subTest(at=at), self.assertRaises((ValueError, AssertionError)):
                verify_batman_handler(0x02008508, code)

    def test_corrected_sequence_waits_before_reusing_the_worker_input(self):
        for corrected, words in ((False, 'd439490b6583d239612221188ffceb00'),
                                 (True, 'd23a612221188bfcd437490b6583eb00')):
            code = bytearray(0xf0)
            code[:16] = bytes.fromhex(words)
            code[0xe8:0xec] = bytes.fromhex('06023df8')
            code[0xec:0xf0] = bytes.fromhex('260d2810')
            events = []
            _, regs, _, _, writes, calls = execute(
                code, 0x06023e44, 0x06023e54, region(0x260d2810, 4),
                external=0x13570900, trace=events, read_values={0x260d2810: [1, 1, 0]})
            polls = [i for i, e in enumerate(events) if e[:2] == ('read', 0x260d2810)]
            call = next(i for i, e in enumerate(events) if e[0] == 'call')
            self.assertEqual([events[i][3] for i in polls], [1, 1, 0])
            self.assertEqual(call > polls[-1], corrected)
            self.assertEqual(calls, [(0x06023df8, 0x13570800)])
            self.assertEqual(regs[11], 0)
            self.assertFalse(writes)

    def test_native_and_handler_entry_keep_each_original_callers_pr_and_delay_slot(self):
        handler_at = 0x02008508
        for pc, words, pr in ((0x0600437a, 'd012400b0009', 0x06004380),
                              (0x06004390, '480b0009', 0x06004394),
                              (0x0600439e, 'e201480b2a22', 0x060043a4)):
            for old in (False, True):
                target = handler_at if old else 0x0600d000
                code = bytearray(0x060043c8-pc)
                code[:len(bytes.fromhex(words))] = bytes.fromhex(words)
                code[-4:] = target.to_bytes(4, 'big')
                ram = (region(0x06023e40, 32) | region(0x13570a00, 4) |
                       dict(enumerate(BATMAN_HANDLER, handler_at)))
                ram.update(enumerate(bytes.fromhex('000b0416'), 0x06023e44))
                _, regs, result_pr, _, writes, _ = execute(
                    code, pc, 0x0600d000, ram, registers={8: target})
                self.assertEqual(result_pr, pr)
                self.assertEqual(writes, [(0x13570a00, 4, 1)] if pc == 0x0600439e else [])


class MarvelSuperInstructionTests(unittest.TestCase):
    def test_original_loader_rmw_guard_priority_and_complete_return_effects(self):
        verify_msh_handler(0x02008b94, MSH_HANDLER, 0x02008990, SSCTRL_HELPER)

    def test_wrong_guard_call_stack_restore_or_rmw_is_rejected(self):
        for which, at, value in [('handler', 4, '0009'), ('handler', 24, '0009'),
                                  ('handler', 46, '0009'), ('handler', 44, '0009'),
                                  ('helper', 10, '2459'), ('helper', 4, '6122')]:
            handler, helper = bytearray(MSH_HANDLER), bytearray(SSCTRL_HELPER)
            (handler if which == 'handler' else helper)[at:at+2] = bytes.fromhex(value)
            with self.subTest(which=which, at=at), self.assertRaises((ValueError, AssertionError)):
                verify_msh_handler(0x02008b94, handler, 0x02008990, helper)


class PiaInstructionTests(unittest.TestCase):
    def test_all_original_late_guard_instruction_and_complete_cpu_effects(self):
        verify_pia_handler(0x02008414, PIA_HANDLER)

    def test_changed_guard_store_or_fixed_entry_is_rejected(self):
        for at, value in ((8, '0009'), (24, '0009'), (50, '0009'), (122, '0004')):
            code = bytearray(PIA_HANDLER)
            code[at:at+2] = bytes.fromhex(value)
            with self.subTest(at=at), self.assertRaises((ValueError, AssertionError)):
                verify_pia_handler(0x02008414, code)

    def test_skipped_call_keeps_existing_signed_result_paths_without_synthesizing_success(self):
        # The original JSR becomes a NOP; the following NOP and
        # native CMP/PZ / BT paths stay intact. In particular R0 is not set
        # to an invented test result when the JSR is skipped.
        code = bytes.fromhex('0009000940118902e0fda0030009e000a0000009')
        for value in (0, 1, 0x7fffffff, 0x80000000, 0xfffffffd, 0xffffffff):
            for initial_t in (0, 1):
                flags = [initial_t]
                before, regs, pr, after, writes, calls = execute(
                    code, 0x0604674c, 0x06046760, {}, registers={0: value}, flags=flags,
                    return_address=0x06054320)
                nonnegative = value < 0x80000000
                before[0] = 0 if nonnegative else 0xfffffffd
                self.assertEqual(regs, before)
                self.assertEqual(flags, [int(nonnegative)])
                self.assertEqual(pr, 0x06054320)
                self.assertEqual((after, writes, calls), ({}, [], []))


if __name__ == '__main__':
    unittest.main()
