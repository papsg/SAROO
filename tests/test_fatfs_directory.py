"""Inject sector and sync failures into the actual FatFs directory operations."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
import test_fatfs_seek as support


class FatFsDirectoryTests(unittest.TestCase):
    fixture_name = 'fatfs-directory'
    fixture_sources = ('fatfs_directory.c', 'test_fatfs_directory.py', *support.FatFsSeekTests.fixture_sources)
    setUpClass = classmethod(support.FatFsSeekTests.setUpClass.__func__)
    tearDownClass = classmethod(support.FatFsSeekTests.tearDownClass.__func__)

    def case(self, layout, cluster, fault='none', ordinal=0):
        args = [layout, str(cluster), fault, str(ordinal)]
        with tempfile.TemporaryDirectory(dir=self.target) as temp:
            image = Path(temp) / 'card.img'
            size = {'fat12': 1, 'fat16': 16}.get(layout, 64)
            with image.open('wb') as out:
                out.truncate(size * 1024**2)
            result = subprocess.run([self.binary, image, *args], capture_output=True, text=True,
                                    timeout=30, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(args=args, status=result.returncode, out=result.stdout, err=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        counts = re.search(r'operation I/O: (\d+) (\d+) (\d+)', result.stdout)
        self.assertIsNotNone(counts)
        return tuple(map(int, counts.groups()))

    def sweep(self, layout):
        for cluster in (1, 4):
            counts = None
            with self.subTest(cluster=cluster, fault='none'):
                counts = self.case(layout, cluster)
            if not counts:
                continue
            for fault, maximum in zip(('read', 'write', 'sync'), counts):
                for ordinal in range(1, maximum + 1):
                    with self.subTest(cluster=cluster, fault=fault, ordinal=ordinal):
                        self.case(layout, cluster, fault, ordinal)

    def test_exfat_multi_growth_and_split_parent_headers(self):
        self.sweep('growth')

    def test_exfat_reused_entry_slots_keep_later_files(self):
        self.sweep('hole')

    def test_exfat_mkdir_does_not_orphan_child_allocation(self):
        self.sweep('mkdir')

    def test_exfat_rename_keeps_one_name_and_retains_data(self):
        self.sweep('rename')

    def test_unavailable_volume_retains_pending_directory_owner(self):
        self.case('unavailable', 1, 'write', 1)

    def test_classic_directory_extension_clears_before_linking(self):
        for layout in ('fat12', 'fat16', 'fat32'):
            for fault in ('none', 'clear'):
                with self.subTest(layout=layout, fault=fault):
                    self.case(layout, 1, fault)
