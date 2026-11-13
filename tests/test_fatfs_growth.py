"""Verify file growth and recovery with actual public FatFs and synthetic media."""
# Adapted from paps-g 2d8ba306.
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

import test_fatfs_seek as support


class FatFsGrowthTests(unittest.TestCase):
    fixture_name = 'fatfs-growth'
    fixture_sources = ('fatfs_growth.c', 'test_fatfs_growth.py', *support.FatFsSeekTests.fixture_sources)
    setUpClass = classmethod(support.FatFsSeekTests.setUpClass.__func__)
    tearDownClass = classmethod(support.FatFsSeekTests.tearDownClass.__func__)

    def case(self, filesystem, layout, operation, fault='read', ordinal=0, close=0, fats=1):
        args = list(map(str, (filesystem, layout, operation, fault, ordinal,
                              close, int(fault == 'write'), fats)))
        with tempfile.TemporaryDirectory(dir=self.target) as temporary:
            image = Path(temporary) / 'card.img'
            with image.open('wb') as stream:
                stream.truncate({'fat12': 1, 'fat16': 16}.get(filesystem, 64) * 1024**2)
            run = subprocess.run([self.binary, image, *args], capture_output=True, text=True,
                                 timeout=45, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(args=args, status=run.returncode, out=run.stdout, err=run.stderr))
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        operations = re.search(r'phase=\d+ reads=(\d+) writes=(\d+) syncs=(\d+)', run.stdout)
        recovery = re.search(r'close_reads=(\d+) close_writes=(\d+) close_syncs=(\d+)', run.stdout)
        self.assertIsNotNone(operations)
        self.assertIsNotNone(recovery)
        return tuple(map(int, operations.groups())), tuple(map(int, recovery.groups()))

    def sweep(self, filesystem, layout, operation, fats=1):
        counts, close_counts = self.case(filesystem, layout, operation, fats=fats)
        for index, fault in enumerate(('read', 'write', 'sync')):
            for ordinal in range(1, counts[index]+1):
                with self.subTest(filesystem=filesystem, layout=layout, operation=operation,
                                  fault=fault, ordinal=ordinal):
                    _, recovery = self.case(filesystem, layout, operation, fault, ordinal, fats=fats)
                for close in range(1, recovery[index]+1):
                    with self.subTest(filesystem=filesystem, layout=layout, operation=operation,
                                      fault=fault, ordinal=ordinal, close=close):
                        self.case(filesystem, layout, operation, fault, ordinal, close, fats)
        for close in range(1, close_counts[2]+1):
            self.case(filesystem, layout, operation, 'sync', close=close, fats=fats)

    def test_original_lost_allocation_witnesses(self):
        for fs, layout, failure, ordinal in (
                ('fat32', 'fragmented', 'read', 2),
                ('exfat', 'contiguous', 'write', 1)):
            with self.subTest(fs=fs, layout=layout, fault='none'):
                self.case(fs, layout, 'write')
            with self.subTest(fs=fs, layout=layout):
                self.case(fs, layout, 'write', failure, ordinal)

    def test_fragmented_write_growth(self):
        for fs in ('fat12', 'fat16', 'fat32', 'exfat'):
            self.sweep(fs, 'fragmented', 'write')

    def test_contiguous_exfat_payload_failure(self):
        for layout in ('contiguous', 'initial', 'prefix'):
            self.sweep('exfat', layout, 'write')

    def test_seek_growth_and_fat12_split_entries(self):
        self.sweep('exfat', 'fragmented', 'seek')
        self.sweep('fat12', 'split-new', 'seek')
        self.sweep('fat12', 'split-tail', 'write')
        self.sweep('fat12', 'initial-split', 'write')

    def test_mirrored_fat_recovery(self):
        self.sweep('fat32', 'fragmented', 'write', fats=2)


class Fat32AppGrowthTests(unittest.TestCase):
    generic_formats = False
    fixture_name = 'fat32-app-growth'
    fixture_sources = FatFsGrowthTests.fixture_sources
    setUpClass = classmethod(support.FatFsSeekTests.setUpClass.__func__)
    tearDownClass = classmethod(support.FatFsSeekTests.tearDownClass.__func__)
    case = FatFsGrowthTests.case
    sweep = FatFsGrowthTests.sweep

    def test_fragmented_write_rollback_and_repeated_failures(self):
        for copies in (1, 2):
            self.case('fat32', 'fragmented', 'write', 'read', 2, fats=copies)
            self.sweep('fat32', 'fragmented', 'write', fats=copies)

    def test_seek_prefix_survives_a_later_failed_allocation(self):
        for copies in (1, 2):
            self.sweep('fat32', 'prefix', 'seek', fats=copies)
