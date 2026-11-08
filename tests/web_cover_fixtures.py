"""Generate original synthetic artwork for offline browser checks (ignored output)."""
from pathlib import Path
import struct
import argparse
import sys
import zlib

from cover_fixtures import bmp, library


def generate(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    entries = []
    for i in range(50):
        w, h = [(128, 192), (128, 128), (37, 59)][i % 3]
        _, palette, pixels = bmp(w, h)
        entries.append((f'GAME{i:08}'.encode(), i, w, h, palette, pixels))
    data = bytearray(library(entries))
    data[24:32] = b'RESERVED'
    data.extend(b'Untouched trailer is part of the original file.')
    (root / 'library.bin').write_bytes(data)
    (root / 'short.bin').write_bytes(b'Short fixture')
    (root / 'unidentified.bmp').write_bytes(bmp(3, 7)[0])
    (root / 'cover_BATCH~22222222.bmp').write_bytes(bmp(37, 59)[0])
    header = bytearray(256)
    header[:15] = b'SEGA SEGASATURN '
    header[32:48] = b'MK-81000 V1.000  '
    header[48:112] = bytes(range(64))
    (root / 'disc.bin').write_bytes(header)

    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
    rows = bytearray()
    for y in range(512):
        rows.append(0)
        for x in range(512):
            rows.extend((x // 2, y // 2, (x * 3 + y * 7) % 256, 255))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', 512, 512, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
    (root / 'cover_COLOR~11111111.png').write_bytes(png)


def full_capacity(root):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    count, index, payload = 16384, 524288, 25600
    headers = bytearray(index)
    for i in range(count):
        struct.pack_into('<12sIIHH', headers, i * 32, f'GAME{i:08}'.encode(),
                         i, index + i * payload, 128, 192)
    palette = bytes((i * 13 + 7) % 256 for i in range(1024))
    pixels = bytearray((i * 19 + 11) % 256 for i in range(128 * 192))
    with (root / 'full-unique.bin').open('wb') as output:
        output.write(headers)
        for i in range(count):
            struct.pack_into('<H', pixels, 0, i)
            output.write(palette)
            output.write(pixels)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tests/build/web-cover-browser')
    parser.add_argument('--capacity', action='store_true')
    args = parser.parse_args()
    (full_capacity if args.capacity else generate)(args.output)
