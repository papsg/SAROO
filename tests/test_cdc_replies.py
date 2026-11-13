"""Check the actual CD reply handlers and four halfword stores."""
# Adapted from paps-g 48f6a105.
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from cdc_reply_source import bswap_source, command_reply_source


ROOT = Path(__file__).resolve().parents[1]


class CDReplyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_CDC_REPLY_SOURCE', ROOT / 'Firm_MCU/Saturn/saturn_cdc.c'))
        raw = source.read_bytes()
        header = (ROOT / 'Firm_MCU/Saturn/cdc.h').read_bytes()
        build = ROOT / 'tests/build/cdc-replies'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))

        def function(signature, data=raw):
            start = data.index(signature)
            return data[start:data.index(b'\n}', start) + 2] + b'\n'

        code = b'static u8 copy_move_error;\nstatic int old_numblocks, old_block_free;\n'
        code += command_reply_source(raw)
        selector = b'static __attribute__((noinline)) FILTER *command_filter('
        if selector in raw:
            code += function(selector)
        code += function(b'void set_status(') + function(b'BLOCK *find_block(')
        main = (ROOT / 'Firm_MCU/Saturn/saturn_main.c').read_bytes()
        code += function(b'TRACK_INFO *get_track_info(', main) + bswap_source(header, main)
        names = ('get_hw_info', 'get_toc_info', 'get_session_info', 'get_subcode',
                 'get_cdev_connect', 'get_last_buffer', 'get_filter_range',
                 'get_filter_subheader', 'get_filter_mode', 'get_filter_connection',
                 'get_buffer_size', 'get_sector_num', 'get_actual_size',
                 'get_sector_info', 'get_copy_error', 'get_file_scope', 'auth_device')
        code += b''.join(function(f'int {name}('.encode()) for name in names)
        (cls.target / 'handlers.inc').write_bytes(code)
        (cls.target / 'types.h').write_bytes(
            header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        (cls.target / 'saturn_cdc.c').write_bytes(raw)
        cls.binary = cls.target / 'cdc-replies'
        flags = ['-O2'] if os.environ.get('SAROO_CDC_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
                   '-I', str(ROOT / 'Firm_MCU/FatFS'), str(ROOT / 'tests/cdc_replies.c'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

    def run_case(self, args, expected):
        result = subprocess.run([str(self.binary), *args], capture_output=True, text=True,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'},
                                timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(expected, result.stdout)
        print(result.stdout.strip())

    def test_complete_replies_keep_values_order_and_command_state(self):
        self.run_case([], '1114112 complete replies pass')

    def test_cd_word_swap_preserves_wire_byte_order(self):
        self.run_case(['byteorder'], '65536 CD words have the expected byte order')

    def test_session_unavailable_keeps_all_ones_for_every_selector(self):
        self.run_case(['unavailable'], '1024 session replies pass')

    def test_session_valid_reports_preserve_leadout_and_transfer_owner(self):
        self.run_case(['valid'], '18432 session replies pass')


if __name__ == '__main__':
    unittest.main()
