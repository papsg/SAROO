"""Actual0x43 reply with independent byte/state/IRQ oracles."""
# Adapted from paps-g f0da2b0b; no preparer, device operations or runtime diagnostics.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from cdc_reply_source import command_reply_source

ROOT=Path(__file__).resolve().parents[1]


class CDSubheaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=Path(os.environ.get('SAROO_CDC_SUBHEADER_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        raw=source.read_bytes();header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-subheader';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        start=raw.index(b'void set_status(')
        functions=raw[start:raw.index(b'\n}',start)+2]+b'\n'
        helper = b'static __attribute__((noinline)) FILTER *command_filter('
        if helper in raw:
            start = raw.index(helper)
            functions += raw[start:raw.index(b'\n}', start) + 2] + b'\n'
        functions += command_reply_source(raw)
        for name in ('set_filter_subheader','get_filter_subheader'):
            start=raw.index(('int '+name+'(void)\n{').encode())
            functions+=raw[start:raw.index(b'\n}',start)+2]+b'\n'
        (cls.target/'reply.inc').write_bytes(functions)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':raw,'cdc.h':header,'cdc_subheader.c':(ROOT/'tests/cdc_subheader.c').read_bytes(),'test_cdc_subheader.py':Path(__file__).read_bytes()}
        for name in ('ff.h','ffconf.h'):
            inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,blob in inputs.items():(cls.target/name).write_bytes(blob)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'subheader'
        command=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,'-I',str(cls.target),str(cls.target/'cdc_subheader.c'),'-o',str(cls.binary)]
        result=subprocess.run(command,capture_output=True,timeout=60);(cls.target/'compile.log').write_bytes(result.stdout+result.stderr);(cls.target/'command.json').write_text(json.dumps(command,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def invoke(self,mode,count):
        result=subprocess.run([str(self.binary),mode],capture_output=True,text=True,timeout=20,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(mode=mode,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr));self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn(f'{count} replies pass',result.stdout)

    def test_status_channel_and_valid_filter_fields(self):self.invoke('full',71680)
    def test_baseline_valid_zero_status_reply_control(self):self.invoke('zero',6144)
    def test_actual_set_get_subheader_roundtrip(self):self.invoke('roundtrip',6144)


if __name__=='__main__':unittest.main()
