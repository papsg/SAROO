"""Pair the actual menu, MCU and FatFs owners on generated volumes."""
import os
from pathlib import Path
import re
import unittest

from mcu_save_support import MCUSaveTest, ROOT
from test_file_transfer import extract_command_source, protocol_definitions
from test_menu_file_cache import host_cache_operations


class MenuSaveOwnerTests(MCUSaveTest):
    fixture = 'owner'
    c_source = 'menu_save_owner.c'

    @classmethod
    def prepare_menu(cls, source, target):
        menu = ROOT / 'Firm_Saturn'
        raw = Path(os.environ.get('SAROO_MENU_OWNER_SOURCE', menu / 'main.c')).read_bytes()
        menu_header = (menu / 'main.h').read_bytes()
        mcu_header = (source / 'cdc.h').read_bytes()
        pattern = rb'^#define\s+(ARCHIVE_IO_VERSION|SSCMD_ARCHIVERD|SSCMD_ARCHIVEWR|SSCMD_SAVE_ACK)\s+(\S+)'
        values = dict(re.findall(pattern, menu_header, re.M))
        assert len(values) == 4 and values == dict(re.findall(pattern, mcu_header, re.M))

        start = raw.index(b'static u32 file_capacity(')
        words = raw.index(b'u32 BE32(')
        code = (raw[words:raw.index(b'/**********************************************************/', words)]
                + raw[start:raw.index(b'/**********************************************************/', start)])
        code = host_cache_operations(code).replace(b'(u32)buf', b'host_bus_address(buf)')
        code = code.replace(b'((TMPBUFF_ADDR & 0x1fffffff) + 0x100)', b'0x020c7100')
        (target / 'menu-owner.inc').write_bytes(code)
        latch = re.search(rb'^[ \t]*archive_ack = LE32[^\n]+', raw, re.M)
        (target / 'archive-latch.inc').write_bytes(latch[0] + b'\n')

        (target / 'protocol.h').write_bytes(protocol_definitions(mcu_header))
        mcu = (source / 'saturn_main.c').read_bytes()
        peer = extract_command_source(mcu)
        buffer = b'(test_shared+(baddr & 0x00ffffff))'
        if b'file_buffer_io(' not in mcu:
            assert peer.count(buffer) == 1
            peer = peer.replace(buffer, b'bus_pointer(baddr & 0x00ffffff)')
        lock_check = b'#define NAMED_FILE_LOCK ' + (b'1' if b'fs_lock();' in peer else b'0')
        (target / 'mcu-owner.inc').write_bytes(lock_check + b'\n' + peer)
        return {'Firm_Saturn/main.c': raw, 'Firm_Saturn/main.h': menu_header}

    def cases(self, mode, devices):
        for filesystem in ('fat32', 'exfat'):
            for device in devices:
                with self.subTest(filesystem=filesystem, device=device):
                    self.assertIn('PASS paired menu/MCU owner transport',
                                  self.run_case([filesystem, mode, device]))

    def test_both_devices_reuse_the_primary_owner_for_reads_writes_and_sync(self):
        self.cases('transport', (0, 1))

    def test_partial_io_and_reset_cannot_confirm_success(self):
        self.cases('errors', (0, 1))

    def test_direct_buffer_bounds_and_legacy_capability(self):
        self.cases('bounds', (0, 1))

    def test_full_keys_holes_duplicate_missing_and_final_slot(self):
        self.cases('lookup', (0,))

    def test_mirrored_io_keeps_whole_archive_admission_and_partial_failure_rules(self):
        self.cases('ram', (0, 1))

    def test_slot_lookup_preserves_bounds_when_either_read_fails(self):
        self.cases('lookup-fault', (0,))

    def test_named_write_refuses_both_primary_archives_through_aliases(self):
        self.cases('named-write', (0, 1))

    def test_resolved_archive_identity_survives_directory_boundaries(self):
        self.cases('named-crossing', (0, 1))

    def test_primary_write_reservation_tracks_the_open_file_lifetime(self):
        self.cases('named-lifetime', (0, 1))


if __name__ == '__main__':
    unittest.main()
