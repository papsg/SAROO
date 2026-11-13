"""Verify the declared game RAM payload in a stock SH-2 menu ELF."""
import argparse
from pathlib import Path

from check_menu_clock_sh2 import linked_symbols

STUB = bytes.fromhex(
    'd1096112 410be401 bc440009 e301c71b 91096202 2211bae5 '
    '1032e009 61c37154 71704c2b 21018001 06000320 000b0009'
)
# SH-2 longword load from @R0+ into R1–R4, followed by stores to R5.
LOADS = bytes.fromhex('6106 6206 6306 6406')
STORES = (
    bytes.fromhex('2512 1521 1532 1543'),
    bytes.fromhex('1514 1525 1536 1547'),
    bytes.fromhex('1518 1529 153a 154b'),
)
CALLBACK = ('_VANDAL_HEARTS_CN_patch',)
SYMBOLS = (*CALLBACK, 'VANDAL_HEARTS_hex')


def check(elf, baseline=None):
    callback, declared = linked_symbols(elf, SYMBOLS)
    assert declared == STUB, 'Expected all 48 copied bytes to belong to the stub'
    copy = b''.join(LOADS + stores for stores in STORES)
    assert copy in callback, 'The callback no longer copies 12 consecutive longwords'
    start = callback.index(STUB)
    if baseline is not None:
        previous, = linked_symbols(baseline, CALLBACK)
        assert previous[:start] == callback[:start], 'The guards or copy instructions changed'
        assert previous[start:start + len(STUB)] == STUB, 'The old RAM payload differs'
    print('48 declared stub bytes and all 12 copy stores match')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    parser.add_argument('--baseline', type=Path)
    args = parser.parse_args()
    check(args.elf, args.baseline)
