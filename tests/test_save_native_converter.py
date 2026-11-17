"""Independent archive-level controls for the opt-in native backup migration."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from save_fixtures import native_slot_fixture, slot_fixture

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/target/debug/saroo-save-native"


class NativeConverterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run(["cargo", "build", "--locked", "--offline", "--manifest-path",
                        str(ROOT / "tools/Cargo.toml"), "-p", "save-native"],
                       check=True, capture_output=True, text=True, timeout=120)

    def invoke(self, *args):
        return subprocess.run([str(TOOL), *map(str, args)], capture_output=True,
                              text=True, timeout=15)

    def test_old_archive_converts_to_independently_encoded_native_slots(self):
        sizes = [0, 1, 27, 28, 29, 86, 87, 992, 1276]
        entries = [(f"SAVE{i:07}".encode(), bytes((i + j * 13) % 251 for j in range(n)))
                   for i, n in enumerate(sizes)]
        old, _ = slot_fixture(entries, gameids=(b"GAME", None))
        expected = native_slot_fixture(entries) + old[2 * 65536:]
        expected[16:32] = old[16:32]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "SS_SAVE.BIN"
            output = Path(directory) / "converted.bin"
            source.write_bytes(old)
            inspect = self.invoke("inspect", source)
            self.assertEqual(inspect.returncode, 0, inspect.stderr)
            self.assertTrue(json.loads(inspect.stdout)["all_fit"])
            result = self.invoke("convert", source, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(report["source_sha256"], hashlib.sha256(old).hexdigest())
            self.assertEqual(report["output_sha256"], hashlib.sha256(expected).hexdigest())
            self.assertEqual(output.read_bytes(), expected)
            self.assertEqual(source.read_bytes(), old)
            self.assertNotEqual(self.invoke("convert", source, output).returncode, 0)
            self.assertEqual(output.read_bytes(), expected)

    def test_capacity_corruption_and_pending_journal_refuse_without_output(self):
        entries = [(b"SAVE0000001", bytes(16384)), (b"SAVE0000002", bytes(16384))]
        old, _ = slot_fixture(entries)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "SS_SAVE.BIN"
            output = Path(directory) / "converted.bin"
            journal = Path(directory) / "SAVEJRN.BIN"
            source.write_bytes(old)
            result = self.invoke("inspect", source)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(json.loads(result.stdout)["all_fit"])
            self.assertNotEqual(self.invoke("convert", source, output).returncode, 0)
            self.assertFalse(output.exists())
            small, _ = slot_fixture([(b"SAVE0000001", b"payload")])
            source.write_bytes(small)
            journal.write_bytes(bytes([1]) * 512)
            self.assertNotEqual(self.invoke("convert", source, output).returncode, 0)
            journal.write_bytes(bytes(512))
            broken = bytearray(small)
            broken[65536 + 62:65536 + 64] = (600).to_bytes(2, "big")
            source.write_bytes(broken)
            self.assertNotEqual(self.invoke("convert", source, output).returncode, 0)
            self.assertFalse(output.exists())
            self.assertEqual(source.read_bytes(), broken)


if __name__ == "__main__":
    unittest.main()
