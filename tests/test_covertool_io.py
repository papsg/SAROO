"""Publication failure injection for the actual native cover utility."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import test_covertool as fixtures


class CoverToolIOTests(fixtures.CoverToolBuild):
    faults = True

    def run_fault(self, mode, fault, succeeds=False, count=1, image_published=False):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp);data, palette, pixels = fixtures.bmp(4, 4)
            filename = fixtures.name()
            if mode == 'pack':
                (root / filename).write_bytes(data)
                (root / 'input.txt').write_text(filename + '\n')
                args = ['-p', 'input.txt'];target = root / 'cover.bin'
                expected = fixtures.library([(b'GAME', 0x12345678, 4, 4, palette, pixels)])
            else:
                entries = [(b'GAME', 0x12345678, 4, 4, palette, pixels)]
                if count == 2:
                    entries.append((b'SECOND', 0x12345678, 4, 4, palette, pixels))
                    (root / filename).write_bytes(fixtures.ORIGINAL)
                    filename = fixtures.name('SECOND')
                (root / 'input.bin').write_bytes(fixtures.library(entries))
                args = ['-u', 'input.bin'];target = root / filename;expected = data
                (root / 'coverlist.txt').write_bytes(fixtures.ORIGINAL)
            target.write_bytes(fixtures.ORIGINAL)
            result = subprocess.run([str(self.binary), *args], cwd=root, capture_output=True, timeout=15,
                                    env={'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1',
                                         'SAROO_COVER_FAULT': fault, 'SAROO_COVER_STATS': str(root / 'stats.json')})
            stats = json.loads((root / 'stats.json').read_text())
            self.results.append(dict(mode=mode, fault=fault, count=count, returncode=result.returncode,
                                     stderr=result.stderr.decode(errors='replace'), stats=stats))
            self.assertEqual(stats['triggered'], 1, (fault, result.stderr))
            self.assertEqual(stats['heap_live'], 0)
            self.assertNotIn(b'AddressSanitizer', result.stderr)
            self.assertNotIn(b'runtime error:', result.stderr)
            self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)
            self.assertEqual(list(root.glob('.saroo-cover-*')), [])
            if not succeeds and fault != 'sync-directory' and not image_published:
                self.assertEqual(target.read_bytes(), fixtures.ORIGINAL)
            else:
                self.assertEqual(target.read_bytes(), expected)
            if mode == 'unpack':
                manifest = (filename + '\n').encode() if succeeds else fixtures.ORIGINAL
                self.assertEqual((root / 'coverlist.txt').read_bytes(), manifest)
                if count == 2:
                    self.assertEqual((root / fixtures.name()).read_bytes(), data)
            if fault == 'sync-directory': self.assertIn(b'Replaced ', result.stderr)

    def test_pack_prepublication_failures_preserve_existing_file(self):
        faults = ['create', 'truncate', 'alloc', 'alloc:2', 'read-error', 'read-zero',
                  'write-error', 'write-error:2', 'write-zero', 'close-input:2',
                  'fclose', 'sync-file', 'close-temp', 'rename']
        if sys.platform == 'darwin': faults.append('metadata')
        for fault in faults:
            with self.subTest(fault=fault): self.run_fault('pack', fault)

    def test_export_failures_never_publish_partial_images(self):
        faults = ['alloc', 'create', 'create:2', 'read-error', 'read-error:4', 'read-zero',
                  'write-error', 'write-zero', 'sync-file', 'close-temp', 'rename']
        if sys.platform == 'darwin': faults.append('metadata:2')
        for fault in faults:
            with self.subTest(fault=fault): self.run_fault('unpack', fault)
        # A later failure leaves earlier complete images, but never advertises
        # an incomplete set through a newly published manifest.
        self.run_fault('unpack', 'rename:2', count=2)
        # Source close fails after a complete BMP was published. Keep the old
        # manifest and report failure; do not pretend the BMP was rolled back.
        self.run_fault('unpack', 'close-input:4', image_published=True)

    def test_short_and_interrupted_syscalls_finish_complete_outputs(self):
        for mode in ['pack', 'unpack']:
            for fault in ['read-short', 'read-intr', 'write-short', 'write-intr']:
                with self.subTest(mode=mode, fault=fault): self.run_fault(mode, fault, succeeds=True)

    def test_directory_sync_failure_reports_completed_replacement(self):
        for mode in ['pack', 'unpack']:
            with self.subTest(mode=mode): self.run_fault(mode, 'sync-directory')


if __name__ == '__main__':
    unittest.main()
