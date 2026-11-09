"""Actual runtime/caller code on public FatFs images with test-only file faults."""
import unittest
from mcu_save_support import MCUSaveTest

class MCURuntimeTests(MCUSaveTest):
    fixture = 'runtime'

    def invoke(self, mode, value=0, fault='none', nth=1):
        return self.run_case([mode, value, fault, nth], 1048576 if mode.startswith('large-') else 131072)

    def test_slot_identity_holes_and_initialization(self):
        for mode in ('slot-first', 'slot-after-hole', 'slot-create', 'slot-append',
                     'slot-short-id', 'slot-append-tail', 'slot-duplicate', 'slot-unbacked', 'slot-empty-id',
                     'slot-null-id', 'slot-unindexed-tail', 'slot-bad-signature', 'slot-signature-terminator', 'slot-not-loaded'):
            with self.subTest(mode=mode): self.invoke(mode)

    def test_last_slot_and_full_directory_on_large_real_filesystem(self):
        for mode in ('large-last-new', 'large-last-existing', 'large-full'):
            with self.subTest(mode=mode): self.invoke(mode)

    def test_runtime_sequences_hold_service_lock(self):
        self.invoke('locking')

    def test_real_disk_errors_stay_reported_until_file_recovery(self):
        for value in (0,1,2):
            with self.subTest(value=value): self.invoke('disk-read-fault',value)

    def test_slot_read_errors_invalidate_ownership_and_allow_retry(self):
        for fault in ('read-error', 'read-short', 'read-partial-error', 'seek-any-error', 'seek-any-short'):
            for nth in (1, 2):
                with self.subTest(fault=fault,nth=nth): self.invoke('slot-read-fault', 0, fault, nth)

    def test_slot_creation_failures_do_not_publish_uninitialized_slots(self):
        for fault, points in (('write-error',2), ('write-short',2), ('sync-error',2),
                              ('seek-any-error',3), ('seek-any-short',3)):
            for nth in range(1, points + 1):
                with self.subTest(fault=fault,nth=nth): self.invoke('slot-create-fault', 0, fault, nth)

    def test_slot_flush_errors_are_reported_and_retry_preserves_neighbors(self):
        for fault in ('none', 'seek-any-error', 'seek-any-short', 'write-error', 'write-short', 'sync-error'):
            with self.subTest(fault=fault): self.invoke('slot-flush', 0, fault)

    def test_mems_load_boundaries_and_failed_cache_ownership(self):
        for header in (0, 0x8000):
            for block in (8, 63, 8000, 8063):
                with self.subTest(header=header,block=block): self.invoke('mems-load', header | block)
            for block in (0, 1, 7, 8064, 8192, 32767):
                with self.subTest(header=header,block=block): self.invoke('mems-invalid', header | block)
            for fault in ('read-error', 'read-short', 'read-partial-error', 'seek-any-error', 'seek-any-short'):
                with self.subTest(header=header,fault=fault): self.invoke('mems-read-fault', header | 100, fault)
        for value in (-1, 0x10008, 0x18008):
            with self.subTest(value=value): self.invoke('mems-invalid', value)

    def test_mems_flush_ranges_overlap_coherence_and_invalid_requests(self):
        for overlap in (0, 16):
            for flags in range(1,8):
                with self.subTest(overlap=overlap,flags=flags): self.invoke('mems-flush', overlap | flags)
        for value in (0, 8, -1, 0x10001, 2, 4, 6, 7):
            with self.subTest(value=value): self.invoke('mems-flush-invalid', value)
        self.invoke('mems-coherence')

    def test_mems_flush_errors_stop_publication_and_preserve_retry_data(self):
        for fault, points in (('write-error',3), ('write-short',3), ('sync-error',1),
                              ('seek-any-error',3), ('seek-any-short',3)):
            for nth in range(1,points + 1):
                with self.subTest(fault=fault,nth=nth): self.invoke('mems-flush-fault', 7, fault, nth)

    def test_disc_id_requires_a_complete_header(self):
        for value in (2048,2352):
            for fault in ('none','read-error','read-short','read-partial-error'):
                with self.subTest(value=value,fault=fault): self.invoke('disc-id',value,fault)
        for value in (0,1,2):
            with self.subTest(value=value): self.invoke('disc-finish',value)

    def test_mems_opaque_trailers_survive_load_and_flush(self):
        for mode,values in [('mems-trailer-load',(8,8063,0x8008,0x9f7f)),('mems-trailer-flush',(1,4,7,23))]:
            for value in values:
                with self.subTest(mode=mode,value=value): self.invoke(mode,value)

if __name__ == '__main__': unittest.main()
