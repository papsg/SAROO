"""Bounded instruction checks of retained callbacks; no game or timing model."""
import argparse
from pathlib import Path

from check_later_callbacks_sh2 import execute, region
from check_menu_clock_sh2 import linked_symbol_records


BATMAN_HANDLER = bytes.fromhex(
    'd108d209631233208b07d2082122d2081121d2081122d2081123d108412b0009'
    '000b000906023e44d439490bd23a612221188bfcd437490b6583eb000600d000')


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


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    check(parser.parse_args().elf)
