"""SAVETOOL-002: resize through the real CLI, then export in fresh processes."""
from pathlib import Path
import struct
import unittest
import zlib

from savetool_support import NativeSaveTest
from test_mems_overwrite import archive_fixture, raw_save, digest, BLOCK, NEW_DATE


class MemsResizeTests(NativeSaveTest):
    def check_resize(self, old_size, new_size, full=False):
        neighbor = b"Neighbor remains intact."*100
        name = b"SAVE_A00001"
        original, locations = archive_fixture([
            (0, name, bytes([7])*old_size), (1, b"SAVE_B00002", neighbor)])
        if full:
            original[16:1024] = bytes([255])*1008
            struct.pack_into(">H", original, 12, 0)
        payload = bytes((i*7+11)%256 for i in range(new_size))
        with self.case() as temp:
            root = Path(temp)
            archive = root / "SS_MEMS.BIN"; archive.write_bytes(original)
            incoming = root / "incoming.SRO"
            replacement = raw_save(b"OTHER_NAME0", payload)
            replacement[32:42] = b"Different!"; replacement[43] = 5
            incoming.write_bytes(replacement)
            self.invoke(root, archive, "-s", 0, "-i", incoming)
            actual = archive.read_bytes()
            self.assertEqual(len(actual), len(original))
            self.assertEqual(actual[BLOCK:8*BLOCK], original[BLOCK:8*BLOCK])
            header = locations[0][0]
            self.assertEqual(actual[header:header+12], original[header:header+12])
            self.assertEqual(actual[header+16:header+28], original[header+16:header+28])
            self.assertEqual(actual[header+28:header+32], NEW_DATE)
            self.assertEqual(struct.unpack_from(">I", actual, header+12)[0], new_size)
            count = 0 if new_size <= 960 else (new_size+1023)//1024
            blocks = [struct.unpack_from(">H", actual, header+64+i*2)[0] for i in range(count)]
            self.assertEqual(len(set(blocks)), count)
            self.assertTrue(all(8 <= b < 8064 and b*BLOCK != header for b in blocks))
            if count:
                self.assertEqual(struct.unpack_from(">H", actual, header+64+count*2)[0], 0)
            old_count = 0 if old_size <= 960 else (old_size+1023)//1024
            allocated = sum(bin(byte).count("1") for byte in actual[16:1024])
            before_allocated = sum(bin(byte).count("1") for byte in original[16:1024])
            self.assertEqual(allocated, before_allocated-old_count+count)
            self.assertEqual(struct.unpack_from(">H", actual, 12)[0], 8064-allocated)
            for b in blocks:
                self.assertTrue(actual[16+b//8] & (1 << (b%8)))
            # Independently allow only the selected size/date/header payload,
            # global allocation bookkeeping and the new selected data blocks.
            before = bytearray(original); after = bytearray(actual)
            for start, size in [(12,2), (16,1008), (header+12,4), (header+28,4),
                                (header+64,960), *[(b*BLOCK, BLOCK) for b in blocks]]:
                before[start:start+size] = after[start:start+size]
            self.assertEqual(digest(before), digest(after), "Mutation outside the replacement's extents")
            self.invoke(root, archive, "-s", 0)
            exported = (root / "SAVE_A00001.SRO").read_bytes()
            self.assertEqual(exported[64:], payload)
            self.assertEqual(exported[16:27], name)
            self.assertEqual(exported[32:42], b"Generated!")
            self.assertEqual(exported[43], 1)
            self.assertEqual(exported[44:48], NEW_DATE)
            self.assertEqual(struct.unpack_from(">I", exported, 12)[0], zlib.crc32(exported[16:]))
            self.invoke(root, archive, "-s", 1)
            self.assertEqual((root / "SAVE_B00002.SRO").read_bytes()[64:], neighbor)
            self.assertEqual(archive.read_bytes(), actual, "Export modified the archive")
            self.assertEqual(incoming.read_bytes(), replacement)

    def test_growth_shrink_inline_transitions_and_maximum(self):
        for old, new in [(32,64), (64,32), (960,961), (961,960), (1024,4097),
                         (4097,1024), (0,2048), (2048,0), (32,479*1024), (479*1024,32)]:
            with self.subTest(old=old,new=new):
                self.check_resize(old,new)

    def test_full_bitmap_reuses_old_blocks_and_supports_inline_changes(self):
        for old,new in [(2048,2047), (4097,1024), (32,64)]:
            with self.subTest(old=old,new=new):
                self.check_resize(old,new,full=True)

    def test_rejected_resize_preserves_every_archive_byte(self):
        for reason in ["full", "oversized", "invalid-chain", "duplicate-chain", "header-as-data", "bad-header"]:
            with self.subTest(reason=reason), self.case() as temp:
                root = Path(temp)
                image, locations = archive_fixture([(0,b"SAVE_A00001",bytes([7])*2048)])
                header = locations[0][0]
                if reason=="full":
                    image[16:1024]=bytes([255])*1008; struct.pack_into(">H",image,12,0)
                elif reason=="invalid-chain": struct.pack_into(">H",image,header+64,8064)
                elif reason=="duplicate-chain": image[header+66:header+68]=image[header+64:header+66]
                elif reason=="header-as-data": struct.pack_into(">H",image,header+64,header//BLOCK)
                elif reason=="bad-header": struct.pack_into(">H",image,BLOCK+14,65535)
                archive=root/"SS_MEMS.BIN";archive.write_bytes(image)
                incoming=root/"incoming.SRO"
                data=raw_save(b"SAVE_A00001",bytes([9])*(480*BLOCK if reason=="oversized" else 4097))
                incoming.write_bytes(data)
                self.invoke(root,archive,"-s",0,"-i",incoming,ok=False)
                self.assertEqual(digest(archive.read_bytes()),digest(image))
                self.assertEqual(incoming.read_bytes(),data)


if __name__ == "__main__":
    unittest.main()
