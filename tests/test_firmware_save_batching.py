"""Bound header traffic without evicting a live payload cache or caching validation."""
import unittest
from firmware_save_support import FirmwareSaveTest

class FirmwareBatchingTests(FirmwareSaveTest):
    fixture = "firmware_save_batching"

    def test_contiguous_ranges_and_directory_order(self):
        cases = [(n, 'dense') for n in (0, 1, 2, 3, 63, 64, 65, 127, 128, 447, 448)]
        cases += [(n, kind) for kind in ('reverse', 'permuted', 'high') for n in (2, 63, 64, 65, 448)]
        cases += [(n, 'holes') for n in (2, 64, 128)]
        cases.append((10, 'mixed'))
        for count, layout in cases:
            with self.subTest(count=count, layout=layout): self.invoke('cost', count, layout)

    def test_sparse_headers_do_not_add_read_traffic(self):
        for count in (1, 64, 448):
            with self.subTest(count=count): self.invoke('cost', count, 'sparse')

    def test_live_payload_cache_and_flush_tracking(self):
        self.invoke('cache')
        self.invoke('dirty')

    def test_full_directory_keeps_live_payload_window(self):
        self.invoke('live-full')

    def test_mutations_after_batch_preserve_neighbors_and_persistence(self):
        for kind in ('same', 'grow', 'shrink', 'delete', 'new-inline', 'new-blocks'):
            with self.subTest(kind=kind): self.invoke('mutate', kind)

    def test_reported_load_failures_preserve_storage_and_allow_retry(self):
        for fault in (-1, 1, 2):
            with self.subTest(fault=fault): self.invoke('failure', fault)

    def test_every_header_is_validated_on_later_requests(self):
        for index in (0, 31, 32, 63):
            with self.subTest(index=index): self.invoke('changed', index)
        for corruption in range(8):
            with self.subTest(corruption=corruption): self.invoke('corrupt', corruption)

if __name__ == '__main__': unittest.main()
