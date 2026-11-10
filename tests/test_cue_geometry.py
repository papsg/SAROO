"""Actual public reader on synthetic FatFs with independent track metadata."""
# Adapted from paps-g659fe4ee; no private preparer or device operation.
import os
from pathlib import Path
import unittest
from mcu_save_support import MCUSaveTest, ROOT

class CueGeometryTests(MCUSaveTest):
    fixture='cue'
    c_source='cue_geometry.c'
    reader_fixture=True
    def test_mixed_width_boundary_payloads_forward_reverse_and_permuted(self):
        for value in range(6):
            with self.subTest(value=value):self.run_case(['boundary',value])
    def test_all_subindices_and_position_only_reporting(self):self.run_case(['reports',0])
    def test_unsigned_high_offsets_and_clipped_seek_refusal(self):self.run_case(['large',0])
    def test_unchanged_single_track_payload_and_warm_cache_control(self):self.run_case(['control',0])
    def test_generated_gap_zero_stride_and_stored_pregap_payloads(self):
        source=Path(os.environ.get('SAROO_MCU_SAVE_DIR',ROOT/'Firm_MCU/Saturn'))
        if b'TRACK_INDEX' not in (source/'cdc.h').read_bytes():self.skipTest('New metadata invariant, absent from old layout')
        self.run_case(['generated',0])

if __name__=='__main__':unittest.main()
