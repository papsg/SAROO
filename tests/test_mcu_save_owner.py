"""Bounded access through the actual primary FIL owners, on FAT32 and exFAT."""
import unittest
from mcu_save_support import MCUSaveTest


class SaveOwnerTests(MCUSaveTest):
    fixture = 'owner'

    def case(self, mode, device):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem, mode=mode, device=device):
                self.assertIn('PASS complete primary archives', self.run_case([filesystem, mode, device]))

    def test_canonical_identity_publishes_only_after_complete_selection(self):
        self.case('identity', 0)

    def test_invalid_extents_and_requests_preserve_archives_and_cache_owners(self):
        for device in (0, 1): self.case('invalid', device)

    def test_exact_reads_preserve_neighbors_and_reuse_existing_owners(self):
        for device in (0, 1): self.case('read', device)

    def test_writes_sync_retry_and_root_reload_preserve_complete_archives(self):
        for device in (0, 1): self.case('write', device)

    def test_partial_failed_writes_revoke_stale_legacy_ownership(self):
        for device in (0, 1):
            for mode in ('write-error', 'write-short', 'seek-any-error', 'seek-any-short'):
                self.case(mode, device)

    def test_failed_direct_root_reload_cannot_publish_partial_metadata(self):
        self.case('root-read-error', 1)

    def test_aborted_owner_recovers_without_creating_missing_archives(self):
        for device in (0, 1):
            for mode in ('reopen', 'missing', 'media'): self.case(mode, device)


if __name__ == '__main__': unittest.main()
