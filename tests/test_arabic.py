"""Fixed-label Arabic adaptation; recorded independent oracle, actual C renderer."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

import test_font_coverage as fonts


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "tools/menu-assets"
SPEC = json.loads((ASSETS / "arabic-font.json").read_text())
ORACLE = json.loads((ROOT / "tests/fixtures/arabic-oracle.json").read_text())["cases"]


def labels(name, source):
    body = re.search(r"static char \*"+name+r"\[LANG_STR_NR\] = \{(.*?)\};", source, re.S)
    if not body:
        raise AssertionError("missing language table: "+name)
    return [json.loads(t) for t in re.findall(r'"(?:\\.|[^"\\])*"', body[1])]


class ArabicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-arabic-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        subprocess.run(["cargo", "build", "--locked", "--manifest-path", str(ROOT / "tools/Cargo.toml")],
                       check=True, timeout=180)
        cls.tool = ROOT / "tools/target/debug/menu-assets"

    def run_tool(self, *args):
        subprocess.run([str(self.tool), *map(str, args)], check=True, timeout=20)

    def test_generated_table_and_independent_oracle(self):
        table = self.directory / "arabic.inc"
        self.run_tool("arabic-table", ASSETS / "arabic.json", table)
        generated = labels("lang_ar", table.read_text())
        source = (ROOT / "Firm_Saturn/language.c").read_text()
        self.assertEqual(generated, labels("lang_ar", source))
        self.assertEqual(len(generated), 20)
        self.assertEqual(len(ORACLE), 36)
        for case in ORACLE:
            self.assertEqual(generated[case["slot"]] % tuple(case["arguments"]), case["expected"])
        self.assertIn("lang_ca,\n\tlang_ar,", source)

    def test_previous_languages_and_real_formatter_raster(self):
        source = (ROOT / "Firm_Saturn/language.c").read_text()
        previous = json.loads((ROOT / "tests/fixtures/languages-before-arabic.json").read_text())
        cases = []
        for id_, (name, old) in enumerate(previous.items()):
            current = labels(name, source)
            self.assertEqual(current[:18], old[:18])
            self.assertEqual(current[18], old[18]+"%d")
            self.assertEqual(current[19], old[19])
            for slot, text in enumerate(old):
                arguments = [()]
                if slot < 2:
                    arguments = [(1, 214), (214, 214), (1, 1)]
                elif slot in (3, 5, 18):
                    arguments = [(v,) for v in (0, 1, -1, -2147483648, 2147483647)]
                for args in arguments:
                    expected = text+str(args[0]) if slot == 18 else text % args
                    cases.append((id_, slot, args, expected))
        self.assertEqual(len(cases), 648)
        cases += [(18, c["slot"], tuple(c["arguments"]), c["expected"]) for c in ORACLE]
        fixture = self.directory / "cases.tsv"
        fixture.write_text("".join(f"{id_} {slot} {(args+(0,0))[0]} {(args+(0,0))[1]}\t{text}\n"
                                   for id_, slot, args, text in cases))
        binary = fonts.build_renderer(self.directory, "arabic_labels.c")
        output = ROOT / "tests/build/arabic-labels.pgm"
        output.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(binary), str(fonts.FONT), str(fixture), str(output)], check=True,
                       env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=20)

    def test_font_records_notices_and_original_atlas_pixels(self):
        # Pillow is used only for this independent PNG pixel oracle, not firmware builds.
        from PIL import Image
        data = fonts.remove_records(fonts.FONT.read_bytes(), {fonts.ROMANIAN})
        self.assertEqual(len(data), SPEC["output_bytes"])
        self.assertEqual(hashlib.sha256(data).hexdigest(), SPEC["output_sha256"])
        records = fonts.read_records(data)
        added = [int(cp, 16) for cp in SPEC["added_codepoints"]]
        self.assertEqual(len(added), 59)
        self.assertEqual(len(records), SPEC["baseline_glyphs"]+59)
        digest = b"".join(cp.to_bytes(2, "big")+records[cp] for cp in added)
        self.assertEqual(hashlib.sha256(digest).hexdigest(), SPEC["added_records_sha256"])
        self.assertEqual(hashlib.sha256(fonts.cjk_only(data)).hexdigest(), SPEC["baseline_font_sha256"])
        bdf = ASSETS / SPEC["source_bdf"]
        self.assertEqual(hashlib.sha256(bdf.read_bytes()).hexdigest(), SPEC["source_sha256"])
        provenance = json.loads((ASSETS / "font-source/provenance.json").read_text())
        for name, entry in provenance["files"].items():
            self.assertEqual(hashlib.sha256((ASSETS / "font-source" / name).read_bytes()).hexdigest(), entry["sha256"])
        image = Image.open(ASSETS / "font-source/bitmapfont/internal/arabic/fe70-feff.png").convert("RGB")
        pixels = 0
        for cp in added:
            record = records[cp]
            x0, y0 = (cp-0xfe70)%16*12, (cp-0xfe70)//16*12
            cell = [[image.getpixel((x0+x, y0+y))[0] == 0 for x in range(12)] for y in range(12)]
            width = 12 if any(row[x] for row in cell for x in range(6, 12)) else 6
            self.assertEqual(record[:3], bytes((width, (width<<4)|12, 3)))
            for y in range(12):
                for x in range(12):
                    bit = (record[3+y*(1 if width <= 8 else 2)+x//8] >> (7-x%8)) & 1 if x < width else 0
                    self.assertEqual(bit, cell[y][x], (hex(cp), x, y))
                    pixels += 1
        self.assertEqual(pixels, 8496)

    def test_selected_font_rebuild_and_production_glyphs(self):
        selected = self.directory / "selected.txt"
        self.run_tool("arabic-glyphs", ASSETS / "arabic.json", selected)
        self.assertEqual(selected.read_text().split(), SPEC["added_codepoints"])
        converter = self.directory / "bdfont"
        subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O1", "-g",
                        "-fsanitize=address,undefined", str(ROOT / "tools/bdfont/bdfont.c"),
                        "-o", str(converter)], check=True)
        extra = self.directory / "extra.bin"
        subprocess.run([str(converter), str(ASSETS / SPEC["source_bdf"]), "-l", str(selected),
                        "-adj", "0", "-c", str(extra)], check=True, timeout=20, stdout=subprocess.PIPE,
                       env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1"))
        base = self.directory / "base.bin"
        base.write_bytes(fonts.cjk_only(fonts.FONT.read_bytes()))
        merged = self.directory / "merged.bin"
        self.run_tool("merge-font", base, extra, merged)
        self.assertEqual(merged.read_bytes(), fonts.remove_records(fonts.FONT.read_bytes(), {fonts.ROMANIAN}))
        renderer = fonts.build_renderer(self.directory)
        subprocess.run([str(renderer), str(fonts.FONT), str(selected), "59"], check=True, timeout=20)


if __name__ == "__main__":
    unittest.main()
