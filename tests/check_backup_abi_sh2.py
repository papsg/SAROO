"""Check the game-facing backup call boundary in a linked stock menu."""
import argparse
from pathlib import Path
import struct

from check_menu_clock_sh2 import linked_symbol_records, signed

ENTRIES = tuple('_bup_' + suffix for suffix in (
    'sel_part', 'format', 'stat', 'write', 'read', 'delete', 'dir', 'verify',
    'get_date', 'set_date', 'init',
))


def verify_entry(entry, common, body):
    address, data = entry
    assert len(data) == 12 and address % 4 == 0, 'Backup entry lacks its call boundary'
    load, branch, slot, _, pointer = struct.unpack('>HHHHI', data)
    assert load == 0xd001 and pointer == body
    assert branch >> 12 == 0xa and address + 6 + 2 * signed(branch & 0xfff, 12) == common
    assert slot == 0x0009


def verify_call(code, seed):
    # Execute only the common entry's instructions. The modeled C body keeps
    # R8–R15 and deliberately clobbers R0–R7 and both multiply registers.
    words = tuple(op for op, in struct.iter_unpack('>H', code))
    regs = [0x12340000 + 37 * n + seed for n in range(16)]
    regs[0], regs[15] = 0x0200be00, 0x060ff000
    original = regs[:]
    mach, macl, pr = 0xaabb0000 + seed, 0x11223344 - seed, 0x06012340
    saved = mach, macl, pr
    frame = {}
    calls = 0
    returning = False
    for index, op in enumerate(words):
        if op in (0x4f02, 0x4f12, 0x4f22):   # STS.L MACH/MACL/PR,@-R15
            regs[15] -= 4
            frame[regs[15]] = {0x4f02: mach, 0x4f12: macl, 0x4f22: pr}[op]
        elif op == 0x400b:                   # JSR @R0 and its delay slot
            assert words[index + 1] == 0x0009
            assert regs[:16] == original[:15] + [original[15] - 12]
            calls += 1
            regs[:8] = [0xdecafbad + n for n in range(8)]
            mach, macl, pr = 0xdeadbeef, 0xdeadface, index + 2
        elif op in (0x4f06, 0x4f16, 0x4f26):   # LDS.L @R15+,MACH/MACL/PR
            value = frame.pop(regs[15])
            regs[15] += 4
            if op == 0x4f06:
                mach = value
            elif op == 0x4f16:
                macl = value
            else:
                pr = value
        elif op == 0x000b:                   # RTS, with a final delay slot
            assert pr == saved[2] and index == len(words) - 2
            returning = True
        else:
            assert op == 0x0009, f'Unexpected boundary opcode {op:04x}'
    assert returning and calls == 1 and not frame
    assert (mach, macl, pr) == saved
    assert regs[0] == 0xdecafbad and regs[8:] == original[8:]


def check(elf):
    entries = linked_symbol_records(elf, ENTRIES)
    for _, data in entries:
        assert len(data) == 12, 'Backup entry lacks its call boundary'
    (common, code), *bodies = linked_symbol_records(
        elf, ('bup_abi_call', *(name + '_impl' for name in ENTRIES)))
    for entry, (body, _) in zip(entries, bodies):
        verify_entry(entry, common, body)
    for seed in range(64):
        verify_call(code, seed)
    # The checker must catch either missing MAC restore and an invalid return.
    for op in (0x4f06, 0x4f16, 0x4f26):
        changed = code.replace(op.to_bytes(2, 'big'), b'\x00\x09')
        assert changed != code
        try:
            verify_call(changed, 0)
        except (AssertionError, KeyError):
            continue
        raise AssertionError(f'Restore removal {op:04x} went undetected')
    print('11 linked backup entries, 64 clobber cases and three broken-restore controls pass')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    check(parser.parse_args().elf)
