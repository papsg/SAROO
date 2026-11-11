"""Exercise the actual CD+G repair CLI with independent generated packets."""
# Packet oracle adapted from paps-g fee3293f.
import os
from pathlib import Path
import random
import shlex
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools/cdgtools/cdgfix.c"
PACKET_BYTES = 24


def compiler():
    batch = int(os.environ.get("SAROO_CDG_BATCH_FRAMES", "64"))
    release = os.name == "nt" or os.environ.get("SAROO_CDG_PROFILE") == "release"
    profile = ["-O2"] if release else [
        "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    return [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11",
            "-D_FILE_OFFSET_BITS=64", f"-DCDG_BATCH_FRAMES={batch}",
            "-Wall", "-Wextra", "-Werror", *profile]


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


def raw_case(frames, damage=True, marker=True, late_only=False):
    rng = random.Random(200 + frames)
    symbols = bytearray(rng.randrange(256) for _ in range(frames * FRAME_BYTES))
    # Keep the original CLI's first-byte marker for the historical control.
    if marker:
        for index in range(8):
            symbols[index] |= 0x80
    else:
        symbols[0] &= 0x7f
    positions = list(range(0, len(symbols) - max(INTERLEAVE), PACKET_BYTES))
    for position in positions:
        for offset, value in zip(INTERLEAVE, packet(position // PACKET_BYTES)):
            symbols[position + offset] = (symbols[position + offset] & 0xc0) | value
    expected = raw_bytes(symbols)
    if damage:
        damaged = positions[-1:] if late_only else positions[:1] + positions[252:260] + positions[-1:]
        for position in set(damaged):
            symbols[position + INTERLEAVE[9]] ^= 1
    return raw_bytes(symbols), expected, len(positions)


class CDGFixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        output = tempfile.TemporaryDirectory(prefix="saroo-cdgfix-")
        cls.addClassCleanup(output.cleanup)
        cls.directory = Path(output.name)
        cls.binary = cls.directory / ("cdgfix.exe" if os.name == "nt" else "cdgfix")
        source = Path(os.environ.get("SAROO_CDG_SOURCE", SOURCE))
        subprocess.run([*compiler(), "-Wno-unused-parameter", "-I", str(SOURCE.parent),
                        str(source), "-o", str(cls.binary)], check=True, timeout=45)

    def repair(self, name, data, expected, status=0, args=()):
        path = self.directory / name
        path.write_bytes(data)
        result = subprocess.run([str(self.binary), *args, str(path)], stdin=subprocess.DEVNULL,
                                capture_output=True, timeout=10,
                                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
        self.assertEqual(result.returncode, status, result.stdout + result.stderr)
        self.assertEqual(path.read_bytes(), expected)
        self.assertFalse(list(self.directory.glob(".saroo-cdg-*")))
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
        self.repair("failed-only.cdg", bad, bad, 2)
        self.repair("mixed.cdg", bad + broken, bad + good, 2)

    def test_raw_complete_endpoint_and_untouched_channel_bits(self):
        self.assertEqual(sorted(offset % PACKET_BYTES for offset in INTERLEAVE),
                         list(range(PACKET_BYTES)))
        for frames in (1, 2, 3, 63, 64, 65, 66, 67, 128, 129, 130):
            with self.subTest(frames=frames):
                broken, expected, count = raw_case(frames)
                run = self.repair("capture.sub", broken, expected)
                self.assertIn(f"{count} packets checked".encode(), run.stdout)
        clean, _, _ = raw_case(3, damage=False)
        self.repair("clean.sub", clean, clean)

    def test_logical_windows_and_late_first_repair(self):
        for count in (1, 255, 256, 257, 263, 264, 265, 273, 1024):
            with self.subTest(packets=count):
                expected = b"".join(packet(i) for i in range(count))
                broken = bytearray(expected)
                broken[-16] ^= 1
                run = self.repair("late.cdg", broken, expected)
                self.assertIn(f"{count} packets checked".encode(), run.stdout)

    def test_raw_late_first_repair_without_marker(self):
        broken, expected, count = raw_case(130, marker=False, late_only=True)
        run = self.repair("capture.SUB", broken, expected)
        self.assertIn(f"{count} packets checked".encode(), run.stdout)

    def test_input_validation_and_explicit_format(self):
        for length in (0, 1, 23, 25, 95, 97):
            with self.subTest(length=length):
                data = bytes(length)
                self.repair("truncated.cdg", data, data, 1)
        self.repair("partial.sub", packet(), packet(), 1)
        incomplete = bytearray(packet())
        incomplete[8] ^= 1
        incomplete.append(0)
        self.repair("trailing.cdg", incomplete, incomplete, 1)

        self.repair("logical.sub", packet(), packet(), args=("--format", "cdg"))
        broken, expected, _ = raw_case(3, marker=False)
        self.repair("unknown", broken, expected, args=("--format", "sub"))
        broken, expected, _ = raw_case(3)
        self.repair("legacy.unknown", broken, expected)
        self.repair("invalid.cdg", packet(), packet(), 1, args=("--format", "invalid"))
        for args in ([], [str(self.directory)], [str(self.directory / "missing.cdg")]):
            run = subprocess.run([str(self.binary), *args], stdin=subprocess.DEVNULL,
                                 capture_output=True, timeout=10)
            self.assertEqual(run.returncode, 1, run.stdout + run.stderr)

    @unittest.skipIf(os.name == "nt", "FIFO is a POSIX input control")
    def test_fifo_is_refused_without_waiting_for_a_writer(self):
        pipe = self.directory / "fifo.cdg"
        os.mkfifo(pipe)
        run = subprocess.run([str(self.binary), str(pipe)], stdin=subprocess.DEVNULL,
                             capture_output=True, timeout=10)
        self.assertEqual(run.returncode, 1, run.stdout + run.stderr)

    @unittest.skipIf(os.name == "nt", "POSIX link and metadata controls")
    def test_replacement_preserves_mode_owner_and_link_semantics(self):
        target = self.directory / "target.cdg"
        link = self.directory / "link.cdg"
        alias = self.directory / "alias.cdg"
        expected = packet()
        broken = bytearray(expected)
        broken[8] ^= 1
        target.write_bytes(broken)
        target.chmod(0o640)
        link.symlink_to(target.name)
        os.link(target, alias)
        if sys.platform == "darwin":
            attribute = "com.saroo.cdg-test"
            subprocess.run(["/usr/bin/xattr", "-w", attribute, "preserved", str(target)], check=True)
            subprocess.run(["/bin/chmod", "+a", "everyone allow read", str(target)], check=True)
            acl_before = subprocess.check_output(["/bin/ls", "-le", str(target)]).splitlines()[1:]
        before = target.stat()
        run = subprocess.run([str(self.binary), str(link)], stdin=subprocess.DEVNULL,
                             capture_output=True, timeout=10)
        after = target.stat()
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertTrue(link.is_symlink())
        self.assertEqual(target.read_bytes(), expected)
        self.assertEqual(alias.read_bytes(), broken)
        self.assertEqual((after.st_mode, after.st_uid, after.st_gid),
                         (before.st_mode, before.st_uid, before.st_gid))
        if sys.platform == "darwin":
            self.assertEqual(subprocess.check_output(["/usr/bin/xattr", "-p", attribute, str(target)]).strip(),
                             b"preserved")
            self.assertEqual(subprocess.check_output(["/bin/ls", "-le", str(target)]).splitlines()[1:], acl_before)
        self.assertFalse(list(self.directory.glob(".saroo-cdg-*")))

    def test_clean_packets_remain_exact(self):
        expected = packet(1) + packet(2)
        path = self.directory / "clean.cdg"
        path.write_bytes(expected)
        before = path.stat()
        run = subprocess.run([str(self.binary), str(path)], stdin=subprocess.DEVNULL,
                             capture_output=True, timeout=10)
        after = path.stat()
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertEqual(path.read_bytes(), expected)
        self.assertEqual((after.st_ino, after.st_mode, after.st_mtime_ns, after.st_ctime_ns),
                         (before.st_ino, before.st_mode, before.st_mtime_ns, before.st_ctime_ns))
        self.assertFalse(list(self.directory.glob(".saroo-cdg-*")))


if __name__ == "__main__":
    unittest.main()
