"""Exercise the App's FAT32-only media and size contract with public FatFs."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_fatfs_seek


class AppMountFixture:
    generic_formats = False
    fixture_name = 'fat32-app'
    fixture_sources = test_fatfs_seek.FatFsSeekTests.fixture_sources
    setUpClass = classmethod(test_fatfs_seek.FatFsSeekTests.setUpClass.__func__)
    tearDownClass = classmethod(test_fatfs_seek.FatFsSeekTests.tearDownClass.__func__)


class Fat32AppTests(AppMountFixture, unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        source = cls.target / 'FatFS'
        cls.formatter = cls.target / 'generic-formatter'
        command = [os.environ.get('CC', 'cc'), '-std=gnu11', '-O2', '-pthread',
                   '-DSAROO_FAT32_ONLY=0', '-I', cls.target, '-I', source,
                   cls.target / 'fatfs_seek.c', source / 'ff.c', source / 'ffunicode.c',
                   '-o', cls.formatter]
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
