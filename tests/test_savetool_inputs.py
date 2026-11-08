"""SAVETOOL-003: real CLI format bounds, successful interchange and loader faults."""
import hashlib
import os
from pathlib import Path
import shlex
import struct
import subprocess
import unittest
import zlib

from savetool_support import NativeSaveTest
from save_fixtures import raw_save, bup_save, mems_fixture, saturn_fixture, disk_bytes


def file_digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1048576), b""):
            result.update(data)
    return result.hexdigest()


class SaveInputTests(NativeSaveTest):
    def test_bad_incoming_files_leave_existing_archive_unchanged(self):
        cases = {"empty": b"", "one-byte": b"S", "unknown": bytes(64)}
        for kind, make, size_offset in [("SRO", raw_save, 28), ("BUP", bup_save, 44)]:
            good = make(b"SAVE_A00001", b"A"*32)
            for length in [4,8,16,63]: cases[f"{kind}-short-{length}"] = good[:length]
            for size in [0,31,33,0xffffffff]:
                data = bytearray(good); struct.pack_into(">I",data,size_offset,size)
                cases[f"{kind}-declared-{size}"] = data
            cases[kind+"-oversized"] = make(b"SAVE_A00001",bytes(1048577))
        bad_crc = raw_save(b"SAVE_A00001",b"A"*32);struct.pack_into(">I",bad_crc,12,1)
        cases["SRO-bad-CRC"] = bad_crc
        image,_ = mems_fixture([(0,b"SAVE_A00001",b"Old payload"),(1,b"NEIGHBOR001",b"Unchanged neighbor")])
        expected = hashlib.sha256(image).hexdigest()
        for name,data in cases.items():
            with self.subTest(name=name),self.case() as temp:
                root=Path(temp);archive=root/"SS_MEMS.BIN";archive.write_bytes(image)
                incoming=root/"incoming.save";incoming.write_bytes(data)
                self.invoke(root,archive,"-s",0,"-i",incoming,ok=False)
                self.assertEqual(file_digest(archive),expected)
                self.assertEqual(incoming.read_bytes(),data)
                self.assertEqual(sorted(p.name for p in root.iterdir()),["SS_MEMS.BIN","incoming.save"])

    def test_valid_headers_and_high_bit_checksums_round_trip(self):
        for kind in ["SRO", "SRO-CRC", "SRO-reserved", "BUP"]:
            for size in [0,32,961]:
                with self.subTest(kind=kind,size=size),self.case() as temp:
                    root=Path(temp);archive=root/"SS_MEMS.BIN"
                    image,_=mems_fixture([(0,b"SAVE_A00001",b"Old payload"),(1,b"NEIGHBOR001",b"Neighbor")])
                    trailer=b"Opaque MEMS trailer must survive" if kind=="SRO" and size==32 else b""
                    archive.write_bytes(image+trailer)
                    payload=bytes((i*7+9)%256 for i in range(size))
                    incoming=(bup_save if kind=="BUP" else raw_save)(b"SAVE_A00001",payload)
                    if kind=="SRO-reserved": incoming[8:12]=b"ABCD"
                    if kind=="SRO-CRC":
                        for stamp in range(256):
                            incoming[47]=stamp; crc=zlib.crc32(incoming[16:])
                            if crc&0x80000000:break
                        self.assertTrue(crc&0x80000000);struct.pack_into(">I",incoming,12,crc)
                    path=root/"incoming.save";path.write_bytes(incoming)
                    self.invoke(root,archive,"-s",0,"-i",path)
                    self.invoke(root,archive,"-x",0)
                    bup=(root/"SAVE_A00001.BUP").read_bytes()
                    self.assertEqual(bup[64:],payload)
                    self.assertEqual(struct.unpack_from(">I",bup,44)[0],size)
                    self.assertEqual(bup[40:44],incoming[40:44] if kind=="BUP" else incoming[44:48])
                    self.invoke(root,archive,"-s",0,"-i",root/"SAVE_A00001.BUP")
                    self.invoke(root,archive,"-s",0)
                    sro=(root/"SAVE_A00001.SRO").read_bytes()
                    self.assertEqual(sro[64:],payload)
                    self.assertEqual(struct.unpack_from(">I",sro,12)[0],zlib.crc32(sro[16:]))
                    self.invoke(root,archive,"-s",1)
                    self.assertEqual((root/"NEIGHBOR001.SRO").read_bytes()[64:],b"Neighbor")
                    self.assertEqual(path.read_bytes(),incoming)
                    self.assertEqual(archive.stat().st_size,len(image)+len(trailer))
                    if trailer:self.assertEqual(archive.read_bytes()[-len(trailer):],trailer)

    def test_invalid_archive_extents_and_signatures_are_rejected(self):
        plain=saturn_fixture([])[0];odd=disk_bytes(plain,True)
        cases={"empty":b"","one-byte":b"S","unknown":bytes(64)}
        for kind,magic,size in [("MEMS",b"SaroMems",8388608),("SAROO",b"Saroo Save File\0",65536),
                                ("Saturn",plain[:64],32768),("odd",odd[:128],65536)]:
            for length in [len(magic),1024,size-1]:cases[f"{kind}-short-{length}"]=magic+bytes(length-len(magic))
            if kind!="MEMS":cases[kind+"-extra"]=magic+bytes(size+1-len(magic))
        corrupt=bytearray(odd);corrupt[31]^=1;cases["odd-partial-magic"]=corrupt
        for name,data in cases.items():
            with self.subTest(name=name),self.case() as temp:
                root=Path(temp);archive=root/"archive.bin";archive.write_bytes(data)
                self.invoke(root,archive,ok=False)
                self.assertEqual(file_digest(archive),hashlib.sha256(data).hexdigest())
                self.assertEqual([p.name for p in root.iterdir()],["archive.bin"])

    def test_complete_empty_archives_and_full_saroo_extent_remain_read_only(self):
        plain=saturn_fixture([])[0]
        saroo=bytearray(65536);saroo[:16]=b"Saroo Save File\0"
        mems=mems_fixture([])[0]
        for name,data in [("SAROO",saroo),("MEMS",mems),("MEMS-trailer",mems+b"Opaque trailer stays intact"),
                          ("Saturn",plain),("odd",disk_bytes(plain,True))]:
            with self.subTest(name=name),self.case() as temp:
                root=Path(temp);archive=root/"archive.bin";archive.write_bytes(data)
                self.invoke(root,archive)
                self.assertEqual(file_digest(archive),hashlib.sha256(data).hexdigest())
        with self.case() as temp:
            root=Path(temp);archive=root/"full-saroo.bin"
            with archive.open("wb") as stream:
                stream.write(saroo);stream.truncate(4096*65536)
            before=file_digest(archive);self.invoke(root,archive)
            self.assertEqual(archive.stat().st_size,268435456)
            self.assertEqual(file_digest(archive),before)
            # The SAROO format cannot address another unit after its full index.
            with archive.open("r+b") as stream:stream.truncate(4097*65536)
            before=file_digest(archive);self.invoke(root,archive,ok=False)
            self.assertEqual(file_digest(archive),before)

    def test_actual_loader_checks_io_failures_and_releases_ownership(self):
        with self.case() as temp:
            root=Path(temp);binary=root/"loader";sample=root/"sample.bin"
            sample.write_bytes(b"loader bytes\n")
            command=[*shlex.split(os.environ.get("CC","cc")),"-std=gnu11","-O1","-g",
                     "-fsanitize=address,undefined","-fno-omit-frame-pointer","-fno-strict-aliasing",
                     "-I",str(self.source),str(Path(__file__).with_name("savetool_loader.c")),
                     *[str(self.source/name) for name in ["sr_bup.c","sr_mems.c","ss_bup.c"]],"-o",str(binary)]
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for mode in range(12):
                with self.subTest(mode=mode):
                    check=subprocess.run([str(binary),str(sample),str(mode)],capture_output=True,text=True,timeout=10,
                                         env={**os.environ,"UBSAN_OPTIONS":"halt_on_error=1","ASAN_OPTIONS":"detect_leaks=0"})
                    self.assertEqual(check.returncode,0,check.stdout+check.stderr)
                    self.assertNotIn("runtime error:",check.stderr)
                    self.assertNotIn("AddressSanitizer",check.stderr)
            for mode,make in [(12,raw_save),(13,bup_save)]:
                with self.subTest(maximum_payload=mode):
                    path=root/"maximum.save";path.write_bytes(make(b"SAVE_A00001",bytes([0xA5])*1048576))
                    check=subprocess.run([str(binary),str(path),str(mode)],capture_output=True,text=True,timeout=10,
                                         env={**os.environ,"UBSAN_OPTIONS":"halt_on_error=1","ASAN_OPTIONS":"detect_leaks=0"})
                    self.assertEqual(check.returncode,0,check.stdout+check.stderr)
                    self.assertNotIn("AddressSanitizer",check.stderr)
            self.assertEqual(sample.read_bytes(),b"loader bytes\n")


if __name__=="__main__":unittest.main()
