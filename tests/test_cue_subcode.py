"""Actual optional sidecar I/O/conversion on synthetic FatFs media."""
# Adapted from paps-g659fe4ee; no private preparer/device operation.
import unittest
from mcu_save_support import MCUSaveTest

class CueSubcodeTests(MCUSaveTest):
    fixture='cue'
    c_source='cue_subcode.c'
    test_source='test_cue_subcode.py'
    reader_fixture=True
    def test_planar_and_raw_eof_windows_and_conversion_bytes(self):
        for n in (1,2):
            with self.subTest(n=n):self.run_case(['subcode',n])
    def test_enabling_subcodes_replaces_sector_only_cache(self):self.run_case(['toggle',0])
    def test_mode_changes_during_each_io_preserve_one_operation(self):
        for n in range(6):
            with self.subTest(n=n):self.run_case(['mode-change',n])
    def test_failed_clipped_short_and_partial_sidecars_do_not_publish(self):
        for n in range(5):
            with self.subTest(n=n):self.run_case(['fault',n])

    def test_payload_failure_after_valid_sidecodes_preserves_ring_and_output(self):
        for n in (1,2):
            with self.subTest(n=n):self.run_case(['payload-fault',n])

if __name__=='__main__':unittest.main()
