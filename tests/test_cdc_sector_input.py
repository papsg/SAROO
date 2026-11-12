"""Actual sector producer with synthetic FatFs outcomes, payloads and cache."""
# Adapted from paps-g selected8e465233; no private preparer or device operations.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

def reader_source(raw):
    code=b''
    for signature in (b'TRACK_INFO *get_track_info(', b'u8 track_subindex(', b'static int read_disc_bytes(', b'int get_sector('):
        start=raw.find(signature)
        if start>=0:code+=raw[start:raw.index(b'\n}',start)+2]+b'\n'
    return code


class CDSectorInputTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=Path(os.environ.get('SAROO_CDC_INPUT_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_main.c')).read_bytes()
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-sector-input';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        code=reader_source(raw)
        (cls.target/'sector-input.inc').write_bytes(code)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_main.c':raw,'cdc.h':header}
        for name in ('cdc_sector_input.c','test_cdc_sector_input.py'):inputs[name]=(ROOT/'tests'/name).read_bytes()
        inputs['track_fixture.h']=(ROOT/'tests/track_fixture.h').read_bytes()
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'sector-input'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',
              '-Wno-sign-compare',*flags,'-I',str(cls.target),str(cls.target/'cdc_sector_input.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=45,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_all_seek_errors_skip_read_and_payload_publication(self):self.run_case(0)
    def test_8192_alignment_short_read_edges(self):self.run_case(1)
    def test_6144_exact_complete_and_full_cache_reads(self):self.run_case(2)
    def test_cache_hits_boundary_misses_and_error_recovery(self):self.run_case(3)
    def test_track_lookup_metadata_audio_and_null_output_controls(self):self.run_case(4)
    def test_baseline_valid_read_error_controls(self):self.run_case(5)
    def test_directory_errors_retain_worker_ownership(self):self.run_case(6)


if __name__=='__main__':unittest.main()
