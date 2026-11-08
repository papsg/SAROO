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
        source = (ROOT / "Firm_Saturn/conio.c").read_text()
        functions = []
        for signature in ["static u8 *find_ucs", "static u8 *find_font", "int fb_draw_font_buf", "static int utf8_to_ucs"]:
            function = re.search(re.escape(signature)+r"\([^\n]*\)\n\{.*?\n\}", source, re.S)
            if not function:
                raise AssertionError(signature)
            functions.append(function[0])
        # Model the SH-2's big-endian 16-bit count read on the little-endian host.
        functions[0], count = re.subn(re.escape("int total = *(u16*)(fdat);"),
                                     "int total = (fdat[0]<<8) | fdat[1];", functions[0])
        if count != 1:
            raise AssertionError("reassess the production count-read adaptation")
        (cls.directory / "font_renderer_source.inc").write_text("\n\n".join(functions))
        cls.renderer = cls.directory / "font-lookup"
        # Retain the original decoder's char/u8 pointer assignments unchanged.
        subprocess.run([compiler, *flags, "-Wall", "-Wextra", "-Werror", "-Wno-pointer-sign", "-I", str(cls.directory),
                        str(ROOT / "tests/font_lookup.c"), "-o", str(cls.renderer)], check=True)
        cls.wanted = cls.directory / "wanted.txt"
        cls.wanted.write_text("\n".join(SPEC["added_codepoints"])+"\n")

    def test_original_records_and_independent_additions(self):
        data = FONT.read_bytes()
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

    def test_reproducible_original_converter(self):
        output = self.directory / "regenerated.bin"
        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
        subprocess.run([str(self.converter), str(ROOT / "tools/bdfont" / SPEC["source_bdf"]),
                        "-l", str(ROOT / "tools/bdfont/cjk.txt"), "-adj", "1", "-c", str(output)],
                       check=True, cwd=self.directory, env=environment, timeout=20, stdout=subprocess.PIPE)
        self.assertEqual(output.read_bytes(), FONT.read_bytes())

    def test_production_lookup_decoder_and_raster(self):
        subprocess.run([str(self.renderer), str(FONT), str(self.wanted)], check=True,
                       env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=15)


if __name__ == "__main__":
    unittest.main()
