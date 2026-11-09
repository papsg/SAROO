"""Full FAT32/exFAT volumes must not strand intact previously published saves."""
import unittest
from mcu_save_support import MCUSaveTest

class MCUExtentTests(MCUSaveTest):
    fixture = 'extents'

    def case(self, filesystem, mode):
        return self.run_case([filesystem, mode, 0, 0])

    def test_unpublished_tail_preserves_runtime_reads(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem): self.case(filesystem, 'runtime')

    def test_boot_preserves_tail_and_published_saves_without_writes(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem): self.case(filesystem, 'boot')

    def test_next_creation_completes_the_unpublished_slot(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem): self.case(filesystem, 'complete')

    def test_published_entry_into_partial_tail_is_rejected(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem): self.case(filesystem, 'dangling')

if __name__ == '__main__':
    unittest.main()
