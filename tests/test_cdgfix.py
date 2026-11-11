"""Exercise the actual CD+G repair CLI with independent generated packets."""
# Packet oracle adapted from paps-g fee3293f.
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/cdgtools/cdgfix.c"
PACKET_BYTES = 24


def multiply(a, b):
    value = 0
    while b:
        if b & 1:
            value ^= a
        b >>= 1
        a <<= 1
        if a & 64:
            a ^= 0x43
    return value


def parity(data, symbols):
    """Divide by the generator in GF(64), independently of the C tables."""
    generator, power = [1], 1
    for _ in range(symbols):
        product = [0] * (len(generator) + 1)
        for i, coefficient in enumerate(generator):
            product[i] ^= coefficient
            product[i + 1] ^= multiply(coefficient, power)
        generator, power = product, multiply(power, 2)
    work = [value & 63 for value in data] + [0] * symbols
    for i in range(len(data)):
        factor = work[i]
        for j, coefficient in enumerate(generator):
            work[i + j] ^= multiply(coefficient, factor)
    return bytes(work[-symbols:])


def packet(seed=0):
    rng = random.Random(seed)
    value = bytearray([9, seed % 64, 0, 0] + [rng.randrange(64) for _ in range(16)] + [0] * 4)
    value[2:4] = parity(value[:2], 2)
    value[20:] = parity(value[:20], 4)
    return bytes(value)


# Original SAROO channel-separated SUB layout. The fixture builds symbols
# first and transposes whole frames to channel bytes independently of the CLI.
FRAME_BYTES = 96
INTERLEAVE = (0, 66, 125, 191, 100, 50, 150, 175, 8, 33, 58, 83,
              108, 133, 158, 183, 16, 41, 25, 91, 116, 141, 166, 75)


def raw_bytes(symbols):
    result = bytearray()
    for frame in range(0, len(symbols), FRAME_BYTES):
        for channel in range(8):
            for byte in range(12):
                result.append(sum(
                    ((symbols[frame + byte * 8 + bit] >> (7 - channel)) & 1) << (7 - bit)
                    for bit in range(8)))
    return bytes(result)


def raw_case(frames, damage=True):
    rng = random.Random(200 + frames)
    symbols = bytearray(rng.randrange(256) for _ in range(frames * FRAME_BYTES))
    # Keep the original CLI's first-byte marker for the historical control.
    for index in range(8):
        symbols[index] |= 0x80
    positions = list(range(0, len(symbols) - max(INTERLEAVE), PACKET_BYTES))
    for position in positions:
        for offset, value in zip(INTERLEAVE, packet(position // PACKET_BYTES)):
            symbols[position + offset] = (symbols[position + offset] & 0xc0) | value
    expected = raw_bytes(symbols)
    if damage:
        for position in set(positions[:1] + positions[252:260] + positions[-1:]):
            symbols[position + INTERLEAVE[9]] ^= 1
    return raw_bytes(symbols), expected, len(positions)


class CDGFixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        output = tempfile.TemporaryDirectory(prefix="saroo-cdgfix-")
        cls.addClassCleanup(output.cleanup)
        cls.directory = Path(output.name)
        cls.binary = cls.directory / "cdgfix"
        source = Path(os.environ.get("SAROO_CDG_SOURCE", SOURCE))
        cc = shlex.split(os.environ.get("CC", "cc"))
        flags = ["-O2"] if os.environ.get("SAROO_CDG_PROFILE") == "release" else [
            "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run([*cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-parameter", *flags, str(source), "-o", str(cls.binary)],
                       check=True, timeout=45)

    def repair(self, name, data, expected, status=0):
        path = self.directory / name
        path.write_bytes(data)
        result = subprocess.run([str(self.binary), str(path)], stdin=subprocess.DEVNULL,
                                capture_output=True, timeout=10,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
        self.assertEqual(result.returncode, status, result.stdout + result.stderr)
        self.assertEqual(path.read_bytes(), expected)
        return result

    def test_every_single_symbol_error_is_corrected(self):
        expected = packet(4)
        for index in range(PACKET_BYTES):
            with self.subTest(symbol=index):
                broken = bytearray(expected)
                broken[index] ^= index + 1
                self.repair("single.cdg", broken, expected)

    def test_q_repair_regenerates_p_even_after_p_corrected(self):
        expected = packet(4)
        broken = bytearray(expected)
        broken[2] ^= 1
        # The damaged Q header had been P-encoded before P also suffered a fault.
        broken[20:] = parity(broken[:20], 4)
        broken[20] ^= 1
        self.repair("both.cdg", broken, expected)

    def test_q_repair_keeps_upper_symbol_bits(self):
        expected = bytes(value | 0xc0 for value in packet(17))
        broken = bytearray(expected)
        broken[2] ^= 1
        broken[20:] = bytes(value | 0xc0 for value in parity(broken[:20], 4))
        self.repair("upper.cdg", broken, expected)

    def test_failed_packet_is_kept_while_other_repairs_are_saved(self):
        bad = bytearray(packet(3))
        # Q cannot correct both symbols. P has a separate correctable error.
        bad[2] ^= 1
        bad[3] ^= 1
        bad[20:] = parity(bad[:20], 4)
        bad[20] ^= 1
        good = packet(4)
        broken = bytearray(good)
        broken[8] ^= 1
        self.repair("failed-only.cdg", bad, bad)
        self.repair("mixed.cdg", bad + broken, bad + good)

    def test_raw_complete_endpoint_and_untouched_channel_bits(self):
        self.assertEqual(sorted(offset % PACKET_BYTES for offset in INTERLEAVE),
                         list(range(PACKET_BYTES)))
        for frames in (1, 2, 3, 63, 64, 65, 66, 67, 128, 129, 130):
            with self.subTest(frames=frames):
                broken, expected, _ = raw_case(frames)
                self.repair("capture.sub", broken, expected)
        clean, _, _ = raw_case(3, damage=False)
        self.repair("clean.sub", clean, clean)

    def test_clean_packets_remain_exact(self):
        expected = packet(1) + packet(2)
        self.repair("clean.cdg", expected, expected)


if __name__ == "__main__":
    unittest.main()
