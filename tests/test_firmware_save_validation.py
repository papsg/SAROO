"""Actual-module malformed metadata, valid API and blank-format policy controls."""
import unittest
from firmware_save_support import FirmwareSaveTest

COMMON = ('magic0', 'magic1', 'total-size', 'unreserved', 'unallocated-header',
          'unallocated-data', 'huge-size', 'oversized', 'empty-name', 'self-data',
          'neighbor-header', 'shared-data', 'missing-data', 'header-oob', 'header-max')
SLOT = ('header-cycle', 'header-self-cycle', 'next-oob', 'block-size-zero',
        'block-size-wrong', 'zero-size-with-data')
MEMS = ('header-reserved', 'header-zero', 'header-duplicate', 'header-name-mismatch',
        'duplicate-data', 'data-oob', 'data-max', 'data-reserved', 'bad-terminator',
        'too-few-blocks', 'too-many-blocks')

class FirmwareValidationTests(FirmwareSaveTest):
    fixture = "firmware_save_validation"
    timeout = 2

    def test_malformed_storage_rejected_without_output_or_persistent_mutation(self):
        for dev in (0, 1):
            for reason in COMMON + (MEMS if dev else SLOT):
                for operation in ('read', 'verify', 'delete', 'dir', 'stat', 'overwrite', 'new'):
                    with self.subTest(dev=dev, reason=reason, operation=operation):
                        self.invoke('corrupt', dev, reason, operation)

    def test_stale_counts_and_exhaustion_preserve_valid_saves(self):
        for dev in (0, 1):
            for reason in ('low', 'high'):
                with self.subTest(dev=dev, reason=reason): self.invoke('stale', dev, reason)
            with self.subTest(dev=dev, reason='full'): self.invoke('full', dev)
        output = self.invoke('maximum', 1)
        self.assertIn('stat=7 read=8; header-window loads=7', output)

    def test_status_capacity_honors_payload_and_directory_limits(self):
        for dev in (0, 1):
            with self.subTest(dev=dev): self.invoke('statistics', dev)

    def test_mems_complete_stored_identity_and_equal_raw_padding(self):
        for operation in ('read', 'verify', 'delete', 'dir', 'stat', 'overwrite', 'new'):
            with self.subTest(operation=operation):
                self.invoke('corrupt', 1, 'header-name-after-nul', operation)
        self.invoke('equal-mems-fields', 1)

    def test_names_and_api_boundaries(self):
        for dev in (0, 1):
            for reason in ('exact', 'short'):
                with self.subTest(dev=dev, reason=reason): self.invoke('names', dev, reason)
            with self.subTest(dev=dev, reason='fixed'): self.invoke('fixed', dev)
            with self.subTest(dev=dev, reason='arguments'): self.invoke('arguments', dev)

    def test_initialization_preserves_damaged_storage_and_formats_blank_metadata(self):
        for reason in ('damaged', 'blank-zero', 'blank-ff'):
            with self.subTest(reason=reason): self.invoke('init', reason)

if __name__ == "__main__":
    unittest.main()
