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

    def test_clean_packets_remain_exact(self):
        expected = packet(1) + packet(2)
        self.repair("clean.cdg", expected, expected)


if __name__ == "__main__":
    unittest.main()
