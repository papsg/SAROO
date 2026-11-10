"""Actual host-write lifecycle with synthetic occupancy, faults and ownership."""
# Adapted from paps-g 50100444/3f7bba1a; no device operations or private preparer.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from test_cdc_copy import copy_source

ROOT=Path(__file__).resolve().parents[1]


class CDPutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=Path(os.environ.get('SAROO_CDC_PUT_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c')).read_bytes()
        main=Path(os.environ.get('SAROO_CDC_PUT_MAIN',ROOT/'Firm_MCU/Saturn/saturn_main.c')).read_bytes()
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-put';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(data,signature):
            start=data.index(signature)
            return data[start:data.index(b'\n}',start)+2]+b'\n'
        (cls.target/'copy.inc').write_bytes(copy_source(raw,required=(b'sector_destination(',b'disconnect_filter_input(',b'free_block_list(')))
        code=b''.join(function(raw,sig) for sig in (b'void free_partition(',b'int reset_filter(',b'int set_cdev_connect(',b'int set_filter_connection(',b'void trans_handle(',b'int put_sector_data(',b'int end_trans('))
        if b'static int sector_transfer_size(' in raw:code=function(raw,b'static int sector_transfer_size(')+code
        init=function(main,b'void ss_sw_init(')
        for old,new in {b'(u8*)(0x2400a000+i*2352)':b'sector_bytes[i]',b'(u8*)0x2400a000+MAX_BLOCKS*2352':b'startup_info'}.items():
            assert init.count(old)==1,old
            init=init.replace(old,new)
        (cls.target/'put.inc').write_bytes(code+init)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':raw,'saturn_main.c':main,'cdc.h':header}
        for name in ('cdc_copy.c','test_cdc_copy.py','cdc_put.c','test_cdc_put.py'):inputs[name]=(ROOT/'tests'/name).read_bytes()
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'put'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror','-Wno-unused-variable',*flags,'-I',str(cls.target),str(cls.target/'cdc_put.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=45,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_admission_bounds_capacity_and_busy_preserve_state(self):self.run_case(0)
    def test_all_200_counts_reserve_before_ready_no_later_allocation(self):self.run_case(1)
    def test_8196_early_fifo_prefixes_preserve_undefined_payload(self):self.run_case(2)
    def test_fifo_empty_threshold_full_span_and_excess_bounds(self):self.run_case(3)
    def test_end_routes_metadata_splits_and_discards_silently(self):self.run_case(4)
    def test_capacity_loss_rollback_after_each_allocation(self):self.run_case(5)
    def test_partition_condition_and_software_reset_ownership(self):self.run_case(6)
    def test_input_theft_stop_retention_termination_and_recovery(self):self.run_case(7)
    def test_actual_defaults_followed_by_real_write(self):self.run_case(8)
    def test_noncontiguous_chain_and_last_pool_index(self):self.run_case(9)
    def test_acknowledgment_before_drain_and_idle_noop(self):self.run_case(10)
    def test_unrelated_inputs_true_output_and_captured_length(self):self.run_case(11)
    def test_baseline_valid_full_payload_control(self):self.run_case(12)


if __name__=='__main__':unittest.main()
