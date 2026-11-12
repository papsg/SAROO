"""Exercise the stock executable folder menu with a modeled MCU mailbox."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuBinBrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_BIN_SOURCE', ROOT / 'Firm_Saturn'))
        raw = (source / 'main.c').read_bytes()
        header = (source / 'main.h').read_bytes()
        current = (ROOT / 'Firm_Saturn/main.h').read_bytes()
        mcu = (ROOT / 'Firm_MCU/Saturn/cdc.h').read_bytes()
        names = (b'BIN_DIRECTORY_VERSION', b'BIN_DIRECTORY_ENTRY', b'BIN_PATH_CAPACITY',
                 b'BIN_ROOT', b'SSCMD_BINDIR', b'SSCMD_SAVE_ACK', b'SSCMD_LISTBINS',
                 b'SSCMD_LISTDISC', b'SSCMD_SELECT', b'MENU_ITEMS', b'MS2TICK',
                 b'PAD_A', b'PAD_B', b'PAD_C', b'PAD_Z', b'BUTTON_DOWN')
        definitions = []
        has_browser = b'static void bin_browser(' in raw
        for name in names:
            pattern = rb'^#define ' + name + rb'\b[^\r\n]+'
            match = re.search(pattern, header, re.M) or re.search(pattern, current, re.M)
            definitions.append(match[0])
            peer = re.search(pattern, mcu, re.M)
            if has_browser and peer:
                assert match[0].split()[-1] == peer[0].split()[-1], name
        definitions.append(f'#define HAS_BIN_BROWSER {int(has_browser)}'.encode())
        code = function(raw, 'select_bins')
        if has_browser:
            start = raw.index(b'static char *bin_catalog_name(')
            end = raw.index(b'static void fill_selmenu(', start)
            core = raw[start:end] + function(raw, 'fill_selmenu') + function(raw, 'load_binary_file')
            start = raw.index(b'enum { BIN_IDLE,')
            code = core + raw[start:raw.index(b'void select_bins(', start)] + code
        build = ROOT / 'tests/build/menu-bin-browser'
        build.mkdir(parents=True, exist_ok=True)
        target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (target / 'constants.inc').write_bytes(b'\n'.join(definitions) + b'\n')
        (target / 'browser.inc').write_bytes(code)
        cls.binary = target / 'browser'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_BIN_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-function', '-Wno-unused-variable', '-Wno-int-to-pointer-cast',
                   *flags, '-I', str(target), str(ROOT / 'tests/menu_bin_browser.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def case(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True, text=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS ' + name, result.stdout)

    def test_negotiated_menu_browses_launches_and_restores(self):
        self.case('browse')

    def test_older_or_unsupported_capability_keeps_the_flat_selector(self):
        self.case('flat')

    def test_cancel_preserves_live_request_and_retries_failed_restore(self):
        self.case('cancel')

    def test_invalid_reply_quarantines_the_request(self):
        self.case('invalid-reply')

    def test_failed_child_scan_restores_current_folder_once(self):
        self.case('retry')

    def test_path_and_catalog_bounds(self):
        self.case('bounds')


if __name__ == '__main__':
    unittest.main()
