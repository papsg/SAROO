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
            raw = sector(151, 0x71)
            edited = bytearray(raw)
            edited[32] = 0xab
            edited = canonical(edited)
            first, last = sector(150, ip=True), sector(152, 0x72)
            pack = bytearray(HEADER_BYTES + RECORD_BYTES)
            pack[:20] = b'SSO1\x01\x01\0\0' + PRODUCT + b'\x01\x74'
            pack[24:32] = (151).to_bytes(4, 'big') + zlib.crc32(raw).to_bytes(4, 'big')
            pack[32] = 1
            pack[36:48] = b'\0\x20\x01\0\x71\0\0\0\xab\0\0\0'
            pack[84:] = edited[2064:]
            seal(pack)
            files = root / 'disc', root / 'expected', root / 'pack'
            for path, data in zip(files, (first + raw + last, edited, pack)):
                path.write_bytes(data)
            result = subprocess.run([self.binary, card, str(value), *files], capture_output=True,
                                    text=True, timeout=25)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            if value == 0:
                source = root / 'sector'
                source.write_bytes(raw)
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
