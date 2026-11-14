"""Bounded instruction checks of retained callbacks; no game or timing model."""
import argparse
import itertools
from pathlib import Path

from check_later_callbacks_sh2 import execute, region
from check_menu_clock_sh2 import linked_symbol_records


BATMAN_HANDLER = bytes.fromhex(
    'd108d209631233208b07d2082122d2081121d2081122d2081123d108412b0009'
    '000b000906023e44d439490bd23a612221188bfcd437490b6583eb000600d000')
MSH_HANDLER = bytes.fromhex(
    'd1144f22410b0009d11395219421410b0009d1126212d11232108b0bd31163323320'
    '8b07d210e1092211d21022114f26000b0009d20e622232108bf8d20d622232108ff4'
    'e109d20c2211d20cafee000920003000060d0ff4020089900600b0fc23301ff0'
    '0600b2d80600b0ac0600b1d406005f5c06005d8c06005d3c06005e6a')
SSCTRL_HELPER = bytes.fromhex('d20464476121611d2419245b2241000b0009000925897004')
PIA_HANDLER = bytes.fromhex(
    'd1156212d11532108b07d21593226121611d31308f01e1092211d1126212d10f3210'
    '8b07d21093156121611d31308f01e1092211d10d6212d10d32108b07d20d93096121'
    '611d31308f01e1092211d10a412b00092122490b0604677c23301ff00604673c'
    '06046808060467de06046784060468f80604674c06010000')


def verify_pia_handler(address, code):
    assert len(code) == 124
    guards = [0x0604677c, 0x06046808, 0x06046784]
    targets = [0x0604673c, 0x060467de, 0x0604674c]
    signatures = [0x23301ff0, 0x23301ff0, 0x060468f8]
    instructions = [0x2122, 0x2122, 0x490b]
    for matches in itertools.product((False, True), repeat=6):
        for initial_t in (0, 1):
            ram = {}
            for i, (guard, target) in enumerate(zip(guards, targets)):
                ram |= region(guard-2, 8) | region(target-2, 8)
                value = signatures[i] if matches[2*i] else 0x12340000+i
                word = instructions[i] if matches[2*i+1] else 0xfed0+i
                ram.update(enumerate(value.to_bytes(4, 'big'), guard))
                ram.update(enumerate(word.to_bytes(2, 'big'), target))
            flags, trace = [initial_t], []
            before, regs, pr, after, writes, calls = execute(
                code, address, 0x06010000, ram, flags=flags, trace=trace,
                return_address=0x06054320)
            expected_writes = [(targets[i], 2, 9) for i in range(3)
                               if matches[2*i] and matches[2*i+1]]
            expected = dict(ram)
            for dest, width, value in expected_writes:
                expected.update(enumerate(value.to_bytes(width, 'big'), dest))
            before[1] = 0x06010000
            before[2] = targets[2] if matches[4] else 0x12340002
            for i in range(3):
                if matches[2*i]:
                    before[3] = instructions[i]
            expected_reads = [dest for i in range(3)
                              for dest in ([guards[i], targets[i]] if matches[2*i] else [guards[i]])]
            assert regs == before and after == expected and writes == expected_writes
            assert flags == [int(matches[4] and matches[5])]
            assert [e[1] for e in trace if e[0] == 'read'] == expected_reads
            assert not calls and pr == 0x06054320


def verify_msh_handler(address, code, helper_at, helper):
    assert len(code) == 128 and len(helper) == 24
    assert int.from_bytes(code[88:92], 'big') == helper_at
    signature = 0x23301ff0
    guards = [0x0600b0fc, 0x0600b2d8, 0x06005f5c, 0x06005d8c]
    targets = [0x0600b0ac, 0x0600b1d4, 0x06005d3c, 0x06005e6a]
    for matches in itertools.product((False, True), repeat=4):
        values = [signature if match else 0xa1b20000+i for i, match in enumerate(matches)]
        for control in (0, 0x6a5a, 0x8000, 0xffff):
            for initial_t in (0, 1):
                ram = (region(0x060feff0, 32) | region(0x25897000, 16) |
                       dict(enumerate(helper, helper_at)))
                for dest, value in zip(guards, values):
                    ram.update(enumerate(value.to_bytes(4, 'big'), dest))
                for dest in targets:
                    ram |= region(dest-2, 8)
                ram.update(enumerate(control.to_bytes(2, 'big'), 0x25897004))
                flags, trace = [initial_t], []
                before, regs, pr, after, writes, calls = execute(
                    code, address, 0x06054320, ram, external=0x060d0ff4,
                    flags=flags, trace=trace)
                retail = matches[0] and matches[1]
                demo = not retail and matches[2] and matches[3]
                new_control = (control & ~0x3000) | 0x2000
                edits = [(targets[i], 2, 9) for i in ([0, 1] if retail else [2, 3] if demo else [])]
                expected_writes = [(0x060feffc, 4, 0x06054320),
                                   (0x25897004, 2, new_control), *edits]
                assert writes == expected_writes and calls == [(before[4], before[5])]
                expected = dict(ram)
                for dest, width, value in expected_writes:
                    expected.update(enumerate(value.to_bytes(width, 'big'), dest))
                assert after == expected and pr == 0x06054320
                expected_regs = before.copy()
                expected_regs[:8] = [0xdead0000+i for i in range(8)]
                expected_regs[1] = 9 if retail or matches[2] else signature
                expected_regs[2] = targets[1] if retail else targets[3] if demo else values[3] if matches[2] else values[2]
                expected_regs[3] = values[1] if matches[0] else 0xdead0003
                expected_regs[4:6] = [new_control, 0x2000]
                assert regs == expected_regs and flags == [int(retail or demo)]
                assert trace[0] == ('call', 0x060d0ff4, before[4], before[5])
                reads = [e[1] for e in trace if e[0] == 'read' and e[1] in guards]
                expected_reads = guards[:2] if matches[0] else guards[:1]
                if not retail:
                    expected_reads += guards[2:4] if matches[2] else guards[2:3]
                assert reads == expected_reads
                assert len([e for e in trace if e[:3] == ('read', 0x25897004, 2)]) == 1


def verify_batman_handler(address, code):
    assert len(code) == 64
    for guard in (0xd439490b, 0x000b0416, 0, 0xffffffff):
        for initial_t in (0, 1):
            for seed in (0, 255):
                ram = region(0x06023e40, 32)
                ram.update(enumerate(guard.to_bytes(4, 'big'), 0x06023e44))
                flags = [initial_t]
                before, regs, pr, after, writes, calls = execute(
                    code, address, 0x0600d000, ram, flags=flags, seed=seed,
                    return_address=0x06054320)
                match = guard == 0xd439490b
                values = [0xd23a6122, 0x21188bfc, 0xd437490b, 0x6583eb00]
                expected_writes = [(0x06023e44+i*4, 4, v) for i, v in enumerate(values)] if match else []
                assert writes == expected_writes and not calls
                expected = dict(ram)
                for dest, width, value in expected_writes:
                    expected.update(enumerate(value.to_bytes(width, 'big'), dest))
                before[1:4] = [0x0600d000, values[-1] if match else 0xd439490b, guard]
                assert after == expected and regs == before and pr == 0x06054320
                assert flags == [int(match)]


def check(elf):
    (patch_at, patch), (handler_at, handler) = linked_symbol_records(
        elf, ('_Batman_Forever_The_Arcade_Game_patch', '_Batman_Forever_The_Arcade_Game_handle1'))
    verify_batman_handler(handler_at, handler)
    for pointer in (0x0600af9c, 0x0600d000, 0, 0xffffffff):
        for initial_t in (0, 1):
            ram = region(0x060043c0, 32)
            ram.update(enumerate(pointer.to_bytes(4, 'big'), 0x060043c4))
            flags = [initial_t]
            before, regs, pr, after, writes, calls = execute(
                patch, patch_at, 0x06012340, ram, flags=flags)
            match = pointer == 0x0600af9c
            expected = dict(ram)
            if match:
                expected.update(enumerate(handler_at.to_bytes(4, 'big'), 0x060043c4))
            before[1:4] = [0x060043c4, handler_at if match else 0x0600af9c, pointer]
            assert writes == ([(0x060043c4, 4, handler_at)] if match else [])
            assert after == expected and regs == before and flags == [int(match)]
            assert not calls and pr == 0x06012340
    print('Linked Batman pointer guard, all conditional writes, complete registers/T and fixed entry pass')
    (patch_at, patch), (handler_at, handler), (helper_at, helper) = linked_symbol_records(
        elf, ('_MARVEL_SUPER_patch', '_MARVEL_SUPER_handle', '_ssctrl_set'))
    verify_msh_handler(handler_at, handler, helper_at, helper)
    sites = [0x060d13f8, 0x060d1424, 0x060d15ac]
    for matches in itertools.product((False, True), repeat=3):
        values = [0x060d0ff4 if match else 0xa1b20000+i for i, match in enumerate(matches)]
        for initial_t in (0, 1):
            ram = {}
            for dest, value in zip(sites, values):
                ram.update(enumerate(value.to_bytes(4, 'big'), dest))
            flags = [initial_t]
            before, regs, pr, after, writes, calls = execute(
                patch, patch_at, 0x06054320, ram, flags=flags)
            chosen = next((i for i, match in enumerate(matches) if match), None)
            index = 2 if chosen is None else chosen
            expected = dict(ram)
            if chosen is not None:
                expected.update(enumerate(handler_at.to_bytes(4, 'big'), sites[chosen]))
            before[1:4] = [0x060d0ff4 if chosen is None else handler_at, sites[index], values[index]]
            assert regs == before and after == expected and flags == [int(chosen is not None)]
            assert writes == ([] if chosen is None else [(sites[chosen], 4, handler_at)])
            assert not calls and pr == 0x06054320
    print('Linked MSH pointer priority, one original loader call, exact SS_CTRL helper/RMW, '
          'retail/demo guard priority, complete writes/registers/flags/stack and return pass')
    (patch_at, patch), (first_at, first), (last_at, last) = linked_symbol_records(
        elf, ('_Pia_Carrot_e_Youkoso_2_patch', '_Pia_Carrot_e_Youkoso_2_handle1',
              '_Pia_Carrot_e_Youkoso_2_handle2'))
    verify_pia_handler(last_at, last)
    for outer, inner, initial_t in itertools.product((False, True), repeat=3):
        ram = region(0x06010688, 8) | region(0x060106ac, 8)
        pointer = 0x06021fe4 if inner else 0x55556666
        ram.update(enumerate((0x00210000 if outer else 0x77778888).to_bytes(4, 'big'), 0x0601068c))
        ram.update(enumerate(pointer.to_bytes(4, 'big'), 0x060106b0))
        flags = [int(initial_t)]
        before, regs, pr, after, writes, calls = execute(patch, patch_at, 0x06054320, ram, flags=flags)
        expected = dict(ram)
        match = outer and inner
        if match:
            expected.update(enumerate(first_at.to_bytes(4, 'big'), 0x060106b0))
        before[1] = 0x060106b0 if outer else 0x00210000
        before[2] = first_at if match else 0x06021fe4 if outer else 0x77778888
        if outer:
            before[3] = pointer
        assert after == expected and regs == before and flags == [int(match)]
        assert writes == ([(0x060106b0, 4, first_at)] if match else [])
        assert pr == 0x06054320 and not calls
    for pointer in (0x06010000, last_at, 0, 0xffffffff):
        for initial_t in (0, 1):
            ram = region(0x002100bc, 12)
            ram.update(enumerate(pointer.to_bytes(4, 'big'), 0x002100c0))
            flags = [initial_t]
            before, regs, pr, after, writes, calls = execute(
                first, first_at, 0x06021fe4, ram, flags=flags, return_address=0x06054320)
            match = pointer == 0x06010000
            expected = dict(ram)
            if match:
                expected.update(enumerate(last_at.to_bytes(4, 'big'), 0x002100c0))
            before[1:4] = [0x06021fe4, last_at if match else 0x06010000, pointer]
            assert after == expected and regs == before and flags == [int(match)]
            assert writes == ([(0x002100c0, 4, last_at)] if match else [])
            assert pr == 0x06054320 and not calls
    print('Linked Pia initial/loader guards, all 64 late predicate combinations, exact writes, '
          'short-circuit reads, complete GPR/T/PR and both native entries pass')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    check(parser.parse_args().elf)
