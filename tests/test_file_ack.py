"""Exercise actual file/save dispatch and capability words at the register boundary."""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_file_transfer import command_dispatch, protocol_definitions

ROOT = Path(__file__).resolve().parents[1]


class FileAcknowledgmentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sources = (ROOT / 'Firm_MCU/Saturn/saturn_main.c', ROOT / 'Firm_Saturn/main.c')
        raw, ui = (path.read_bytes() for path in sources)
        header = (ROOT / 'Firm_MCU/Saturn/cdc.h').read_bytes()
        menu_header = (ROOT / 'Firm_Saturn/main.h').read_bytes()
        build = ROOT / 'tests/build/file-ack'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []

        # Keep the actual dispatcher, the intervening LISTBIN case, and the default.
        list_start = raw.index(b'\tcase SSCMD_LISTBIN:')
        list_end = raw.index(b'\tcase SSCMD_LISTDISC:', list_start)
        start = raw.index(b'\tcase SSCMD_FILERD:')
        end = raw.index(b'\tcase SSCMD_STARTUP:', start)
        default = raw.index(b'\tdefault:', end)
        dispatch = command_dispatch(raw)
        code = (b'static void command(void) { int retv; u32 cmd = SS_CMD;\n'
                + dispatch + b'\n' + raw[list_start:list_end] + raw[start:end]
                + raw[default:raw.index(b'\n\t}', default)] + b'\n}}\n')
        (cls.target / 'command.inc').write_bytes(code)
        (cls.target / 'normalizer.inc').write_bytes(dispatch[:dispatch.index(b'switch(save_cmd){')])
        start = raw.index(b'_Static_assert(FR_INVALID_PARAMETER')
        (cls.target / 'invariants.inc').write_bytes(raw[start:raw.index(b'void ss_cmd_handle(', start)])

        (cls.target / 'protocol.h').write_bytes(protocol_definitions(header))
        for name in (b'SSCMD_FILERD', b'SSCMD_FILEWR', b'SSCMD_SAVE_ACK',
                     b'FILE_ACK_VERSION', b'SAVE_ACK_VERSION'):
            pattern = rb'^#define ' + name + rb'\s+(\S+)'
            assert re.search(pattern, header, re.M)[1] == re.search(pattern, menu_header, re.M)[1]

        for name, offset in ((b'save', b'10'), (b'file', b'14')):
            publish = re.search(rb'^[ \t]*\*\(u32\*\)\(SYSINFO_ADDR\+0x' + offset + rb'\) = '
                                + name.upper() + rb'_ACK_VERSION;', raw, re.M)
            latch = re.search(rb'^[ \t]*' + name + rb'_ack = LE32.*$', ui, re.M)
            (cls.target / (name.decode() + '-publish.inc')).write_bytes(publish[0])
            (cls.target / (name.decode() + '-latch.inc')).write_bytes(latch[0])
        start = ui.index(b'u32 LE32(void *ptr)')
        (cls.target / 'le32.inc').write_bytes(ui[start:ui.index(b'\n}', start) + 2])

        flags = ['-O2'] if os.environ.get('SAROO_FILE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'file-ack'
        inputs = (*sources, ROOT / 'Firm_MCU/Saturn/cdc.h', ROOT / 'Firm_Saturn/main.h',
                  ROOT / 'Firm_MCU/FatFS/ff.h', ROOT / 'Firm_MCU/FatFS/ffconf.h',
                  ROOT / 'tests/file_ack.c', Path(__file__).resolve())
        (cls.target / 'inputs.json').write_text(json.dumps({
            str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in inputs
        }, indent=2) + '\n')
        result = subprocess.run(
            [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall', '-Wextra',
             '-Werror', *flags, '-I', str(cls.target), '-I', str(ROOT / 'Firm_MCU/FatFS'),
             str(ROOT / 'tests/file_ack.c'), '-o', str(cls.binary)],
            capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, mode, count):
        result = subprocess.run([str(self.binary), mode], capture_output=True, text=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(mode=mode, returncode=result.returncode, stdout=result.stdout,
                                 stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, f'PASS {mode} {count} checks\n')

    def test_independent_capabilities_and_entry_latches(self):
        self.invoke('capability', 68)

    def test_every_opcode_preserves_listbin_gap_and_unknown_commands(self):
        self.invoke('dispatch', 65536)

    def test_every_fatfs_error_preserves_signed_file_and_unsigned_save_results(self):
        self.invoke('errors', 280)


if __name__ == '__main__':
    unittest.main()
