"""Actual save-tool byte reader and valid high-bit SRO interoperability."""
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile
import unittest
import zlib

from test_mems_overwrite import archive_fixture, raw_save

ROOT = Path(__file__).resolve().parents[1]


class SaveEndianTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-save-endian-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.compiler = shlex.split(os.environ.get("CC", "cc"))
        cls.flags = ["-std=gnu11", "-O1", "-g", "-fno-strict-aliasing",
                     "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        cls.environment = {**os.environ, "UBSAN_OPTIONS": "halt_on_error=1", "ASAN_OPTIONS": "detect_leaks=0"}

    def test_actual_reader_and_signed_shift_control(self):
        source = (ROOT / "tools/savetool/main.c").read_text()
        matches = re.findall(r"u32 get_be32\(void \*p\)\n\{.*?\n\}", source, re.S)
        self.assertEqual(len(matches), 1)
        probe = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint32_t u32;
READER
int main(int argc, char **argv) {
    (void)argv;
    u8 bytes[8] = {0}; unsigned cases = 0;
    const u8 patterns[] = {0, 0x55, 0xaa, 0xff};
    for (unsigned top = 0; top < (argc > 1 ? 128u : 256u); ++top)
        for (unsigned offset = 0; offset < 4; ++offset)
            for (unsigned j = 0; j < sizeof(patterns); ++j) {
                u8 *p = bytes+offset;
                p[0] = top; p[1] = patterns[j]; p[2] = patterns[(j+1)%4]; p[3] = patterns[(j+2)%4];
                uint64_t expected = (uint64_t)p[0]*16777216 + (uint64_t)p[1]*65536 + (uint64_t)p[2]*256 + p[3];
                assert(get_be32(p) == expected); ++cases;
            }
    printf("%u aligned/unaligned field values passed\n", cases);
    return 0;
}
'''
        for name, reader in [("actual", matches[0]), ("signed-control", matches[0].replace("(u32)", ""))]:
            path = self.directory / (name+".c")
            path.write_text(probe.replace("READER", reader))
            binary = path.with_suffix("")
            subprocess.run([*self.compiler, *self.flags, str(path), "-o", str(binary)], check=True)
            low = subprocess.run([str(binary), "low"], env=self.environment, capture_output=True, text=True)
            self.assertEqual(low.returncode, 0, low.stderr)
            all_values = subprocess.run([str(binary)], env=self.environment, capture_output=True, text=True)
            if name == "actual":
                self.assertEqual(all_values.returncode, 0, all_values.stderr)
                self.assertIn("4096", all_values.stdout)
            else:
                self.assertNotEqual(reader, matches[0])
                self.assertNotEqual(all_values.returncode, 0)
                self.assertIn("left shift", all_values.stderr)

    def test_valid_high_bit_crc_and_full_date_round_trip(self):
        binary = self.directory / "savetool"
        subprocess.run([*self.compiler, *self.flags, *map(str, sorted((ROOT / "tools/savetool").glob("*.c"))),
                        "-o", str(binary)], check=True, capture_output=True, text=True)
        name = b"SAVE_A00001"
        image, locations = archive_fixture([(0, name, b"original payload"), (1, b"NEIGHBOR001", b"untouched")])
        for seed in range(256):
            payload = bytes((i+seed)%256 for i in range(16))
            incoming = raw_save(name, payload)
            incoming[44:48] = b"\xff"*4
            crc = zlib.crc32(incoming[16:])
            if crc >= 0x80000000:
                break
        else:
            self.fail("no high-bit CRC fixture")
        struct.pack_into(">I", incoming, 12, crc)
        archive = self.directory / "SS_MEMS.BIN"
        archive.write_bytes(image)
        path = self.directory / "incoming.SRO"
        path.write_bytes(incoming)
        for arguments in [(str(archive), "-s", "0", "-i", str(path)), (str(archive), "-s", "0")]:
            result = subprocess.run([str(binary), *arguments], cwd=self.directory, env=self.environment,
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            self.assertNotIn("runtime error:", result.stderr)
        expected = bytearray(image)
        header, extents = locations[0]
        expected[header+28:header+32] = b"\xff"*4
        self.assertEqual(len(extents), 1)
        offset, size = extents[0]
        self.assertEqual(size, len(payload))
        expected[offset:offset+size] = payload
        self.assertEqual(archive.read_bytes(), expected)
        exported = (self.directory / "SAVE_A00001.SRO").read_bytes()
        self.assertEqual(exported[44:48], b"\xff"*4)
        self.assertEqual(exported[64:], payload)
        self.assertEqual(struct.unpack_from(">I", exported, 12)[0], zlib.crc32(exported[16:]))


if __name__ == "__main__":
    unittest.main()
