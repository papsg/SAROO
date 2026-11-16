"""Optional contiguous allocation on real FatFs and synthetic sparse media.
Adapted by papsg from paps-g 491f8b63/SSR-013. No device operations.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import unittest
import test_fatfs_seek as support


def sparse_copy(source, destination):
    with source.open('rb') as src, destination.open('wb') as dst:
        dst.truncate(source.stat().st_size)
        offset=0
        for block in iter(lambda: src.read(1024*1024), b''):
            if block != bytes(len(block)):
                dst.seek(offset);dst.write(block)
            offset+=len(block)


class FatFsExpandTests(unittest.TestCase):
    fixture_name='fatfs-expand'
    generic_formats=False
    fixture_sources=(*support.FatFsSeekTests.fixture_sources,
                     'test_fatfs_expand.py','fatfs_expand_bitmap.c')
    tearDownClass=classmethod(support.FatFsSeekTests.tearDownClass.__func__)

    @classmethod
    def setUpClass(cls):
        support.FatFsSeekTests.setUpClass.__func__(cls)
        cls.variants=[]
        for name,generic in [('app',False),('retained',True)]:
            folder=cls.target/name;shutil.copytree(cls.target/'FatFS',folder)
            p=folder/'ffconf.h';data=p.read_text()
            data,count=re.subn(r'(#define FF_USE_EXPAND\s+)0',r'\g<1>1',data,count=1)
            assert count==1;p.write_text(data)
            cls.variants.append((name,cls.compile(folder,name,generic)))
        cls.formatter=cls.variants[0][1]
        cls.bitmap_binary=cls.compile(cls.target/'retained','bitmap',True,bitmap=True)

    @classmethod
    def compile(cls,source,name,generic,bitmap=False):
        flags=['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        fixture=cls.target/('fatfs_expand_bitmap.c' if bitmap else 'fatfs_seek.c')
        command=[os.environ.get('CC','cc'),'-std=gnu11','-g',*flags,'-pthread',
                 '-DSAROO_FAT32_ONLY='+str(int(not generic)),
                 '-I'+str(source),'-I'+str(cls.target),str(fixture),
                 *([] if bitmap else [str(source/'ff.c')]),str(source/'ffunicode.c'),
                 '-o',str(cls.target/(name+'-host'))]
        (cls.target/(name+'-compile.json')).write_text(json.dumps(command,indent=2)+'\n')
        result=subprocess.run(command,capture_output=True,timeout=60)
        (cls.target/(name+'-compile.log')).write_bytes(result.stdout+result.stderr)
        result.check_returncode()
        return cls.target/(name+'-host')

    def expand_providers(self): return self.variants

    def command(self,name,*arguments,expected=0,binary=None):
        result=subprocess.run([str(binary or self.formatter),*map(str,arguments)],
                              capture_output=True,text=True,timeout=60,
                              env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        row=dict(name=name,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr)
        self.results.append(row)
        (self.target/(name+'-result.json')).write_text(json.dumps(row,indent=2)+'\n')
        self.assertEqual(result.returncode,expected,row)
        return row

    def image(self,name,megabytes,filesystem):
        self.assertEqual(filesystem,'fat32')
        path=self.target/(name+'.img')
        with path.open('wb') as image:image.truncate(megabytes*1024**2)
        self.command(name+'-format',path,'format-expand',0)
        return path

    @staticmethod
    def digest(path):
        value=hashlib.sha256()
        with path.open('rb') as image:
            for block in iter(lambda:image.read(1024*1024),b''):value.update(block)
        return value.hexdigest()

    def test_bitmap_matches_independent_linear_span_oracle(self):
        row=self.command('bitmap-exhaustive',binary=self.bitmap_binary)
        self.assertIn('invalid=0 missed=0 writes=0',row['stdout'])

    def expand_setup(self,name,free,hint):
        path=self.image(name,64,'fat32')
        with path.open('r+b') as image:
            boot=image.read(512);reserved=struct.unpack_from('<H',boot,14)[0]
            fatsectors=struct.unpack_from('<I',boot,36)[0]
            total=struct.unpack_from('<I',boot,32)[0];data=reserved+fatsectors
            limit=total-data+2;last=limit-1
            free=set(free(last));root=4
            self.assertEqual((boot[13],boot[16]),(1,1));self.assertNotIn(root,free)
            owners=[i for i in range(2,limit) if i not in free and i!=root]
            table=bytearray(fatsectors*512)
            struct.pack_into('<II',table,0,0x0ffffff8,0x0fffffff)
            struct.pack_into('<I',table,root*4,0x0fffffff)
            for cluster,next_cluster in zip(owners,owners[1:]+[0x0fffffff]):
                struct.pack_into('<I',table,cluster*4,next_cluster)
            image.seek(reserved*512);image.write(table)
            for base in (0,struct.unpack_from('<H',boot,50)[0]):
                image.seek(base*512+44);image.write(struct.pack('<I',root))
                image.seek((base+1)*512+488);image.write(struct.pack('<II',len(free),hint(last)))
            directory=bytearray(512);directory[:11]=b'FILLER  BIN';directory[11]=0x20
            struct.pack_into('<H',directory,20,owners[0]>>16)
            struct.pack_into('<H',directory,26,owners[0]&0xffff)
            struct.pack_into('<I',directory,28,len(owners)*512)
            image.seek((data+root-2)*512);image.write(directory)
        return path

    def expand_reference(self,path):
        # Complete ownership and linear-span oracle for these FAT32 fixtures.
        with path.open('rb') as image:
            boot=image.read(512);reserved=struct.unpack_from('<H',boot,14)[0]
            fatsectors=struct.unpack_from('<I',boot,36)[0];data=reserved+fatsectors
            limit=struct.unpack_from('<I',boot,32)[0]-data+2
            root=struct.unpack_from('<I',boot,44)[0]
            image.seek(6*512);self.assertEqual(image.read(512),boot)
            image.seek(reserved*512);table=image.read(fatsectors*512)
            fat=[value&0x0fffffff for value, in struct.iter_unpack('<I',table)][:limit]
            image.seek((data+root-2)*512);directory=image.read(512)
            self.assertGreaterEqual(fat[root],0x0ffffff8)
            owned={root};chains={};sizes={}
            for offset in (0,32):
                entry=directory[offset:offset+32]
                if not entry[0]: continue
                name=entry[:11];size=struct.unpack_from('<I',entry,28)[0]
                first=struct.unpack_from('<H',entry,26)[0]|struct.unpack_from('<H',entry,20)[0]<<16
                chain=[];cluster=first
                if not size: self.assertEqual(first,0)
                while cluster:
                    self.assertTrue(2<=cluster<limit and cluster not in owned)
                    chain.append(cluster);owned.add(cluster)
                    link=fat[cluster]
                    if link>=0x0ffffff8: break
                    cluster=link
                self.assertEqual(len(chain)*512,size)
                chains[name]=chain;sizes[name]=size
                if name==b'PROBE   BIN' and size:
                    payload=bytearray()
                    for cluster in chain:
                        image.seek((data+cluster-2)*512);payload.extend(image.read(512))
                    self.assertEqual(payload,bytes((i*43+7)&255 for i in range(size)))
            free={i for i in range(2,limit) if fat[i]==0}
            self.assertTrue(owned.isdisjoint(free));self.assertEqual(len(owned)+len(free),limit-2)
            run=maximum=0
            for cluster in range(2,limit):
                run=run+1 if cluster in free else 0;maximum=max(maximum,run)
            image.seek(512+488);free_hint,origin=struct.unpack('<II',image.read(8))
            self.assertEqual(free_hint,len(free))
        return dict(free=free,chains=chains,sizes=sizes,maximum=maximum,table=table,origin=origin)

    def test_optional_expand_respects_linear_heap_and_complete_search(self):
        cases=[('split',lambda last:{2,last},lambda last:last,2,False),
               ('end',lambda last:{last-1,last},lambda last:last-1,2,True),
               ('head',lambda last:{2,3},lambda last:last,2,True),
               ('cursor',lambda last:{10,11,12},lambda last:11,3,True)]
        for provider,binary in self.expand_providers():
            for mode,free,hint,needed,admitted in cases:
                for opt in (0,1):
                    with self.subTest(provider=provider,mode=mode,opt=opt):
                        name=f'expand-{provider}-{mode}-{opt}'
                        path=self.expand_setup(name,free,hint)
                        preimage=Path(str(path)+'.before');sparse_copy(path,preimage)
                        before=self.expand_reference(preimage)
                        self.results.append(dict(name=name+'-precondition',image=str(preimage),
                            sha256=self.digest(preimage),free=sorted(before['free']),
                            maximum_linear_run=before['maximum'],origin=before['origin'],
                            filler_clusters=len(before['chains'][b'FILLER  BIN']),
                            complete_unique_cluster_ownership=True))
                        self.assertEqual(before['maximum']>=needed,admitted)
                        row=self.command(name,path,'expand',needed | (opt << 4),expected=0 if admitted else 7,binary=binary)
                        after=self.expand_reference(path)
                        self.assertEqual(after['chains'][b'FILLER  BIN'],before['chains'][b'FILLER  BIN'])
                        if admitted:
                            chain=after['chains'][b'PROBE   BIN']
                            self.assertEqual(len(chain),needed)
                            self.assertEqual(chain,list(range(chain[0],chain[0]+needed)))
                            self.assertTrue(set(chain)<=before['free'])
                        else:
                            self.assertIn(f"hint_after={before['origin']}",row['stdout'])
                            self.assertEqual(after['table'],before['table'])
                            self.assertEqual(after['sizes'][b'PROBE   BIN'],0)

    def test_optional_expand_refuses_more_clusters_than_the_heap(self):
        for provider,binary in self.expand_providers():
            for opt in (0,1):
                with self.subTest(provider=provider,opt=opt):
                    name=f'expand-oversize-{provider}-{opt}'
                    path=self.expand_setup(name,lambda last:{last-1,last},lambda last:last-1)
                    before=self.expand_reference(path)
                    self.command(name,path,'expand',opt << 4,expected=7,binary=binary)
                    after=self.expand_reference(path)
                    self.assertEqual(after['table'],before['table'])
                    self.assertEqual(after['origin'],before['origin'])
                    self.assertEqual(after['sizes'][b'PROBE   BIN'],0)
