"""Overlay admission and disc lifetime against the actual loader and FatFs."""
from pathlib import Path
import subprocess
import tempfile

import mcu_save_support as support
import test_cdc_filter as filter_fixture
from test_sector_overlay import PRODUCT, RECORD_BYTES, HEADER_BYTES, canonical, seal, sector
import zlib


class OverlayMountTests(support.MCUSaveTest):
    fixture = 'cue'
    c_source = 'sector_overlay_mount.c'
    overlay_fixture = True

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        filter_fixture.CDFilterTests.setUpClass()

    def invoke(self, value):
        with tempfile.TemporaryDirectory(dir=self.target) as folder:
            root = Path(folder)
            card = root / 'card.img'
            support.make_fat32(card)
            count = 12 if value in (22, 23) else 1
            first = sector(150, ip=True, version=b'V1.001' if value == 14 else b'V1.000')
            sources = [sector(fad) for fad in range(151, 151 + count)]
            last = sector(151 + count, 0x72)
            pack = bytearray(HEADER_BYTES + count * RECORD_BYTES)
            pack[:20] = b'SSO1\x01' + bytes([count, 0, 0]) + PRODUCT + len(pack).to_bytes(2, 'big')
            if value in (11, 12, 22, 23):
                pack[4], pack[7] = 3, 0x80
            for index, raw in enumerate(sources):
                edited = bytearray(raw)
                edited[32] = 0xab
                edited = canonical(edited)
                offset = HEADER_BYTES + index * RECORD_BYTES
                pack[offset:offset + 8] = (151 + index).to_bytes(4, 'big') + zlib.crc32(raw).to_bytes(4, 'big')
                pack[offset + 8] = 1
                pack[offset + 12:offset + 24] = b'\0\x20\x01\0\x71\0\0\0\xab\0\0\0'
                pack[offset + 60:offset + RECORD_BYTES] = edited[2064:]
            seal(pack)
            if value == 23:
                sources[-1] = sector(162, 0x72)
            files = root / 'disc', root / 'expected', root / 'pack'
            for path, data in zip(files, (first + b''.join(sources) + last, edited, pack)):
                path.write_bytes(data)
            result = subprocess.run([self.binary, card, str(value), *files], capture_output=True,
                                    text=True, timeout=25)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            if value == 0:
                source = root / 'sector'
                source.write_bytes(sources[0])
                result = subprocess.run(
                    [filter_fixture.CDFilterTests.binary, '14', files[2], source, files[1]],
                    capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_mount_delivery_retry_and_disc_replacement(self):
        for value in (0, 10):
            with self.subTest(value=value):
                self.invoke(value)

    def test_ordinary_launch_and_bad_pack_keep_source_unchanged(self):
        for value in range(1, 5):
            with self.subTest(value=value):
                self.invoke(value)

    def test_transport_failures_clear_admission_and_close_handles(self):
        for value in range(5, 10):
            with self.subTest(value=value):
                self.invoke(value)

    def test_replacement_handoff_requires_complete_mount_and_save_selection(self):
        for value in (11, 12):
            with self.subTest(value=value):
                self.invoke(value)

    def test_full_ip_lookup_selects_each_revision_and_falls_back_only_on_absence(self):
        for value in (13, 14, 19, 21):
            with self.subTest(value=value):
                self.invoke(value)

    def test_specific_pack_rejection_and_io_never_try_legacy(self):
        for value in (15, 16, 17, 18, 20):
            with self.subTest(value=value):
                self.invoke(value)

    def test_twelve_record_preflight_admits_all_or_none(self):
        for value in (22, 23):
            with self.subTest(value=value):
                self.invoke(value)
