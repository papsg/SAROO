"""Test the linked ROM clock hook at its actual WRAM destinations.

Adapted from paps-g ff3f9d2f. Only the instructions in these two fragments,
BIOS calls and MMIO are modeled.
"""
from pathlib import Path
import struct
import sys

HOOK = 0x06000c80
ISSUE = 0x060006b8
ASR = 0x25fe00b0
AREF = ASR + 8
SF = 0x20100063
COM = 0x2010001f
EXTEN = 0x25f80002
VCNT = EXTEN + 8
BUDGET = 1 << 24


def signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def execute(hook, issue, *, before_busy=0, after_busy=0, unsafe=0,
            external=False, line=32, seed=0, test_budget=None):
    if len(hook) != 96 or len(issue) != 36:
        raise ValueError('ROM clock fragments exceed their reserved WRAM extents')
    memory = dict(enumerate(hook, HOOK)) | dict(enumerate(issue, ISSUE))
    regs = [0x13570000 + 0x100*i + seed for i in range(16)]
    regs[15] = 0x060ff000
    initial = regs.copy()
    caller = 0x06012340
    pc, pr, t, delayed = HOOK, caller, 0, None
    events = []
    asr = aref = prepared = clocked = submitted = completed = 0
    before_reads = after_reads = latch_reads = 0
    last_busy = last_unsafe = False
    sf_written = budget_seen = False

    def require(ok, message):
        if not ok:
            raise ValueError(message)

    def raw(address, width):
        require(all(address+i in memory for i in range(width)),
                f'Clock hook reads outside its retained code/stack: {address:08x}')
        return int.from_bytes(bytes(memory[address+i] for i in range(width)), 'big')

    def read(address, width):
        nonlocal before_reads, after_reads, completed
        nonlocal latch_reads, last_busy, last_unsafe, budget_seen
        if address == SF:
            require(width == 1, 'SF must use byte access')
            if not budget_seen:
                require(regs[2] == BUDGET, 'Clock wait budget changed')
                budget_seen = True
                if test_budget is not None:
                    require(0 < test_budget < BUDGET, 'Invalid reduced test budget')
                    regs[2] = test_budget
            if submitted:
                last_busy = after_reads < after_busy
                after_reads += 1
                completed = not last_busy
            else:
                last_busy = before_reads < before_busy
                before_reads += 1
            return 0xa0 | int(last_busy)  # unrelated SF bits are not busy
        if address == EXTEN:
            require(width == 2 and not last_busy and not submitted,
                    'Unsafe software-latch request')
            latch_reads += 1
            return 0x0200 if external else 0
        if address == VCNT:
            require(width == 2 and latch_reads and not external,
                    'Counter read lacks an owned fresh latch')
            current = 250 if latch_reads <= unsafe else line
            last_unsafe = bool(current & 0x0380)
            return current
        return raw(address, width)

    def write(address, width, value):
        nonlocal asr, aref, sf_written, submitted
        value &= (1 << (8*width))-1
        if address == ASR:
            require(width == 4 and clocked and value == 0x34403440, 'Invalid ASR restore')
            asr = value
            events.append('asr')
        elif address == AREF:
            require(width == 4 and prepared and value == 0x13, 'Invalid AREF restore')
            aref = value
            events.append('aref')
        elif address == SF:
            require(width == 1 and value == 1 and not submitted and not sf_written
                    and not last_busy and latch_reads and not last_unsafe
                    and not external and clocked, 'Unadmitted reset-enable command')
            sf_written = True
            events.append('sf')
        elif address == COM:
            require(width == 1 and value == 0x19 and sf_written and not submitted,
                    'Missing or incorrect reset-enable command')
            submitted = 1
            events.append('resenab')
        else:
            require(address == initial[15]-4 and width == 4,
                    f'Unexpected ROM hook write: {address:08x}')
            for i, byte in enumerate(value.to_bytes(width, 'big')):
                memory[address+i] = byte

    for _ in range(10000):
        if pc in (0x04c8, 0x1800):
            if pc == 0x04c8:
                require(not prepared and not clocked, 'Repeated BIOS clock call')
                prepared, asr, aref = 1, 0, 0
                events.append('clock')
            else:
                require(prepared and not clocked and aref == 0x13,
                        'SCU initialization lost its preceding refresh restore')
                clocked, asr, aref = 1, 0x1ff01ff0, 0x1f
                events.append('scu')
            # Neither BIOS helper implements the replaced tail's RESENAB.
            # Clobber every caller-saved GPR; the fragments cannot rely on them.
            for i in range(8):
                regs[i] = 0xdead0000 + 4*i
            pc = pr
            continue
        if pc == caller:
            require(prepared and clocked and asr == 0x34403440 and aref == 0x13,
                    'Incomplete BIOS/bus handoff')
            require(regs[8:16] == initial[8:16] and pr == caller,
                    'Clock hook damaged callee-saved registers, stack or PR')
            require(events[:5] == ['clock', 'aref', 'scu', 'asr', 'aref'],
                    'Bus/refresh ordering changed')
            return dict(submitted=submitted, completed=bool(completed),
                        before_reads=before_reads, after_reads=after_reads,
                        latch_reads=latch_reads, events=events)
        require(HOOK <= pc < HOOK+96 or ISSUE <= pc < ISSUE+36,
                f'Clock fetch escaped retained WRAM: {pc:08x}')
        op = raw(pc, 2)
        n, m = (op >> 8) & 15, (op >> 4) & 15
        pending, delayed = delayed, None
        next_pc = pc + 2
        if op >> 12 in (9, 13):
            width = 2 if op >> 12 == 9 else 4
            address = pc+4+(op & 255)*2 if width == 2 else ((pc+4)&~3)+(op&255)*4
            regs[n] = signed(read(address, width), 16) & 0xffffffff if width == 2 else read(address, width)
        elif op >> 12 == 14:
            regs[n] = signed(op & 255, 8) & 0xffffffff
        elif op >> 12 == 7:
            regs[n] = (regs[n] + signed(op & 255, 8)) & 0xffffffff
        elif op & 0xf00f in (0x6000, 0x6001, 0x6002):
            width = 1 << (op & 3)
            value = read(regs[m], width)
            regs[n] = signed(value, width*8) & 0xffffffff
        elif op & 0xf00f in (0x2000, 0x2001, 0x2002):
            write(regs[n], 1 << (op & 3), regs[m])
        elif op >> 12 == 1:
            write(regs[n] + (op & 15)*4, 4, regs[m])
        elif op & 0xff00 == 0x8500:
            regs[0] = signed(read(regs[m] + (op & 15)*2, 2), 16) & 0xffffffff
        elif op & 0xf00f == 0x6008:
            regs[n] = (regs[m] & 0xffff0000) | ((regs[m]&255)<<8) | ((regs[m]>>8)&255)
        elif op & 0xf00f == 0x2008:
            t = not (regs[n] & regs[m])
        elif op & 0xff00 == 0xc800:
            t = not (regs[0] & (op & 255))
        elif op & 0xf0ff in (0x4018, 0x4028):
            regs[n] = (regs[n] << (8 if op & 255 == 0x18 else 16)) & 0xffffffff
        elif op & 0xf0ff == 0x4010:
            regs[n] = (regs[n]-1) & 0xffffffff
            t = regs[n] == 0
        elif op & 0xff00 in (0x8900, 0x8b00):
            if bool(t) == (op & 0xff00 == 0x8900):
                next_pc = pc+4+signed(op&255, 8)*2
        elif op & 0xf0ff == 0x4022:
            regs[n] -= 4
            write(regs[n], 4, pr)
        elif op & 0xf0ff == 0x4026:
            pr = read(regs[n], 4)
            regs[n] += 4
        elif op & 0xf0ff == 0x400b:
            pr, delayed = pc+4, regs[n]
        elif op >> 12 == 11:
            pr, delayed = pc+4, pc+4+signed(op&4095, 12)*2
            require(delayed == ISSUE, 'Clock BSR misses the retained issue entry')
        elif op == 0x000b:
            delayed = pr
        elif op != 0x0009:
            raise ValueError(f'Unsupported linked clock opcode {op:04x}')
        require(pending is None or delayed is None, 'Branch in delay slot')
        pc = pending if pending is not None else next_pc
    raise ValueError('Clock hook exceeded bounded execution fixture')


def linked_fragments(path):
    image = path.read_bytes()
    if image[:7] != b'\x7fELF\x01\x02\x01':
        raise ValueError('Expected a big-endian ELF32 menu')
    header = struct.unpack_from('>HHIIIIIHHHHHH', image, 16)
    if header[1] != 42:
        raise ValueError('Expected an SH ELF')
    sections = [struct.unpack_from('>10I', image, header[5] + i * header[10])
                for i in range(header[11])]
    symbols = next(section for section in sections if section[1] == 2)
    names = sections[symbols[6]]
    found = {}
    for offset in range(symbols[4], symbols[4] + symbols[5], symbols[9]):
        name, address, size, _, _, section = struct.unpack_from('>IIIBBH', image, offset)
        start = names[4] + name
        name = image[start:image.index(b'\0', start)].decode('ascii')
        if name in ('_jump_06b0', '_rom_reset_issue'):
            owner = sections[section]
            location = owner[4] + address - owner[3]
            assert owner[4] <= location <= owner[4] + owner[5] - size
            found[name] = image[location:location + size]
    return found['_jump_06b0'], found['_rom_reset_issue']


def verify(hook, issue):
    count = 0
    for seed in range(4):
        for line in (0, 1, 126, 127, 0xfc7f):
            for before_busy in (0, 3):
                result = execute(hook, issue, line=line, before_busy=before_busy,
                                 after_busy=4, unsafe=2, seed=seed)
                assert result['submitted'] == 1 and result['completed']
                assert result['latch_reads'] == 3 and result['after_reads'] == 5
                count += 1

    for inputs in ({'before_busy': 100}, {'external': True}, {'line': 128},
                   {'line': 191}, {'line': 224}, {'line': 511}, {'line': 767}):
        result = execute(hook, issue, test_budget=9, **inputs)
        assert not result['submitted'] and result['before_reads'] == 9
        count += 1
    result = execute(hook, issue, before_busy=3, after_busy=100, test_budget=9)
    assert result['submitted'] and not result['completed']
    assert result['before_reads'] + result['after_reads'] == 9
    print(f'{count + 1} linked ROM clock cases pass at the relocated WRAM addresses')


if __name__ == '__main__':
    verify(*linked_fragments(Path(sys.argv[1])))
