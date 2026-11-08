"""Check new labels against the shipped font and the menu's cleared width."""
import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class LanguageCatalogTests(unittest.TestCase):
    def test_added_labels_have_glyphs_and_fit(self):
        source = (ROOT / "Firm_Saturn/language.c").read_text()
        registered = re.search(r"static char \*\*lang_list\[\] = \{(.*?)\};", source, re.S)[1]
        names = re.findall(r"\blang_\w+\b", registered)[14:]
        self.assertTrue(names, "no language additions registered")
        font_source = (ROOT / "Firm_Saturn/font_latin.h").read_text()
        font_array = re.search(r"font_latin\[\] = \{(.*?)\};", font_source, re.S)[1]
        font = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", font_array))
        glyphs = {}
        for index in range(int.from_bytes(font[:2], "big")):
            record = font[2+index*5:7+index*5]
            code = int.from_bytes(record[:2], "big")
            offset = int.from_bytes(record[2:], "big")
            glyphs[code] = font[offset:offset+3]
        checked = 0
        for name in names:
            body = re.search(r"static char \*"+name+r"\[LANG_STR_NR\] = \{(.*?)\};", source, re.S)[1]
            labels = [json.loads(value) for value in re.findall(r'"(?:\\.|[^"\\])*"', body)]
            self.assertEqual(len(labels), 20, name)
            for key, label in enumerate(labels):
                arguments = label.count("%d")
                self.assertEqual(arguments, 2 if key in (0, 1) else 1 if key in (3, 5) else 0,
                                 (name, key, label))
                versions = [label]
                if arguments == 2:
                    versions = [label % (214, 214)]
                elif arguments == 1:
                    versions = [label % value for value in (-2147483648, 2147483647)]
                for rendered in versions:
                    position = rightmost = 0
                    for character in rendered:
                        self.assertIn(ord(character), glyphs, (name, key, character))
                        advance, geometry, bearing = glyphs[ord(character)]
                        rightmost = max(rightmost, position+(bearing >> 4)+(geometry >> 4))
                        position += advance
                    self.assertLessEqual(max(position, rightmost), 288, (name, key, rendered))
                    checked += 1
        print(f"{len(names)} new language tables: {checked} complete labels/error variants fit shipped Latin glyphs and 288 pixels")


if __name__ == "__main__":
    unittest.main()
