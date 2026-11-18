"""Actual FatFs enumeration with injected read errors and valid retries."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import test_fatfs_seek as support


class ReaddirTests(unittest.TestCase):
    fixture_name = "fatfs-readdir"
    fixture_sources = (
        "fatfs_readdir.c", "test_fatfs_readdir.py",
        *support.FatFsSeekTests.fixture_sources,
    )
    generic_formats = False
    extra_cflags = ("-DEXPECT_FSIZE_BITS=32",)
    setUpClass = classmethod(support.FatFsSeekTests.setUpClass.__func__)
    tearDownClass = classmethod(support.FatFsSeekTests.tearDownClass.__func__)

    def case(self, filesystem, cluster):
        with tempfile.TemporaryDirectory(dir=self.target) as temp:
            image = Path(temp) / "card.img"
            size = 64 * 1024**2 * (cluster if filesystem == "fat32" else 1)
            with image.open("wb") as out:
                out.truncate(size)
            result = subprocess.run(
                [str(self.binary), str(image), filesystem, str(cluster)],
                capture_output=True, timeout=60,
                env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                     "ASAN_OPTIONS": "detect_leaks=0"},
            )
        self.results.append(dict(filesystem=filesystem, cluster=cluster,
                                 status=result.returncode))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        rows = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(rows[-1]["bad_names"], 0)
        self.assertEqual(rows[-1]["content_checked"], 40)

    def test_fat32_one_sector_clusters(self):
        self.case("fat32", 1)

    def test_fat32_four_sector_clusters(self):
        self.case("fat32", 4)


class OptionalReaddirTests(ReaddirTests):
    fixture_name = "fatfs-readdir-optional"
    generic_formats = True
    extra_cflags = ("-DEXPECT_FSIZE_BITS=64",)

    def test_exfat_one_sector_clusters(self):
        self.case("exfat", 1)


if __name__ == "__main__":
    unittest.main()
