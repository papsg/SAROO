"""Actual firmware resize/allocation behavior with synchronous host storage."""
import unittest
from firmware_save_support import FirmwareSaveTest


class FirmwareResizeTests(FirmwareSaveTest):
    fixture = "firmware_save_resize"

    def test_resizing_survives_reload_for_both_backup_devices(self):
        cases = [(dev, before, after) for dev in (0, 1)
                 for before, after in ((32, 64), (64, 32), (0, 2048), (2048, 0),
                                       (960, 961), (961, 960), (128, 4097), (4097, 128))]
        cases += [(1, 32, 479 * 1024), (1, 479 * 1024, 32), (0, 32, 508 * 128),
                  (1, 960, 960), (1, 64 * 1024, 64 * 1024)]
        for dev, before, after in cases:
            with self.subTest(device=dev, before=before, after=after):
                self.invoke(dev, before, after)

    def test_metadata_only_overwrites_preserve_other_archive_bytes(self):
        for dev in (0, 1):
            with self.subTest(device=dev):
                self.invoke('metadata', dev, '')

    def test_extended_backup_reports_consistent_geometry(self):
        self.invoke('geometry', 0, '')

    def test_last_cartridge_block_accepts_only_inline_payloads(self):
        for size in (0, 1, 960, 961):
            with self.subTest(size=size):
                self.invoke('last-block', size, '')

    def test_cartridge_directory_counts_all_allocated_blocks(self):
        for size in (0, 1, 960, 961, 1024, 1025):
            with self.subTest(size=size):
                self.invoke('directory-blocks', size, '')

    def test_rejected_writes_preserve_persistent_state(self):
        cases = [(dev, reason) for dev in (0, 1)
                 for reason in ('full', 'oversized', 'new-oversized', 'bad-chain', 'protected')]
        cases.append((1, 'directory-full'))
        for dev, reason in cases:
            with self.subTest(device=dev, reason=reason):
                self.invoke('reject', dev, reason)


if __name__ == "__main__":
    unittest.main()
