"""Actual incoming router/allocator with independent ownership/payload oracles."""
# Adapted from paps-g 661fd2d4; physical registers/IRQs are modeled.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
OVERLAY = ROOT / 'Firm_MCU/Saturn/sector_overlay.c'


def overlay_source():
    return b'#include "sector_overlay.h"\n'


def filter_source(raw):
    code = b''
    for signature in (b'static u8 sector_bcd(', b'static void mode2_header(', b'int filter_sector('):
        if signature in raw:
            start = raw.index(signature)
            code += raw[start:raw.index(b'\n}', start) + 2] + b'\n'
    return code


class CDFilterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=Path(os.environ.get('SAROO_CDC_FILTER_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        raw=source.read_bytes();header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-filter';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(signature):
            start=raw.index(signature)
            return raw[start:raw.index(b'\n}',start)+2]+b'\n'
        code=function(b'BLOCK *alloc_block(void)')+function(b'void append_block(')
        if b'static int sector_destination(' in raw:code+=function(b'static int sector_destination(')
        if b'sector_overlay_apply(' in raw:
            code += b'#define TEST_FILTER_OVERLAY 1\n' + function(b'void free_block(')
        code+=overlay_source() + filter_source(raw)
        (cls.target/'filter.inc').write_bytes(code)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':raw,'cdc.h':header,'cdc_filter.c':(ROOT/'tests/cdc_filter.c').read_bytes(),'test_cdc_filter.py':Path(__file__).read_bytes()}
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,blob in inputs.items():(cls.target/name).write_bytes(blob)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'filter'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,'-I',str(cls.target),str(cls.target/'cdc_filter.c'),'-I',str(OVERLAY.parent),str(OVERLAY),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60);(cls.target/'compile.log').write_bytes(result.stdout+result.stderr);(cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_cases(self,cases):
        for case in cases:
            with self.subTest(case=case):
                result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=5,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
                self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr));self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_disconnected_true_outputs(self):self.run_cases((0,))
    def test_invalid_device_and_false_selectors(self):self.run_cases((1,))
    def test_invalid_true_outputs(self):self.run_cases((2,))
    def test_cycles_terminate_without_allocation(self):self.run_cases((3,))
    def test_all_24_filter_chains_to_each_partition(self):self.run_cases((4,))
    def test_discard_replaces_prior_destination(self):self.run_cases((5,))
    def test_full_buffer_retry_retains_completed_destination(self):self.run_cases((6,))
    def test_format_payload_order_and_byte_counts(self):self.run_cases((7,))
    def test_existing_mode2_conditions_and_range_edges(self):self.run_cases((8,))
    def test_validated_destination_survives_modeled_connection_change(self):self.run_cases((9,))
    def test_existing_false_disconnect_logging(self):self.run_cases((10,))
    def test_baseline_valid_delivery_controls(self):self.run_cases((11,))
    def test_disconnected_input_needs_no_sector_context(self):self.run_cases((12,))
    def test_list_and_last_buffer_publish_together(self):self.run_cases((13,))


if __name__=='__main__':unittest.main()
