"""Actual shared-memory commands on public FatFs with sparse synthetic media."""
# Adapted from paps-g 52ba5498. No private preparer or runtime diagnostics.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest
from mcu_save_support import make_fat32

ROOT = Path(__file__).resolve().parents[1]


class FileTransferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_FILE_TRANSFER_SOURCE', ROOT/'Firm_MCU/Saturn/saturn_main.c'))
        raw = source.read_bytes()
        build=ROOT/'tests/build/file-transfer'; build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build)); cls.results=[]
        helper=b''
        if b'int ss_file_transfer(' in raw:
            start=raw.rfind(b'\n',0,raw.index(b'int ss_file_transfer('))+1
            helper=raw[start:raw.index(b'\n}',start)+2]+b'\n'
        start=raw.index(b'\tcase SSCMD_FILERD:')
        cases=raw[start:raw.index(b'\tcase SSCMD_SSAVE:',start)]
        code=helper+b'\nstatic void command(void) { u32 cmd=SS_CMD; '+(b'int retv; ' if helper else b'')+b'switch(cmd) {\n'+cases+b'\n} }\n'
        code,replacements=re.subn(rb'\(u8\s*\*\)\s*\(\(baddr\s*&\s*0x00ffffff\)\s*\|\s*0x61000000\)',b'(test_shared+(baddr & 0x00ffffff))',code)
        assert replacements==(1 if helper else 2)
        (cls.target/'file-transfer.inc').write_bytes(code)
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        addresses=dict(re.findall(rb'#define\s+(TMPBUFF_ADDR|FIRM_ADDR)\s+(0x[0-9a-fA-F]+)',header))
        assert addresses=={b'TMPBUFF_ADDR':b'0x610c7000',b'FIRM_ADDR':b'0x61000000'}
        (cls.target/'log.inc').write_bytes(b'\n'.join(line for line in header.splitlines() if line.startswith((b'#define _INFO ',b'#define SSLOG(',b'#define SSCMD_FILERD ',b'#define SSCMD_FILEWR ')))+b'\n')
        inputs={'Saturn/saturn_main.c':raw,'Saturn/cdc.h':header}
        fatfs=cls.target/'FatFS';fatfs.mkdir()
        for name in ('ff.c','ff.h','ffconf.h','ffunicode.c','diskio.h'):
            blob=(ROOT/'Firm_MCU/FatFS'/name).read_bytes();(fatfs/name).write_bytes(blob);inputs['FatFS/'+name]=blob
        for name in ('file_transfer.c','test_file_transfer.py','mcu_save_support.py'):
            blob=(ROOT/'tests'/name).read_bytes();(cls.target/name).write_bytes(blob);inputs['tests/'+name]=blob
        for name,blob in inputs.items():
            target=cls.target/'inputs'/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(blob)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_FILE_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'file-transfer'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,'-I',str(cls.target),'-I',str(fatfs),str(cls.target/'file_transfer.c'),str(fatfs/'ff.c'),str(fatfs/'ffunicode.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def invoke(self, mode, fault='none', logging=True):
        with tempfile.TemporaryDirectory(dir=self.target) as tmp:
            card=Path(tmp)/'card.img';make_fat32(card)
            result=subprocess.run([str(self.binary),str(card),mode,fault,str(int(logging))],capture_output=True,timeout=20,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
            row=dict(mode=mode,fault=fault,logging=logging,returncode=result.returncode,stdout=result.stdout.decode(errors='replace'),stderr=result.stderr.decode(errors='replace'));self.results.append(row)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn(b'pass',result.stdout)

    def test_bounds_reject_before_open_and_preserve_targets(self):
        for mode in ('read-cross-window','write-cross-window','read-wrap-size','write-wrap-size',
                     'read-request','write-request','read-request-last','write-zero-request',
                     'read-cross-request','write-cross-request','read-name-no-nul','write-name-no-nul'):
            with self.subTest(mode=mode):self.invoke('bounds-'+mode)

    def test_exact_request_window_and_name_boundaries_remain_valid(self):
        for mode in ('read-before-request','write-before-request','read-bounce','write-bounce',
                     'read-end-exact','write-end-exact','read-name-limit','write-name-limit'):
            with self.subTest(mode=mode):self.invoke('bounds-'+mode)

    def test_whole_file_extents_remain_wide_until_checked(self):
        for mode in ('whole-cross-window','whole-cross-request','whole-wide-wrap','whole-wide-zero',
                     'whole-end-exact','whole-tail','whole-eof','whole-beyond'):
            with self.subTest(mode=mode):self.invoke('bounds-'+mode)

    def test_reads_preserve_whole_file_eof_and_partial_count_semantics(self):
        for mode in ('read','read-whole','read-eof','read-beyond','read-empty'):
            with self.subTest(mode=mode):self.invoke(mode)
        self.invoke('read','short')

    def test_writes_preserve_overwrite_create_extend_and_zero_length_semantics(self):
        for mode in ('write','write-create','write-negative','write-extend','write-zero'):
            with self.subTest(mode=mode):self.invoke(mode)
        self.invoke('write','short')

    def test_open_failure_clears_count_without_seek_transfer_or_close(self):
        for mode in ('read','write'):
            with self.subTest(mode=mode):self.invoke(mode,'open')

    def test_seek_errors_stop_data_transfer_and_close_once(self):
        for mode in ('read','write'):
            for fault in ('seek','seek-close','disk-seek'):
                with self.subTest(mode=mode,fault=fault):self.invoke(mode,fault)
        self.invoke('write','short-seek')

    def test_real_allocation_exhaustion_cannot_write_at_clipped_offset(self):
        self.invoke('write-extend','full-seek')

    def test_transfer_errors_keep_partial_counts_and_first_error(self):
        for mode in ('read','write'):
            for fault in ('transfer','partial-error','transfer-close'):
                with self.subTest(mode=mode,fault=fault):self.invoke(mode,fault)

    def test_close_failure_reaches_the_saturn_command_result(self):
        for mode in ('read','write'):
            with self.subTest(mode=mode):self.invoke(mode,'close')

    def test_real_disk_write_failure_during_close_is_not_success(self):
        self.invoke('write','disk-close')

    def test_existing_log_mask_and_text_remain(self):
        self.invoke('read',logging=False);self.invoke('write',logging=False)


if __name__=='__main__':unittest.main()
