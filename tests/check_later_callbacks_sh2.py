"""Check linked later-load callbacks with bounded SH-2 instruction semantics.

Only the selected callbacks and the ssctrl_set call boundary are modeled.
This is neither a game emulator nor a hardware timing qualification.
"""
import argparse
from pathlib import Path

from check_menu_clock_sh2 import linked_symbol_records, signed


def execute(code, start, stop, ram, *, cart=None, seed=0, return_address=None, flags=None):
    program = dict(enumerate(code, start))
    memory = dict(ram)
    regs = [0x13570000 + i * 0x100 + seed for i in range(16)]
    regs[15] = 0x060ff000
    initial = regs.copy()
    pc, pr, delayed = start, stop if return_address is None else return_address, None
    writes, calls = [], []
    t = flags[0] if flags is not None else 0

    def read(address, width):
        source = program if address in program else memory
        if not all(address + i in source for i in range(width)):
            raise ValueError(f'Unmodeled callback read {address:08x}/{width}')
        return int.from_bytes(bytes(source[address+i] for i in range(width)), 'big')

    def write(address, width, value):
        if not all(address + i in memory for i in range(width)):
            raise ValueError(f'Unmodeled callback write {address:08x}/{width}')
        value &= (1 << (8 * width)) - 1
        writes.append((address, width, value))
        memory.update(enumerate(value.to_bytes(width, 'big'), address))

    for _ in range(128):
        if pc == stop:
            assert delayed is None
            if flags is not None:
                flags[0] = t
            return initial, regs, pr, memory, writes, calls
        if pc == cart:
            assert delayed is None
            calls.append((regs[4], regs[5]))
            # A caller must tolerate volatile-register changes across the
            # existing MMIO accessor. The accessor's hardware is not modeled.
            for i in range(8):
                regs[i] = 0xdead0000 + i
            pc = pr
            continue
        op = read(pc, 2)
        n, m = (op >> 8) & 15, (op >> 4) & 15
        target, delayed = delayed, None
        following = pc + 2
        if op >> 12 == 0xd:
            regs[n] = read(((pc + 4) & ~3) + (op & 255) * 4, 4)
        elif op >> 12 == 9:
            regs[n] = signed(read(pc + 4 + (op & 255) * 2, 2), 16) & 0xffffffff
        elif op >> 12 == 0xe:
            regs[n] = signed(op & 255, 8) & 0xffffffff
        elif op >> 12 == 7:
            regs[n] = (regs[n] + signed(op & 255, 8)) & 0xffffffff
        elif op & 0xff00 == 0x8800:
            t = int(regs[0] == (signed(op & 255, 8) & 0xffffffff))
        elif op & 0xf00f == 0x6000:
            regs[n] = signed(read(regs[m], 1), 8) & 0xffffffff
        elif op & 0xf00f == 0x600c:
            regs[n] = regs[m] & 255
        elif op & 0xf00f == 0x6003:
            regs[n] = regs[m]
        elif op >> 12 == 0xa:
            assert target is None
            delayed = pc + 4 + signed(op & 0xfff, 12) * 2
        elif op & 0xff00 in (0x8900, 0x8b00, 0x8d00, 0x8f00):
            assert target is None
            taken = not t if op & 0x200 else bool(t)
            if taken:
                branch = pc + 4 + signed(op & 255, 8) * 2
                if op & 0x400:
                    delayed = branch
                else:
                    following = branch
        elif op & 0xf00f in (0x2000, 0x2001, 0x2002):
            write(regs[n], 1 << (op & 3), regs[m])
        elif op == 0x4f22:
            regs[15] -= 4
            write(regs[15], 4, pr)
        elif op == 0x4f26:
            pr = read(regs[15], 4)
            regs[15] += 4
        elif op & 0xf0ff in (0x400b, 0x402b):
            assert target is None
            delayed = regs[n]
            if op & 0xff == 0x0b:
                pr = pc + 4
        elif op == 0x000b:
            assert target is None
            delayed = pr
        elif op != 0x0009:
            raise ValueError(f'Unsupported callback opcode {op:04x} at {pc:08x}')
        pc = target if target is not None else following
    raise ValueError('Callback instruction bound exceeded')


def region(address, width):
    return {address+i: (i * 17 + 0x31) & 255 for i in range(width)}


def verify_cyber_handler(address, code):
    assert len(code) == 24
    for seed in (0, 1, 127, 255):
        ram = region(0x06016950, 32)
        before, regs, pr, after, writes, calls = execute(
            code, address, 0x0600d000, ram, seed=seed, return_address=0x06054320)
        assert writes == [(0x0601695a, 2, 9)] and not calls
        expected = dict(ram)
        expected.update({0x0601695a: 0, 0x0601695b: 9})
        assert after == expected and pr == 0x06054320
        before[1], before[2] = 0x0600d000, 9
        assert regs == before, 'Unexpected incoming-register change'


def verify_groove_handler(address, code):
    assert len(code) == 40
    for seed in (0, 1, 127, 255):
        ram = (region(0x06008330, 32) | region(0x06099a80, 32) |
               region(0x00280000, 32))
        before, regs, pr, after, writes, calls = execute(
            code, address, 0x06004000, ram, seed=seed, return_address=0x06054320)
        assert writes == [(0x06008338, 2, 9), (0x06099a84, 2, 9),
                          (0x00280008, 4, 0x06004000)] and not calls
        expected = dict(ram)
        for dest, width, value in writes:
            expected.update(enumerate(value.to_bytes(width, 'big'), dest))
        assert after == expected and pr == 0x06054320
        before[1], before[2] = 0x06004000, 0x00280008
        assert regs == before


def verify_habitat_handler(address, code):
    assert len(code) == 48
    for pad in (0, 0x11, 0x80, 0xf4, 0xdc):
        for habitat in (0, 0x11, 0x80, 0x26):
            for initial_t in (0, 1):
                ram = region(0x0602a150, 32) | region(0x0602b520, 32)
                ram[0x0602a15d], ram[0x0602b52d] = pad, habitat
                flags = [initial_t]
                before, regs, pr, after, writes, calls = execute(
                    code, address, 0x06020000, ram, return_address=0x06054320, flags=flags)
                chosen = 0x0602a15e if pad == 0x11 else 0x0602b52e if habitat == 0x11 else 0
                expected_writes = [(chosen, 2, 9)] if chosen else []
                assert writes == expected_writes and not calls
                expected = dict(ram)
                if chosen:
                    expected.update({chosen: 0, chosen+1: 9})
                    before[2] = 9
                before[0] = signed(pad if pad == 0x11 else habitat, 8) & 0xffffffff
                before[1] = 0x06020000
                assert regs == before and after == expected and pr == 0x06054320
                assert flags == [int(chosen != 0)], 'Shared guard order or T effect changed'


def check(elf):
    (patch_at, patch), (handler_at, handler), (cart_at, _) = linked_symbol_records(
        elf, ('_CYBER_BOTS_patch', '_CYBER_BOTS_handle1', '_ssctrl_set'))
    verify_cyber_handler(handler_at, handler)
    for seed in (0, 1, 127, 255):
        ram = region(0x060dd040, 64) | region(0x060feff0, 32)
        before, regs, pr, after, writes, calls = execute(
            patch, patch_at, 0x06012340, ram, cart=cart_at, seed=seed)
        assert calls == [(0x3000, 0x2000)], 'Missing 1 MB cartridge selection'
        assert writes == [(0x060feffc, 4, 0x06012340),
                          (0x060dd058, 4, handler_at)]
        expected = dict(ram)
        expected.update(enumerate(handler_at.to_bytes(4, 'big'), 0x060dd058))
        expected.update(enumerate(bytes.fromhex('06012340'), 0x060feffc))
        assert after == expected and regs[8:] == before[8:] and pr == 0x06012340
    print('Linked Cyber Bots launch/cart call, all handler writes, continuation '
          'and incoming-register effects pass')
    (patch_at, patch), (handler_at, handler) = linked_symbol_records(
        elf, ('_GROOVE_ON_FIGHT_patch', '_GROOVE_ON_FIGHT_handle'))
    verify_groove_handler(handler_at, handler)
    for seed in (0, 1, 127, 255):
        ram = (region(0x00200a40, 32) | region(0x00202eb0, 32) |
               region(0x00200450, 32))
        before, regs, pr, after, writes, calls = execute(
            patch, patch_at, 0x06012340, ram, seed=seed)
        assert writes == [(0x00200a44, 2, 9), (0x00202eb8, 2, 9),
                          (0x00200460, 4, handler_at)] and not calls
        expected = dict(ram)
        for dest, width, value in writes:
            expected.update(enumerate(value.to_bytes(width, 'big'), dest))
        before[1], before[2] = 0x00200460, handler_at
        assert after == expected and regs == before and pr == 0x06012340
    print('Linked Groove launch edits, delay-slot pointer store, late writes '
          'and original continuation pass')
    (patch_at, patch), (handler_at, handler) = linked_symbol_records(
        elf, ('_HABITAT2_patch', '_HABITAT2_handle'))
    verify_habitat_handler(handler_at, handler)
    ram = region(0x060105a0, 32)
    before, regs, pr, after, writes, calls = execute(patch, patch_at, 0x06012340, ram)
    assert writes == [(0x060105b4, 4, handler_at)] and not calls
    expected = dict(ram)
    expected.update(enumerate(handler_at.to_bytes(4, 'big'), 0x060105b4))
    before[1], before[2] = 0x060105b4, handler_at
    assert after == expected and regs == before and pr == 0x06012340
    print('Linked shared Habitat/Pad guard priority, all conditional writes, '
          'launch pointer, T and continuation pass')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    check(parser.parse_args().elf)
