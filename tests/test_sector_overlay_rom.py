"""SOR1 compiler/production decoder with authored sectors, not game images."""
import copy
import hashlib
import json
from pathlib import Path
import shlex
import os
import struct
import subprocess
import tempfile
import unittest
import zlib

import test_sector_overlay as overlay
import test_sector_overlay_later as later
from check_later_callbacks_sh2 import execute,region


class ROMRecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        overlay.SectorOverlayTests.setUpClass();cls.fixture=overlay.SectorOverlayTests
        cls.legacy_dir=Path(tempfile.mkdtemp(prefix='abi4-',dir=cls.fixture.output))
        for relative in ['Firm_MCU/Saturn/sector_overlay.c','Firm_MCU/Saturn/sector_overlay.h','tests/sector_overlay.c']:
            data=subprocess.check_output(['git','show',f'be3423d:{relative}'],cwd=overlay.ROOT)
            name='core.c' if relative.endswith('Saturn/sector_overlay.c') else Path(relative).name
            (cls.legacy_dir/name).write_bytes(data)
        cls.legacy=cls.legacy_dir/'decoder'
        subprocess.run([*shlex.split(os.environ.get('CC','cc')),'-std=c11','-O1','-g',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',cls.legacy_dir,
                        cls.legacy_dir/'sector_overlay.c',cls.legacy_dir/'core.c','-o',cls.legacy],
                       check=True,capture_output=True,timeout=60)

    setUp=later.LaterRecipeTests.setUp
    compile=later.LaterRecipeTests.compile

    def source(self,jp=False):
        name='kof95-jp-1M' if jp else 'kof95-eu-v1001'
        return json.loads((overlay.ROOT/'recipes'/f'{name}-rom-sector.json').read_text())

    def fixture_source(self,source):
        recipe=copy.deepcopy(source);version=source['disc_id'][10:].encode()
        recipe['product_id']=overlay.PRODUCT.decode().strip();recipe['disc_id']=(overlay.PRODUCT+version).decode()
        last=max([165]+[w['fad'] for w in source['writes']]+[g['fad'] for g in source.get('guards',[])])
        raws={fad:bytearray(overlay.sector(fad,ip=fad==150,version=version)) for fad in range(150,last+1)}
        struct.pack_into('>I',raws[150],16+0xe0,0x1800)
        raws[165][2000:2064]=bytes(64)
        for write in source['writes']:
            before=bytes.fromhex(write['before']);raws[write['fad']][write['offset']:write['offset']+len(before)]=before
        raws={fad:overlay.canonical(raw) for fad,raw in raws.items()}
        self.path.joinpath('disc.bin').write_bytes(b''.join(raws.values()))
        self.path.joinpath('fixture.cue').write_text('FILE "disc.bin" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n')
        recipe['tracks']=[{'number':1,'sha256':hashlib.sha256(self.path.joinpath('disc.bin').read_bytes()).hexdigest()}]
        return recipe,raws

    def check_pack(self,recipe,raws):
        run,pack=self.compile(recipe);self.assertEqual(run.returncode,0,run.stderr)
        value=pack.read_bytes();self.assertEqual((value[4],value[6],value[7]),(5,0,0x80))
        d=bytearray(64);d[:4]=b'SOR1';rom=recipe['rom_cart'];d[4]=1 if rom['phase']=='game_entry' else 2
        struct.pack_into('>III',d,8,rom['resume'],0x200000,int(rom['crc32'],16))
        path=rom['path'].encode();d[24:24+len(path)]=path;struct.pack_into('>I',d,60,zlib.crc32(d[:60]))
        targets=sorted(set([165]+[w['fad'] for w in recipe['writes']]+[g['fad'] for g in recipe.get('guards',[])]))
        fads=[int.from_bytes(value[i:i+4],'big') for i in range(24,len(value),348)]
        for fad in targets:
            before=raws[fad];after=bytearray(before)
            changes=[w for w in recipe['writes'] if w['fad']==fad]
            for w in changes:
                data=bytes.fromhex(w['after']);after[w['offset']:w['offset']+len(data)]=data
            if fad==165:after[2000:2064]=d
            after=overlay.canonical(after)
            self.path.joinpath('before').write_bytes(before);self.path.joinpath('after').write_bytes(after)
            run=subprocess.run([self.fixture.decoder,'apply' if changes or fad==165 else 'guard',pack,
                                self.path/'before',self.path/'after',str(fads.index(fad)),
                                str(2000 if fad==165 else changes[0]['offset'] if changes else 104)],capture_output=True,text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        run=subprocess.run([self.legacy,'reject',pack],capture_output=True,text=True,timeout=15)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)
        return value

    def test_kof95_both_regions_native_edits_descriptor_fragments_and_old_decoder_refusal(self):
        for jp in (False,True):
            with self.subTest(jp=jp),tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path=Path(folder);source=self.source(jp)
                self.assertEqual([w['fad'] for w in source['writes']], [175,195,195] if jp else [173,193,193])
                self.assertEqual([(w['offset'],w['before'],w['after']) for w in source['writes'][1:]],
                                 [(146,'2802','0009'),(334,'400b4f22','00090009')])
                self.assertEqual(sum(a!=b for w in source['writes'] for a,b in zip(bytes.fromhex(w['before']),bytes.fromhex(w['after']))),21)
                recipe,raws=self.fixture_source(source);pack=self.check_pack(recipe,raws)
                self.assertEqual((len(pack),pack[5]),(2112,6))

    def test_japanese_alias_recipes_are_identical_and_do_not_add_an_ip_helper(self):
        values=[json.loads((overlay.ROOT/'recipes'/f'kof95-jp-{n}M-rom-sector.json').read_text()) for n in (1,2,3)]
        self.assertEqual(values,[values[0]]*3)
        self.assertTrue(all(w['fad'] in (175,195) for w in values[0]['writes']))

    def test_native_ims_initial_call_and_service_tail_keep_registers_and_original_literals(self):
        for jp in (False,True):
            write=self.source(jp)['writes'][0];base=0x0607cd80;shift=0 if jp else 4
            variants=[]
            for kind in ('before','after'):
                code=bytearray(0x80+shift);span=bytes.fromhex(write[kind]);code[:len(span)]=span
                code[0x7c+shift:0x80+shift]=bytes.fromhex('06000310')
                ram=region(0x25fe00a0,4)|region(0x06000310,4)|region(0x060fefc0,128)
                ram.update(enumerate((0x06090000).to_bytes(4,'big'),0x06000310))
                variants.append(execute(code,base,base+0x14,ram,external=0x06090000,return_address=0x06012340))
            self.assertEqual(variants[0],variants[1])
            self.assertEqual(variants[1][4][0],(0x25fe00a0,4,0xbfff))
            code=bytearray.fromhex(write['after']);ram=region(0x02000f18,4)
            ram.update(enumerate((0x02012340).to_bytes(4,'big'),0x02000f18))
            before,after,pr,_,writes,calls=execute(code,base,0x02012340,ram,entry_pc=base+0x2a+shift,return_address=0x06012340)
            before[0]=0x02012340
            self.assertEqual((after,pr,writes,calls),(before,0x06012340,[],[]))
            if not jp:
                a,b=bytes.fromhex(write['before']),bytes.fromhex(write['after'])
                self.assertEqual(a[0x2a:0x2e],bytes.fromhex('b0630009'))
                self.assertEqual(a[0x2a:0x2e],b[0x2a:0x2e])

    def test_invalid_rom_metadata_refuses_without_output(self):
        mutations=[('path','/SAROO/ISO/../KOF95.BIN'),('path','/SAROO/ISO/.BIN'),('path','/SAROO/ISO/test.BIN'),
                   ('sha256','A'*64),('crc32','XYZ'),('phase','other'),('resume',0x06002049),('resume',0x06003800)]
        for key,value in mutations:
            with self.subTest(key=key,value=value),tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path=Path(folder);recipe,_=self.fixture_source(self.source());recipe['rom_cart'][key]=value
                run,pack=self.compile(recipe);self.assertNotEqual(run.returncode,0);self.assertFalse(pack.exists())

    def test_declared_ip_overlap_occupied_tail_and_bad_source_refuse(self):
        for wrong in ('overlap','alignment','tail','hash','mode','mixed','callback','cart'):
            with self.subTest(wrong=wrong),tempfile.TemporaryDirectory(dir=self.fixture.output) as folder:
                self.path=Path(folder);recipe,raws=self.fixture_source(self.source())
                if wrong in ('overlap','alignment','tail'):
                    fad=165 if wrong=='tail' else 150;raw=bytearray(raws[fad])
                    if wrong=='tail':raw[2000]=1
                    else:struct.pack_into('>I',raw,16+0xe0,0x8000 if wrong=='overlap' else 0x1801)
                    raws[fad]=overlay.canonical(raw);self.path.joinpath('disc.bin').write_bytes(b''.join(raws.values()))
                    recipe['tracks'][0]['sha256']=hashlib.sha256(self.path.joinpath('disc.bin').read_bytes()).hexdigest()
                if wrong=='hash':recipe['tracks'][0]['sha256']='0'*64
                if wrong=='mode':self.path.joinpath('fixture.cue').write_text('FILE "disc.bin" BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\n')
                if wrong=='mixed':recipe['first_read_guard']={'sha256':'0'*64,'reason':'Refuse mixed ownership'}
                if wrong=='callback':recipe['replaces_callback']=False
                if wrong=='cart':recipe['cart_mode']=1
                run,pack=self.compile(recipe);self.assertNotEqual(run.returncode,0);self.assertFalse(pack.exists())

    def test_ultraman_has_no_game_edit_and_uses_first_read_resume(self):
        source=json.loads((overlay.ROOT/'recipes/ultraman-jp-v1001-rom-sector.json').read_text())
        self.assertEqual(source['disc_id'],'T-13308G  V1.001')
        self.assertEqual(source['writes'],[])
        self.assertEqual(source['rom_cart']['phase'],'first_read')
        self.assertEqual(source['rom_cart']['resume'],0x06002222)
        self.assertEqual(source['rom_cart']['crc32'],'b9644b52')
        self.assertEqual([g['fad'] for g in source['guards']],[151])
        recipe,raws=self.fixture_source(source);pack=self.check_pack(recipe,raws)
        self.assertEqual((len(pack),pack[5]),(1068,3))

    def test_ultraman_compiler_changes_only_the_reserved_initial_transfer_tail(self):
        source=json.loads((overlay.ROOT/'recipes/ultraman-jp-v1001-rom-sector.json').read_text())
        recipe,raws=self.fixture_source(source)
        run,pack=self.compile(recipe);self.assertEqual(run.returncode,0,run.stderr)
        original=b''.join(raws.values());post=bytearray(original)
        # Independent descriptor is checked by check_pack above; reconstruct it
        # from the two actual chunks here, then verify the complete raw stream.
        value=pack.read_bytes();d=value[24+348+16:24+348+60]+value[24+696+16:24+696+36]
        changed=bytearray(raws[165]);changed[2000:2064]=d
        post[15*2352:16*2352]=overlay.canonical(changed)
        self.assertEqual(b''.join(post[i+16:i+2064] for i in range(0,len(post),2352))[:-64],
                         b''.join(original[i+16:i+2064] for i in range(0,len(original),2352))[:-64])
        self.path.joinpath('original.raw').write_bytes(original);self.path.joinpath('post.raw').write_bytes(post)
        run=subprocess.run([self.fixture.decoder,'stream',pack,self.path/'original.raw',self.path/'post.raw','150','1'],
                           capture_output=True,text=True,timeout=15)
        self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__': unittest.main()
