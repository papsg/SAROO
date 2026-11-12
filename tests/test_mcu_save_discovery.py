"""Actual discovery/catalog/sort functions with public FatFs and generated files."""
# Adapted from paps-g be30cf11; no private preparation/device access.
import unittest
from mcu_save_support import MCUSaveTest

class DiscDiscoveryTests(MCUSaveTest):
    fixture = 'discovery'
    timeout = 90

    def invoke(self, mode, value=0):
        return self.run_case([mode, value])

    def test_discovery_handles_dotless_files_preference_and_path_limits(self):
        for value in range(7):
            with self.subTest(value=value):self.invoke('discover',value)

    def test_directory_failures_do_not_publish_partial_candidates_or_catalogs(self):
        for mode in ('scan-fault','list-fault','bins-fault'):
            for fault in range(1,5):
                with self.subTest(mode=mode,fault=fault):self.invoke(mode,fault)

    def test_catalog_filtering_order_lookup_and_repeated_scans(self):
        for mode in ('list','bins'):
            for order in range(3):
                with self.subTest(mode=mode,order=order):self.invoke(mode,order)

    def test_full_catalog_preserves_last_usable_slot_and_string_byte(self):
        for mode,values in (('count-capacity',(2559,2560)),('name-capacity',(520,521)),('sorted-capacity',(2559,)),('reverse-capacity',(2559,))):
            for count in values:
                with self.subTest(mode=mode,count=count):self.invoke(mode,count)

    def test_category_and_game_paths_are_bounded_before_copying(self):
        for value in range(8):
            with self.subTest(value=value):self.invoke('path',value)
        for value in (-1,1,255):
            with self.subTest(category=value):self.invoke('category',value)

    def test_shorter_rescan_preserves_live_entries_and_measures_current_clearing(self):
        self.invoke('reuse')

    def test_actual_bin_consumer_rejects_stale_failed_and_empty_catalogs(self):
        self.invoke('producer-consumer')

    def test_bin_folders_publish_tagged_directories_and_files(self):
        self.invoke('folders')

    def test_bin_folder_paths_reject_aliases_and_respect_the_shared_bound(self):
        self.invoke('folder-paths')

    def test_bin_folder_errors_empty_the_catalog_and_allow_retry(self):
        for value in range(1, 5):
            with self.subTest(value=value):
                self.invoke('folder-io', value)

if __name__ == '__main__': unittest.main()
