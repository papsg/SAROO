"""Actual CUE/raw parser, loader, unloader and TOC on synthetic public FatFs."""
# Adapted from paps-g 60d5df01; no private preparer/device operation.
import unittest
from mcu_save_support import MCUSaveTest

class CueParserTests(MCUSaveTest):
    fixture = 'cue'
    timeout = 30

    def invoke(self, mode, value=0):
        return self.run_case([mode, value])

    def test_valid_single_and_multiple_files_tracks_and_indices(self):
        for value in range(10):
            with self.subTest(value=value):self.invoke('valid',value)

    def test_rejected_syntax_closes_partial_files_and_publishes_no_tracks(self):
        for value in range(26):
            with self.subTest(value=value):self.invoke('syntax',value)

    def test_preview_checks_complete_read_and_preserves_disc_state(self):
        for value in range(5):
            with self.subTest(value=value):self.invoke('preview',value)

    def test_cue_extent_complete_io_and_embedded_nul(self):
        for value in range(10):
            with self.subTest(value=value):self.invoke('input',value)

    def test_track_subindex_capacity_and_recovery(self):
        for value in (2048,2049):
            with self.subTest(value=value):self.invoke('indices',value)

    def test_path_limits_relative_root_and_quoted_spaces(self):
        for value in range(7):
            with self.subTest(value=value):self.invoke('path',value)

    def test_raw_extent_and_live_handle_ownership(self):
        for value in (0, 1, 2, 3, 4, 7, 8, 9):
            with self.subTest(value=value):self.invoke('raw',value)

    def test_cue_track_file_extent_boundaries(self):
        for value in range(4):
            with self.subTest(value=value):self.invoke('track-extent',value)

    def test_index_requires_a_complete_sector_including_preview(self):
        for value in range(42):
            with self.subTest(value=value):
                self.invoke('indexed-sector', value)

    def test_invalid_mode_prefixes_do_not_publish_tracks(self):
        self.invoke('mode-prefix')

    def test_partial_handle_cleanup_and_subcode_unload(self):
        for value in range(7):
            with self.subTest(value=value):self.invoke('cleanup',value)

    def test_loader_dispatch_and_failed_unload(self):
        for value in range(3):
            with self.subTest(value=value):self.invoke('dispatch',value)

    def test_non_game_cue_and_iso_never_request_a_save(self):
        for value in range(5):
            with self.subTest(value=value): self.invoke('media',value)

    def test_game_first_mixed_disc_and_signed_empty_identity(self):
        for value in (5,6):
            with self.subTest(value=value): self.invoke('media',value)

class GenericCueExtentTests(MCUSaveTest):
    fixture = 'cue'
    generic_formats = True
    timeout = 30

    def test_above_fat32_raw_and_cue_extents_are_rejected(self):
        for mode, value in (('raw', 5), ('raw', 6), ('track-extent', 4)):
            with self.subTest(mode=mode, value=value):
                self.run_case([mode, value])


if __name__ == '__main__': unittest.main()
