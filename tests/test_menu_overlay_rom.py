"""Actual generic service, complete synthetic ROM/copy and explicit I/O models."""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest
import zlib
from check_later_callbacks_sh2 import execute
from check_menu_clock_sh2 import linked_symbol_records

from test_menu_cdplayer import ROOT, function


class OverlayROMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        patch=(ROOT/'Firm_Saturn/game_patch.c').read_bytes()
        header=(ROOT/'Firm_Saturn/main.h').read_bytes()
        start=patch.index(b'#define ROM_IMAGE_ADDR')
        definitions=patch[start:patch.index(b'int prepare_game_rom(',start)]
        start=patch.index(b'/* Adapted from paps-g 3fafb587.')
        service=patch[start:patch.index(b'#ifdef __sh__',start)]
        copy=function(patch,'jump_copy_code').replace(b'(void*)type',b'host_target(type)')
        service=copy+service
        service=service.replace(b'(const u8 *)OVERLAY_ROM_DESCRIPTOR',b'(const u8 *)memory(OVERLAY_ROM_DESCRIPTOR)')
        service=service.replace(b'(void *)ROM_IMAGE_ADDR',b'memory(ROM_IMAGE_ADDR)')
        service=service.replace(b'(u8 *)ROM_IMAGE_ADDR',b'(u8 *)memory(ROM_IMAGE_ADDR)')
        service=service.replace(b'((void (*)(int))0x00200000)((int)entry)',b'host_copy((int)entry)')
        crc=(ROOT/'Firm_Saturn/crc32.c').read_bytes().replace(b'#include "main.h"',b'')
        constants=b'\n'.join(re.search(rb'^#define '+name+rb'\b[^\r\n]+',header,re.M)[0]
                              for name in (b'TMPBUFF_ADDR',b'SYSINFO_ADDR',b'SD_OVERLAY_ROM'))
        payload=bytes(((i*29)^(i>>8)^0x5a)&255 for i in range(0x200000))
        constants+=f'\n#define EXPECTED_PAYLOAD_CRC 0x{zlib.crc32(payload):08x}u\n'.encode()+definitions
        build=ROOT/'tests/build/menu-overlay-rom';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build))
        inputs={'game_patch.c':patch,'main.h':header,'crc32.inc':crc,
                'service.inc':service,'constants.inc':constants,
                'menu_overlay_rom.c':(ROOT/'tests/menu_overlay_rom.c').read_bytes()}
        for name,data in inputs.items(): (cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in inputs.items()},indent=2)+'\n')
        cls.binary=cls.target/'rom'
        flags=['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        run=subprocess.run([*shlex.split(os.environ.get('CC','cc')),'-std=c11','-g','-Wall','-Wextra','-Werror',
                            '-I',cls.target,*flags,cls.target/'menu_overlay_rom.c','-o',cls.binary],capture_output=True,text=True,timeout=60)
        (cls.target/'compile.log').write_text(run.stdout+run.stderr)
        if run.returncode: raise RuntimeError(run.stderr)

    def case(self,n):
        run=subprocess.run([self.binary,str(n)],capture_output=True,text=True,timeout=30,
                           env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        (self.target/f'{n}.log').write_text(run.stdout+run.stderr)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_complete_rom_both_phases_and_entire_overlapping_copy(self): self.case(0)
    def test_failed_short_oversized_and_wrong_crc_never_arm(self): self.case(1)
    def test_corrupt_fields_reserved_bytes_header_bounds_refuse_before_io(self): self.case(2)
    def test_noncanonical_paths_and_padding_refuse_before_io(self): self.case(3)
    def test_action_skip_and_physical_scope_do_not_consume_other_profiles(self): self.case(4)
    def test_next_launch_clears_stale_legacy_and_generic_ownership(self): self.case(5)

    @unittest.skipUnless(os.environ.get('SAROO_OVERLAY_MENU_ELF'), 'set SAROO_OVERLAY_MENU_ELF for linked SH-2 checks')
    def test_linked_launch_installs_fragments_then_executes_complete_native_forward_copy(self):
        elf=Path(os.environ['SAROO_OVERLAY_MENU_ELF'])
        names=('_overlay_rom_launch','_card_init','_overlay_rom_after_read','_memcpy',
               '_overlay_rom_phase','_overlay_rom_entry','_skip_patch','_need_bup',
               '_jump_06b0','_rom_reset_issue','_jump_copy_code','__ip_end')
        symbols=dict(zip(names,linked_symbol_records(elf,names)))
        base=0x02000000;body_size=symbols['__ip_end'][0]-base
        body=elf.with_name('ssfirm.bin').read_bytes()[:body_size]
        for name in names[:4]+names[8:11]:
            address,data=symbols[name]
            self.assertEqual(body[address-base:address-base+len(data)],data)
        header=(ROOT/'Firm_Saturn/main.h').read_bytes()
        staging=int(re.search(rb'^#define TMPBUFF_ADDR\s+(0x[0-9a-f]+)',header,re.M)[1],16)+0x100
        copy_at,copy=symbols['_jump_copy_code']
        op=int.from_bytes(copy[:2],'big');self.assertEqual(op>>8,0xd2)
        literal=((copy_at+4)&~3)+(op&255)*4
        self.assertEqual(int.from_bytes(body[literal-base:literal-base+4],'big'),staging)
        fragments=[(0x00200000,body[copy_at-base:copy_at-base+96]),
                   (0x06000c80,symbols['_jump_06b0'][1]),(0x060006b8,symbols['_rom_reset_issue'][1])]
        self.assertEqual([len(data) for _,data in fragments],[96,96,36])
        payload=bytes(((i*29)^(i>>8)^0x5a)&255 for i in range(0x200000))

        def return_pc(name):
            address,data=symbols[name]
            calls=[address+i+4 for i in range(0,len(data),2)
                   if int.from_bytes(data[i:i+2],'big')&0xf0ff==0x400b]
            self.assertEqual(len(calls),1)
            return calls[0]

        for phase,resume in [(1,0x06002048),(2,0x06002222)]:
            with self.subTest(phase=phase):
                memory=NativeMemory()
                memory.store(base,body);memory.store(staging,payload)
                for name,value,width in [('_overlay_rom_phase',phase,1),('_overlay_rom_entry',resume,4),
                                         ('_skip_patch',0,4),('_need_bup',phase-1,4)]:
                    memory.store(symbols[name][0],value.to_bytes(width,'big'))
                expected=memory.copy();copied=[]
                sp=0x060ff000;caller=0x06012340
                seed=phase*73
                initial=[0x13570000+i*0x100+seed for i in range(16)];initial[15]=sp
                frame=4 if phase==2 else 0
                if frame: expected.store(sp-4,caller.to_bytes(4,'big'))
                expected.store(sp-frame-4,initial[8].to_bytes(4,'big'))
                expected.store(sp-frame-8,(return_pc('_overlay_rom_after_read') if frame else caller).to_bytes(4,'big'))
                expected.store(sp-frame-12,resume.to_bytes(4,'big'))
                expected.store(sp-frame-16,initial[9].to_bytes(4,'big'))
                expected.store(sp-frame-20,return_pc('_overlay_rom_launch').to_bytes(4,'big'))
                for address,data in fragments: expected.store(address,data)
                for address,value in [(0x06000358,0x7d600),(0x0600026c,0x186c),(0x06000320,0x06000c80)]:
                    expected.store(address,value.to_bytes(4,'big'))
                if phase==2: expected.store(symbols['_need_bup'][0],bytes(4))
                expected.store(base,payload)

                def copy_call(regs,ram):
                    dest,source,length=regs[4:7]
                    self.assertEqual((dest,length),(fragments[len(copied)][0],len(fragments[len(copied)][1])))
                    ram.store(dest,ram.load(source,length));copied.append((dest,source,length))
                    for i in range(8): regs[i]=0xdead0000+i
                    regs[0]=dest
                    return phase&1

                def publication(address,width,value,ram):
                    if address==0x06000320:
                        self.assertEqual((width,value,len(copied)),(4,0x06000c80,3))
                        for at,data in fragments: self.assertEqual(ram.load(at,len(data)),data)

                begin=symbols['_overlay_rom_after_read' if phase==2 else '_overlay_rom_launch'][0]
                _,regs,pr,after,writes,calls=execute(body,base,resume,memory,entry_pc=begin,
                    mapped_code=True,external=symbols['_memcpy'][0],external_handler=copy_call,
                    max_steps=4000000,return_address=caller,seed=seed,write_observer=publication)
                self.assertEqual(after.buffers,expected.buffers,'complete cart/WRAM/stack poststate')
                self.assertEqual((regs[4],regs[8:15],regs[15]),(resume,initial[8:15],sp-frame))
                self.assertEqual(pr,return_pc('_overlay_rom_after_read') if frame else caller)
                self.assertEqual((regs[1],regs[2],regs[3],regs[7]),
                                 (0xffffffff,staging+0x200000,0x221ffffc,int.from_bytes(payload[-4:],'big')))
                self.assertEqual(len(calls),3)
                self.assertEqual(sum(a==0x06000320 for a,_,_ in writes),1)


class NativeMemory:
    """Compact, bounded RAM/cart aliases for the existing instruction model."""
    def __init__(self):
        self.buffers={0x02000000:bytearray([0xa5])*0x400000,
                      0x06000000:bytearray([0x73])*0x100000,
                      0x00200000:bytearray([0xb9])*0x100000}
    def location(self,address):
        address&=0x1fffffff
        for base,data in self.buffers.items():
            if base<=address<base+len(data): return data,address-base
        raise KeyError(f'unmapped native address {address:08x}')
    def __contains__(self,address):
        try: self.location(address);return True
        except KeyError: return False
    def __getitem__(self,address):
        data,offset=self.location(address);return data[offset]
    def update(self,rows):
        for address,value in rows:
            data,offset=self.location(address);data[offset]=value
    def store(self,address,values):
        data,offset=self.location(address)
        if len(values)>len(data)-offset: raise ValueError('native span exceeds mapped RAM')
        data[offset:offset+len(values)]=values
    def load(self,address,size):
        data,offset=self.location(address)
        if size>len(data)-offset: raise ValueError('native read exceeds mapped RAM')
        return bytes(data[offset:offset+size])
    def copy(self):
        result=NativeMemory();result.buffers={a:bytearray(d) for a,d in self.buffers.items()};return result


if __name__=='__main__': unittest.main()
