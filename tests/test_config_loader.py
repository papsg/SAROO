"""Actual configuration parser: complete I/O, bounded outputs and publication."""
import json
import os
from pathlib import Path
import struct
import subprocess
import hashlib
import shlex
import tempfile
import unittest
from mcu_save_support import config_loader_source

ROOT = Path(__file__).resolve().parents[1]


class ConfigLoaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_CONFIG_LOADER_DIR', ROOT / 'Firm_MCU/Saturn'))
        module = (source / 'saturn_utils.c').read_bytes()
        main = (source / 'saturn_main.c').read_bytes()
        build = ROOT / 'tests/build/config-loader'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'config-loader.inc').write_bytes(config_loader_source(module))
        call = main.index(b'parse_config("/SAROO/saroocfg.txt", NULL)')
        gate = main[main.rfind(b'\n', 0, call)+1:main.index(b'\n', call)]
        (cls.target / 'config-startup-gate.inc').write_bytes(gate)
        inputs = {'saturn_utils.c': module, 'saturn_main.c': main,
                  'example.cfg': (ROOT / 'doc/saroocfg.txt').read_bytes()}
        for name in ('config_loader.c', 'test_config_loader.py', 'mcu_save_support.py'):
            data = (ROOT / 'tests' / name).read_bytes()
            (cls.target / name).write_bytes(data); inputs[name] = data
        for name,data in inputs.items(): (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name,data in inputs.items()}, indent=2)+'\n')
        cls.binary = cls.target / 'config-loader'
        flags = ['-O2'] if os.environ.get('SAROO_CONFIG_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                   '-Wno-unused-parameter', '-Wno-missing-field-initializers',
                   *flags, '-I', str(cls.target), str(cls.target / 'config_loader.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(command, indent=2)+'\n')
        result.check_returncode()
        cls.results = []

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, text, game=None, fault=0, code=1, size=None, startup=False):
        if isinstance(text, str): text = text.encode()
        path = self.target / ('input-%03d.cfg' % len(self.results))
        path.write_bytes(text)
        result = subprocess.run([str(self.binary), str(path), game or '-', str(fault),
                                 str(code), str(len(text) if size is None else size),
                                 *(['startup'] if startup else [])],
                                capture_output=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        record = dict(case=len(self.results), bytes=len(text), game=game, fault=fault,
                      code=code, size=size, startup=startup, returncode=result.returncode)
        if result.returncode:
            record['stderr'] = result.stderr.decode(errors='replace')
        self.results.append(record)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        output = json.loads(result.stdout)
        record['result'] = output['result']
        output['system'] = bytes.fromhex(output.pop('system_hex'))
        output['multi_bytes'] = bytes.fromhex(output.pop('multi_hex'))
        output['multi'] = output['multi_bytes'].split(b'\0')[0]
        self.assertEqual(output['locks'], [1, 1, 0] if game else [0, 0, 0])
        # System metadata outside patch/category ownership remains untouched.
        self.assertEqual(output['system'][:12], b'\xa5' * 12)
        self.assertEqual(output['system'][13:256], b'\xa5' * 243)
        return output

    def rejected(self, text, game=None, **kwargs):
        out = self.invoke(text, game, **kwargs)
        self.assertLess(out['result'], 0)
        self.assertEqual(out['values'], list(range(41, 49)))
        self.assertEqual(out['multi_bytes'], b'old-disc' + bytes(8))
        self.assertEqual(out['category_num'], 3)
        self.assertEqual(out['system'], b'\xa5' * 4096)
        self.assertEqual(out['leds'], 1)
        return out

    def test_global_and_game_precedence_and_scoped_values(self):
        text = ('[global]\nlang_id=2\ndebug=ffffffff\nlog_mask=0x80000000\n'
                'auto_update=1\nsort_mode=3\ncategory="Action"\n'
                'sector_delay=100\nplay_delay=200\npend_delay=300\n'
                'exmem_1M\nmulti_disc="Disc "\n'
                '[OTHER]\nsector_delay=999\n[T-12345 V1.000]\n'
                'lang_id=9\ncategory="Ignored"\nsector_delay=321\n'
                'exmem_4M\nM_06001000=12345678\n'
                '[NEXT]\nplay_delay=999')
        global_out = self.invoke(text)
        self.assertEqual(global_out['result'], 0)
        self.assertEqual(global_out['values'], [2, 0xffffffff, 0x80000000, 1, 100, 200, 300, 3])
        self.assertEqual(global_out['category_num'], 1)
        self.assertEqual(global_out['system'][0xe80:0xe87], b'Action\0')
        self.assertEqual(global_out['multi'], b'Disc ')
        game_out = self.invoke(text, 'T-12345 V1.000')
        self.assertEqual(game_out['result'], 0)
        self.assertEqual(game_out['values'], [41, 42, 43, 44, 321, 200, 300, 48])
        self.assertEqual(game_out['category_num'], 3)
        self.assertEqual(game_out['system'][0xe80:], b'\xa5' * 384)
        self.assertEqual(struct.unpack_from('<5I', game_out['system'], 0x100),
                         (0x30000001, 0x30000004, 0x46001000, 0x12345678, 0))
        unmatched = self.invoke(text, 'T-12345')
        self.assertEqual(unmatched['result'], 0)
        self.assertEqual(unmatched['values'][4:7], [100, 200, 300])

    def test_unknown_keys_comments_bom_and_line_endings(self):
        for ending in ('\n', '\r\n', '\r'):
            with self.subTest(ending=ending):
                text = '\ufeff# header' + ending + ending.join([
                    '  [global] # section with ] inside the comment', '\tplay_delay = +25 ; comment',
                    'debug_extra=666', 'future_option=anything', ' # play_delay=999',
                    'sector_delay=0', 'multi_disc="disc#; " ; suffix'])
                out = self.invoke(text)
                self.assertEqual(out['result'], 0)
                self.assertEqual(out['values'][1], 42)
                self.assertEqual(out['values'][4:7], [0, 25, 0])
                self.assertEqual(out['multi'], b'disc#; ')

    def test_strings_capacity_and_categories_are_complete(self):
        for key, capacity in (('category', 32), ('multi_disc', 16)):
            for length in (0, capacity-1, capacity, capacity+1, 2048):
                with self.subTest(key=key, length=length):
                    text = '[global]\nplay_delay=25\n' + key + '="' + 'X'*length + '"'
                    if length >= capacity: self.rejected(text)
                    else:
                        out = self.invoke(text)
                        self.assertEqual(out['result'], 0)
                        actual = out['multi'] if key == 'multi_disc' else out['system'][0xe80:].split(b'\0')[0]
                        self.assertEqual(actual, b'X' * length)
        for count in (12, 13):
            text = '[global]\n' + '\n'.join('category="%s"' % ('X'*30 + chr(65+i)) for i in range(count))
            if count == 13: self.rejected(text)
            else:
                out = self.invoke(text)
                self.assertEqual(out['result'], 0)
                self.assertEqual(out['category_num'], 12)
                self.assertEqual(out['system'][12], 12)
                for i in range(12):
                    self.assertEqual(out['system'][0xe80+i*32:0xea0+i*32], b'X'*30 + bytes([65+i, 0]))

    def test_numeric_and_memory_command_validation_precedes_publication(self):
        for field in ('play_delay=', 'play_delay=-1', 'play_delay=2147483648',
                      'play_delay=42949672960', 'play_delay=1garbage', 'play_delay 5',
                      'debug=100000000', 'debug=0x', 'debug=-1',
                      'multi_disc=noquote', 'multi_disc="unfinished',
                      'multi_disc="ok" junk', 'exmem_2M', 'exmem_1Mgarbage',
                      'M_=ff', 'M_06001000=1', 'M_06001000=123456',
                      'M_06001000=100000000', 'M_06001001=1234',
                      'M_06001002=12345678', 'M_100000000=ff',
                      'M_06001000=ff junk'):
            with self.subTest(field=field):
                out = self.rejected('[global]\nlang_id=5\nexmem_1M\n' + field)
        text = ('[global]\nplay_delay=2147483647\ndebug=FFFFFFFF\n'
                'M_06001001=ff # byte\nM_06001002=0Xabcd ; half\n'
                'M_26001004=01234567\n')
        out = self.invoke(text)
        self.assertEqual(out['result'], 0)
        self.assertEqual(struct.unpack_from('<7I', out['system'], 0x100),
                         (0x16001001, 255, 0x26001002, 0xabcd, 0x46001004, 0x1234567, 0))

    def test_command_capacity_reserves_the_terminator_and_complete_pairs(self):
        for singles, pairs, success in ((863, 0, True), (864, 0, False),
                                         (0, 431, True), (0, 432, False),
                                         (1, 431, True), (2, 431, False)):
            with self.subTest(singles=singles, pairs=pairs):
                text = '[global]\n' + 'exmem_4M\n' * singles + 'M_06001000=12345678\n' * pairs
                if not success: self.rejected(text)
                else:
                    out = self.invoke(text)
                    self.assertEqual(out['result'], 0)
                    expected = [0x30000004]*singles + [0x46001000, 0x12345678]*pairs + [0]
                    self.assertEqual(out['system'][256:256+len(expected)*4], struct.pack('<%dI' % len(expected), *expected))
                    self.assertEqual(out['system'][0xe80:], b'\xa5'*384)

    def test_global_and_game_commands_share_the_same_capacity(self):
        for extra, accepted in ((3, True), (4, False)):
            text = '[global]\n' + 'exmem_1M\n'*860 + '[GAME]\n' + 'exmem_4M\n'*extra
            if not accepted: self.rejected(text, 'GAME')
            else:
                out = self.invoke(text, 'GAME')
                self.assertEqual(out['result'], 0)
                expected = [0x30000001]*860 + [0x30000004]*extra + [0]
                self.assertEqual(out['system'][256:0xe80], struct.pack('<864I', *expected))

    def test_file_extent_short_reads_and_close_failures_leave_state_unchanged(self):
        valid = '[global]\nplay_delay=12\n'
        for size in (32768, 32769, 0xffffffff, 0x100000000, 0xffffffffffffffff):
            with self.subTest(size=size):
                out = self.rejected(valid, size=size)
                self.assertEqual(out['io'][:3], [1, 0, 1])
        text = valid.encode() + b'#' + b'X'*(32767-len(valid)-1)
        out = self.invoke(text)
        self.assertEqual(out['result'], 0)
        self.assertEqual(out['io'], [1, 1, 1, 32767, 32767])
        for fault in (1, 2, 3, 4):
            for code in (1, 3, 9, 15):
                with self.subTest(fault=fault, code=code):
                    out = self.rejected(valid, fault=fault, code=code)
                    self.assertEqual(out['io'][:3], [1, 0, 0] if fault == 1 else [1, 1, 1])
        out = self.rejected(valid, size=len(valid)+1)
        self.assertEqual(out['io'][:3], [1, 1, 1])

    def test_invalid_structure_does_not_commit_prior_selected_lines(self):
        for text in (b'[GAME]\nplay_delay=3',
                     b'[global\nplay_delay=3', b'[global] junk\n',
                     b'[global]\nplay_delay=3\0bad',
                     b'[global]\nplay_delay=3\n[BROKEN'):
            with self.subTest(text=text):
                self.rejected(text, 'GAME')
        # Nonselected options remain forward-compatible. Selection stops after
        # the first matching game; unrelated later sections are not applied.
        out = self.invoke('[global]\n[OTHER]\nplay_delay=bad\n[GAME]\nplay_delay=7\n[NEXT]\nplay_delay=bad', 'GAME')
        self.assertEqual(out['result'], 0)
        self.assertEqual(out['values'][5], 7)
        self.rejected('[global]\nlang_id=1\ncategory="okay"\nplay_delay=2\n'
                      'multi_disc="new"\nexmem_1M\nM_06001000=abcd\nplay_delay=oops')

    def test_absent_file_and_empty_selected_settings_reset_game_values(self):
        for text in ('', '# comments only\r\n; nothing selected\n', 'play_delay=999\n'):
            out = self.invoke(text)
            self.assertEqual(out['result'], 0)
            self.assertEqual(out['values'], [41, 42, 43, 44, 0, 0, 0, 48])
            self.assertEqual(out['multi'], b'')
            self.assertEqual(out['category_num'], 0)
            self.assertEqual(out['system'][256:260], b'\0'*4)
        for game in (None, 'GAME'):
            for missing in (False, True):
                with self.subTest(game=game, missing=missing):
                    out = self.invoke('[global]\n', game, fault=1 if missing else 0, code=4)
                    self.assertEqual(out['result'], 0)
                    self.assertEqual(out['values'], [41, 42, 43, 44, 0, 0, 0, 48])
                    self.assertEqual(out['multi'], b'')
                    self.assertEqual(out['system'][256:260], b'\0'*4)
                    self.assertEqual(out['category_num'], 3 if game else 0)
                    self.assertEqual(out['leds'], 0)

    def test_actual_startup_gate_requires_an_accepted_configuration(self):
        for text in ('[global]\n', '[global]\ncategory="Action"\n'):
            self.assertEqual(self.invoke(text, startup=True)['started'], 1)
        for text in ('[wrong]\n', '[global]\nlang_id=2\ncategory="' + 'X'*32 + '"'):
            out = self.rejected(text, startup=True)
            self.assertEqual(out['started'], 0)

    def test_shipped_example_global_and_every_game_selection(self):
        text = (self.target / 'example.cfg').read_bytes()
        out = self.invoke(text)
        self.assertEqual(out['result'], 0)
        self.assertEqual(out['values'], [0, 42, 43, 44, 0, 0, 0, 48])
        # Independent expected delay tuples: sector, play, pend. No parser reuse.
        expected = {
            'T-1248G V1.004': (2000, 0, 0), 'T-1109G V1.002': (4000, 0, 0),
            'T-10307G V1.002': (1000, 0, 0), 'T-4804G V1.004': (2000, 0, 0),
            'T-9520G V1.010': (4000, 0, 0), 'T-14418G V1.004': (6000, 0, 0),
            'GS-9043 V1.002': (4000, 0, 0), 'T-21001G V1.003': (1000, 0, 0),
            'T-7643G V1.001': (2000, 0, 0), 'T-20607G V1.003': (0, 2000, 0),
            'GS-9009 V1.000': (5000, 16000, 0), 'GS-9152 V1.113': (0, 50000, 0),
            'GS-9133 V1.003': (5000, 16000, 0), 'T-31505G V1.004': (0, 50000, 0),
            'MK-81004 V1.017': (1000, 0, 0), 'GM-81004 V1.017': (1000, 0, 0),
            'T-8118G V1.000': (4200, 100000, 0), 'MK_8120050V1.000': (500, 0, 0),
            'MK-81200 V1.000': (500, 0, 0), 'GS-9013 V1.000': (500, 0, 0),
            'T-4804H V1.000': (1000, 0, 0), 'GS-9084 V1.100': (5000, 50000, 0),
            'T-20507G V1.002': (0, 5000, 5000), 'T-6006G V1.001': (8000, 0, 0),
            'T-11501H00V1.000': (8000, 0, 0), 'T-7902H V3.000': (8000, 0, 0),
            'MK-81005 V1.000': (1000, 0, 0), 'MK_8100550V1.000': (1000, 0, 0),
            'GS-9001 V1.000': (1000, 0, 0), 'T-1313G V1.005': (0, 8000, 0),
        }
        for game, delays in expected.items():
            with self.subTest(game=game):
                out = self.invoke(text, game)
                self.assertEqual(out['result'], 0)
                self.assertEqual(out['values'], [41, 42, 43, 44, *delays, 48])
                self.assertEqual(out['system'][256:260], bytes(4))

    def test_scalar_types_validate_full_values_at_32_bit_boundaries(self):
        names = ['lang_id', 'debug', 'log_mask', 'auto_update',
                 'sector_delay', 'play_delay', 'pend_delay', 'sort_mode']
        for index, name in enumerate(names):
            hexadecimal = name in ('debug', 'log_mask')
            for value in (0, 1, 65535, 0x7fffffff, 0x80000000, 0xffffffff, 0x100000000):
                with self.subTest(name=name, value=value):
                    text = '[global]\n' + name + '=' + (format(value, 'x') if hexadecimal else str(value))
                    if value > (0xffffffff if hexadecimal else 0x7fffffff):
                        self.rejected(text)
                    else:
                        out = self.invoke(text)
                        self.assertEqual(out['result'], 0)
                        self.assertEqual(out['values'][index], value)


    def test_setting_names_require_complete_case_sensitive_tokens(self):
        names = ('lang_id', 'debug', 'log_mask', 'auto_update', 'category',
                 'sector_delay', 'play_delay', 'pend_delay', 'multi_disc', 'sort_mode')
        for name in names:
            # Truncated names and longer near-matches remain unknown options;
            # none may alias a setting or validate its deliberately bad value.
            variants = [name[:n] for n in range(1, len(name))]
            variants += [name.upper(), name + '_extra', name + '\u00e9']
            for variant in variants:
                with self.subTest(key=variant):
                    out = self.invoke('[global]\n' + variant + '=invalid')
                    self.assertEqual(out['result'], 0)
                    self.assertEqual(out['values'], [41, 42, 43, 44, 0, 0, 0, 48])
                    self.assertEqual(out['multi'], b'')
                    self.assertEqual(out['category_num'], 0)
                    self.assertEqual(out['system'][256:260], b'\0' * 4)
                    self.assertEqual(out['system'][0xe80:], b'\xa5' * 384)

    def test_scalar_last_assignment_wins_without_cross_setting_aliases(self):
        names = ('lang_id', 'debug', 'log_mask', 'auto_update',
                 'sector_delay', 'play_delay', 'pend_delay', 'sort_mode')
        values = [101, 102, 103, 104, 105, 106, 107, 108]
        lines = ['[global]'] + [name + '=0' for name in names]
        for index in reversed(range(len(names))):
            value = format(values[index], 'x') if names[index] in ('debug', 'log_mask') else str(values[index])
            lines.append(names[index] + '\t= ' + value + ' ; retained')
        out = self.invoke('\n'.join(lines))
        self.assertEqual(out['result'], 0)
        self.assertEqual(out['values'], values)

    def test_leading_zeroes_preserve_numeric_limits_and_overflow_rejection(self):
        for key, base, limit, index in (('debug', 16, 0xffffffff, 1),
                                         ('play_delay', 10, 0x7fffffff, 5)):
            for value in (0, limit - 1, limit, limit + 1, 0x100000001):
                for zeroes in (0, 1, 32, 1000):
                    with self.subTest(key=key, value=value, zeroes=zeroes):
                        digits = format(value, 'x') if base == 16 else str(value)
                        prefix = '+0X' if base == 16 else '+'
                        text = '[global]\nsector_delay=12\n' + key + '=' + prefix + '0' * zeroes + digits
                        if value > limit:
                            self.rejected(text)
                        else:
                            out = self.invoke(text)
                            self.assertEqual(out['result'], 0)
                            self.assertEqual(out['values'][index], value)

if __name__ == '__main__':
    unittest.main()
