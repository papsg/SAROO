"""Production loaded-file qualification, before normal callback/configuration."""
import os
import hashlib
import json
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import unittest
import zlib

import test_sector_overlay as overlay
import test_sector_overlay_later as later

ROOT = Path(__file__).resolve().parents[1]


class LoadedFileGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/overlay-guard'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        for source, dest in [('sector_overlay.c', 'guard.inc'), ('crc32.c', 'crc32.inc')]:
            cls.target.joinpath(dest).write_bytes((ROOT / 'Firm_Saturn' / source).read_bytes()
                                                  .replace(b'#include "main.h"', b''))
        flags = ['-O2'] if os.environ.get('SAROO_OVERLAY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'guard'
        result = subprocess.run([*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g',
                                 '-Wall', '-Wextra', '-Werror', *flags, '-I', cls.target,
                                 ROOT / 'tests/menu_overlay_guard.c', '-o', cls.binary],
                                capture_output=True, text=True, timeout=60)
        (cls.target / 'compile.log').write_text(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def check(self, case):
        result = subprocess.run([self.binary, case], capture_output=True, text=True, timeout=20,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / f'{case}.log').write_text(result.stdout+result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_complete_postimage_and_all_valid_cartridge_actions(self):
        self.check('valid')

    def test_wrong_file_restores_only_source_word_and_wrong_word_is_untouched(self):
        self.check('mismatch')

    def test_corrupt_transport_and_header_refuse_before_game_memory(self):
        self.check('metadata')

    def test_independent_unsigned_bounds_include_wrap_and_alignment(self):
        self.check('bounds')

    def test_explicit_skip_restores_source_word_without_loaded_file_crc(self):
        self.check('skip')

    def test_old_actions_are_untouched_and_invalid_guard_actions_refuse(self):
        self.check('actions')


class FirstFileRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay.SectorOverlayTests.setUpClass()
        cls.fixture = overlay.SectorOverlayTests

    setUp = later.LaterRecipeTests.setUp
    compile = later.LaterRecipeTests.compile

    def source(self, *, attributes=0, unit=0, gap=0, write_block=0):
        data = bytes((i*17+3)&255 for i in range(4098))
        location = 21+attributes+(gap if attributes and unit else 0)
        blocks = []
        for i in range(3):
            if i and unit and i % unit == 0:
                location += gap
            blocks.append(location)
            location += 1
        payloads = [bytearray(2048) for _ in range(location)]
        payloads[0][:16] = b'SEGA SEGASATURN '
        payloads[0][32:48] = overlay.PRODUCT+b'V1.000'
        struct.pack_into('>I',payloads[0],0xe0,4096)
        struct.pack_into('>I',payloads[0],0xf0,0x06004000)

        def record(lba, size, name, flags=0):
            out = bytearray(34+len(name)-(len(name)%2));out[0]=len(out)
            struct.pack_into('<I',out,2,lba);struct.pack_into('>I',out,6,lba)
            struct.pack_into('<I',out,10,size);struct.pack_into('>I',out,14,size)
            out[25],out[32]=flags,len(name)
            struct.pack_into('<H',out,28,1);struct.pack_into('>H',out,30,1)
            out[33:33+len(name)]=name
            return out

        root = record(20,2048,b'\0',2)
        pvd = payloads[16];pvd[:7]=b'\1CD001\1'
        struct.pack_into('<I',pvd,80,len(payloads));struct.pack_into('>I',pvd,84,len(payloads))
        for field,value in [(120,1),(124,1),(128,2048)]:
            struct.pack_into('<H',pvd,field,value);struct.pack_into('>H',pvd,field+2,value)
        pvd[156:156+len(root)]=root
        first=record(21,len(data),b'FIRST.BIN;1')
        first[1],first[26],first[27]=attributes,unit,gap
        directory=root+record(20,2048,b'\1',2)+first
        payloads[20][:len(directory)]=directory
        for i,block in enumerate(blocks):
            part=data[i*2048:(i+1)*2048];payloads[block][:len(part)]=part
        raws=[]
        for lba,payload in enumerate(payloads):
            raw=bytearray(overlay.sector(lba+150));raw[16:2064]=payload
            raws.append(overlay.canonical(raw))
        track=self.path/'first.bin';track.write_bytes(b''.join(raws))
        self.path.joinpath('fixture.cue').write_text('FILE "first.bin" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n')
        source={'abi':2,'product_id':overlay.PRODUCT.decode().strip(),
                'disc_id':(overlay.PRODUCT+b'V1.000').decode(),'origin':'Authored first-file fixture',
                'title':'Guard fixture','summary':'One source word','rationale':'Retain complete file qualification',
                'replaces_callback':True,'tracks':[{'number':1,'sha256':hashlib.sha256(track.read_bytes()).hexdigest()}],
                'writes':[{'fad':150+blocks[write_block],'offset':118,
                           'before':data[write_block*2048+102:write_block*2048+104].hex(),'after':'0009',
                           'effect':'Reviewed word substitution','reason':'Exercise source delivery'}],
                'first_read_guard':{'sha256':hashlib.sha256(data).hexdigest(),'reason':'Complete first-file guard'}}
        return source,raws,data,blocks

    def test_full_iso_compiler_and_decoder_keep_completion_failure_and_repeated_read_controls(self):
        source,raws,data,blocks=self.source()
        result,pack=self.compile(source)
        self.assertEqual(result.returncode,0,result.stderr)
        value=pack.read_bytes();post=bytearray(data);post[102:104]=b'\0\x09'
        d=struct.pack('>IIIIHH',0x06004000,len(data),zlib.crc32(post),102,
                      int.from_bytes(data[102:104],'big'),9)
        d+=struct.pack('>I',zlib.crc32(d))
        self.assertEqual((len(value),value[4],value[5],value[7]),(720,4,2,0x80))
        self.assertEqual(value[48:72],d)
        self.assertEqual(value[381],1)
        raw=raws[blocks[0]];expected=bytearray(raw);expected[118:120]=b'\0\x09'
        expected=overlay.canonical(expected)
        for leaf,blob in [('original',raw),('expected',expected),('marker',raws[blocks[-1]])]:
            self.path.joinpath(leaf).write_bytes(blob)
        run=subprocess.run([self.fixture.decoder,'guarded',pack,self.path/'original',
                            self.path/'expected',self.path/'marker'],capture_output=True,text=True,timeout=15)
        self.assertEqual(run.returncode,0,run.stderr)

    def test_attributes_and_interleaves_bind_logical_file_offset_and_actual_completion_sector(self):
        for attributes,unit,gap,expected_blocks in [(1,0,0,[22,23,24]),(0,1,1,[21,23,25]),(2,2,2,[25,26,29])]:
            with self.subTest(layout=(attributes,unit,gap)), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path=Path(folder)
                source,raws,data,blocks=self.source(attributes=attributes,unit=unit,gap=gap,write_block=1)
                self.assertEqual(blocks,expected_blocks)
                result,pack=self.compile(source);self.assertEqual(result.returncode,0,result.stderr)
                value=pack.read_bytes();self.assertEqual(int.from_bytes(value[372:376],'big'),150+blocks[-1])
                self.assertEqual(int.from_bytes(value[60:64],'big'),2150)
                original=b''.join(raws);post=list(raws)
                changed=bytearray(post[blocks[1]]);changed[118:120]=b'\0\x09';post[blocks[1]]=overlay.canonical(changed)
                self.path.joinpath('original').write_bytes(original);self.path.joinpath('post').write_bytes(b''.join(post))
                run=subprocess.run([self.fixture.decoder,'stream',pack,self.path/'original',
                                    self.path/'post','150','1'],capture_output=True,text=True,timeout=15)
                self.assertEqual(run.returncode,0,run.stderr)

    def test_guard_compilation_refuses_wrong_full_source_and_last_sector_edits(self):
        for bad in ('sha','last','width','ip','endian','volume','multi','gap'):
            with self.subTest(bad=bad), tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path=Path(folder)
                source,raws,_,blocks=self.source()
                if bad=='sha':source['first_read_guard']['sha256']='0'*64
                elif bad=='last':source['writes'][0]['fad']=150+blocks[-1]
                elif bad=='width':source['writes'][0].update(before='03',after='09')
                else:
                    lba=0 if bad=='ip' else 16 if bad in ('endian','volume') else 20
                    raw=bytearray(raws[lba])
                    if bad=='ip':raw[16+0xe3]^=1
                    elif bad=='endian':raw[16+84]^=1
                    elif bad=='volume':raw[16+124]=2
                    elif bad=='multi':raw[16+68+25]=0x80
                    else:raw[16+68+27]=1
                    raws[lba]=overlay.canonical(raw);track=self.path/'first.bin';track.write_bytes(b''.join(raws))
                    source['tracks'][0]['sha256']=hashlib.sha256(track.read_bytes()).hexdigest()
                result,pack=self.compile(source)
                self.assertNotEqual(result.returncode,0)
                self.assertFalse(pack.exists())
