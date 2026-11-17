"""Exercise the stock executable folder menu with a modeled MCU mailbox."""
import os
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function
from tests.menu_source_flags import menu_source_variant

OWNER = os.environ.get('SAROO_MENU_BIN_OWNER') == '1'


class MenuBinBrowserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_BIN_SOURCE', ROOT / 'Firm_Saturn'))
        original_raw = (source / 'main.c').read_bytes()
        raw = menu_source_variant(original_raw, staged=OWNER)
        header = (source / 'main.h').read_bytes()
        current = (ROOT / 'Firm_Saturn/main.h').read_bytes()
        mcu = (ROOT / 'Firm_MCU/Saturn/cdc.h').read_bytes()
        names = (b'BIN_DIRECTORY_VERSION', b'BIN_DIRECTORY_ENTRY', b'CATALOG_NAME_CAPACITY', b'BIN_PATH_CAPACITY',
                 b'BIN_ROOT', b'SSCMD_BINDIR', b'SSCMD_SAVE_ACK', b'SSCMD_LISTBINS',
                 b'SSCMD_LISTDISC', b'SSCMD_SELECT', b'SSCMD_CHECK', b'SSCMD_UPDATE',
                 b'SSCMD_STARTUP', b'SSCMD_LOADDISC', b'SSCMD_LSAVE',
                 b'SELECT_ENTER', b'SELECT_EXIT', b'MENU_ITEMS', b'MS2TICK',
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
        helper = current.index(b'static inline int button_pressed(')
        definitions.append(current[helper:current.index(b'\n}', helper) + 3])
        code = function(raw, 'select_bins')
        if has_browser:
            start = raw.index(b'static char *bin_catalog_name(')
            end = raw.index(b'static void fill_selmenu(', start)
            core = raw[start:end] + function(raw, 'fill_selmenu') + function(raw, 'load_binary_file')
            start = raw.index(b'enum { BIN_IDLE,')
            selector = b'int select_bins(' if OWNER else b'void select_bins('
            code = core + raw[start:raw.index(selector, start)] + code
        build = ROOT / 'tests/build/menu-bin-browser'
        build.mkdir(parents=True, exist_ok=True)
        target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (target / 'constants.inc').write_bytes(b'\n'.join(definitions) + b'\n')
        (target / 'browser.inc').write_bytes(code)
        inputs = {'main.c': original_raw, 'main.h': header, 'current-main.h': current, 'mcu-cdc.h': mcu,
                  'constants.inc': (target / 'constants.inc').read_bytes(), 'browser.inc': code,
                  'menu_bin_browser.c': (ROOT / 'tests/menu_bin_browser.c').read_bytes(),
                  'test_menu_bin_browser.py': Path(__file__).read_bytes()}
        for name, data in inputs.items():
            (target / ('input-' + name)).write_bytes(data)
        (target / 'menu_bin_browser.c').write_bytes(inputs['menu_bin_browser.c'])
        (target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2) + '\n')
        cls.target, cls.results = target, []
        cls.binary = target / 'browser'
        flags = ['-O2'] if os.environ.get('SAROO_MENU_BIN_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall', '-Wextra',
                   '-Werror', '-Wno-unused-function', '-Wno-unused-variable', '-Wno-int-to-pointer-cast',
                   *flags, *(['-DSAROO_SAVE_GENERATION'] if OWNER else []),
                   '-I', str(target), '-I', str(ROOT / 'Firm_Saturn'),
                   str(target / 'menu_bin_browser.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (target / 'compile.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def case(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True, text=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=name, status=result.returncode, out=result.stdout, err=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS ' + name, result.stdout)

    def test_negotiated_menu_browses_launches_and_restores(self):
        self.case('browse')

    def test_older_or_unsupported_capability_keeps_the_flat_selector(self):
        if OWNER:
            self.skipTest('the opt-in owner requires the negotiated command channel')
        self.case('flat')

    def test_cancel_preserves_live_request_and_retries_failed_restore(self):
        self.case('cancel')

    def test_invalid_reply_quarantines_the_request(self):
        self.case('invalid-reply')

    def test_owned_browser_defers_foreign_command(self):
        if not OWNER:
            self.skipTest('the standard browser has no command-channel lease')
        self.case('foreign-owner')

    def test_owned_browser_copies_reply_before_lease_release(self):
        if not OWNER:
            self.skipTest('the standard browser has no command-channel lease')
        self.case('output-owner')

    def test_failed_child_scan_restores_current_folder_once(self):
        self.case('retry')

    def test_path_and_catalog_bounds(self):
        self.case('bounds')

    def test_full_utf8_leaf_retains_neighbors_and_independent_launch_limits(self):
        self.case('utf8')


if __name__ == '__main__':
    unittest.main()
