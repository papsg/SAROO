"""Independent binary encoders for synthetic cover tests, never user artwork."""
import struct

INDEX = 16384 * 32


def bmp(w=128, h=128, *, palette=None, pixels=None, dib=40, top=False, gap=0):
    palette = palette if palette is not None else bytes((i * 13 + 7) % 256 for i in range(1024))
    pixels = pixels if pixels is not None else bytes((i * 19 + 11) % 256 for i in range(w * h))
    stride = (w + 3) & ~3
    offset = 14 + dib + 1024 + gap
    head = bytearray(offset)
    struct.pack_into('<2sIHHI', head, 0, b'BM', offset + stride * h, 0, 0, offset)
    struct.pack_into('<IiiHHIIiiII', head, 14, dib, w, -h if top else h,
                     1, 8, 0, stride * h, 2835, 2835, 256, 0)
    head[14 + dib:14 + dib + 1024] = palette
    rows = [pixels[y * w:(y + 1) * w] + bytes([0xDA]) * (stride - w) for y in range(h)]
    return bytes(head) + b''.join(rows if top else rows[::-1]), palette, pixels



def library(entries):
    """Independent encoder: (serial bytes, IP hash, width, height, palette, pixels)."""
    headers = bytearray(INDEX)
    data = bytearray()
    for i, (serial, hashed, w, h, palette, pixels) in enumerate(entries):
        struct.pack_into('<12sIIHH8s', headers, i * 32, serial, hashed, INDEX + len(data), w, h, b'')
        data.extend(palette + pixels)
    return bytes(headers + data)
