"""Offline cover-format and linked-application checks; no device access."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest

from cover_fixtures import bmp, library, INDEX
from test_web_save import node_check

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "tools/covertool/web"
BUILD = ROOT / "tests/build/web-cover"


class BrowserCoverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.temp = tempfile.TemporaryDirectory(prefix="fixtures-", dir=BUILD)
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        subprocess.run(["cargo", "build", "--locked", "--manifest-path", str(ROOT / "tools/Cargo.toml"),
                        "-p", "web-assets"], check=True, timeout=180)
        cls.tool = ROOT / "tools/target/debug/web-assets"

    def test_archive_and_native_controls(self):
        root = self.directory
        manifest, entries = [], []
        for i, (w, h, dib, top, gap) in enumerate([
            (1, 1, 40, False, 0), (3, 7, 52, True, 5), (127, 97, 56, False, 16),
            (17, 3, 108, True, 0), (128, 128, 40, False, 0), (128, 192, 124, True, 200),
        ]):
            data, palette, pixels = bmp(w, h, dib=dib, top=top, gap=gap)
            serial = f"GAME-{i:07}".encode()
            meta = dict(width=w, height=h, serial=serial.decode(), hash=0xFEDC0000+i)
            (root / f"{i}.bmp").write_bytes(data)
            (root / f"{i}.payload").write_bytes(palette+pixels)
            manifest.append(meta); entries.append((serial, meta["hash"], w, h, palette, pixels))
        (root / "fixtures.json").write_text(json.dumps(manifest))
        (root / "expected.bin").write_bytes(library(entries))
        noncanonical = bytearray(library([(b"ONE", 42, 3, 7, entries[1][4], entries[1][5])]))
        noncanonical[4:12] = b"PADDING!"; noncanonical[24:32] = b"RESERVED"
        noncanonical[64:72] = b"INACTIVE"
        noncanonical.extend(b"Trailing bytes must survive an untouched export.")
        (root / "noncanonical.bin").write_bytes(noncanonical)
        full = bytearray(INDEX)
        for i in range(16384):
            struct.pack_into("<12sIIHH", full, i*32, f"G{i}".encode(), i, INDEX, 1, 1)
        full.extend(bytes(range(256))*4+b"\x01")
        (root / "full.bin").write_bytes(full)
        batched = []
        for i in range(48):
            pixels = bytearray(entries[-1][5]); struct.pack_into("<H", pixels, 0, i)
            batched.append((f"B{i}".encode(), i, 128, 192, entries[-1][4], bytes(pixels)))
        (root / "batch.bin").write_bytes(library(batched))
        # Existing canonical interchange controls plus unaligned row widths.
        # Native safety/escaping/collision coverage has its own focused suite.
        native_entries = []
        for i, (w, h) in enumerate([(128, 128), (128, 192), (4, 4), (1, 1), (3, 7), (127, 97), (17, 3)]):
            _, palette, _ = bmp(w, h)
            pixels = bytes((j*7+i*37)%256 for j in range(w*h))
            native_entries.append((f"NATIVE{i}".encode(), 0x12345670+i, w, h, palette, pixels))
        (root / "native-expected.bin").write_bytes(library(native_entries))
        node_check("web_cover_codec.test.mjs", {"SAROO_WEB_FIXTURES": str(root)}, BUILD / "codec.log")
        self.assertEqual((root / "browser.bin").read_bytes(), (root / "expected.bin").read_bytes())
        self.assertEqual((root / "native-browser.bin").read_bytes(), (root / "native-expected.bin").read_bytes())
        native = root / "covertool"
        result = subprocess.run([os.environ.get("CC", "cc"), "-O1", "-g",
                                 "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                                 str(ROOT / "tools/covertool/main.c"), "-o", str(native)],
                                capture_output=True, text=True)
        (BUILD / "native-compile.log").write_text(result.stdout+result.stderr)
        self.assertEqual(result.returncode, 0, result.stderr)
        (root / "cover.bin").write_bytes((root / "native-browser.bin").read_bytes())
        commands = []
        for args in [("-u", "cover.bin"), ("-p", "coverlist.txt")]:
            check = subprocess.run([str(native), *args], cwd=root, capture_output=True, text=True, timeout=30,
                                   env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1", "ASAN_OPTIONS": "detect_leaks=0"})
            self.assertEqual(check.returncode, 0, check.stdout+check.stderr)
            self.assertNotIn("AddressSanitizer", check.stderr); self.assertNotIn("runtime error:", check.stderr)
            commands.append(dict(args=args, returncode=check.returncode, stdout=check.stdout, stderr=check.stderr))
        self.assertEqual((root / "cover.bin").read_bytes(), library(native_entries))
        for serial, hashed, w, h, palette, pixels in native_entries:
            actual = (root / f"cover_{serial.decode()}~{hashed:08X}.bmp").read_bytes()
            expected = bytearray(bmp(w, h, palette=palette, pixels=pixels)[0])
            # The independent input fixture fills padding with 0xDA; canonical
            # native exports deliberately clear non-pixel padding bytes.
            stride = (w + 3) & ~3
            for y in range(h):
                expected[1078+y*stride+w:1078+(y+1)*stride] = bytes(stride-w)
            self.assertEqual(actual, expected)
        (BUILD / "native-interchange.json").write_text(json.dumps({
            "commands": commands, "covers": len(native_entries),
            "browser_sha256": hashlib.sha256((root / "native-browser.bin").read_bytes()).hexdigest(),
            "native_source_sha256": hashlib.sha256((ROOT / "tools/covertool/main.c").read_bytes()).hexdigest(),
            "source": "current public native source", "hardware": False,
            "limits": "Seven canonical browser/native round trips, including padded rows; native format/escaping/fault checks are separate; no hardware qualification",
        }, indent=2)+"\n")

    def test_images_and_worker_ownership(self):
        node_check("web_cover_image.test.mjs", {}, BUILD / "image-worker.log")

    def test_reproducible_html_linked_exports_and_rejected_builds(self):
        receipts = []
        for name in ["one.html", "two.html"]:
            result = subprocess.run([str(self.tool), "cover", str(APP), str(self.directory / name)],
                                    check=True, capture_output=True, text=True, timeout=20)
            receipts.append(json.loads(result.stdout))
        self.assertEqual(receipts[0], receipts[1])
        self.assertEqual(receipts[0], json.loads((APP / "index.build.json").read_text()))
        self.assertEqual((self.directory / "one.html").read_bytes(), (self.directory / "two.html").read_bytes())
        self.assertEqual((self.directory / "one.html").read_bytes(), (APP / "index.html").read_bytes())
        html = (self.directory / "one.html").read_text()
        self.assertNotIn("__SAROO_", html); self.assertIn("GNU GENERAL PUBLIC LICENSE", html)
        worker = json.loads(re.search(r'<script id="worker-source" type="application/json">(.*?)</script>', html, re.S)[1])
        script = self.directory / "linked.mjs"
        script.write_text("import assert from 'node:assert/strict';import vm from 'node:vm';\n"
                          "const context=vm.createContext({self:{postMessage(){}},TextEncoder,TextDecoder,"
                          "performance,setTimeout,clearTimeout,AbortController,DOMException,Uint8Array,"
                          "Uint16Array,Uint32Array,DataView,ArrayBuffer,Blob,File});\n"
                          + "vm.runInContext("+json.dumps(worker)+",context);\n"
                          + "assert.equal(typeof context.self.onmessage,'function');\n"
                          + "\n".join("assert.deepEqual(Array.from(vm.runInContext('Object.keys("+ns+").sort()',context)),"
                                       "Object.keys(await import("+json.dumps((APP / "src" / name).as_uri())+")).sort());"
                                       for name, ns in [("cover-codec.mjs", "Cover"), ("cover-image.mjs", "Images"),
                                                        ("cover-worker.mjs", "WorkerApp")]))
        result = subprocess.run(["node", str(script)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        copied = self.directory / "source"; shutil.copytree(APP, copied)
        template = copied / "src/cover.html"
        template.write_text(template.read_text().replace("__SAROO_APP__", "absent"))
        output = self.directory / "preserved.html"; output.write_text("previous output")
        result = subprocess.run([str(self.tool), "cover", str(copied), str(output)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0); self.assertIn("exactly one", result.stderr)
        self.assertEqual(output.read_text(), "previous output")


if __name__ == "__main__":
    unittest.main()
