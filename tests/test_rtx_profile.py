"""Actual configured RTX startup and tick paths."""
# Adapted from paps-g 1b49c143; no private preparation, transport or device access.
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


class RTXProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        config = Path(os.environ.get('SAROO_RTX_CONFIG', ROOT / 'Firm_MCU/RTE/CMSIS/RTX_Config.h'))
        build = ROOT / 'tests/build/rtx-profile'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        inputs = {'RTX_Config.h': config.read_bytes()}
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        p = subprocess.run([*compiler, '-E', '-dM', '-x', 'c', str(config)],
                           capture_output=True, timeout=30)
        (cls.target / 'preprocess.log').write_bytes(p.stderr)
        p.check_returncode()
        keys = {b'OS_TIMER_THREAD_STACK_SIZE', b'OS_TIMER_CB_QUEUE', b'OS_TICK_FREQ', b'OS_ROBIN_ENABLE'}
        definitions = [line for line in p.stdout.splitlines() if len(line.split()) > 2 and line.split()[1] in keys]
        if len(definitions) != len(keys):
            raise AssertionError('reassess configured timer/tick macros')
        (cls.target / 'profile-macros.h').write_bytes(b'\n'.join(
            b'#ifndef '+line.split()[1]+b'\n'+line+b'\n#endif' for line in definitions)+b'\n')
        vendor = ROOT / 'tests/vendor/rtx'
        mapping = {'timer_config.inc': 'timer-config.inc', 'thread_startup.inc': 'thread-startup.inc',
                   'thread_delay_tick.inc': 'delay-tick.inc', 'system_tick.inc': 'system-tick.inc'}
        provenance = (vendor / 'timer_profile_provenance.json').read_bytes()
        inputs['vendor/provenance.json'] = provenance
        expected = {row['file']: row['body_sha256'] for row in json.loads(provenance)['extracts']}
        for name, destination in mapping.items():
            data = (vendor / name).read_bytes()
            # Original vendor header ends before the selected function/configuration.
            body = data[data.index(b'#if ((OS_TIMER_THREAD_STACK_SIZE' if name == 'timer_config.inc' else
                                   {'thread_startup.inc': b'bool_t osRtxThreadStartup (',
                                    'thread_delay_tick.inc': b'void osRtxThreadDelayTick (',
                                    'system_tick.inc': b'void osRtxTick_Handler ('}[name]):]
            if hashlib.sha256(body).hexdigest() != expected[name]:
                raise AssertionError('pinned RTX extraction changed: '+name)
            (cls.target / destination).write_bytes(data)
            inputs['vendor/'+name] = data
        for name in ('rtx_profile.c', 'test_rtx_profile.py'):
            inputs[name] = (ROOT / 'tests' / name).read_bytes()
        for name, data in inputs.items():
            path = cls.target / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps({
            n: hashlib.sha256(b).hexdigest() for n, b in inputs.items()}, indent=2)+'\n')
        flags = ['-O2'] if os.environ.get('SAROO_SHRINK_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binaries = {}
        for mode in ('selected', 'enabled'):
            binary = cls.target / ('profile-'+mode)
            command = [*compiler, '-std=gnu11', '-g', '-Wall', '-Wextra', *flags,
                       *(['-DOS_TIMER_THREAD_STACK_SIZE=256', '-DOS_TIMER_CB_QUEUE=4'] if mode == 'enabled' else []),
                       '-I', str(cls.target), str(cls.target / 'rtx_profile.c'), '-o', str(binary)]
            p = subprocess.run(command, capture_output=True, timeout=60)
            (cls.target / (mode+'-compile.log')).write_bytes(p.stdout+p.stderr)
            (cls.target / (mode+'-command.json')).write_text(json.dumps(command, indent=2)+'\n')
            p.check_returncode()
            cls.binaries[mode] = binary

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def invoke(self, case, mode='selected'):
        r = subprocess.run([str(self.binaries[mode]), case], timeout=10, capture_output=True, text=True,
                           env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=case, mode=mode, returncode=r.returncode,
                                 stdout=r.stdout, stderr=r.stderr))
        self.assertEqual(r.returncode, 0, r.stdout+r.stderr)
        self.assertIn('pass', r.stdout)

    def test_selected_profile_starts_only_idle(self): self.invoke('startup')

    def test_ticks_delay_and_wait_timeouts_cross_wrap(self): self.invoke('ticks')

    def test_restoring_timer_configuration_restores_worker(self): self.invoke('restore', 'enabled')

    def test_application_has_no_software_timer_api_dependency(self):
        # SDK providers are deliberately not application callers. Any new live
        # use requires reviewing/re-enabling the service before this check passes.
        pattern = re.compile(rb'\bosTimer(?:New|Start|Stop|Delete|GetName|IsRunning)\b')
        uses = []
        for directory in ('Main', 'Saturn', 'FatFS'):
            for path in (ROOT / 'Firm_MCU' / directory).glob('*.c'):
                if pattern.search(path.read_bytes()): uses.append(str(path.relative_to(ROOT)))
        self.assertEqual(uses, [])


if __name__ == '__main__':
    unittest.main()
