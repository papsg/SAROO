"""Actual CD command bounds and valid controls; no devices or private preparer."""
# Adapted from paps-g 89e644f6; independent synthetic state and word/list oracles.
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


class CDBoundsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw=Path(os.environ.get('SAROO_CDC_BOUNDS_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c')).read_bytes()
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-bounds';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(signature):
            start=raw.index(signature)
            return raw[start:raw.index(b'\n}',start)+2]+b'\n'
        (cls.target/'copy.inc').write_bytes(copy_source(raw,required=(b'sector_destination(',b'detach_sector_range(',b'free_block_list(',b'disconnect_filter_input(')))
        code=function(b'static int sector_transfer_size(') if b'static int sector_transfer_size(' in raw else b''
        code+=b''.join(function(sig) for sig in (b'void fill_fifo(',b'void trans_start(',b'void trans_handle(',b'int end_trans(',
            b'int set_filter_range(',b'int get_filter_range(',b'int set_filter_subheader(',b'int get_filter_mode(',
            b'int get_filter_connection(',b'int get_sector_num(',b'int cal_actual_size(',b'int get_actual_size(',
            b'int get_sector_info(',b'static __attribute__((noinline)) int get_sector_data_common(',
            b'int get_sector_data(',b'int get_del_sector_data(',b'int del_sector_data(',b'static void _set_filter(',
            b'void free_partition('))
        if b'static u32 directory_record;' in raw:
            code+=b'static u32 directory_record;\n'
            code+=b''.join(function(sig) for sig in (b'static u32 file_sector_count(',
                b'static int valid_file_extent(',b'static void set_file_selector(',
                b'static void start_file_play(',b'static int file_info_index('))
        code+=b''.join(function(sig) for sig in (b'static __attribute__((noinline)) void start_directory_read(',
            b'int change_dir(',b'int read_dir(',b'int read_file('))
        (cls.target/'bounds.inc').write_bytes(code)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':raw,'cdc.h':header}
        for name in ('cdc_copy.c','test_cdc_copy.py','cdc_bounds.c','test_cdc_bounds.py'):inputs[name]=(ROOT/'tests'/name).read_bytes()
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'bounds'
        # The preceding source has an unparenthesized AND/OR expression. Keep
        # its actual precedence; suppress its style warning for baseline use.
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',
              '-Wno-unused-variable','-Wno-parentheses',*flags,'-I',str(cls.target),str(cls.target/'cdc_bounds.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=30,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_3480_invalid_selectors_preserve_state_and_do_not_wait(self):self.run_case(0)
    def test_all_valid_filter_register_and_disconnected_output_controls(self):self.run_case(1)
    def test_all_65536_sector_position_words(self):self.run_case(2)
    def test_last_empty_and_short_list_queries(self):self.run_case(3)
    def test_busy_get_precedence_all_transfer_types_and_invalid_selectors(self):self.run_case(4)
    def test_408_valid_read_delete_tail_controls(self):self.run_case(5)
    def test_all_valid_directory_and_file_command_selectors(self):self.run_case(6)
    def test_valid_size_calculation_controls(self):self.run_case(7)
    def test_405_size_ranges_sentinels_and_retained_refusals(self):self.run_case(8)
    def test_800_calculations_match_actual_fifo_payload_and_full_counts(self):self.run_case(9)
    def test_incomplete_variable_list_error_and_reset_lifetime(self):self.run_case(10)


if __name__=='__main__':unittest.main()
