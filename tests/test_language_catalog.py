"""Measure menu labels and check status bounds in the production renderer."""
import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

import test_font_coverage as fonts


ROOT = Path(__file__).resolve().parents[1]
STATUS_SLOTS = (2, 3, 4, 5, 12, 13, 14, 16, 17, 18)
ERROR_SLOTS = (3, 5, 18)
STATUS_WIDTH = 288


def catalog():
    source = (ROOT / "Firm_Saturn/language.c").read_text()
    registered = re.search(r"static char \*\*lang_list\[\] = \{(.*?)\};", source, re.S)[1]
    names = re.findall(r"\blang_\w+\b", registered)
    tables = {}
    for name in names:
        body = re.search(r"static char \*"+name+r"\[LANG_STR_NR\] = \{(.*?)\};", source, re.S)[1]
        tables[name] = [json.loads(value) for value in re.findall(r'"(?:\\.|[^"\\])*"', body)]
    return tables


class LanguageCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tables = catalog()
        # The renderer gives the Latin table precedence over the appended font.
        cls.glyphs = fonts.read_records(fonts.FONT.read_bytes())
        cls.glyphs.update(fonts.read_records(fonts.latin_bytes()))

    def text_width(self, text):
        position = rightmost = 0
        for character in text:
            code = ord(character)
            self.assertTrue(code in self.glyphs, (text, f"U+{code:04X}"))
            advance, geometry, bearing = self.glyphs[code][:3]
            rightmost = max(rightmost, position + (bearing >> 4) + (geometry >> 4))
            position += advance
        return max(position, rightmost)

    def test_added_labels_have_glyphs_and_fit(self):
        tables = list(self.tables.items())[14:]
        self.assertTrue(tables, "no language additions registered")
        checked = 0
        for name, labels in tables:
            self.assertEqual(len(labels), 20, name)
            for key, label in enumerate(labels):
                arguments = label.count("%d")
                self.assertEqual(arguments, 2 if key in (0, 1) else 1 if key in ERROR_SLOTS else 0,
                                 (name, key, label))
                versions = [label]
                if arguments == 2:
                    versions = [label % (214, 214)]
                elif arguments == 1:
                    versions = [label % value for value in (-2147483648, 2147483647)]
                for rendered in versions:
                    self.assertLessEqual(self.text_width(rendered), STATUS_WIDTH, (name, key, rendered))
                    checked += 1
        print(f"{len(tables)} added language tables: {checked} complete label variants fit")

    def test_status_labels_fit_in_every_language(self):
        self.assertEqual(len(self.tables), 19)
        checked = 0
        for name, labels in self.tables.items():
            self.assertEqual(len(labels), 20, name)
            for slot in STATUS_SLOTS:
                with self.subTest(language=name, slot=slot):
                    label = labels[slot]
                    if slot in ERROR_SLOTS:
                        self.assertEqual(label.count("%d"), 1)
                        variants = [label % code for code in (-2147483648, -1, 0, 1, 2147483647)]
                    else:
                        variants = [label]
                    checked += len(variants)
                    for text in variants:
                        self.assertLessEqual(self.text_width(text), STATUS_WIDTH, text)
        self.assertEqual(checked, 418)


class MenuStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory(prefix="saroo-menu-status-")
        cls.addClassCleanup(temporary.cleanup)
        cls.binary = fonts.build_renderer(Path(temporary.name), "menu_status.c", (
            "int fb_draw_font", "void put_hline", "void put_box", "int menu_draw_string", "void menu_status",
        ))

    def run_case(self, case):
        run = subprocess.run([str(self.binary), str(fonts.FONT), case], capture_output=True,
                             env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1"), timeout=15)
        self.assertEqual(run.returncode, 0, run.stderr.decode(errors="replace"))

    def test_long_status_stays_within_clear_bounds(self):
        self.run_case("bounds")

    def test_replacement_clears_the_previous_long_status(self):
        self.run_case("replace")

    def test_short_and_empty_status_controls(self):
        self.run_case("controls")


if __name__ == "__main__":
    unittest.main()
