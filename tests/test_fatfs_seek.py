"""Actual FatFs geometry, seek, sync and metadata tests on generated host media."""
# Adapted from paps-g 60d5df01; all compilation and media stay in public tests/build.
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FatFsSeekTests(unittest.TestCase):
    fixture_name = 'fatfs-seek'
    fixture_sources = ('fatfs_seek.c', 'test_fatfs_seek.py')

    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build' / cls.fixture_name; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build)); cls.results = []
        fatfs = cls.target / 'FatFS'; fatfs.mkdir()
        inputs = {}
        directory = Path(os.environ.get('SAROO_FATFS_DIR', ROOT / 'Firm_MCU/FatFS'))
        for name in ('ff.c', 'ff.h', 'ffconf.h', 'ffunicode.c', 'diskio.h'):
            source = Path(os.environ.get('SAROO_FATFS_SOURCE', directory / name)) if name == 'ff.c' else directory / name
            data = source.read_bytes()
            if name == 'ffconf.h':
                inputs['production-ffconf.h'] = data
                old = b'#define FF_USE_MKFS\t\t0'; assert data.count(old) == 1
                data = data.replace(old, b'#define FF_USE_MKFS\t\t1')
            (fatfs / name).write_bytes(data); inputs['FatFS/' + name] = data
        data = (fatfs / 'ff.c').read_bytes(); begin = data.index(b'if (ifptr ', data.index(b'FRESULT f_lseek (')); end = data.index(b'{', begin)
        (cls.target / 'seek.inc').write_bytes(b'if (!ofs) return 0;\n' + data[begin:end] + b'{ return 1; }\nreturn 0;\n')
        for name in cls.fixture_sources:
            data = (ROOT / 'tests' / name).read_bytes(); (cls.target / name).write_bytes(data); inputs[name] = data
        for name, data in inputs.items():
            path = cls.target / 'inputs' / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps(
            {name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2)+'\n')
        flags = ['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        flags.extend(getattr(cls, 'extra_cflags', ()))
        cls.binary = cls.target / cls.fixture_name
        result = subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-g', *flags, '-pthread',
            '-I', str(cls.target), '-I', str(fatfs), str(cls.target / cls.fixture_sources[0]),
            str(fatfs / 'ff.c'), str(fatfs / 'ffunicode.c'), '-o', str(cls.binary)], capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr); result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def invoke(self, mode, value=0):
        size_mib = {'seek': 8192, 'metadata-fat12': 1, 'metadata-fat16': 16,
                    'readonly-fat12': 1, 'readonly-fat16': 16}.get(mode, 64)
        with tempfile.TemporaryDirectory(dir=self.target) as tmp:
            card = Path(tmp) / 'card.img'
            with card.open('wb') as file: file.truncate(size_mib * 1024**2)
            if mode == 'seek': self.assertLess(card.stat().st_blocks*512, 1024**2, 'Wide fixture requires sparse host files')
            result = subprocess.run([str(self.binary), str(card), mode, str(value)], capture_output=True,
                timeout=90, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'})
            physical = card.stat().st_blocks*512
        self.results.append(dict(mode=mode, value=value, returncode=result.returncode, physical_bytes=physical,
            stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace')))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cluster_comparison_matches_division_at_64_bit_boundaries(self): self.invoke('math')
    def test_actual_seek_read_write_preserves_wide_offsets_and_neighbors(self): self.invoke('seek')

    def test_fragmented_fat32_and_exfat_cross_cluster_reads(self):
        for value in (0, 1):
            with self.subTest(value=value): self.invoke('fragmented', value)

    def test_valid_utf8_names_round_trip_after_remount(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'utf8-{filesystem}', 0)

    def test_nonshortest_utf8_names_are_rejected_without_writes(self):
        for filesystem in ('fat32', 'exfat'):
            for value in range(1, 5):
                with self.subTest(filesystem=filesystem, value=value):
                    self.invoke(f'utf8-{filesystem}', value)

    def test_other_malformed_utf8_names_are_rejected_without_writes(self):
        for filesystem in ('fat32', 'exfat'):
            for value in range(5, 13):
                with self.subTest(filesystem=filesystem, value=value):
                    self.invoke(f'utf8-{filesystem}', value)

    def test_exfat_mount_rejects_unsafe_geometry_shifts(self):
        for value in (512, 8, 9, 16, 31, 32, 63, 127, 255, 272, 287, 288, 319, 383, 511):
            with self.subTest(value=value): self.invoke('mount', value)

    def test_mount_fat32_extent_arithmetic_and_valid_controls(self):
        for value in range(6):
            with self.subTest(value=value): self.invoke('mount-fat-extent', value)

    def test_mount_exfat_extent_and_minimum_fat_length(self):
        for value in range(5):
            with self.subTest(value=value): self.invoke('mount-exfat-extent', value)

    def test_exfat_bitmap_rejects_missing_backing_before_allocation(self):
        for kind in ('short-chain', 'short-length', 'over-length', 'fragmented',
                     'fat-span', 'outside-heap', 'large-heap'):
            with self.subTest(kind=kind):
                self.invoke(f'bitmap-{kind}')

    def test_exfat_bitmap_valid_allocation_preserves_neighbor(self):
        for kind in ('exact', 'padded', 'partitioned'):
            with self.subTest(kind=kind):
                self.invoke(f'bitmap-{kind}')
        for cluster_sectors in (0, 1, 4, 128):
            with self.subTest(cluster_sectors=cluster_sectors):
                self.invoke('bitmap-formatted', cluster_sectors)

    def test_exfat_bitmap_read_errors_allow_remount(self):
        for ordinal in (1, 2, 3):
            with self.subTest(ordinal=ordinal):
                self.invoke('bitmap-exact', ordinal)

    def test_readonly_seek_cannot_allocate_a_missing_chain(self):
        for kind in ('fat12', 'fat16', 'fat32', 'exfat'):
            for scenario in (0, 1, 2):
                with self.subTest(kind=kind, scenario=scenario):
                    self.invoke(f'readonly-{kind}', scenario)

    def test_readonly_seek_and_writable_growth_controls(self):
        for kind in ('fat12', 'fat16', 'fat32', 'exfat'):
            for scenario in (3, 4, 5, 6):
                with self.subTest(kind=kind, scenario=scenario):
                    self.invoke(f'readonly-{kind}', scenario)

    def test_failed_directory_write_keeps_pending_sync(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'sync-{filesystem}', 0)

    def test_failed_device_sync_can_be_retried(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'sync-{filesystem}', 1)

    def test_failed_close_keeps_the_file_open_for_retry(self):
        for filesystem in ('fat32', 'exfat'):
            for value in (2, 3):
                with self.subTest(filesystem=filesystem, value=value):
                    self.invoke(f'sync-{filesystem}', value)

    def test_clean_sync_issues_no_new_device_io(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'sync-{filesystem}', 4)

    def test_retained_mount_and_dirty_file_resume_only_with_same_media(self):
        for value in range(4):
            with self.subTest(fat32=bool(value & 1), reopen_by_path=bool(value & 2)):
                self.invoke('retained-media', value)

    def test_failed_fat_mirror_retains_the_pending_window(self):
        for filesystem in ('fat12', 'fat16', 'fat32'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'metadata-{filesystem}', 0)

    def test_failed_primary_fat_write_stops_before_the_mirror(self):
        for filesystem in ('fat12', 'fat16', 'fat32'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'metadata-{filesystem}', 1)

    def test_single_fat_writes_only_its_own_table(self):
        for filesystem in ('fat12', 'fat16', 'fat32'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'metadata-{filesystem}', 2)

    def test_fsinfo_failure_remains_retryable_without_blocking_reads(self):
        self.invoke('metadata-fat32', 3)

    def test_partial_metadata_writes_recover(self):
        for filesystem, value in (('fat12', 4), ('fat16', 5), ('fat32', 4), ('fat32', 6)):
            with self.subTest(filesystem=filesystem, value=value):
                self.invoke(f'metadata-{filesystem}', value)

    def test_cached_write_abort_keeps_the_completed_prefix_and_close_retry(self):
        for filesystem in ('fat32', 'exfat'):
            with self.subTest(filesystem=filesystem):
                self.invoke(f'write-abort-{filesystem}', 0)

    def test_file_growth_retains_reported_prefix_after_abort(self):
        for filesystem in ('fat32', 'exfat'):
            for value in (1,2):
                with self.subTest(filesystem=filesystem, value=value):
                    self.invoke(f'write-abort-{filesystem}', value)

    def test_failed_first_transfer_read_only_and_zero_length_controls(self):
        for filesystem in ('fat32', 'exfat'):
            for value in (3,4,5):
                with self.subTest(filesystem=filesystem, value=value):
                    self.invoke(f'write-abort-{filesystem}', value)


if __name__ == '__main__': unittest.main()
