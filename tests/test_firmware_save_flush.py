"""Clear dirty windows only on the matching reported successful flush."""
import unittest
from firmware_save_support import FirmwareSaveTest

class FirmwareFlushTests(FirmwareSaveTest):
    fixture = "firmware_save_flush"

    def test_flush_status_flags_and_retry_preserve_cache_owner(self):
        for flag in range(1, 9):
            for status in (0, -1, 1, 2):
                with self.subTest(flag=flag, status=status):
                    self.invoke('status', flag, status)

    def test_read_after_committed_write_does_not_reflush(self):
        self.invoke('read-after-write')

if __name__ == '__main__':
    unittest.main()
