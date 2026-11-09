"""Actual menu/MCU handlers exchange requests on synthetic public FatFs media."""
# Adapted from paps-g 4df2e67a. Address/register handshakes are explicit models.
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
from test_file_transfer import extract_command_source

ROOT=Path(__file__).resolve().parents[1]


class MenuFileTransferTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=Path(os.environ.get('SAROO_MENU_FILE_SOURCE',ROOT/'Firm_Saturn/main.c'))
        raw=source.read_bytes();header=(ROOT/'Firm_Saturn/main.h').read_bytes();mcu_header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        mappings=dict(re.findall(rb'#define\s+(TMPBUFF_ADDR|IMGINFO_ADDR)\s+(0x[0-9a-fA-F]+)',header))
        assert mappings=={b'TMPBUFF_ADDR':b'0x220c7000',b'IMGINFO_ADDR':b'0x22080000'}
        mappings=dict(re.findall(rb'#define\s+(TMPBUFF_ADDR|FIRM_ADDR)\s+(0x[0-9a-fA-F]+)',mcu_header))
        assert mappings=={b'TMPBUFF_ADDR':b'0x610c7000',b'FIRM_ADDR':b'0x61000000'}
        build=ROOT/'tests/build/menu-file-transfer';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        start=raw.index(b'static u32 file_capacity(') if b'static u32 file_capacity(' in raw else raw.index(b'int read_file (')
        end=raw.index(b'/**********************************************************/',start);words=raw.index(b'u32 BE32(')
        code=raw[words:raw.index(b'/**********************************************************/',words)]+raw[start:end]
        for mask in (b'0x1fffffff',b'0x0fffffff'):
            code=code.replace(b'(u32)buf & '+mask,b'host_bus_address(buf)')
        code=code.replace(b'file_capacity((u32)buf, 0)',b'file_capacity(host_bus_address(buf), 0)')
        code=code.replace(b'((TMPBUFF_ADDR & 0x1fffffff) + 0x100)',b'0x020c7100')
        code=code.replace(b'LE32W((void*)(TMPBUFF_ADDR+0x08), TMPBUFF_ADDR+0x0100)',b'LE32W((void*)(TMPBUFF_ADDR+0x08), 0x020c7100)')
        if b'int read_file_bounded(' not in raw:
            code+=b'\nstatic int read_file_bounded(char *n,int o,int s,void *b,u32 c) { (void)c; return read_file(n,o,s,b); }\n'
        (cls.target/'menu-file.inc').write_bytes(code)
        (cls.target/'mode.h').write_text('#define HAS_BOUNDED_READER %d\n'%(b'int read_file_bounded(' in raw))
        (cls.target/'protocol.inc').write_bytes(b'\n'.join(line for line in mcu_header.splitlines() if line.startswith((b'#define SSCMD_FILERD ',b'#define SSCMD_FILEWR ')))+b'\n')
        slots=[line.strip() for line in raw.splitlines() if any(('0x02000f%02x'%x).encode() in line for x in (8,12,16,20)) and b'=' in line and line.lstrip().startswith(b'*(u32*)')]
        for i,slot in enumerate(slots):
            for address in (8,12,16,20):slot=slot.replace(('*(u32*)(0x02000f%02x)'%address).encode(),('abi_slots[%d]'%((address-8)//4)).encode())
            slots[i]=slot.replace(b'(u32)read_file',b'(uintptr_t)read_file').replace(b'(u32)write_file',b'(uintptr_t)write_file')
        (cls.target/'abi.inc').write_bytes(b'\n'.join(slots)+b'\n')
        peer=Path(os.environ.get('SAROO_MENU_MCU_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_main.c')).read_bytes()
        (cls.target/'mcu-file.inc').write_bytes(extract_command_source(peer))
        inputs={'Firm_Saturn/main.c':raw,'Firm_Saturn/main.h':header,'Firm_MCU/Saturn/saturn_main.c':peer,'Firm_MCU/Saturn/cdc.h':mcu_header}
        fatfs=cls.target/'FatFS';fatfs.mkdir()
        for name in ('ff.c','ff.h','ffconf.h','ffunicode.c','diskio.h'):
            blob=(ROOT/'Firm_MCU/FatFS'/name).read_bytes();(fatfs/name).write_bytes(blob);inputs['FatFS/'+name]=blob
        for name in ('menu_file_transfer.c','test_menu_file_transfer.py','test_file_transfer.py','mcu_save_support.py'):
            blob=(ROOT/'tests'/name).read_bytes();(cls.target/name).write_bytes(blob);inputs['tests/'+name]=blob
        for name,blob in inputs.items():
            path=cls.target/'inputs'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_FILE_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'menu-file-transfer'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,'-I',str(cls.target),'-I',str(fatfs),str(cls.target/'menu_file_transfer.c'),str(fatfs/'ff.c'),str(fatfs/'ffunicode.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60);(cls.target/'compile.log').write_bytes(result.stdout+result.stderr);(cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_cases(self,cases):
        for case in cases:
            with self.subTest(case=case),tempfile.TemporaryDirectory(dir=self.target) as tmp:
                path=Path(tmp)/'card.img';make_fat32(path)
                result=subprocess.run([str(self.binary),str(path),str(case)],capture_output=True,timeout=20,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
                self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout.decode(errors='replace'),stderr=result.stderr.decode(errors='replace')));self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn(b'pass',result.stdout)

    def test_whole_file_eof_capacity_and_tail(self):self.run_cases(range(0,6))
    def test_fixed_capacity_and_invalid_arguments(self):self.run_cases(range(6,12))
    def test_bad_response_counts_and_failed_eof_probe(self):self.run_cases((12,13,28,29))
    def test_bounded_names_before_request_publication(self):self.run_cases(range(14,16))
    def test_writes_and_zero_length_create(self):self.run_cases(range(16,19))
    def test_bounce_window_rejects_before_copy(self):self.run_cases(range(19,21))
    def test_direct_buffers_request_overlap_and_window_edges(self):self.run_cases(range(21,24))
    def test_legacy_four_argument_whole_file_calls(self):self.run_cases(range(24,26))
    def test_one_mib_binary_and_two_mib_companion_loads(self):self.run_cases(range(26,28))


if __name__=='__main__':unittest.main()
