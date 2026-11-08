"""Compile actual menu code, extracting only the hardware-free error helper/call."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class MenuTextTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="saroo-menu-text-")
        cls.addClassCleanup(cls.temp.cleanup)
        directory = Path(cls.temp.name)
        conio = (ROOT / "Firm_Saturn/conio.c").read_text()
        main = (ROOT / "Firm_Saturn/main.c").read_text()
        helper = re.search(r"void menu_error\([^\n]+\)\n\{.*?\n\}", conio, re.S)
        call = re.search(r'(?m)^([ \t]*)(?:menu_error\(menu, TT\("游戏启动失败! %d"\), retv\);|'
                         r'char buf\[40\];\s+sprintf\(buf, TT\("游戏启动失败! %d"\), retv\);'
                         r'\s+menu_status\(menu, buf\);)', main)
        if not call:
            raise AssertionError("actual game-error call was not found")
        code = ""
        if helper:
            code += "#define HAS_MENU_ERROR 1\n" + helper[0] + "\n"
        code += "static void test_game_error(MENU_DESC *menu, int retv)\n{\n"
        code += call[0] + "\n}\n"
        (directory / "menu_error_source.inc").write_text(code)
        cls.binary = directory / "menu-text"
        subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O1", "-g",
                        "-Wall", "-Wextra", "-Werror", "-fno-builtin", "-fsanitize=address,undefined",
                        "-fno-omit-frame-pointer", "-I", str(directory),
                        str(ROOT / "tests/menu_text.c"), "-o", str(cls.binary)], check=True)

    def run_case(self, case):
        environment = dict(os.environ, UBSAN_OPTIONS="halt_on_error=1")
        subprocess.run([str(self.binary), case], check=True, env=environment, timeout=15)

    def test_formatter(self):
        self.run_case("formats")

    def test_actual_translated_error_call(self):
        self.run_case("actual-error")

    def test_complete_translated_errors_and_titles(self):
        self.run_case("translations")


if __name__ == "__main__":
    unittest.main()
