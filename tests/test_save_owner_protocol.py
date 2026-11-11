"""Actual mapped file/archive command handling, with an observable owner boundary."""
import os
import subprocess
import unittest
from test_file_transfer import FileTransferTests as Fixture


class SaveOwnerProtocolTests(unittest.TestCase):
    c_source = 'save_owner_protocol.c'
    setUpClass = classmethod(Fixture.setUpClass.__func__)
    tearDownClass = classmethod(Fixture.tearDownClass.__func__)

    def case(self, mode):
        p = subprocess.run([str(self.binary), mode], capture_output=True, timeout=15,
                           env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(mode=mode, returncode=p.returncode,
            stdout=p.stdout.decode(errors='replace'), stderr=p.stderr.decode(errors='replace')))
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertIn(b'PASS', p.stdout)

    def test_reject_request_overlap_window_crossings_and_signed_size_wrap(self): self.case('bounds')
    def test_exact_extents_and_mapped_aliases_reach_the_selected_owner(self): self.case('transfer')
    def test_only_zero_length_writes_request_sync(self): self.case('sync')
    def test_partial_failures_clear_count_and_preserve_full_negative_error(self): self.case('errors')
    def test_capability_publishes_exact_version_without_overwriting_neighbor_words(self): self.case('capability')


del Fixture
if __name__ == '__main__': unittest.main()
