"""SATURN-SAVE-002: all dirty bits in both words of the actual firmware."""
import unittest
from firmware_save_support import FirmwareSaveTest


class FirmwareDirtyTests(FirmwareSaveTest):
    fixture = "firmware_save_dirty"

    def test_every_position_and_repeated_marks(self):
        for start in [8, 200, 8000]:
            for bit in range(64):
                with self.subTest(window=start, bit=bit):
                    self.invoke(start, bit, 0)

    def test_combined_words(self):
        self.invoke(8, 0, 1)

    def test_slot_device_leaves_mems_tracking_unchanged(self):
        self.invoke(8, 0, 2)


if __name__ == "__main__":
    unittest.main()
