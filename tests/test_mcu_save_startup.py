"""Actual startup/caller fragments and public FatFs on synthetic FAT32 media."""
import unittest
from mcu_save_support import MCUSaveTest

class MCUStartupTests(MCUSaveTest):
    def invoke(self, mode, target=0, value=0, gate=0):
        return self.run_case([mode, target, value, gate])

    def test_valid_missing_and_compatible_extents(self):
        for mode, target in (('healthy', 0), ('new-both', 0), ('missing', 0), ('missing', 1)):
            with self.subTest(mode=mode, target=target): self.invoke(mode, target)
        for target, size in ((0, 65537), (0, 131071), (1, 8388609), (1, 8392705)):
            with self.subTest(target=target, size=size): self.invoke('compatible-size', target, size)

    def test_existing_archive_errors_never_recreate(self):
        for target in (0, 1):
            for code in range(1, 20):
                with self.subTest(target=target, code=code): self.invoke('open-error', target, code)
            for mode in ('read-error', 'read-short', 'seek-error'):
                for value in ((0, 1) if mode == 'read-short' else (0,)):
                    with self.subTest(target=target, mode=mode, value=value): self.invoke(mode, target, value)
        self.invoke('cleanup-error', 1)

    def test_invalid_extents_and_signatures_are_preserved(self):
        for target, sizes in ((0, (0, 1, 63, 64, 65535)), (1, (0, 8191, 8192, 8388607))):
            for size in sizes:
                with self.subTest(target=target, size=size): self.invoke('size', target, size)
            for damage in (1, 2):
                with self.subTest(target=target, damage=damage): self.invoke('signature', target, damage)
        self.invoke('signature', 0, 3)

    def test_failed_creation_and_initialization_stop_service(self):
        for target in (0, 1):
            for code in (1, 3, 7, 8, 9, 10):
                with self.subTest(target=target, code=code): self.invoke('create-error', target, code)
            for mode in ('write-error', 'write-short', 'sync-error'):
                with self.subTest(target=target, mode=mode): self.invoke(mode, target)
        for mode in ('extend-error', 'extend-short'):
            with self.subTest(mode=mode): self.invoke(mode, 1)

    def test_actual_caller_stops_on_error(self):
        for mode, target, value in (('healthy', 0, 0), ('new-both', 0, 0), ('signature', 0, 1),
                                    ('signature', 1, 1), ('read-error', 1, 0), ('create-error', 1, 7)):
            with self.subTest(mode=mode, target=target): self.invoke(mode, target, value, 1)

if __name__ == '__main__':
    unittest.main()
