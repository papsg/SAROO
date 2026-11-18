"""Actual stock early-console/startup expressions and actual translation C."""
# Adapted from paps-g 6491b42b / SSR-111; excludes private command channels.
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import function

ROOT = Path(__file__).resolve().parents[1]


class MenuLanguageStartupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/menu-language-startup'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build)); cls.results = []
        original = Path(os.environ.get('SAROO_MENU_STARTUP_SOURCE', ROOT / 'Firm_Saturn/main.c'))
        inputs = {'main.c': original.read_bytes(),
                  'sci_shell.c': (ROOT / 'Firm_Saturn/sci_shell.c').read_bytes(),
                  'language.c': (ROOT / 'Firm_Saturn/language.c').read_bytes(),
                  'production-main.h': (ROOT / 'Firm_Saturn/main.h').read_bytes(),
                  'fixture.c': (ROOT / 'tests/menu_language_startup.c').read_bytes(),
                  'driver.py': Path(__file__).read_bytes()}
        main, shell, header = inputs['main.c'], inputs['sci_shell.c'], inputs['production-main.h']
        begin = main.index(b'int _main(void)'); end = main.index(b'\n}', begin)
        begin = main.index(b'    cdblock_off();', begin, end)
        inputs['startup.inc'] = b'static int startup_tail(void)\n{\n' + main[begin:end] + b'\n}\n'
        inputs['restore.inc'] = (function(main, 'restore_game_catalog')
                                 if b'static int restore_game_catalog(' in main else b'')
        begin = main.index(b'}else if(index==4){')
        end = main.index(b'}else if(index==update_index)', begin)
        inputs['main_console.inc'] = (b'static int main_console_case(void)\n{\n'
                                      b'    int index = 4;\n    if(0){' + main[begin:end]
                                      + b'}\n    return -99;\n}\n')
        commands = re.findall(rb'\bCMD\(menu\)\s*\{[^}]*\}', shell)
        assert len(commands) == 1
        inputs['shell_menu.inc'] = commands[0] + b'\n'
        listb_start = shell.index(b'CMD(listb){')
        listb_end = shell.index(b'CMD(loadb){', listb_start)
        inputs['shell_listb.inc'] = shell[listb_start:listb_end]
        opcodes = (b'#define SSCMD_LISTDISC ', b'#define SSCMD_LISTBINS ')
        inputs['opcodes.inc'] = b'\n'.join(line for line in header.splitlines()
                                         if line.startswith(opcodes)) + b'\n'
        assert len(inputs['opcodes.inc'].splitlines()) == 2
        handler = main[main.index(b'int main_handle('):main.index(b'void menu_init(')]
        cycles = re.findall(rb'if\(BUTTON_DOWN\(ctrl, PAD_LT\)\)\{[^}]*\}', handler)
        assert len(cycles) == 1 and b'lang_next();' in cycles[0]
        inputs['cycle.inc'] = cycles[0] + b'\n'
        # Source-traced UI path: known text precedes handler installation/run.
        ui = main[main.index(b'void menu_init('):main.index(b'// Sega Saturn 1.00J')]
        assert ui.index(b'TT(') < ui.index(b'main_menu.handle = main_handle;') < ui.index(b'menu_run(&main_menu);')
        buttons = header[header.index(b'static inline int button_pressed'):header.index(b'/* Changed buttons')]
        lt, exit_value = [re.findall(pattern, header, re.MULTILINE) for pattern in (
            rb'^#define PAD_LT[^\n]+', rb'^#define MENU_EXIT[^\n]+')]
        assert len(lt) == len(exit_value) == 1
        inputs['buttons.inc'] = lt[0] + b'\n' + exit_value[0] + b'\n' + buttons
        inputs['main.h'] = b'#include <stdint.h>\n#include <string.h>\ntypedef uint32_t u32;typedef uint8_t u8;\n'
        for name, data in inputs.items(): (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps({n: hashlib.sha256(v).hexdigest()
            for n, v in inputs.items()}, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'startup'
        cls.legacy_binary = cls.target / 'startup-legacy'
        for name, binary, defines in (('standard', cls.binary, ['-DSAROO_SAVE_GENERATION=1']),
                                      ('legacy', cls.legacy_binary, [])):
            command = [os.environ.get('CC', 'cc'), '-std=c99', *defines,
                       '-g', '-Wall', '-Wextra', '-Werror', *flags,
                       '-I', str(cls.target), str(cls.target / 'fixture.c'), '-o', str(binary)]
            (cls.target / f'compile-{name}-command.json').write_text(
                json.dumps(command, indent=2) + '\n')
            result = subprocess.run(command, capture_output=True, timeout=45)
            (cls.target / f'compile-{name}.log').write_bytes(result.stdout + result.stderr)
            result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def case(self, language, debug=0, cycling=1, listing=0, restore_error=0,
             menu_entry=0, legacy=0):
        command = [str(self.legacy_binary if legacy else self.binary),
                   str(language), str(debug), str(cycling),
                   str(listing), str(restore_error), str(menu_entry)]
        with self.subTest(language=language, debug=debug, cycling=cycling,
                          listing=listing, restore_error=restore_error,
                          menu_entry=menu_entry, legacy=legacy):
            result = subprocess.run(command, capture_output=True, text=True, timeout=10,
                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
            self.results.append(dict(command=command, exit=result.returncode,
                                     stdout=result.stdout, stderr=result.stderr))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stderr, '')

    def test_normal_language_admission_and_cycle(self):
        for language in [*range(19), -2147483648, -1631327553, 19, 65536, 2147483647]: self.case(language)

    def test_early_console_lookup(self):
        for language in (0, 1, 18): self.case(language, 3, 0)

    def test_early_console_cycles_and_selection_on_return(self):
        for language in (0, 1, 18, -2147483648, -1631327553, 19, 2147483647): self.case(language, 3)

    def test_debug_bit_dispatch(self):
        for debug in (0, 1, 2, 3, 4, 5, 7, 8, 11): self.case(1, debug)

    def test_console_listb_restores_game_catalog_before_menu(self):
        for language in (0, 1, 18): self.case(language, 3, 0, listing=1)

    def test_failed_catalog_restore_does_not_enter_menu(self):
        self.case(1, 3, 0, listing=1, restore_error=1)

    def test_menu_console_restores_catalog_on_return(self):
        self.case(1, 3, 0, listing=1, menu_entry=1)

    def test_menu_console_restore_failure_keeps_menu_open(self):
        self.case(1, 3, 0, listing=1, restore_error=1, menu_entry=1)

    def test_legacy_console_restores_existing_bin_catalog(self):
        self.case(1, 3, 0, menu_entry=1, legacy=1)


if __name__ == '__main__': unittest.main()
