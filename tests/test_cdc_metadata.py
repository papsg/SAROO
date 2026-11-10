"""Actual disc/host producers; synthetic cache and occupancy-windowed input, no device I/O."""
# Public adaptation of paps-g 50100444; producer controls maintained through007.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from test_cdc_sector_input import reader_source

ROOT=Path(__file__).resolve().parents[1]


class CDMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cdc=Path(os.environ.get('SAROO_CDC_METADATA_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c')).read_bytes()
        main=Path(os.environ.get('SAROO_CDC_METADATA_MAIN',ROOT/'Firm_MCU/Saturn/saturn_main.c')).read_bytes()
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-metadata';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(raw,signature):
            start=raw.index(signature)
            return raw[start:raw.index(b'\n}',start)+2]+b'\n'
        code=b''.join(function(cdc,sig) for sig in (b'BLOCK *alloc_block(void)',b'void free_block(',b'void append_block('))
        if b'static void unlink_block(' in cdc:code+=function(cdc,b'static void unlink_block(')
        for signature in (b'static void disconnect_partition_output(',b'static void free_block_list('):
            if signature in cdc:code+=function(cdc,signature)
        code+=function(cdc,b'void remove_block(')+function(cdc,b'void set_status(')
        operations=b''.join(function(cdc,sig) for sig in (b'void trans_handle(void)',b'int put_sector_data(',b'int end_trans('))
        if b'static int sector_transfer_size(' in cdc:code+=function(cdc,b'static int sector_transfer_size(')
        for signature,call in ((b'static int sector_destination(',b'sector_destination('),(b'static void disconnect_filter_input(',b'disconnect_filter_input(')):
            if call in operations:code+=function(cdc,signature)
        code+=operations
        # Existing FAD/cache comparisons mix signed/unsigned; no new arithmetic.
        code+=b'#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored "-Wsign-compare"\n'
        code+=reader_source(main)+b'#pragma GCC diagnostic pop\n'
        (cls.target/'metadata.inc').write_bytes(code)
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')])
        inputs={'saturn_cdc.c':cdc,'saturn_main.c':main,'cdc.h':header,'cdc_metadata.c':(ROOT/'tests/cdc_metadata.c').read_bytes(),'test_cdc_metadata.py':Path(__file__).read_bytes()}
        inputs['test_cdc_sector_input.py']=(ROOT/'tests/test_cdc_sector_input.py').read_bytes()
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'metadata'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror','-Wno-unused-variable',*flags,'-I',str(cls.target),str(cls.target/'cdc_metadata.c'),'-o',str(cls.binary)]
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

    def test_disc_formats_zero_stale_fields_preserve_payload_and_logical_fad(self):self.run_case(0)
    def test_baseline_valid_mode2_disc_control(self):self.run_case(1)
    def test_host_reuse_all_sizes_and_append_requests(self):self.run_case(2)
    def test_baseline_valid_host_transmitted_payload_control(self):self.run_case(3)
    def test_raw_header_modes_and_bcd_without_validity_rejection(self):self.run_case(4)


if __name__=='__main__':unittest.main()
