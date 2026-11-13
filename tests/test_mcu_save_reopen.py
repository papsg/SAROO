"""Actual save commands retry aborted FatFs objects on generated volumes."""
# Adapted from paps-g d5cb72ea. These tests use host image files.
import unittest

from mcu_save_support import MCUSaveTest


class MCUSaveReopenTests(MCUSaveTest):
    generic_formats = True
    fixture = 'reopen'

    def check_cases(self, modes, kinds):
        for filesystem in ('fat32', 'exfat'):
            for mode in modes:
                for kind in kinds:
                    with self.subTest(filesystem=filesystem, mode=mode, kind=kind):
                        self.run_case([filesystem, mode, kind])

    def test_ordinary_load_recovers_after_latched_disk_error(self):
        self.check_cases(('read',), range(3))

    def test_failed_write_and_close_preserve_the_pending_save(self):
        self.check_cases(('write', 'close-failure'), (0, 1))

    def test_incomplete_append_keeps_existing_slots_and_can_retry(self):
        self.check_cases(('append',), (0,))

    def test_failed_open_and_missing_file_are_retryable_without_creation(self):
        self.check_cases(('open-failure', 'missing'), (0, 1))

    def test_replacement_signature_size_and_generation_are_rejected(self):
        self.check_cases(('replaced', 'size', 'signature', 'generation'), (0, 1))

    def test_header_read_failure_keeps_the_original_identity_for_retry(self):
        self.check_cases(('header-io',), (0, 1))

    def test_revoked_media_retries_healthy_and_aborted_handles(self):
        self.check_cases(('media', 'media-unlatched'), (0, 1))

    def test_structural_error_stays_sticky(self):
        self.check_cases(('structural',), (0, 1))

    def test_replacement_refusal_preserves_the_pending_buffer_and_both_files(self):
        self.check_cases(('write-replaced',), (0, 1))


if __name__ == '__main__':
    unittest.main()
