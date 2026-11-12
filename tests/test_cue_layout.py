"""Actual parser/reader on real synthetic FatFs files and independent bytes."""
# Adapted from paps-g659fe4ee; source-only, no hardware operation.
import unittest
from mcu_save_support import MCUSaveTest

class CueLayoutTests(MCUSaveTest):
    fixture='cue'
    c_source='cue_layout.c'
    test_source='test_cue_layout.py'
    reader_fixture=True
    def test_mixed_width_file_positions_and_payloads(self):
        for n in range(6):
            with self.subTest(n=n):self.run_case(['mixed',n])
    def test_stored_generated_gaps_and_multiple_files(self):self.run_case(['gaps',0])
    def test_first_index_one_preserves_offset_at_fad150(self):
        for n in (0,2,150,300):
            with self.subTest(n=n):self.run_case(['first',n])
    def test_duplicate_misordered_overflowing_gaps_are_rejected(self):
        for n in range(13):
            with self.subTest(n=n):self.run_case(['bad-gap',n])
    def test_full_index_capacity_overflow_and_preview_preservation(self):
        for n in range(5):
            with self.subTest(n=n):self.run_case(['gap-pool',n])
    def test_existing_99_track_read_control(self):self.run_case(['tracks',0])
    def test_full_width_indices_reach_parser_reader_and_position_lookup(self):
        for n in range(3):
            with self.subTest(n=n):self.run_case(['high-indices',n])

if __name__=='__main__':unittest.main()
