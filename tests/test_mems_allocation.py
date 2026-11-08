"""SAVETOOL-004: admit complete new saves before changing archive bytes."""
from pathlib import Path
import struct
import unittest
import zlib

from savetool_support import NativeSaveTest
from test_mems_overwrite import archive_fixture, raw_save, digest, BLOCK, NEW_DATE
from save_fixtures import bup_save


class MemsAllocationTests(NativeSaveTest):
    def check_new(self, size, layout="empty", stale_count=False, bup=False):
        entries=[]
        if layout in ["gap","fragmented"]:
            entries=[(0,b"OLD00000000",b"Preserve first"),(2,b"OLD00000002",b"Preserve second"*100)]
        elif layout=="last":entries=[(i,f"OLD{i:08d}".encode(),b"Preserve") for i in range(447)]
        image,locations=archive_fixture(entries)
        slot=1 if layout in ["gap","fragmented"] else 447 if layout=="last" else 0
        if layout=="fragmented":
            image[16:1024]=bytes([255])*1008
            for block in [100,503,8063]:image[16+block//8]&=~(1<<(block%8))
            struct.pack_into(">H",image,12,3)
        if stale_count:struct.pack_into(">H",image,12,0)
        payload=bytes((i*13+7)%256 for i in range(size))
        with self.case() as temp:
            root=Path(temp);archive=root/"SS_MEMS.BIN";archive.write_bytes(image)
            incoming=root/"incoming.save";data=(bup_save if bup else raw_save)(b"NEW00000000",payload);incoming.write_bytes(data)
            self.invoke(root,archive,"-i",incoming)
            actual=archive.read_bytes();self.assertEqual(len(actual),len(image))
            directory=BLOCK+slot*16
            self.assertEqual(actual[directory:directory+11],b"NEW00000000")
            self.assertEqual(actual[directory+11:directory+14],bytes(3))
            header_id=struct.unpack_from(">H",actual,directory+14)[0]
            self.assertTrue(8<=header_id<8064)
            header=header_id*BLOCK
            self.assertEqual(struct.unpack_from(">I",actual,header+12)[0],size)
            self.assertEqual(actual[header+28:header+32],NEW_DATE)
            needed=0 if size<=960 else (size+1023)//1024
            blocks=[header_id]+[struct.unpack_from(">H",actual,header+64+i*2)[0] for i in range(needed)]
            if needed:self.assertEqual(actual[header+64+needed*2:header+66+needed*2],bytes(2))
            self.assertEqual(len(set(blocks)),len(blocks))
            bitmap=bytearray(image[16:1024])
            for block in blocks:
                self.assertTrue(8<=block<8064)
                self.assertFalse(bitmap[block//8]&(1<<(block%8)))
                bitmap[block//8]|=1<<(block%8)
            self.assertEqual(actual[16:1024],bitmap)
            self.assertEqual(struct.unpack_from(">H",actual,12)[0],8064-sum(bin(b).count("1") for b in bitmap))
            if layout=="fragmented":self.assertEqual(blocks,[100,503,8063])
            restored=bytearray(actual)
            for offset,count in [(12,2),(16,1008),(directory,16),*[(b*BLOCK,BLOCK) for b in blocks]]:
                restored[offset:offset+count]=image[offset:offset+count]
            self.assertEqual(digest(restored),digest(image),"Writes outside the new allocation")
            for old_header,extents in locations:
                for offset,count in [(old_header,BLOCK),*extents]:
                    self.assertEqual(actual[offset:offset+count],image[offset:offset+count])
            index=sum(old_slot<slot for old_slot,_,_ in entries)
            self.invoke(root,archive,"-s",index)
            exported=(root/"NEW00000000.SRO").read_bytes()
            self.assertEqual(exported[64:],payload)
            self.assertEqual(exported[44:48],NEW_DATE)
            self.assertEqual(struct.unpack_from(">I",exported,12)[0],zlib.crc32(exported[16:]))
            self.assertEqual(incoming.read_bytes(),data)
            self.assertEqual(archive.read_bytes(),actual)

    def test_sizes_through_complete_maximum_table(self):
        for size in [0,1,960,961,1024,4097,479*BLOCK]:
            with self.subTest(size=size):self.check_new(size)

    def test_directory_hole_and_final_entry(self):
        for layout in ["gap","last"]:
            with self.subTest(layout=layout):self.check_new(961,layout)

    def test_bup_imports_zero_and_maximum_payloads(self):
        for size in [0,479*BLOCK]:
            with self.subTest(size=size):self.check_new(size,bup=True)

    def test_actual_bitmap_overrides_stale_count_and_supports_fragmentation(self):
        self.check_new(2048,"gap",stale_count=True)
        self.check_new(2048,"fragmented")

    def test_capacity_and_name_refusals_preserve_complete_archive(self):
        cases=[("directory-full",32),("oversized",480*BLOCK),("empty-name",32),
               ("insufficient",2048),("stale-high",2048),("only-reserved-free",0)]
        cases += [("bitmap-full",size) for size in [0,1,32,960,961]]
        for reason,size in cases:
            with self.subTest(reason=reason,size=size),self.case() as temp:
                root=Path(temp)
                entries=[(i,f"OLD{i:08d}".encode(),b"Keep") for i in range(448)] if reason=="directory-full" else [(0,b"OLD00000000",b"Keep")]
                image,_=archive_fixture(entries)
                if reason in ["insufficient","stale-high","bitmap-full","only-reserved-free"]:
                    image[16:1024]=bytes([255])*1008;free_count=0
                    if reason=="insufficient":
                        for block in [8062,8063]:image[16+block//8]&=~(1<<(block%8))
                        free_count=2
                    elif reason=="stale-high":free_count=100
                    elif reason=="only-reserved-free":image[16]=0;free_count=8
                    struct.pack_into(">H",image,12,free_count)
                archive=root/"SS_MEMS.BIN";archive.write_bytes(image)
                data=raw_save(b"" if reason=="empty-name" else b"NEW00000000",bytes(size))
                incoming=root/"incoming.SRO";incoming.write_bytes(data)
                self.invoke(root,archive,"-i",incoming,ok=False)
                self.assertEqual(digest(archive.read_bytes()),digest(image))
                self.assertEqual(incoming.read_bytes(),data)


if __name__=="__main__":unittest.main()
