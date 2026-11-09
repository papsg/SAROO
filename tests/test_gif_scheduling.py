"""Actual startup/UART/editor source and pinned RTX priority decisions."""
# Adapted from paps-g f285ede8; public UART shell kept, no reload/USB fixture.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GifSchedulingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_GIF_SCHEDULING_DIR', ROOT / 'Firm_MCU'))
        cls.inputs = {p: (source / p).read_bytes() for p in (
            'Main/main.c', 'Main/shell.c', 'Main/uart4.c', 'Saturn/saturn_main.c')}
        main, shell, uart, saturn = (cls.inputs[p] for p in cls.inputs)
        build = ROOT / 'tests/build/gif-scheduling'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        start = main.index(b'void main_task(')
        end = main.index(b'\n}', start) + 2
        (cls.target / 'startup-task.inc').write_bytes(main[start:end])
        start = main.index(b'int main(void)')
        (cls.target / 'task-creation.inc').write_bytes(main[start:].replace(
            b'int main(void)', b'int firmware_main(void)', 1))
        start = saturn.index(b'\tosThreadAttr_t attr;', saturn.index(b'void saturn_config('))
        end = saturn.index(b'\n}', start)
        (cls.target / 'saturn-creation.inc').write_bytes(
            b'static void firmware_saturn_workers(void) {\n' + saturn[start:end] + b'\n}\n')
        start = shell.index(b'static char hbuf[')
        end = shell.index(b'static int arg_base', start)
        (cls.target / 'line-editor.inc').write_bytes(shell[start:end])
        start = uart.index(b'uint8_t _getc(void)')
        end = uart.index(b'void _putc(', start)
        (cls.target / 'uart.inc').write_bytes(uart[start:end])
        vendor = ROOT / 'tests/vendor/rtx'
        provenance = json.loads((vendor / 'provenance.json').read_text())
        dispatch = (vendor / 'thread_dispatch.inc').read_bytes()
        function = dispatch[dispatch.index(b'void osRtxThreadDispatch ('):]
        if hashlib.sha256(function).hexdigest() != provenance['function_sha256']:
            raise AssertionError('pinned scheduler fixture changed')
        (cls.target / 'thread_dispatch.inc').write_bytes(dispatch)
        cls.inputs['vendor/thread_dispatch.inc'] = dispatch
        cls.inputs['vendor/provenance.json'] = (vendor / 'provenance.json').read_bytes()
        cls.inputs['vendor/LICENSE.txt'] = (vendor / 'LICENSE.txt').read_bytes()
        for name in ('gif_scheduling.c', 'test_gif_scheduling.py'):
            cls.inputs[name] = (ROOT / 'tests' / name).read_bytes()
        for name, data in cls.inputs.items():
            path = cls.target / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps({
            n: hashlib.sha256(b).hexdigest() for n, b in cls.inputs.items()}, indent=2)+'\n')
        flags = ['-O2'] if os.environ.get('SAROO_GIF_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binaries = {}
        for profile in ('app', 'boot'):
            binary = cls.target / ('scheduling-' + profile)
            command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                       '-Wall', '-Wextra', '-fno-strict-aliasing', *flags,
                       *(['-DBOOT'] if profile == 'boot' else []), '-I', str(cls.target),
                       str(cls.target / 'gif_scheduling.c'), '-o', str(binary)]
            result = subprocess.run(command, capture_output=True, timeout=60)
            (cls.target / (profile + '-compile.log')).write_bytes(result.stdout + result.stderr)
            (cls.target / (profile + '-command.json')).write_text(json.dumps(command, indent=2)+'\n')
            result.check_returncode()
            cls.binaries[profile] = binary

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def invoke(self, mode, profile='app'):
        result = subprocess.run([str(self.binaries[profile]), mode], timeout=10,
            capture_output=True, text=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(profile=profile, mode=mode, returncode=result.returncode,
                                 stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('scheduling: pass', result.stdout)

    def test_background_progress_after_startup(self): self.invoke('background')

    def test_preemption_during_modeled_menu_music_waits(self): self.invoke('music')

    def test_uart_editing_and_history_are_preserved(self):
        for profile in ('app', 'boot'):
            with self.subTest(profile=profile): self.invoke('editor', profile)

    def test_boot_startup_remains_free_of_gif_service(self): self.invoke('background', 'boot')

    def test_original_task_priorities_and_pinned_dispatcher(self): self.invoke('priority')

    def test_timer_has_only_one_background_caller(self):
        caller = re.compile(rb'\bgif_decode_timer\s*\(\s*\)\s*;')
        owners = [n for n in ('Main/main.c', 'Main/shell.c', 'Saturn/saturn_main.c')
                  for _ in caller.finditer(self.inputs[n])]
        self.assertEqual(owners, ['Main/shell.c'])


if __name__ == '__main__':
    unittest.main()
