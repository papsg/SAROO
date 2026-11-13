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

    def test_alternative_admission_requires_exactly_one_source_and_retains_retries(self):
        for value in range(24, 32):
            with self.subTest(value=value), tempfile.TemporaryDirectory(dir=self.target) as folder:
                root = Path(folder)
                card = root / 'card.img'
                support.make_fat32(card)
                source = sector(151)
                edited = bytearray(source)
                edited[32] = 0xab
                edited = canonical(edited)
                candidates = [source, sector(152, 0x72 if value != 26 else 0x71)]
                if value == 25:
                    candidates[0] = sector(151, 0x72)
                if value in (27, 28, 31):
                    candidates[1] = sector(151, 0x72 if value == 27 else 0x71)
                if value == 30:
                    candidates[1] = sector(159)
                records = []
                for raw in candidates:
                    fad = sum((int(f'{byte:02x}') * multiplier for byte, multiplier in
                               zip(raw[12:15], (4500, 75, 1))))
                    record = bytearray(RECORD_BYTES)
                    record[:8] = fad.to_bytes(4, 'big') + zlib.crc32(raw).to_bytes(4, 'big')
                    record[8] = 1
                    record[12:24] = bytes([0, 32, 1, 0, raw[32], 0, 0, 0, 0xab, 0, 0, 0])
                    changed = bytearray(raw)
                    changed[32] = 0xab
                    record[60:] = canonical(changed)[2064:]
                    records.append(record)
                if value == 31:
                    records[1][20] ^= 1  # Same source with conflicting output.
                records.sort(key=lambda r: r[:8])
                pack = bytearray(b'SSO1\x03\x02\x01\x80' + PRODUCT +
                                 (HEADER_BYTES + 2 * RECORD_BYTES).to_bytes(2, 'big') + bytes(4))
                pack.extend(b''.join(records))
                seal(pack)
                files = root / 'disc', root / 'expected', root / 'pack'
                for path, data in zip(files, (sector(150, ip=True) + source +
                                             sector(152) + sector(153), edited, pack)):
                    path.write_bytes(data)
                result = subprocess.run([self.binary, card, str(value), *files],
                                        capture_output=True, text=True, timeout=25)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_guard_sector_controls_complete_mount_and_runtime_retry(self):
        for value in range(32, 36):
            with self.subTest(value=value), tempfile.TemporaryDirectory(dir=self.target) as folder:
                root = Path(folder)
                card = root / 'card.img'
                support.make_fat32(card)
                raw, guard = sector(151), sector(152)
                edited = bytearray(raw)
                edited[32:44] = b'\xaa' * 12
                edited = canonical(edited)
                pack = bytearray(b'SSO1\x03\x02\0\x80' + PRODUCT +
                                 (HEADER_BYTES + 2 * RECORD_BYTES).to_bytes(2, 'big') + bytes(4))
                record = bytearray(RECORD_BYTES)
                record[:8] = (151).to_bytes(4, 'big') + zlib.crc32(raw).to_bytes(4, 'big')
                record[8] = 0x80 | 12
                record[12:14] = (32).to_bytes(2, 'big')
                record[16:28] = b'\xaa' * 12
                record[60:] = edited[2064:]
                pack.extend(record)
                record = bytearray(RECORD_BYTES)
                record[:8] = (152).to_bytes(4, 'big') + zlib.crc32(guard).to_bytes(4, 'big')
                pack.extend(record)
                seal(pack)
                if value == 33:
                    guard = sector(152, 0x72)
                files = root / 'disc', root / 'expected', root / 'pack'
                for path, data in zip(files, (sector(150, ip=True) + raw + guard +
                                             sector(153), edited, pack)):
                    path.write_bytes(data)
                result = subprocess.run([self.binary, card, str(value), *files],
                                        capture_output=True, text=True, timeout=25)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
