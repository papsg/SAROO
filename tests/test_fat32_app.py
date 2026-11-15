"""Exercise the App's FAT32-only media and size contract with public FatFs."""
import hashlib
import json
import struct
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_fatfs_seek


class AppMountFixture:
    generic_formats = False
    fixture_name = 'fat32-app'
    fixture_sources = (*test_fatfs_seek.FatFsSeekTests.fixture_sources, "test_fat32_app.py")
    setUpClass = classmethod(test_fatfs_seek.FatFsSeekTests.setUpClass.__func__)
    tearDownClass = classmethod(test_fatfs_seek.FatFsSeekTests.tearDownClass.__func__)


class Fat32AppTests(AppMountFixture, unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        source = cls.target / 'FatFS'
        cls.formatter = cls.target / 'generic-formatter'
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE')=='release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [os.environ.get('CC', 'cc'), '-std=gnu11', *flags, '-pthread',
                   '-DSAROO_FAT32_ONLY=0', '-I', cls.target, '-I', source,
                   cls.target / 'fatfs_seek.c', source / 'ff.c', source / 'ffunicode.c',
                   '-o', cls.formatter]
        (cls.target / 'formatter-command.json').write_text(json.dumps(list(map(str,command)),indent=2)+'\n')
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'formatter.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    def run_case(self, binary, card, mode, value=0):
        result = subprocess.run([binary, card, mode, str(value)], capture_output=True, text=True,
                                timeout=45, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(mode=mode, value=value, code=result.returncode,
                                 out=result.stdout, err=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def profile(self, kind, size):
        for partitioned in (0, 1):
            with self.subTest(kind=kind, partitioned=partitioned):
                with tempfile.TemporaryDirectory(dir=self.target) as folder:
                    card = Path(folder) / 'sd.img'
                    with card.open('wb') as out:
                        out.truncate(size * 1024**2)
                    value = kind | (partitioned << 3)
                    self.run_case(self.formatter, card, 'format-profile', value)
                    self.run_case(self.binary, card, 'mount-profile', value)

    def test_fat32_app_reads_writes_and_grows_directories_in_both_layouts(self):
        self.profile(3, 64)

    def test_other_filesystems_refuse_mount_and_write_in_both_layouts(self):
        for kind, size in ((1, 1), (2, 32), (4, 64)):
            self.profile(kind, size)

    def test_full_fat32_file_size_is_representable(self):
        with tempfile.TemporaryDirectory(dir=self.target) as folder:
            card = Path(folder) / 'sd.img'
            with card.open('wb') as out:
                out.truncate(9000000 * 512)
            self.assertLess(card.stat().st_blocks * 512, 1024**2, 'Requires sparse host files')
            self.run_case(self.binary, card, 'max-fat32-file')

    @staticmethod
    def digest(path):
        value=hashlib.sha256()
        with path.open('rb') as stream:
            for block in iter(lambda: stream.read(1024*1024), b''):
                value.update(block)
        return value.hexdigest()

    def partition_fixture(self, name, length, slot=0, kind=3, mib=64, steer=False, alias=False):
        volume=self.target/(name+'-volume.img')
        with volume.open('wb') as out: out.truncate(mib*1024**2)
        self.run_case(self.formatter, volume, 'format-profile', kind)
        start=0xffffffff-length+1 if alias else 2048
        card=self.target/(name+'.img')
        boundary=start+(length if 0<length<=135168 else 135168)
        with volume.open('rb') as source, card.open('w+b') as out:
            out.truncate((volume.stat().st_size+512) if alias else 80*1024**2)
            mbr=bytearray(512); mbr[446+slot*16+4]={1:1,2:6,3:12,4:7}[kind]
            struct.pack_into('<II', mbr, 446+slot*16+8, start, length)
            if not alias:
                index=1 if slot==0 else 0; mbr[446+index*16+4]=0x83
                struct.pack_into('<II', mbr, 446+index*16+8,
                                 boundary if slot==0 else 128, 1024 if slot==0 else 64)
            mbr[510:512]=b'\x55\xaa';out.write(mbr)
            # Retain sparse synthetic media without allocating every zero page.
            offset=512 if alias else start*512
            for block in iter(lambda: source.read(1024*1024), b''):
                if block != bytes(len(block)):
                    out.seek(offset);out.write(block)
                offset+=len(block)
            out.seek(512 if alias else start*512);boot=out.read(512)
            declared=struct.unpack_from('<Q',boot,72)[0] if kind==4 else (
                struct.unpack_from('<H',boot,19)[0] or struct.unpack_from('<I',boot,32)[0])
            if steer:
                data=start+struct.unpack_from('<H',boot,14)[0]+boot[16]*struct.unpack_from('<I',boot,36)[0]
                self.assertTrue(data<boundary<start+declared);self.assertEqual(boot[13],1)
                hint=boundary-data+1
                for info in (1,struct.unpack_from('<H',boot,50)[0]+1):
                    out.seek((start+info)*512+492);out.write(struct.pack('<I',hint))
            if not alias: out.seek(boundary*512);out.write(b'\xa5'*512)
        return card,dict(start=start,length=length,declared=declared,boundary=boundary,slot=slot)

    def extent(self, card, layout, expected, binary=None, alias=False):
        before=self.digest(card)
        with card.open('rb') as stream:
            mbr=stream.read(512)
            if not alias:
                stream.seek(layout['boundary']*512);marker=stream.read(512)
                stream.seek((layout['start']+layout['declared'])*512)
                tail=stream.read(max(0,layout['length']-layout['declared'])*512) if expected==0 else b''
        value=layout['slot'] | (12 if alias else 0)
        result=subprocess.run([binary or self.binary,card,'partition-extent',str(value)],
                              capture_output=True,text=True,timeout=45,
                              env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        row=dict(layout=layout,expected=expected,status=result.returncode,
                 out=result.stdout,err=result.stderr,before=before,after=self.digest(card))
        with card.open('rb') as stream:
            self.assertEqual(stream.read(512),mbr)
            if not alias:
                stream.seek(layout['boundary']*512);after_marker=stream.read(512)
                row['marker_changed']=after_marker!=marker
                row['marker_is_payload']=after_marker==b'\x5a'*512
                stream.seek((layout['start']+layout['declared'])*512)
                if expected==0: self.assertEqual(stream.read(len(tail)),tail)
        self.results.append(row)
        (card.with_suffix('.json')).write_text(json.dumps(row,indent=2)+'\n')
        self.assertEqual(result.returncode,expected,row)
        if expected:
            self.assertIn('mount=13 type=0',result.stdout);self.assertIn('writes=0',result.stdout)
            if layout['length'] in (0,0xffffffff): self.assertIn('vbr_reads=0',result.stdout)
            self.assertEqual(row['before'],row['after'])
        elif alias:
            self.assertIn('readonly writes=0',result.stdout);self.assertEqual(row['before'],row['after'])
        else:
            self.assertIn('outside=0',result.stdout);self.assertFalse(row['marker_changed'])

    def test_mbr_refuses_short_empty_and_wrapping_extents_before_writes(self):
        for length in (100000,0,0xffffffff):
            with self.subTest(length=length):
                card,layout=self.partition_fixture('invalid-'+str(length),length,steer=length==100000)
                self.extent(card,layout,13)

    def test_mbr_accepts_exact_and_larger_partition_in_later_slot(self):
        for length,slot in ((131072,0),(135168,3)):
            with self.subTest(length=length,slot=slot):
                card,layout=self.partition_fixture('valid-'+str(length),length,slot)
                self.assertEqual(layout['declared'],131072)
                self.extent(card,layout,0)

    def test_mbr_skips_rejected_early_extent_and_finds_later_volume(self):
        card,layout=self.partition_fixture('skip-short-first',135168,slot=3)
        with card.open('r+b') as image:
            image.seek(layout['start']*512);boot=image.read(512)
            image.seek(256*512);image.write(boot)
            image.seek(446+4);image.write(b'\x0c')
            image.seek(446+8);image.write(struct.pack('<II',256,1))
        self.extent(card,layout,0)

    def test_mbr_last_sector_at_lba_max_is_representable(self):
        card,layout=self.partition_fixture('inclusive-end',131072,alias=True)
        self.assertEqual(layout['start']+layout['length']-1,0xffffffff)
        self.extent(card,layout,0,alias=True)

    def test_mbr_retained_formats_obey_partition_bounds(self):
        for kind,mib in ((1,1),(2,16),(4,64)):
            for valid in (False,True):
                with self.subTest(kind=kind,valid=valid):
                    length=mib*2048+(128 if valid else -1)
                    card,layout=self.partition_fixture('generic-'+str(kind)+'-'+str(valid),length,kind=kind,mib=mib)
                    self.extent(card,layout,0 if valid else 13,binary=self.formatter)


class Fat32AppStorageTests(AppMountFixture, unittest.TestCase):
    fixture_name = 'fat32-app-storage'
    invoke = test_fatfs_seek.FatFsSeekTests.invoke
    test_mount_fat32_extent_arithmetic_and_valid_controls = test_fatfs_seek.FatFsSeekTests.test_mount_fat32_extent_arithmetic_and_valid_controls
    test_fat32_selects_only_the_active_copy_and_restores_mirroring = test_fatfs_seek.FatFsSeekTests.test_fat32_selects_only_the_active_copy_and_restores_mirroring
    test_fat32_refuses_out_of_range_active_copies = test_fatfs_seek.FatFsSeekTests.test_fat32_refuses_out_of_range_active_copies
    test_fsinfo_failure_remains_retryable_without_blocking_reads = test_fatfs_seek.FatFsSeekTests.test_fsinfo_failure_remains_retryable_without_blocking_reads

    def test_mirrored_and_single_copy_write_recovery(self):
        for case in (0, 1, 2, 4, 5, 6):
            with self.subTest(case=case):
                self.invoke('metadata-fat32', case)

    def test_retained_card_recovers_pending_file_and_path_operations(self):
        for value in (1, 3):
            with self.subTest(value=value):
                self.invoke('retained-media', value)
