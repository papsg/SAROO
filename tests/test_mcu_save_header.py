"""Actual header/preview readers on generated public FAT32 files."""
# Adapted from paps-g cb3abf06; final-sector control from 8a98f5f6.
import unittest
from mcu_save_support import MCUSaveTest

class DiscHeaderTests(MCUSaveTest):
    fixture='header'
    timeout=30
    def invoke(self, mode, value=0):return self.run_case([mode,value])

    def test_game_id_uses_declared_offset_for_each_sector_format(self):
        for value in range(12):
            with self.subTest(value=value):self.invoke('header-full',value)
    def test_preview_returns_all_user_header_bytes_without_changing_live_disc(self):
        for value in range(12):
            with self.subTest(value=value):self.invoke('header-preview',value)
    def test_failed_or_clipped_id_reads_preserve_output_and_reader_ownership(self):
        for value in range(10):
            with self.subTest(value=value):self.invoke('header-fault',value)
    def test_preview_errors_are_negative_and_close_only_preview_handles(self):
        for value in range(1,16):
            with self.subTest(value=value):self.invoke('cue-preview-fault',value)
        for value in (0,1,2,3,4,5,9,10,11,12,13,14):
            with self.subTest(iso=value):self.invoke('iso-preview-fault',value)
    def test_final_complete_indexed_sector_is_a_valid_header(self):
        self.invoke('header-boundary')
    def test_header_failure_unloads_candidate_before_returning(self):
        for value in range(5):
            with self.subTest(value=value):self.invoke('load-header-fault',value)

if __name__=='__main__':unittest.main()
