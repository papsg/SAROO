"""Compare actual shared CD setup against preserved public handlers."""
# Adapted from paps-g 96f9329a; no private preparer, logging changes or device use.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CDSetupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_CDC_SETUP_SOURCE', ROOT / 'Firm_MCU/Saturn/saturn_cdc.c'))
        candidate = source.read_bytes()
        original = (ROOT / 'tests/fixtures/cdc_setup_baseline.inc').read_bytes()
        provenance = (ROOT / 'tests/fixtures/cdc_setup_baseline.json').read_bytes()
        body = original[original.index(b'int get_sector_data('):]
        if hashlib.sha256(body).hexdigest() != json.loads(provenance)['body_sha256']:
            raise AssertionError('preserved public baseline changed')
        build = ROOT / 'tests/build/cdc-setup'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        def function(data, signature):
            start = data.index(signature)
            return data[start:data.index(b'\n}', start)+2]+b'\n'
        names = ('get_sector_data', 'get_del_sector_data', 'change_dir', 'read_dir')
        for label, data in (('original', original), ('candidate', candidate)):
            code = b''
            if label == 'candidate':
                for signature in (b'static int sector_range(',
                                  b'static __attribute__((noinline)) int get_sector_data_common(',
                                  b'static __attribute__((noinline)) void start_directory_read('):
                    if signature in data: code += function(data, signature)
            for name in names:
                code += function(data, ('int '+name+'(void)\n{').encode())
            if label == 'original':
                for name in names:
                    code = code.replace((name+'(').encode(), ('original_'+name+'(').encode())
            (cls.target / (label+'.inc')).write_bytes(code)
        (cls.target / 'filter.inc').write_bytes(function(candidate, b'static void _set_filter('))
        header = (ROOT / 'Firm_MCU/Saturn/cdc.h').read_bytes()
        definitions = header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')]
        logs = header[header.index(b'#define _BUFIO '):header.index(b'void cdc_cmd_process(')]
        (cls.target / 'types.h').write_bytes(definitions+logs)
        inputs = {'saturn_cdc.c': candidate, 'cdc.h': header, 'baseline.inc': original,
                  'baseline.json': provenance, 'cdc_setup.c': (ROOT/'tests/cdc_setup.c').read_bytes(),
                  'test_cdc_setup.py': (ROOT/'tests/test_cdc_setup.py').read_bytes()}
        for name in ('ff.h', 'ffconf.h'):
            inputs[name] = (ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name, data in inputs.items(): (cls.target/name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps({
            n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()}, indent=2)+'\n')
        flags = ['-O2'] if os.environ.get('SAROO_SHRINK_PROFILE') == 'release' else [
            '-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary = cls.target / 'cdc-setup'
        command = [*shlex.split(os.environ.get('CC','cc')), '-std=gnu11', '-g',
                   '-Wall','-Wextra','-Werror','-Wno-unknown-attributes', *flags,
                   '-I',str(cls.target),str(cls.target/'cdc_setup.c'),'-o',str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(command,indent=2)+'\n')
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def invoke(self, mode):
        r = subprocess.run([str(self.binary),mode],capture_output=True,text=True,timeout=30,
                           env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(mode=mode,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('pass:',r.stdout)

    def test_transfer_ranges_busy_state_logs_and_partition_selection(self): self.invoke('transfer')

    def test_directory_state_and_wakeup_order(self): self.invoke('directory')

    def test_independent_transfer_and_directory_contracts(self): self.invoke('contracts')


if __name__ == '__main__':
    unittest.main()
