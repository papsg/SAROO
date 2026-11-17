"""Optional native device-0 allocation against the actual backup module.

Adapted from paps-g 24a4b069. The default public firmware stays in its
expanded layout until checked archive conversion is available.
"""

from firmware_save_support import FirmwareSaveTest


class FirmwareNativeBackupTests(FirmwareSaveTest):
    fixture = "firmware_save_native"
    extra_cflags = ("-DSAROO_NATIVE_INTERNAL_BACKUP",)
    timeout = 120

    def test_native_internal_and_extended_cartridge_contracts(self):
        self.invoke()
