"""Exercise the optional stack-LFN/exFAT enumeration workspace.

Adapted from paps-g 49a1c53d for the public FatFs source and host disk seam.
Run against the parent with SAROO_ENUM_EXPECT_SHARED=0 and against the port
with SAROO_ENUM_EXPECT_SHARED=1. Both runs must have matching result records.
"""

import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FatFsEnumerationWorkspaceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.expected_shared = int(os.environ.get("SAROO_ENUM_EXPECT_SHARED", "1"))
        assert cls.expected_shared in (0, 1)
        cls.target = Path(os.environ.get(
            "SAROO_ENUM_OUTPUT", ROOT / "tests/build/fatfs-enumeration-workspace"))
        cls.target.mkdir(parents=True, exist_ok=True)
        cls.results = []
        cls.binaries = {}
        cls.inputs = {}
        cls.source = Path(os.environ.get(
            "SAROO_ENUM_SOURCE", ROOT / "Firm_MCU/FatFS/ff.c"))
        for name, lfn_capacity in (("full", 765), ("narrow", 255)):
            folder = cls.target / name
            if folder.exists():
                shutil.rmtree(folder)
            folder.mkdir()
            for filename in ("ff.c", "ff.h", "ffconf.h", "ffunicode.c", "diskio.h"):
                original = cls.source if filename == "ff.c" else ROOT / "Firm_MCU/FatFS" / filename
                data = original.read_bytes()
                (folder / filename).write_bytes(data)
                cls.inputs[name + "/" + filename] = hashlib.sha256(data).hexdigest()
            config = folder / "ffconf.h"
            settings = config.read_text()
            for key, value in (
                ("FF_USE_MKFS", 1), ("FF_USE_LFN", 2),
                ("FF_FS_REENTRANT", 1), ("FF_LFN_BUF", lfn_capacity),
            ):
                settings, count = re.subn(
                    r"(?m)^(#define\s+" + key + r"\s+)\d+",
                    lambda match: match[1] + str(value), settings)
                assert count == 1, key
            config.write_text(settings)
            source = (folder / "ff.c").read_bytes()
            begin = source.index(b"if (ifptr ", source.index(b"FRESULT f_lseek ("))
            end = source.index(b"{", begin)
            (folder / "seek.inc").write_bytes(
                b"if (!ofs) return 0;\n" + source[begin:end] +
                b"{ return 1; }\nreturn 0;\n")
            start = source.index(b"FRESULT f_readdir (")
            marker = b"\t\t\tres = DIR_READ_FILE(dp);"
            position = source.index(marker, start)
            assert position < source.index(b"FRESULT f_findnext (", start)
            source = (source[:position] +
                      b"\t\t\textern void enumeration_workspace_probe(FATFS*);\n"
                      b"\t\t\tenumeration_workspace_probe(fs);\n" + source[position:])
            (folder / "ff.c").write_bytes(source)
            for filename in ("fatfs_seek.c", "fatfs_enumeration_workspace.c"):
                shutil.copy2(ROOT / "tests" / filename, folder / filename)
            binary = folder / "enumeration"
            command = [
                os.environ.get("CC", "cc"), "-std=gnu11", "-O1", "-g",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                "-fno-omit-frame-pointer", "-pthread", "-DSAROO_FAT32_ONLY=0",
                "-DEXPECT_SHARED=" + str(cls.expected_shared), "-I", str(folder),
                str(folder / "fatfs_enumeration_workspace.c"), str(folder / "ff.c"),
                str(folder / "ffunicode.c"), "-o", str(binary),
            ]
            result = subprocess.run(command, capture_output=True, timeout=90)
            (folder / "compile.json").write_text(json.dumps({
                "command": command, "status": result.returncode,
                "stdout": result.stdout.decode(errors="replace"),
                "stderr": result.stderr.decode(errors="replace"),
            }, indent=2) + "\n")
            result.check_returncode()
            cls.binaries[name] = binary
        (cls.target / "inputs.json").write_text(
            json.dumps(cls.inputs, indent=2) + "\n")

    @classmethod
    def tearDownClass(cls):
        (cls.target / "results.json").write_text(
            json.dumps(cls.results, indent=2) + "\n")

    def run_case(self, filesystem, cluster, binary="full"):
        with tempfile.TemporaryDirectory(dir=self.target) as temp:
            disk = Path(temp) / "card.img"
            with disk.open("wb") as image:
                image.truncate({1: 1, 2: 16}.get(filesystem, 64 * cluster) * 1024**2)
            command = [str(self.binaries[binary]), str(disk),
                       str(filesystem), str(cluster)]
            result = subprocess.run(
                command, capture_output=True, timeout=90,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                     "UBSAN_OPTIONS": "halt_on_error=1"})
        record = {
            "filesystem": filesystem, "cluster": cluster, "binary": binary,
            "status": result.returncode,
            "stdout": result.stdout.decode(errors="replace"),
            "stderr": result.stderr.decode(errors="replace"),
        }
        self.results.append(record)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, b"")

    def test_fat_name_boundaries_and_injected_read_failures(self):
        for filesystem in (1, 2, 3):
            for cluster in (1, 4):
                with self.subTest(filesystem=filesystem, cluster=cluster):
                    self.run_case(filesystem, cluster)

    def test_exfat_name_boundaries_and_injected_read_failures(self):
        for cluster in (1, 4):
            with self.subTest(cluster=cluster):
                self.run_case(4, cluster)

    def test_narrow_output_keeps_fallback(self):
        for filesystem in (3, 4):
            for cluster in (1, 4):
                with self.subTest(filesystem=filesystem, cluster=cluster):
                    self.run_case(filesystem, cluster, "narrow")


if __name__ == "__main__":
    unittest.main()
