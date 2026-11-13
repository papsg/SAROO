"""Actual shared routing/copy/move and real cache-hit disc producer."""
# Adapted from paps-g 50100444; no private preparer or device operation.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from test_cdc_copy import copy_source
from test_cdc_filter import filter_source
from test_cdc_sector_input import reader_source

ROOT=Path(__file__).resolve().parents[1]


class CDRoutingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=Path(os.environ.get('SAROO_CDC_ROUTING_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c')).read_bytes()
        main=Path(os.environ.get('SAROO_CDC_ROUTING_MAIN',ROOT/'Firm_MCU/Saturn/saturn_main.c')).read_bytes()
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-routing';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(data,signature):
            start=data.index(signature)
            return data[start:data.index(b'\n}',start)+2]+b'\n'
        (cls.target/'copy.inc').write_bytes(copy_source(raw,required=(b'sector_destination(',)))
        code=filter_source(raw)
        code+=b'#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wsign-compare"\n'
        code+=reader_source(main)+b'#pragma GCC diagnostic pop\n'
        if b'static int sector_destination(' in raw:code+=b'#define TEST_HAS_SHARED_ROUTE 1\n'
        (cls.target/'routing.inc').write_bytes(code)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':raw,'saturn_main.c':main,'cdc.h':header}
        for name in ('cdc_copy.c','test_cdc_copy.py','cdc_routing.c','test_cdc_routing.py','test_cdc_sector_input.py','test_cdc_filter.py'):inputs[name]=(ROOT/'tests'/name).read_bytes()
        inputs['track_fixture.h']=(ROOT/'tests/track_fixture.h').read_bytes()
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'routing'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror','-Wno-unused-variable',*flags,'-I',str(cls.target),str(cls.target/'cdc_routing.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=20,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_144_split_copy_move_original_selection_shapes(self):self.run_case(0)
    def test_discard_avoids_copy_work_retains_admission_and_last_cd(self):self.run_case(1)
    def test_real_disc_metadata_and_logical_fad_survive_routed_copy(self):self.run_case(2)
    def test_4096_baseline_valid_mode2_predicate_controls(self):self.run_case(3)
    def test_normalized_non_mode2_conditions(self):self.run_case(4)
    def test_defined_full_width_range_representation_and_edges(self):self.run_case(5)

    def test_96_mode2_reader_filter_and_routed_copy_layouts(self):self.run_case(6)
    def test_96_mode2_filter_container_payload_and_metadata_controls(self):self.run_case(7)


if __name__=='__main__':unittest.main()
