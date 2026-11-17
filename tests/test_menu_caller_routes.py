"""Prove opt-in synchronous menu commands use the owned channel.

The browser's two asynchronous direct producers remain an explicit next
checkpoint. These source checks prevent another synchronous caller from
silently bypassing the gate while the host channel model checks its behavior.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
COMMAND_WRITE = re.compile(r"25897010.*?\)\)\s*=(?!=)")


def preprocess(path, staged):
    command = [*shlex.split(os.environ.get("CC", "cc")), "-E",
               "-I", str(ROOT / "Firm_Saturn")]
    if staged:
        command.append("-DSAROO_SAVE_GENERATION")
    command.append(str(ROOT / "Firm_Saturn" / path))
    return subprocess.check_output(command, text=True, timeout=30)


def writes(source):
    return [line.strip() for line in source.splitlines()
            if COMMAND_WRITE.search(line)]


class MenuCallerRoutesTests(unittest.TestCase):
    def test_synchronous_producers_take_channel(self):
        normal = preprocess("main.c", False)
        staged = preprocess("main.c", True)
        self.assertEqual(len(writes(normal)), 15)
        outstanding = writes(staged)
        self.assertEqual(len(outstanding), 6, outstanding)
        for marker in ("0x0001 | menu_ack", "0x000e | 0x0020",
                       "command | 0x0020", "archive ?", "0x0002 | 0x0020",
                       "0x0038"):
            self.assertTrue(any(marker in line for line in outstanding), marker)
        # Exclude the declaration and implementation inside menu_channel.c.
        self.assertEqual(len(re.findall(r"menu_request\(", staged)), 12)
        self.assertEqual(len(re.findall(r"menu_select_notify\(", staged)), 3)

        shell = preprocess("sci_shell.c", True)
        self.assertEqual(len(writes(preprocess("sci_shell.c", False))), 1)
        self.assertEqual(writes(shell), [])
        self.assertIn("menu_request(0x0008, 0)", shell)
        load = preprocess("game_load.c", True)
        self.assertEqual(writes(load), [])
        self.assertIn("menu_load_save((u8 *)0x06002020)", load)
        menu = preprocess("conio.c", True)
        self.assertIn("menu_channel_poll()", menu)


if __name__ == "__main__":
    unittest.main()
