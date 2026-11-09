"""Actual cover/cache/catalog/display C with real FatFs on synthetic media.

Adapted from paps-g 681387bc; no devices or private build framework.
"""
import unittest
from mcu_save_support import MCUSaveTest


class CoverSafetyTests(MCUSaveTest):
    fixture = 'cover'
    timeout = 120

    def test_complete_palette_pixels_cache_hit_and_forced_reload(self):
        for value in range(5):
            with self.subTest(value=value): self.run_case(['cover-valid', value])

    def test_header_index_and_field_boundaries(self):
        for value in range(13):
            with self.subTest(value=value): self.run_case(['cover-header', value])

    def test_invalid_selection_category_and_count_do_not_read_or_index(self):
        for value in range(11):
            with self.subTest(value=value): self.run_case(['cover-index', value])

    def test_all_categories_and_same_count_rescans_reset_cache_ownership(self):
        for value in range(7):
            with self.subTest(value=value): self.run_case(['cover-catalog', value])

    def test_file_initialization_is_complete_and_retains_failed_close_ownership(self):
        for value in range(7):
            with self.subTest(value=value): self.run_case(['cover-init', value])

    def test_failed_payload_and_preview_reads_remain_retryable(self):
        for value in range(8):
            with self.subTest(value=value): self.run_case(['cover-fault', value])

    def test_busy_consumer_preserves_its_data_and_has_bounded_wait(self):
        for value in range(4):
            with self.subTest(value=value): self.run_case(['cover-busy', value])


if __name__ == '__main__':
    unittest.main()
