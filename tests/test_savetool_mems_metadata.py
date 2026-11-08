"""Whole-archive MEMS admission against the actual native CLI."""
from pathlib import Path
import struct
import unittest

from savetool_support import NativeSaveTest
from test_mems_overwrite import archive_fixture, raw_save, digest, BLOCK


class MemsMetadataTests(NativeSaveTest):
    def test_invalid_metadata_refuses_all_commands_without_changes(self):
        original,locations=archive_fixture([(0,b"SAVE_A00001",b"A"*2048),(1,b"SAVE_B00002",b"B"*2048)])
        header,extents=locations[0];neighbor,other_extents=locations[1];cases={}
        def change(name,offset,data):
            image=bytearray(original);image[offset:offset+len(data)]=data;cases[name]=image
        for block in [0,7,8064,65535]:
            change(f"header-{block}",BLOCK+14,struct.pack(">H",block))
            change(f"data-{block}",header+64,struct.pack(">H",block))
        for size in [479*BLOCK+1,0xffffffff]:change(f"size-{size}",header+12,struct.pack(">I",size))
        for name,block in [("self-header",header//BLOCK),("neighbor-header",neighbor//BLOCK),
                           ("shared-data",other_extents[0][0]//BLOCK)]:
            change(name,header+64,struct.pack(">H",block))
        change("duplicate-data",header+66,original[header+64:header+66])
        change("extra-chain-entry",header+68,original[header+64:header+66])
        change("name-mismatch",header,b"X")
        change("wrong-total-size",8,struct.pack(">I",32768))
        change("unreserved-header",16,b"\x7f")
        for name,block in [("unallocated-header",header//BLOCK),("unallocated-data",extents[0][0]//BLOCK)]:
            offset=16+block//8;change(name,offset,bytes([original[offset]&~(1<<(block%8))]))
        change("duplicate-header",BLOCK+16,original[BLOCK:BLOCK+16])
        commands={"list":[],"export":["-s","0"],"delete":["-s","0","-d"],
                  "overwrite":["-s","0","-i","incoming.SRO"],"create":["-i","incoming.SRO"]}
        for reason,image in cases.items():
            for command,args in commands.items():
                with self.subTest(reason=reason,command=command),self.case() as temp:
                    root=Path(temp);archive=root/"archive.bin";archive.write_bytes(image)
                    data=raw_save(b"NEW00000000",b"N"*2048);incoming=root/"incoming.SRO";incoming.write_bytes(data)
                    self.invoke(root,archive,*args,ok=False)
                    self.assertEqual(digest(archive.read_bytes()),digest(image))
                    self.assertEqual(incoming.read_bytes(),data)
                    self.assertEqual(sorted(p.name for p in root.iterdir()),["archive.bin","incoming.SRO"])

    def test_delete_recounts_stale_free_counts_and_preserves_orphans_neighbors(self):
        for size in [0,960,961,2048,479*BLOCK]:
            for stale in [0,65535]:
                with self.subTest(size=size,stale=stale),self.case() as temp:
                    root=Path(temp);image,locations=archive_fixture([(0,b"SAVE_A00001",b"A"*size),
                                                                   (3,b"SAVE_B00002",b"Neighbor"*200)])
                    struct.pack_into(">H",image,12,stale);image[16+8063//8]|=1<<(8063%8)
                    archive=root/"archive.bin";archive.write_bytes(image)
                    self.invoke(root,archive)
                    self.assertEqual(digest(archive.read_bytes()),digest(image))
                    self.invoke(root,archive,"-s",0,"-d")
                    expected=bytearray(image);expected[BLOCK:BLOCK+16]=bytes(16)
                    header,extents=locations[0]
                    blocks=[header//BLOCK]+([offset//BLOCK for offset,count in extents] if size>960 else [])
                    for block in blocks:expected[16+block//8]&=~(1<<(block%8))
                    struct.pack_into(">H",expected,12,8064-sum(bin(b).count("1") for b in expected[16:1024]))
                    self.assertEqual(digest(archive.read_bytes()),digest(expected))
                    self.invoke(root,archive,"-s",0)
                    self.assertEqual((root/"SAVE_B00002.SRO").read_bytes()[64:],b"Neighbor"*200)

    def test_unused_padding_is_not_a_c_string_terminator(self):
        with self.case() as temp:
            root=Path(temp);image,locations=archive_fixture([(0,b"SAVE_A00001",b"A"*32)])
            image[BLOCK+11:BLOCK+14]=b"XYZ";image[locations[0][0]+11]=ord("Z")
            archive=root/"archive.bin";archive.write_bytes(image)
            listed=self.invoke(root,archive);self.assertIn("SAVE_A00001  size=",listed.stdout)
            (root/"SAVE_A00001.bin").write_bytes(raw_save(b"SAVE_A00001",b"R"*32))
            self.invoke(root,archive,"-s",0,"-i")
            self.invoke(root,archive,"-s",0)
            self.assertEqual((root/"SAVE_A00001.SRO").read_bytes()[64:],b"R"*32)


class MemsNameIdentityTests(NativeSaveTest):
    def test_divergent_suffix_after_nul_refuses_all_commands(self):
        commands={"list":[],"sro":["-s","0"],"bup":["-x","0"],"delete":["-s","0","-d"],
                  "overwrite":["-s","0","-i","incoming.SRO"],"create":["-i","incoming.SRO"]}
        for nul in [1,3,9]:
            name=bytearray(b"ABCDEFGHIJK");name[nul]=0
            for size in [0,961]:
                image,locations=archive_fixture([(0,name,b"A"*size),(1,b"NEIGHBOR001",b"Neighbor")])
                image[locations[0][0]+nul+1]^=1
                for command,args in commands.items():
                    with self.subTest(nul=nul,size=size,command=command),self.case() as temp:
                        root=Path(temp);archive=root/"archive.bin";archive.write_bytes(image)
                        data=raw_save(b"NEW00000000",b"New payload");incoming=root/"incoming.SRO";incoming.write_bytes(data)
                        self.invoke(root,archive,*args,ok=False)
                        self.assertEqual(digest(archive.read_bytes()),digest(image))
                        self.assertEqual(incoming.read_bytes(),data)
                        self.assertEqual(sorted(p.name for p in root.iterdir()),["archive.bin","incoming.SRO"])

    def test_equal_raw_names_preserve_all_bytes_through_operations(self):
        for name in [b"ABC\0AAAAAAA",b"ABC\0\0\0\0\0\0\0\0",b"ABCDEFGHIJK"]:
            for size in [0,961]:
                with self.subTest(name=name,size=size),self.case() as temp:
                    root=Path(temp);image,locations=archive_fixture([(0,name,b"A"*size),(1,b"NEIGHBOR001",b"Neighbor")])
                    archive=root/"archive.bin";archive.write_bytes(image)
                    prefix=name.split(b"\0")[0].decode()
                    self.invoke(root,archive)
                    for flag,extension in [("-s","SRO"),("-x","BUP")]:
                        self.invoke(root,archive,flag,0)
                        result=(root/f"{prefix}.{extension}").read_bytes()
                        self.assertEqual(result[16:27],name);self.assertEqual(result[64:],b"A"*size)
                        self.assertEqual(digest(archive.read_bytes()),digest(image))
                    path=root/"incoming.SRO";path.write_bytes(raw_save(name,b"R"*size))
                    self.invoke(root,archive,"-s",0,"-i",path)
                    changed=archive.read_bytes();header=locations[0][0]
                    self.assertEqual(changed[BLOCK:BLOCK+11],name)
                    self.assertEqual(changed[header:header+11],name)
                    self.invoke(root,archive,"-s",0)
                    self.assertEqual((root/f"{prefix}.SRO").read_bytes()[64:],b"R"*size)
                    self.invoke(root,archive,"-s",0,"-d");self.invoke(root,archive,"-s",0)
                    self.assertEqual((root/"NEIGHBOR001.SRO").read_bytes()[64:],b"Neighbor")
                    neighbor,extents=locations[1];final=archive.read_bytes()
                    for offset,count in [(neighbor,BLOCK),*extents]:self.assertEqual(final[offset:offset+count],image[offset:offset+count])


if __name__=="__main__":unittest.main()
