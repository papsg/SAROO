"""Reproduce the CJK asset with the original converter and retain its baseline."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "Firm_Saturn/font_cjk.bin"
SPEC = json.loads((ROOT / "tools/bdfont/cjk-pr203.json").read_text())
ADDED = {int(value, 16) for value in SPEC["added_codepoints"]}
ARABIC = json.loads((ROOT / "tools/menu-assets/arabic-font.json").read_text())
ARABIC_ADDED = {int(value, 16) for value in ARABIC["added_codepoints"]}
ROMANIAN = 0x0219
ROMANIAN_BDF = ROOT / "tools/menu-assets/font-source/romanian14.bdf"

LATIN_REPAIRS = {0x0122, 0x0136, 0x0137, 0x013B, 0x013C, 0x0145, 0x0146,
                 0x0156, 0x0157, 0x021A, 0x021B, 0x0375, 0x046E}


def remove_records(data, codepoints):
    return encode_records({cp: record for cp, record in read_records(data).items()
                           if cp not in codepoints})


def cjk_only(data):
    return remove_records(data, ARABIC_ADDED | {ROMANIAN})


def latin_bytes():
    source = (ROOT / "Firm_Saturn/font_latin.h").read_text()
    array = source.split("font_latin[] = {", 1)[1].split("};", 1)[0]
    return bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", array))


def build_renderer(directory, fixture="font_lookup.c"):
    source = (ROOT / "Firm_Saturn/conio.c").read_text()
    functions = []
    for signature in ["static u8 *find_ucs", "static u8 *find_font", "int fb_draw_font_buf", "static int utf8_to_ucs"]:
        function = re.search(re.escape(signature)+r"\([^\n]*\)\n\{.*?\n\}", source, re.S)
        if not function:
            raise AssertionError(signature)
        functions.append(function[0])
    # Model the SH-2's big-endian count load on the little-endian host.
    functions[0], count = re.subn(re.escape("int total = *(u16*)(fdat);"),
                                 "int total = (fdat[0]<<8) | fdat[1];", functions[0])
    if count != 1:
        raise AssertionError("reassess the production count-read adaptation")
    (directory / "font_renderer_source.inc").write_text("\n\n".join(functions))
    binary = directory / Path(fixture).stem
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O1", "-g", "-fno-builtin",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-Wall", "-Wextra", "-Werror", "-Wno-pointer-sign", "-I", str(directory),
                    str(ROOT / "tests" / fixture), "-o", str(binary)], check=True)
    return binary


def read_records(data):
    count = int.from_bytes(data[:2], "big")
    start = 2+5*count
    if start > len(data):
        raise ValueError("truncated font index")
    result = {}
    previous = -1
    for i in range(count):
        entry = 2+5*i
        code = int.from_bytes(data[entry:entry+2], "big")
        offset = int.from_bytes(data[entry+2:entry+5], "big")
        end = int.from_bytes(data[entry+7:entry+10], "big") if i+1 < count else len(data)
        if code <= previous or offset != start or not offset+3 <= end <= len(data):
            raise ValueError("invalid font order/record span")
        result[code] = data[offset:end]
        previous, start = code, end
    return result


def encode_records(records):
    index = bytearray(len(records).to_bytes(2, "big"))
    payload = bytearray()
    for code, record in sorted(records.items()):
        index += code.to_bytes(2, "big")+(2+5*len(records)+len(payload)).to_bytes(3, "big")
        payload += record
    return bytes(index+payload)


class FontCoverageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-font-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        compiler = os.environ.get("CC", "cc")
        flags = ["-std=c99", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        cls.converter = cls.directory / "bdfont"
        subprocess.run([compiler, *flags, str(ROOT / "tools/bdfont/bdfont.c"), "-o", str(cls.converter)], check=True)
        cls.renderer = build_renderer(cls.directory)
        cls.wanted = cls.directory / "wanted.txt"
        cls.wanted.write_text("\n".join(SPEC["added_codepoints"])+"\n")

    def test_original_records_and_independent_additions(self):
        composed = FONT.read_bytes()
        arabic_stage = remove_records(composed, {ROMANIAN})
        self.assertEqual(len(arabic_stage), ARABIC["output_bytes"])
        self.assertEqual(hashlib.sha256(arabic_stage).hexdigest(), ARABIC["output_sha256"])
        data = cjk_only(composed)
        self.assertEqual(len(data), SPEC["output_bytes"])
        self.assertEqual(hashlib.sha256(data).hexdigest(), SPEC["output_sha256"])
        records = read_records(data)
        self.assertEqual(len(ADDED), 108)
        self.assertEqual(set(records) & ADDED, ADDED)
        self.assertEqual(len(records), SPEC["baseline_glyphs"]+108)
        original = encode_records({cp:record for cp,record in records.items() if cp not in ADDED})
        self.assertEqual(hashlib.sha256(original).hexdigest(), SPEC["baseline_font_sha256"])
        additions = b"".join(cp.to_bytes(2, "big")+records[cp] for cp in sorted(ADDED))
        self.assertEqual(hashlib.sha256(additions).hexdigest(), SPEC["added_records_sha256"])
        bdf = ROOT / "tools/bdfont" / SPEC["source_bdf"]
        self.assertEqual(hashlib.sha256(bdf.read_bytes()).hexdigest(), SPEC["source_sha256"])

    def test_romanian_comma_below_matches_source_and_renders(self):
        data = FONT.read_bytes()
        shipped = read_records(data)
        self.assertTrue(ROMANIAN in shipped, "Romanian loading label falls back to a square")
        selected = self.directory / "romanian.txt"
        selected.write_text("0219\n")
        source = (ROOT / "tools/bdfont/bdf/helvR14.bdf").read_text()
        glyph = re.search(r"^STARTCHAR scommaaccent\n(.*?)^ENDCHAR$", source, re.M | re.S)[1]
        self.assertIn("ENCODING 537\n", glyph)
        self.assertIn("DWIDTH 8 0\nBBX 6 13 1 -5\n", glyph)
        self.assertIn("FONT_ASCENT 13\n", source)
        rows = bytes.fromhex(glyph.split("BITMAP\n", 1)[1])
        self.assertEqual(len(rows), 13)
        self.assertEqual(rows[9:], bytes.fromhex("10 10 10 20"))
        latin_s = read_records(latin_bytes())[ord("s")]
        self.assertEqual(rows[:8], latin_s[3:])
        expected = bytes.fromhex("08 6B 15") + rows[:9] + rows[11:]

        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
        output = self.directory / "romanian.bin"
        subprocess.run([str(self.converter), str(ROMANIAN_BDF), "-adj", "0", "-c", str(output)],
                       check=True, cwd=self.directory, env=environment,
                       timeout=20, stdout=subprocess.PIPE)
        self.assertEqual(read_records(output.read_bytes()), {ROMANIAN: expected})
        self.assertEqual(shipped[ROMANIAN], expected)
        self.assertEqual(len(data), ARABIC["output_bytes"] + 5 + len(expected))
        subprocess.run([str(self.renderer), str(FONT), str(selected), "1"], check=True,
                       env=environment, timeout=15)

    def render_geometry(self, name, codes, *table):
        selected = self.directory / name
        selected.write_text("".join(f"{cp:04x}\n" for cp in sorted(codes)))
        subprocess.run([str(self.renderer), str(FONT), str(selected), str(len(codes)), *table],
                       check=True, env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=15)

    def test_full_height_cjk_raster(self):
        self.render_geometry("cjk-geometry.txt", {0x9515})

    def test_ksi_full_height_raster(self):
        self.render_geometry("ksi-geometry.txt", {0x046E}, "latin")

    def test_latin_glyphs_fit_the_row_and_space_stays_empty(self):
        self.render_geometry("latin-geometry.txt", (LATIN_REPAIRS - {0x046E}) | {0x20, ord("s")}, "latin")

    def test_all_indexed_bitmaps_match_their_geometry(self):
        latin_data = latin_bytes()
        latin = read_records(latin_data)
        appended = read_records(FONT.read_bytes())
        for records in (latin, appended):
            for cp, record in records.items():
                width, stored_height = record[1] >> 4, record[1] & 15
                height = stored_height or (16 if width else 0)
                top = record[2] & 15
                self.assertEqual(bool(width), bool(height), hex(cp))
                self.assertEqual(len(record), 3 + height * (2 if width > 8 else 1), hex(cp))
                self.assertLessEqual(top + height, 16, hex(cp))

        repaired = encode_records({cp: latin[cp] for cp in LATIN_REPAIRS})
        unchanged = remove_records(latin_data, LATIN_REPAIRS)
        self.assertEqual(hashlib.sha256(repaired).hexdigest(),
                         "3c42d22d869968443d4f07b2658ed986738d6904b1588e4995e677519c5bf5c9")
        self.assertEqual(hashlib.sha256(unchanged).hexdigest(),
                         "6222c6a05f7b890b31b1fda0a30fb1a77bb2d641d51f9fb339c5c516a1f9f73e")

    def test_reproducible_latin_font(self):
        output = self.directory / "font_latin.h"
        bdf = ROOT / "tools/bdfont"
        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
        subprocess.run([str(self.converter), str(bdf / "cbdf/helvR14c.bdf"), "-l",
                        str(bdf / "latin.txt"), "-h", str(output), "font_latin"],
                       check=True, cwd=self.directory, env=environment,
                       timeout=20, stdout=subprocess.PIPE)
        self.assertEqual(output.read_bytes(), (ROOT / "Firm_Saturn/font_latin.h").read_bytes())

    def test_reproducible_original_converter(self):
        output = self.directory / "regenerated.bin"
        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
        subprocess.run([str(self.converter), str(ROOT / "tools/bdfont" / SPEC["source_bdf"]),
                        "-l", str(ROOT / "tools/bdfont/cjk.txt"), "-adj", "1", "-c", str(output)],
                       check=True, cwd=self.directory, env=environment, timeout=20, stdout=subprocess.PIPE)
        self.assertEqual(output.read_bytes(), cjk_only(FONT.read_bytes()))

    def test_production_lookup_decoder_and_raster(self):
        subprocess.run([str(self.renderer), str(FONT), str(self.wanted), "108"], check=True,
                       env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=15)


if __name__ == "__main__":
    unittest.main()
