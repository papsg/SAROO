"""Actual connection/reset commands and relocated software initializer."""
# Adapted from paps-g 50100444/3f7bba1a; no hardware or private preparer.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class CDConnectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=Path(os.environ.get('SAROO_CDC_CONNECTION_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c')).read_bytes()
        main=Path(os.environ.get('SAROO_CDC_CONNECTION_MAIN',ROOT/'Firm_MCU/Saturn/saturn_main.c')).read_bytes()
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-connections';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(data,signature):
            start=data.index(signature)
            return data[start:data.index(b'\n}',start)+2]+b'\n'
        code=b''.join(function(raw,sig) for sig in (b'BLOCK *alloc_block(void)',b'void free_block('))
        for sig in (b'static void disconnect_partition_output(',b'static void free_block_list('):
            if sig in raw:code+=function(raw,sig)
        code+=function(raw,b'void free_partition(')+function(raw,b'void set_status(')
        code+=re.search(rb'static (?:int|u8) copy_move_error;',raw).group()+b'\n'
        if b'static void disconnect_filter_input(' in raw:code+=function(raw,b'static void disconnect_filter_input(')
        if b'void invalidate_directory(' in raw:code+=function(raw,b'void invalidate_directory(')
        if b'FILTER *command_filter(' in raw:
            code += function(raw, b'static __attribute__((noinline)) FILTER *command_filter(')
        code+=function(main,b'int pause_drive(')
        code+=b''.join(function(raw,sig) for sig in (b'int set_cdev_connect(',b'int set_filter_connection(',b'int set_filter_mode(',b'int reset_filter(',b'int init_cdblock(',b'int get_copy_error('))
        init=function(main,b'void ss_sw_init(')
        # Only physical buffer addresses are relocated; all initialization stays.
        replacements={b'(u8*)(0x2400a000+i*2352)':b'(u8*)payload[i]',b'(u8*)0x2400a000+MAX_BLOCKS*2352':b'info_bytes'}
        for old,new in replacements.items():
            assert init.count(old)==1,old
            init=init.replace(old,new)
        (cls.target/'connections.inc').write_bytes(code+init)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':raw,'saturn_main.c':main,'cdc.h':header,'cdc_connections.c':(ROOT/'tests/cdc_connections.c').read_bytes(),'test_cdc_connections.py':Path(__file__).read_bytes()}
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'connections'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror','-Wno-unused-variable',*flags,'-I',str(cls.target),str(cls.target/'cdc_connections.c'),'-o',str(cls.binary)]
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

    def test_576_input_owner_pairs_preserve_shared_true_outputs(self):self.run_case(0)
    def test_validation_atomicity_disconnection_and_ignored_fields(self):self.run_case(1)
    def test_mode_initialization_and_selected_reset_bounds(self):self.run_case(2)
    def test_all_nonzero_reset_flags_ignore_partition_argument(self):self.run_case(3)
    def test_pause_only_for_relevant_incoming_data_erasure(self):self.run_case(4)
    def test_actual_software_startup_defaults(self):self.run_case(5)
    def test_software_reset_defaults_and_nonreset_nochange_controls(self):self.run_case(6)
    def test_partition_reset_preserves_unpublished_allocation(self):self.run_case(7)
    def test_baseline_valid_configuration_control(self):self.run_case(8)


if __name__=='__main__':unittest.main()
