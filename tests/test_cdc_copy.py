"""Actual CD allocator/copy/query/reset with independently seeded pool lists."""
# Adapted from paps-g 93391efb/8432f7d8; physical registers/IRQs are modeled.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from cdc_reply_source import command_reply_source

ROOT=Path(__file__).resolve().parents[1]


def filter_reset_source(raw):
    signature = b'static __attribute__((noinline,noclone)) void reset_filter_conditions('
    if signature not in raw:
        return b''
    start = raw.index(signature)
    return (raw[start:raw.index(b'\n}', start) + 2] + b'\n').replace(b'noinline,noclone', b'noinline')


def copy_source(raw,required=()):
    def function(signature):
        start=raw.index(signature)
        return raw[start:raw.index(b'\n}',start)+2]+b'\n'
    # Rename only the real allocator's symbol, then count calls through a shim.
    # Its body/IRQ/allocation decisions remain the production implementation.
    code=b'#define alloc_block actual_alloc_block\n'+function(b'BLOCK *alloc_block(void)')
    if b'static int sector_range(' in raw:code=function(b'static int sector_range(')+code
    code+=b'#undef alloc_block\nstatic unsigned allocation_count;\n'
    code+=b'static BLOCK *alloc_block(void) { allocation_count++; return actual_alloc_block(); }\n'
    code+=b''.join(function(sig) for sig in (b'void free_block(BLOCK *block)',b'BLOCK *dup_block(BLOCK *block)',b'BLOCK *find_block(',b'void append_block('))
    if b'static void unlink_block(' in raw:code+=function(b'static void unlink_block(')
    if b'static void disconnect_partition_output(' in raw:code+=function(b'static void disconnect_partition_output(')
    for signature,call in ((b'static BLOCK *detach_sector_range(',b'detach_sector_range('),
                           (b'static void free_block_list(',b'free_block_list(')):
        if signature in raw and call in required:code+=function(signature)
    code+=function(b'void remove_block(')+function(b'void set_status(')
    code+=re.search(rb'static (?:int|u8) copy_move_error;',raw).group()+b'\n'
    start=raw.index(b'// 0x65 [SR]')
    operations=raw[start:raw.index(b'/******************************************************************************/',start)]
    for signature,call in ((b'static int sector_destination(',b'sector_destination('),(b'static void disconnect_filter_input(',b'disconnect_filter_input(')):
        if signature in raw and (call in operations or call in required):code+=function(signature)
    code += command_reply_source(raw)
    code+=operations
    if b'void invalidate_directory(' in raw:code+=function(b'void invalidate_directory(')
    if b'FILTER *command_filter(' in raw:
        code += function(b'static __attribute__((noinline)) FILTER *command_filter(')
    code += filter_reset_source(raw)
    code+=function(b'int get_filter_subheader(void)')+function(b'int init_cdblock(void)')
    return code


class CDCopyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=Path(os.environ.get('SAROO_CDC_COPY_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        raw=source.read_bytes();header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-copy';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        code=copy_source(raw)
        (cls.target/'copy.inc').write_bytes(code)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':raw,'cdc.h':header,'cdc_copy.c':(ROOT/'tests/cdc_copy.c').read_bytes(),'test_cdc_copy.py':Path(__file__).read_bytes()}
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,blob in inputs.items():(cls.target/name).write_bytes(blob)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'copy'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror','-Wno-unused-variable',*flags,'-I',str(cls.target),str(cls.target/'cdc_copy.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60);(cls.target/'compile.log').write_bytes(result.stdout+result.stderr);(cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_cases(self,cases):
        for case in cases:
            with self.subTest(case=case):
                result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=20,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
                self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr));self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_power_on_and_success(self):self.run_cases((0,1,4))
    def test_copy_admission_and_real_exhaustion(self):self.run_cases((2,3))
    def test_full_buffer_move_needs_no_allocation(self):self.run_cases((5,))
    def test_next_successful_copy_or_move_replaces_error(self):self.run_cases((6,7))
    def test_software_reset_clears_but_nonreset_and_ignore_preserve(self):self.run_cases((8,9,10))
    def test_all_invalid_selectors(self):self.run_cases((20,21))
    def test_invalid_ranges_and_active_transfers(self):self.run_cases((22,23))
    def test_full_same_partition_head_middle_tail_moves(self):self.run_cases((24,))
    def test_copy_sentinels_and_same_partition(self):self.run_cases((25,))
    def test_remove_one_block_or_preserve_invalid_index(self):self.run_cases((26,))
    def test_baseline_valid_payload_and_reset_controls(self):self.run_cases((27,))


if __name__=='__main__':unittest.main()
