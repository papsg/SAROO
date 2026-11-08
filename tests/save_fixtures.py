"""Generated disposable save archives; independent of the JavaScript readers."""

import struct
import hashlib
import json
import zlib

BLOCK = 1024

IMAGE_BYTES = 0x800000

UNIT = 65536

NEW_DATE = bytes.fromhex("0102ccdd")

def raw_save(name, data, date=NEW_DATE):
    header = bytearray(64)
    header[:8] = b'SSAVERAW'
    header[0x10:0x10 + len(name)] = name
    struct.pack_into('>I', header, 0x1c, len(data))
    header[0x20:0x2a] = b'Generated!'
    header[0x2b] = 1
    header[0x2c:0x30] = date
    # Zero means no recorded CRC in the upstream format. Export is checked below.
    return header + data

def bup_save(name, data, date=NEW_DATE):
    header = bytearray(64)
    header[:4] = b'Vmem'
    header[16:16 + len(name)] = name
    header[28:38] = b'Generated!'
    header[39] = 1
    header[40:44] = date
    struct.pack_into('>I', header, 44, len(data))
    return header + data

def mems_fixture(entries):
    """Return a valid image and header/data extents for (directory slot, name, data)."""
    image = bytearray(IMAGE_BYTES)
    image[:8] = b'SaroMems'
    struct.pack_into('>I', image, 8, IMAGE_BYTES)
    image[16] = 0xff  # Header and directory reserve blocks 0 through 7.
    next_block = 8
    locations = []
    for slot, name, data in entries:
        header_block = next_block
        next_block += 1
        header = header_block * BLOCK
        directory = BLOCK + slot * 16
        image[directory:directory + len(name)] = name
        struct.pack_into('>H', image, directory + 14, header_block)
        image[header:header + len(name)] = name
        struct.pack_into('>I', image, header + 12, len(data))
        image[header + 16:header + 26] = b'Generated!'
        image[header + 27] = 1
        image[header + 28:header + 32] = b'\x01\x02\x03\x04'
        extents = []
        if len(data) <= BLOCK - 64:
            extents.append((header + 64, len(data)))
        else:
            for index, offset in enumerate(range(0, len(data), BLOCK)):
                struct.pack_into('>H', image, header + 64 + index * 2, next_block)
                extents.append((next_block * BLOCK, min(BLOCK, len(data) - offset)))
                next_block += 1
        consumed = 0
        for offset, size in extents:
            image[offset:offset + size] = data[consumed:consumed + size]
            consumed += size
        locations.append((header, extents))
    for block in range(8, next_block):
        image[16 + block // 8] |= 1 << (block % 8)
    struct.pack_into('>H', image, 12, 8064 - next_block)
    return image, locations

def slot_fixture(entries=(), gameids=(b'GAME',)):
    image = bytearray((len(gameids) + 1) * UNIT)
    image[:16] = b'Saroo Save File\0'
    for index, gameid in enumerate(gameids, 1):
        if gameid is None:
            continue
        image[index * 16:index * 16 + len(gameid)] = gameid
        base = index * UNIT
        image[base:base + 8] = b'SaroSave'
        struct.pack_into('>IHH', image, base + 8, UNIT, 128, 511)
        image[base + 32:base + 32 + len(gameid)] = gameid
        image[base + 64] = 1
    base, next_block, last = UNIT, 1, 0
    locations = []
    for name, data in entries:
        header = next_block
        next_block += 1
        bp = base + header * 128
        struct.pack_into('>H', image, base + last * 128 + 62, header)
        image[bp:bp + len(name)] = name
        struct.pack_into('>I', image, bp + 12, len(data))
        image[bp + 16:bp + 26] = b'Generated!'
        image[bp + 27] = 1
        image[bp + 28:bp + 32] = b'\x01\x02\x03\x04'
        extents = []
        for offset in range(0, len(data), 128):
            block = next_block
            next_block += 1
            image[bp + 64 + block // 8] |= 1 << (block % 8)
            count = min(128, len(data) - offset)
            start = base + block * 128
            image[start:start + count] = data[offset:offset + count]
            extents.append((start, count))
        locations.append((bp, extents))
        last = header
    for block in range(1, next_block):
        image[base + 64 + block // 8] |= 1 << (block % 8)
    struct.pack_into('>H', image, base + 14, 512 - next_block)
    return image, locations

def saturn_fixture(entries):
    """Encode metadata, terminated block IDs, then payload as one logical stream.

    Each block reserves a four-byte tag; the first also reserves thirty metadata
    bytes. Thus N blocks hold 60*N - 30 - 2*N payload bytes. This declarative
    packing does not use the reader's table-cursor algorithm.
    """
    image = bytearray(32768)
    image[:64] = b'BackUpRam Format' * 4
    free = list(range(2, 512, 2)) + list(range(3, 512, 2))
    locations = []
    for name, data in entries:
        count = max(1, (len(data) + 30 + 57) // 58)
        assert count <= len(free)
        blocks, free = free[:count], free[count:]
        header = blocks[0] * 64
        struct.pack_into('>I', image, header, 0x80000000)
        image[header + 4:header + 15] = name.ljust(11, b'\0')
        image[header + 15] = 1
        image[header + 16:header + 26] = b'Generated!'
        image[header + 26:header + 30] = b'\x01\x02\x03\x04'
        struct.pack_into('>I', image, header + 30, len(data))
        stream = b''.join(struct.pack('>H', block) for block in blocks[1:]) + b'\0\0' + data
        positions = [header + i for i in range(34, 64)]
        positions += [block * 64 + i for block in blocks[1:] for i in range(4, 64)]
        for offset, value in zip(positions, stream): image[offset] = value
        locations.append(dict(header=header, blocks=blocks, table=positions[:count * 2:2],
                              payload=positions[count * 2:count * 2 + len(data)]))
    return image, locations

def disk_bytes(image, odd):
    if not odd: return image
    encoded = bytearray([255]) * 65536
    encoded[1::2] = image
    return encoded

def fixtures(root):
    manifest = {"reads": [], "rejects": []}

    def write(name, image):
        (root / name).write_bytes(image)
        return name

    # The existing Python encoders predate and are independent of this JS core.
    for kind, sizes in [('saroo', [0, 1, 127, 128, 129, 4097, 65280]),
                        ('mems', [0, 1, 959, 960, 961, 1024, 1025, 490496]),
                        ('saturn', [0, 1, 2, 27, 28, 29, 30, 31, 86, 87, 144, 145,
                                    782, 783, 840, 841, 898, 899, 2580, 2581, 29548, 29549, 29550])]:
        for size in sizes:
            data = bytes((i * 13 + 17) % 251 for i in range(size))
            if kind == 'saroo': image, _ = slot_fixture([(b'SAVE_A00001', data)])
            elif kind == 'mems': image, _ = mems_fixture([(447, b'SAVE_A00001', data)])
            else: image, _ = saturn_fixture([(b'SAVE_A00001', data)])
            for odd in ([False, True] if kind == 'saturn' else [False]):
                name = write(f'{kind}-{size}-{int(odd)}.bin', disk_bytes(image, odd))
                manifest['reads'].append(dict(file=name, type=kind, size=size, odd=odd,
                    payload_sha256=hashlib.sha256(data).hexdigest()))

    for kind in ['saroo', 'mems']:
        if kind == 'saroo':
            image, locations = slot_fixture([(b'A\0PAD123456', b'A' * 129),
                                              (b'NEIGHBOR001', b'neighbor' * 19)],
                                             (b'GAME', None, b'OTHER'))
            # Unreferenced hole data must survive until explicitly creating it.
            image[2 * 65536:3 * 65536] = b'Z' * 65536
            base, width = 65536, 128
            struct.pack_into('>H', image, base + 14, 0)  # stale cached free count
        else:
            image, locations = mems_fixture([(0, b'A\0PAD123456', b'A' * 129),
                                               (447, b'NEIGHBOR001', b'neighbor' * 19)])
            base, width = 0, 1024
            struct.pack_into('>H', image, 12, 0)
            # Identical raw bytes after NUL remain part of both stored names.
            image.extend(b'Trailing bytes are preserved.')
        header, extents = locations[0]
        image[header + 16:header + 26] = b'\x82\xa0\0RAW\xffPAD'
        image[header + 32:header + 40] = b'RESERVED'
        expected = bytearray(image)
        struct.pack_into('>I', expected, header + 28, 0xffffffff)
        data = bytes((i * 5 + 9) % 256 for i in range(129))
        cursor = 0
        for offset, count in extents:
            expected[offset:offset + count] = data[cursor:cursor + count]; cursor += count
        write(f'{kind}-preserve.bin', image)
        write(f'{kind}-preserve-expected.bin', expected)
        # On exhaustion even an inline/empty save still needs a header block.
        full = bytearray(image)
        first, count = (16, 1008) if kind == 'mems' else (65536 + 64, 64)
        full[first:first + count] = b'\xff' * count
        write(f'{kind}-full.bin', full)
        if kind == 'saroo':
            malformed = {'signature': (base, b'DAMAGED!'), 'header-cycle': (header + 62, struct.pack('>H', 1)),
                         'missing-data': (header + 64, bytes(64)), 'geometry': (base + 12, bytes(2)),
                         'reserved-data': (header + 64, bytes([image[header + 64] | 1])),
                         'huge-size': (header + 12, b'\xff' * 4)}
        else:
            malformed = {'geometry': (8, bytes(4)), 'reserved': (16, b'\xfe'),
                         'shared-header': (1024 + 447 * 16 + 14, struct.pack('>H', header // width)),
                         'name': (header, b'Z'), 'name-after-nul': (header + 10, b'Z'),
                         'huge-size': (header + 12, b'\xff' * 4)}
        for label, (offset, replacement) in malformed.items():
            damaged = bytearray(image); damaged[offset:offset + len(replacement)] = replacement
            name = write(f'reject-{kind}-{label}.bin', damaged)
            manifest['rejects'].append(dict(file=name, type=kind))

    mems, locations = mems_fixture([(0, b'FIRST', b'F' * 2049), (1, b'SECOND', b'S' * 2049)])
    first, second = locations
    for label, offset, value in [('shared-data', first[0] + 64, second[1][0][0] // 1024),
                                  ('header-data', first[0] + 64, second[0] // 1024),
                                  ('terminator', first[0] + 70, 8),
                                  ('outside-data', first[0] + 64, 8064)]:
        damaged = bytearray(mems); struct.pack_into('>H', damaged, offset, value)
        manifest['rejects'].append(dict(file=write(f'reject-mems-{label}.bin', damaged), type='mems'))
    saturn, locations = saturn_fixture([(b'FIRST', b'F' * 2049), (b'SECOND', b'S' * 2049)])
    for label, value in [('header', locations[1]['blocks'][0]), ('shared', locations[1]['blocks'][1]),
                          ('reserved', 1), ('outside', 512), ('missing', 0)]:
        damaged = bytearray(saturn); struct.pack_into('>H', damaged, locations[0]['table'][0], value)
        manifest['rejects'].append(dict(file=write(f'reject-saturn-{label}.bin', damaged), type='saturn'))

    for kind, encode in [('SRO', raw_save), ('BUP', bup_save)]:
        envelope = encode(b'\x82\xa0\0RAWBYTES', bytes(range(256)) * 4096)
        if kind == 'SRO': struct.pack_into('>I', envelope, 12, zlib.crc32(envelope[16:]))
        else: envelope[52:56] = envelope[40:44]
        write(f'input.{kind}', envelope)
    write('saroo-empty.bin', slot_fixture()[0])
    write('mems-empty.bin', mems_fixture([])[0])
    write('saroo-directory-full.bin', slot_fixture([(f'S{i:010}'.encode(), b'') for i in range(511)])[0])
    write('mems-directory-full.bin', mems_fixture([(i, f'S{i:010}'.encode(), b'') for i in range(448)])[0])
    write('saturn-directory-full.bin', saturn_fixture([(f'S{i:010}'.encode(), b'') for i in range(510)])[0])
    # Fully owned exhaustion, without orphaned allocations: one inline save,
    # sixteen maximum records and the final 374-block payload fill MEMS exactly.
    owned = [(0, b'FIRST', bytes(129))]
    owned += [(i+1, f'FULL{i:07}'.encode(), bytes(479*1024)) for i in range(16)]
    owned.append((17, b'LAST', bytes(374*1024)))
    write('mems-all-owned-full.bin', mems_fixture(owned)[0])
    (root / 'fixtures.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest
