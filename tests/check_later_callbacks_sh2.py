"""Check linked later-load callbacks with bounded SH-2 instruction semantics.

Only selected callbacks and explicit external-call boundaries are modeled.
This is neither a game emulator nor a hardware timing qualification.
"""
import argparse
from pathlib import Path

from check_menu_clock_sh2 import linked_symbol_records, signed


def execute(code, start, stop, ram, *, external=None, seed=0, return_address=None, flags=None,
            read_values=None, trace=None, registers=None):
    program = dict(enumerate(code, start))
    memory = dict(ram)
    regs = [0x13570000 + i * 0x100 + seed for i in range(16)]
    regs[15] = 0x060ff000
    if registers is not None:
        for number, value in registers.items():
            assert 0 <= number < 16 and 0 <= value <= 0xffffffff
            regs[number] = value
    initial = regs.copy()
    pc, pr, delayed = start, stop if return_address is None else return_address, None
    writes, calls = [], []
    t = flags[0] if flags is not None else 0

    def read(address, width):
        source = program if address in program else memory
        if not all(address + i in source for i in range(width)):
            raise ValueError(f'Unmodeled callback read {address:08x}/{width}')
        value = int.from_bytes(bytes(source[address+i] for i in range(width)), 'big')
        if source is memory:
            if read_values is not None and address in read_values:
                values = read_values[address]
                value = values.pop(0) if len(values) > 1 else values[0]
            if trace is not None:
                trace.append(('read', address, width, value))
        return value

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
        if pc == external:
            assert delayed is None
            calls.append((regs[4], regs[5]))
            if trace is not None:
                trace.append(('call', pc, regs[4], regs[5]))
            # The declared external callee preserves the ABI's saved state
            # and returns with arbitrary volatile-register values.
            for i in range(8):
                regs[i] = 0xdead0000 + i
            pc = pr
            continue
        op = read(pc, 2)
        n, m = (op >> 8) & 15, (op >> 4) & 15
        target, delayed = delayed, None
        following = pc + 2
        # SH-2 PC-relative delay-slot loads use branch destination + 2.
        relative_pc = pc + 4 if target is None else target + 2
        if op >> 12 == 0xd:
            regs[n] = read((relative_pc & ~3) + (op & 255) * 4, 4)
        elif op >> 12 == 9:
            regs[n] = signed(read(relative_pc + (op & 255) * 2, 2), 16) & 0xffffffff
        elif op >> 12 == 0xe:
            regs[n] = signed(op & 255, 8) & 0xffffffff
        elif op >> 12 == 7:
            regs[n] = (regs[n] + signed(op & 255, 8)) & 0xffffffff
        elif op & 0xff00 == 0x8800:
            t = int(regs[0] == (signed(op & 255, 8) & 0xffffffff))
        elif op & 0xf00f == 0x3000:
            t = int(regs[n] == regs[m])
        elif op & 0xf0ff == 0x4011:
            t = int(signed(regs[n], 32) >= 0)
        elif op & 0xf00f == 0x2008:
            t = int((regs[n] & regs[m]) == 0)
        elif op & 0xf00f == 0x2009:
            regs[n] &= regs[m]
        elif op & 0xf00f == 0x200b:
            regs[n] |= regs[m]
        elif op & 0xf00f == 0x6007:
            regs[n] = regs[m] ^ 0xffffffff
        elif op & 0xf00f == 0x6000:
            regs[n] = signed(read(regs[m], 1), 8) & 0xffffffff
        elif op & 0xf00f == 0x6001:
            regs[n] = signed(read(regs[m], 2), 16) & 0xffffffff
        elif op & 0xf00f == 0x6002:
            regs[n] = read(regs[m], 4)
        elif op & 0xf00f == 0x6006:
            value = read(regs[m], 4)
            if n != m:
                regs[m] = (regs[m] + 4) & 0xffffffff
            regs[n] = value
        elif op & 0xf00f == 0x600c:
            regs[n] = regs[m] & 255
        elif op & 0xf00f == 0x600d:
            regs[n] = regs[m] & 65535
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
        elif op >> 12 == 1:
            write(regs[n] + (op & 15) * 4, 4, regs[m])
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


def verify_marvel_handler(address, code):
    assert len(code) == 60
    for cpk in (0, 0x2012, 0x4610, 0xffff):
        for system in (0, 0x2102, 0x2fc6, 0xffff):
            for initial_t in (0, 1):
                ram = region(0x06002860, 32) | region(0x06002050, 32)
                ram.update(enumerate(cpk.to_bytes(2, 'big'), 0x0600286e))
                ram.update(enumerate(system.to_bytes(2, 'big'), 0x0600205a))
                flags = [initial_t]
                before, regs, pr, after, writes, calls = execute(
                    code, address, 0x06002000, ram, return_address=0x06054320, flags=flags)
                expected_writes = ([(0x0600286e, 2, 9)] if cpk == 0x2012 else [])
                expected_writes += [(0x0600205a, 2, 9)] if system == 0x2102 else []
                assert writes == expected_writes and not calls
                expected = dict(ram)
                for dest, width, value in expected_writes:
                    expected.update(enumerate(value.to_bytes(width, 'big'), dest))
                before[1:4] = [0x06002000, 0x0600205a, 0x2102]
                assert regs == before and after == expected and pr == 0x06054320
                assert flags == [int(system == 0x2102)], 'Independent guards became mutually exclusive'


def verify_phantasy_handler(address, code):
    assert len(code) == 36
    for pointer in (0, 0x08000009, 0x06021b58, 0x0600083c, 0xffffffff):
        for initial_t in (0, 1):
            ram = region(0x06023450, 32)
            ram.update(enumerate(pointer.to_bytes(4, 'big'), 0x0602345c))
            flags = [initial_t]
            before, regs, pr, after, writes, calls = execute(
                code, address, 0x06020000, ram, flags=flags, return_address=0x06054320)
            match = pointer == 0x06021b58
            expected = dict(ram)
            if match:
                expected.update(enumerate(bytes.fromhex('0600083c'), 0x0602345c))
            assert writes == ([(0x0602345c, 4, 0x0600083c)] if match else [])
            assert after == expected and not calls and pr == 0x06054320
            before[1:4] = [0x06020000, 0x0600083c if match else 0x06021b58, pointer]
            assert regs == before and flags == [int(match)]


def verify_vampire_handler(address, code):
    assert len(code) == 24
    for seed in (0, 1, 127, 255):
        for initial_t in (0, 1):
            ram = region(0x06014a40, 32)
            flags = [initial_t]
            before, regs, pr, after, writes, calls = execute(
                code, address, 0x0600d000, ram, seed=seed,
                return_address=0x06054320, flags=flags)
            expected = dict(ram)
            expected.update({0x06014a52: 0, 0x06014a53: 9})
            assert writes == [(0x06014a52, 2, 9)] and not calls
            before[1:3] = [0x0600d000, 9]
            assert after == expected and regs == before and pr == 0x06054320
            assert flags == [initial_t]


def check(elf):
    (patch_at, patch), (handler_at, handler), (cart_at, _) = linked_symbol_records(
        elf, ('_CYBER_BOTS_patch', '_CYBER_BOTS_handle1', '_ssctrl_set'))
    verify_cyber_handler(handler_at, handler)
    for seed in (0, 1, 127, 255):
        ram = region(0x060dd040, 64) | region(0x060feff0, 32)
        before, regs, pr, after, writes, calls = execute(
            patch, patch_at, 0x06012340, ram, external=cart_at, seed=seed)
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
    (patch_at, patch), (handler_at, handler) = linked_symbol_records(
        elf, ('_mshvssf_patch', '_mshvssf_handle1'))
    verify_marvel_handler(handler_at, handler)
    for initial_t in (0, 1):
        ram = region(0x060f0000, 48) | region(0x060f0120, 16)
        flags = [initial_t]
        before, regs, pr, after, writes, calls = execute(
            patch, patch_at, 0x06012340, ram, flags=flags)
        assert writes == [(0x060f000b, 1, 0x46), (0x060f001d, 1, 0x41),
                          (0x060f0124, 4, handler_at)] and not calls
        expected = dict(ram)
        for dest, width, value in writes:
            expected.update(enumerate(value.to_bytes(width, 'big'), dest))
        before[1], before[2] = 0x060f0124, handler_at
        assert after == expected and regs == before and pr == 0x06012340 and flags == [initial_t]
    print('Linked Marvel two-handoff redirection, both independent late guards '
          'and original continuation pass')
    (patch_at, patch), (handler_at, handler) = linked_symbol_records(
        elf, ('_Phantasy_Star_Collection_patch', '_Phantasy_Star_Collection_handle'))
    verify_phantasy_handler(handler_at, handler)
    for pointer in (0, 0x06020000, 0x06020004, 0xffffffff):
        for initial_t in (0, 1):
            ram = region(0x060053d0, 32)
            ram.update(enumerate(pointer.to_bytes(4, 'big'), 0x060053dc))
            flags = [initial_t]
            before, regs, pr, after, writes, calls = execute(
                patch, patch_at, 0x06012340, ram, flags=flags)
            match = pointer == 0x06020000
            expected = dict(ram)
            if match:
                expected.update(enumerate(handler_at.to_bytes(4, 'big'), 0x060053dc))
            assert writes == ([(0x060053dc, 4, handler_at)] if match else [])
            before[1:4] = [0x060053dc, handler_at if match else 0x06020000, pointer]
            assert after == expected and regs == before and flags == [int(match)]
            assert not calls and pr == 0x06012340
    print('Linked Phantasy launch and late pointer predicates, all writes, '
          'register/T effects and native continuation pass')
    (patch_at, patch), (handler_at, handler) = linked_symbol_records(
        elf, ('_VAMPIRE_SAVIOR_patch', '_VAMPIRE_SAVIOR_handle'))
    verify_vampire_handler(handler_at, handler)
    for initial_t in (0, 1):
        ram = (region(0x060a02d0, 16) | region(0x060a4420, 32) |
               region(0x060a03b0, 16) | region(0x060a4470, 16))
        flags = [initial_t]
        before, regs, pr, after, writes, calls = execute(
            patch, patch_at, 0x06012340, ram, flags=flags)
        assert writes == [(0x060a02d4, 1, 0x43), (0x060a442c, 1, 0x43),
                          (0x060a03b8, 4, handler_at), (0x060a447c, 4, handler_at)]
        expected = dict(ram)
        for dest, width, value in writes:
            expected.update(enumerate(value.to_bytes(width, 'big'), dest))
        before[1:3] = [handler_at, 0x060a447c]
        assert after == expected and regs == before and pr == 0x06012340
        assert not calls and flags == [initial_t]
    print('Linked Vampire both dispatch edits, exact later NOP, fixed entry, '
          'register/T effects and return state pass')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    check(parser.parse_args().elf)
