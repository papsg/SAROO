"""Actual menu CD transfers with an ordered FIFO and scripted command errors."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_menu_cdplayer import ROOT, function


class MenuCDTransferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CD_SOURCE', ROOT / 'Firm_Saturn'))
        raw = (source / 'cdblock.c').read_bytes()
        names = ('cdc_set_size', 'cdc_reset_selector', 'cdc_cddev_connect',
                 'cdc_play_fad', 'cdc_get_numsector', 'cdc_get_del_data',
                 'cdc_end_trans', 'cdc_trans_data')
        code = b''.join(function(raw, name) for name in names)
        if b'static int cdc_file_info(' in raw:
            code += b'\n#define HAS_FILE_INFO_COUNT 1\n' + function(raw, 'cdc_file_info')
        code += function(raw, 'cdc_get_file_info') + function(raw, 'cdc_read_sector')
        hooks = (source / 'game_patch.c').read_bytes()
        if b'static u8 kaitei_music_ready;' in hooks:
            code += b'\nstatic u8 kaitei_music_ready;\n'
        for name in ('KAITEI_DAISENSOU_handle1', 'KAITEI_DAISENSOU_handle2',
                     'KAITEI_DAISENSOU_patch'):
            code += function(hooks, name)
        for original, replacement in (
            (b'(void*)0x22400000', b'hook_ram'),
            (b'(u8*)0x22400000', b'hook_ram'),
            (b'(u8*)0x2AA000', b'hook_game'),
            (b'(u32*)(0x6007324)', b'&hook_install[0]'),
            (b'(u32*)(0x60044E0)', b'&hook_install[1]'),
        ):
            assert code.count(original) == 1
            code = code.replace(original, replacement)

        build = ROOT / 'tests/build/menu-cd-transfer'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'transfers.inc').write_bytes(code)
        (cls.target / 'commands.inc').write_bytes(
            function(raw, 'cdc_cmd') + function(raw, 'cdc_end_trans'))
        flags = ['-O2'] if os.environ.get('SAROO_MENU_CD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binaries = {}
        for kind, definitions in (('transfers', []), ('commands', ['-DTEST_COMMAND'])):
            binary = cls.target / kind
            result = subprocess.run(
                [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall',
                 '-Wextra', '-Werror', '-Wno-pointer-to-int-cast', *flags, *definitions,
                 '-I', str(cls.target), str(ROOT / 'tests/menu_cd_transfer.c'), '-o', str(binary)],
                capture_output=True, text=True, timeout=60)
            (cls.target / f'{kind}-compile.log').write_text(result.stdout + result.stderr)
            if result.returncode:
                raise RuntimeError(result.stderr)
            cls.binaries[kind] = binary

    def scenario(self, name, kind='transfers'):
        result = subprocess.run([str(self.binaries[kind]), name], capture_output=True,
                                text=True, timeout=30,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'{name}.log').write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_fifo_copy_preserves_alignment_and_byte_extent(self):
        self.scenario('copy')

    def test_reads_preserve_partial_tails_and_form2_slicing(self):
        self.scenario('read')

    def test_reader_propagates_all_command_failures(self):
        self.scenario('errors')

    def test_reader_rejects_invalid_completion_counts(self):
        self.scenario('counts')

    def test_reader_admission_and_size_arithmetic(self):
        self.scenario('admission')

    def test_file_information_bounds_and_cleanup(self):
        self.scenario('files')

    def test_music_hook_requires_complete_preload(self):
        self.scenario('hooks')

    def test_accepted_data_ready_timeout_ends_once(self):
        self.scenario('cleanup', 'commands')

    def test_other_commands_skip_transfer_cleanup(self):
        self.scenario('ordinary', 'commands')


if __name__ == '__main__':
    unittest.main()
