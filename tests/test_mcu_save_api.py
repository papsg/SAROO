"""Actual Saturn API -> actual MCU -> public FatFs; no physical bus emulator."""
import unittest
from mcu_save_support import MCUSaveTest

class MCUSaveAPITests(MCUSaveTest):
    fixture = 'api'

    def invoke(self, mode, dev=1, fault='none', nth=1):
        return self.run_case([mode, dev, fault, nth])

    def test_read_stat_verify_and_delete_propagate_load_failures(self):
        for mode in ('read','stat','verify','delete','new-header'):
            for fault in ('read-error','read-short','read-partial-error','seek-any-error','seek-any-short'):
                for nth in ((1,2,3,4,5) if mode in ('read','verify') else (1,2,3) if mode in ('delete','new-header') else (1,2)):
                    with self.subTest(mode=mode,fault=fault,nth=nth): self.invoke(mode,1,fault,nth)

    def test_incomplete_payload_requires_matching_retry_and_preserves_dirty_window(self):
        for mode in ('new-large','overwrite-large','grow-large','overlap-large'):
            for fault,nth in (('write-error',1),('write-short',1),('sync-error',1),('read-partial-error',4),('read-partial-error',5)):
                with self.subTest(mode=mode,fault=fault,nth=nth): self.invoke(mode,1,fault,nth)

    def test_complete_pending_saves_retry_without_false_success(self):
        for dev in (0,1):
            for mode in ('commit-new','commit-overwrite','commit-resupply','commit-resupply-short','format','delete-commit'):
                for fault in ('write-error','write-short','sync-error','seek-any-error','seek-any-short'):
                    with self.subTest(dev=dev,mode=mode,fault=fault): self.invoke(mode,dev,fault)

    def test_directory_output_is_unchanged_after_any_failed_header(self):
        for nth in (1,2,15,29,33,65):
            for fault in ('read-error','read-short','read-partial-error'):
                with self.subTest(fault=fault,nth=nth): self.invoke('directory-fault',1,fault,nth)

    def test_directory_counts_scratch_boundary_and_warm_cache(self):
        for copies in (0,1,28,29,64,448):
            with self.subTest(copies=copies): self.invoke('directory-count',1,'none',copies)
        self.invoke('directory-warm-small')
        self.invoke('directory-warm-large')

    def test_latched_disk_errors_preserve_state_until_explicit_mcu_file_recovery(self):
        for mode in ('disk-retry-write','disk-retry-read'):
            with self.subTest(mode=mode): self.invoke(mode)

if __name__=='__main__':unittest.main()
