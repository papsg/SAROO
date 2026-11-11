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

    def test_canonical_index_lookup_at_page_boundaries(self):
        for record in (0, 2047, 2048, 4095, 4096, 8192, 16383):
            with self.subTest(record=record): self.run_case(['cover-index-boundary', record])

    def test_compact_terminator_extent_and_full_key_checks(self):
        for value in range(6):
            with self.subTest(value=value): self.run_case(['cover-index-layout', value])

    def test_resident_pages_are_reused_until_file_reopen(self):
        self.run_case(['cover-index-cache', 0])

    def test_failed_extension_remains_retryable(self):
        for value in (1, 2, 3, 4, 5, 8):
            with self.subTest(value=value): self.run_case(['cover-index-retry', value])

    def test_extension_preserves_a_busy_maximum_gif_canvas(self):
        self.run_case(['cover-index-busy', 0])

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
