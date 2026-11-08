"""Exercise the real save-tool CLI with generated, disposable MEMS archives."""
import hashlib
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
BLOCK = 1024
IMAGE_BYTES = 0x800000
NEW_DATE = bytes.fromhex("0102ccdd")


def raw_save(name, data):
    header = bytearray(64)
    header[:8] = b"SSAVERAW"
    header[16:16 + len(name)] = name
    struct.pack_into(">I", header, 28, len(data))
    header[32:42] = b"Generated!"
    header[43] = 1
    header[44:48] = NEW_DATE
    # Zero is the existing format's optional/absent CRC representation.
    return header + data


def archive_fixture(entries):
    image = bytearray(IMAGE_BYTES)
    image[:8] = b"SaroMems"
    struct.pack_into(">I", image, 8, IMAGE_BYTES)
    image[16] = 0xff  # Header and directory own blocks 0 through 7.
    next_block = 8
    locations = []
    for slot, name, data in entries:
        header_block = next_block
        next_block += 1
        header = header_block * BLOCK
        directory = BLOCK + slot * 16
        image[directory:directory + len(name)] = name
        struct.pack_into(">H", image, directory + 14, header_block)
        image[header:header + len(name)] = name
        struct.pack_into(">I", image, header + 12, len(data))
        image[header + 16:header + 26] = b"Generated!"
        image[header + 27] = 1
        image[header + 28:header + 32] = bytes.fromhex("01020304")
        if len(data) <= BLOCK - 64:
            extents = [(header + 64, len(data))]
        else:
            extents = []
            for index, offset in enumerate(range(0, len(data), BLOCK)):
                struct.pack_into(">H", image, header + 64 + index * 2, next_block)
                extents.append((next_block * BLOCK, min(BLOCK, len(data) - offset)))
                next_block += 1
        consumed = 0
        for offset, size in extents:
            image[offset:offset + size] = data[consumed:consumed + size]
            consumed += size
        locations.append((header, extents))
    for block in range(8, next_block):
        image[16 + block // 8] |= 1 << (block % 8)
    struct.pack_into(">H", image, 12, 8064 - next_block)
    return image, locations


def digest(data):
    return hashlib.sha256(data).hexdigest()


class MemsOverwriteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        selected = os.environ.get("SAROO_SAVETOOL_BINARY")
        if selected:
            cls.binary = Path(selected).resolve(strict=True)
            return
        cls.build = tempfile.TemporaryDirectory(prefix="saroo-mems-build-")
        cls.addClassCleanup(cls.build.cleanup)
        cls.binary = Path(cls.build.name) / "sst"
        source = ROOT / "tools/savetool"
        command = [*shlex.split(os.environ.get("CC", "cc")), "-std=gnu11",
                   "-O1", "-g", "-Wall", "-fno-strict-aliasing",
                   "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                   *[str(source / name) for name in
                     ("main.c", "sr_bup.c", "ss_bup.c", "sr_mems.c")],
                   "-o", str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def invoke(self, directory, *args):
        environment = dict(os.environ)
        environment["UBSAN_OPTIONS"] = "halt_on_error=1"
        # The original CLI leaves process-lifetime input buffers allocated.
        # This regression checks bounds and archive preservation, not leaks.
        environment["ASAN_OPTIONS"] = "detect_leaks=0"
        result = subprocess.run([str(self.binary), *map(str, args)], cwd=directory,
                                capture_output=True, text=True, timeout=30,
                                env=environment)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("runtime error:", result.stderr)

    def check_overwrite(self, size, slot=0, inferred=False):
        name = b"SAVE_A00001"
        payload = bytes((i * 3 + 7) % 256 for i in range(size))
        replacement = bytes((i * 5 + 9) % 256 for i in range(size))
        neighbor = b"Other save must survive." * 3
        entries = [(slot, name, payload)]
        if slot < 447:
            entries.append((slot + 1, b"SAVE_B00002", neighbor))
        original, locations = archive_fixture(entries)
        expected = bytearray(original)
        header, extents = locations[0]
        expected[header + 28:header + 32] = NEW_DATE
        consumed = 0
        for offset, count in extents:
            expected[offset:offset + count] = replacement[consumed:consumed + count]
            consumed += count
        with tempfile.TemporaryDirectory(prefix="saroo-mems-case-") as temp:
            directory = Path(temp)
            archive = directory / "SS_MEMS.BIN"
            archive.write_bytes(original)
            incoming = directory / ("SAVE_A00001.bin" if inferred else "incoming.SRO")
            incoming.write_bytes(raw_save(name, replacement))
            self.invoke(directory, archive, "-s", 0, "-i",
                        *([] if inferred else [incoming]))
            actual = archive.read_bytes()
            self.assertEqual(digest(actual[BLOCK:8 * BLOCK]),
                             digest(original[BLOCK:8 * BLOCK]),
                             "Overwrite changed a MEMS directory entry")
            self.assertEqual(actual[header + 28:header + 32], NEW_DATE,
                             "Overwrite did not update the save header date")
            self.assertEqual(digest(actual), digest(expected),
                             "Overwrite changed bytes outside selected date/data")
            self.invoke(directory, archive, "-s", 0)
            exported = (directory / "SAVE_A00001.SRO").read_bytes()
            self.assertEqual(exported[64:], replacement)
            self.assertEqual(exported[44:48], NEW_DATE)
            self.assertEqual(struct.unpack_from(">I", exported, 12)[0],
                             zlib.crc32(exported[16:]))
            if len(entries) > 1:
                self.invoke(directory, archive, "-s", 1)
                self.assertEqual((directory / "SAVE_B00002.SRO").read_bytes()[64:],
                                 neighbor)

    def test_overwrite_preserves_neighbor_and_updates_date(self):
        for size in (1, 32, 960, 961, 2048, 4097):
            with self.subTest(size=size):
                self.check_overwrite(size)

    def test_directory_holes(self):
        self.check_overwrite(100, slot=5)

    def test_final_directory_slot(self):
        self.check_overwrite(64, slot=447)

    def test_inferred_input_filename(self):
        self.check_overwrite(31, inferred=True)

    def test_export_is_a_read_only_valid_control(self):
        payload = b"Unchanged export control."
        original, _ = archive_fixture([(0, b"SAVE_A00001", payload)])
        with tempfile.TemporaryDirectory(prefix="saroo-mems-export-") as temp:
            directory = Path(temp)
            archive = directory / "SS_MEMS.BIN"
            archive.write_bytes(original)
            self.invoke(directory, archive, "-s", 0)
            self.assertEqual((directory / "SAVE_A00001.SRO").read_bytes()[64:], payload)
            self.assertEqual(digest(archive.read_bytes()), digest(original))


if __name__ == "__main__":
    unittest.main()
