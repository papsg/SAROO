"""Actual metadata producer/native transfer; explicit FIFO and SH-2 store seams.

Adapted from paps-g's acf616d6 / SSR-037. No vendor SDK or device is required.
"""
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


def function(raw, signature):
    start = raw.index(signature)
    return raw[start:raw.index(b'\n}', start) + 2] + b'\n'


def structure(raw, name):
    return re.search(rb'typedef struct\s*\{[^}]*\}\s*' + name + rb';', raw).group() + b'\n'


class CDFileInfoWireTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_CDC_FILE_INFO_SOURCE', ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        inputs = {
            'saturn_cdc.c': source.read_bytes(),
            'cdc.h': (ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes(),
            'cdblock.c': (ROOT/'Firm_Saturn/cdblock.c').read_bytes(),
            'main.h': (ROOT/'Firm_Saturn/main.h').read_bytes(),
            'SSMaster.v': (ROOT/'FPGA/SSMaster.v').read_bytes(),
            'fixture.c': (ROOT/'tests/cdc_file_info_wire.c').read_bytes(),
            'driver.py': Path(__file__).read_bytes(),
        }
        permutation = b'wire[15:0]ss_fifo_data_out={fifo_q[7:0],fifo_q[15:8]};'
        if permutation not in re.sub(rb'\s+', b'', inputs['SSMaster.v']):
            raise AssertionError('Reconcile the FIFO seam with the actual RTL')
        build = ROOT/'tests/build/cdc-file-info-wire'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        for name, data in inputs.items():
            (cls.target/name).write_bytes(data)
        (cls.target/'metadata.inc').write_bytes(structure(inputs['cdc.h'], b'FILE_INFO'))
        (cls.target/'native-types.inc').write_bytes(structure(inputs['main.h'], b'CdcFile') +
                                                   structure(inputs['main.h'], b'CDCMD'))
        (cls.target/'producer.inc').write_bytes(function(inputs['saturn_cdc.c'],
            b'static __attribute__((noinline)) void fill_file_info_fifo('))
        transfer = function(inputs['cdblock.c'], b'void cdc_trans_data(')
        transfer = transfer.replace(b'(u32)buf', b'(uintptr_t)buf')
        (cls.target/'consumer.inc').write_bytes(transfer +
            function(inputs['cdblock.c'], b'static int cdc_file_info(') +
            function(inputs['cdblock.c'], b'int cdc_get_file_info('))
        (cls.target/'inputs.json').write_text(json.dumps({n: hashlib.sha256(v).hexdigest()
            for n, v in inputs.items()}, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_CDC_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target/'wire'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
                   str(cls.target/'fixture.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target/'command.json').write_text(json.dumps(command, indent=2) + '\n')
        (cls.target/'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def scenario(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True, text=True,
            timeout=30, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=name, returncode=result.returncode,
                                 stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('pass', result.stdout)

    def test_distinct_fields_through_both_transfer_branches(self): self.scenario('distinct')
    def test_equal_fields_are_valid_controls(self): self.scenario('equal')
    def test_all_declared_unit_gap_combinations(self): self.scenario('domain')
    def test_batched_queries_preserve_every_other_field(self): self.scenario('batch')
    def test_literal_native_record_without_struct_derived_offsets(self): self.scenario('literal')


if __name__ == '__main__': unittest.main()
