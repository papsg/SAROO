"""Public source/fixture checks for the offline save editor; no device operations."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from save_fixtures import fixtures, slot_fixture

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "tools/savetool/web"
BUILD = ROOT / "tests/build/web-save"
SAX = APP / "vendor/sax/lib/sax.js"


def node_check(name, environment, output):
    node = shutil.which("node")
    if not node:
        raise AssertionError("Node.js 22 or newer is required for the browser-editor checks")
    result = subprocess.run([node, "--test", "--test-reporter=tap", str(ROOT / "tests" / name)],
                            cwd=ROOT, env={**os.environ, **environment}, capture_output=True,
                            text=True, timeout=180)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(result.stdout+result.stderr)
    if result.returncode:
        raise AssertionError(result.stdout+result.stderr)
    return result



class BrowserSaveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BUILD.mkdir(parents=True, exist_ok=True)
        cls.temp = tempfile.TemporaryDirectory(prefix="fixtures-", dir=BUILD)
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        cls.manifest = fixtures(cls.directory)
        subprocess.run(["cargo", "build", "--locked", "--manifest-path", str(ROOT / "tools/Cargo.toml"),
                        "-p", "web-assets"], check=True, timeout=180)
        cls.tool = ROOT / "tools/target/debug/web-assets"

    def test_archive_core_independent_corpus_and_native_interchange(self):
        node_check("web_save_codec.test.mjs", {"SAROO_WEB_SAVE_FIXTURES": str(self.directory)}, BUILD / "codec.log")
        # Use the actual public C utility without private patches or preparers.
        native = self.directory / "savetool"
        sources = sorted((ROOT / "tools/savetool").glob("*.c"))
        compile_result = subprocess.run([os.environ.get("CC", "cc"), "-O1", "-g", "-fcommon",
                                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                                        *map(str, sources), "-o", str(native)], capture_output=True, text=True)
        (BUILD / "native-compile.log").write_text(compile_result.stdout+compile_result.stderr)
        self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
        checked = []
        for output in json.loads((self.directory / "interchange.json").read_text()):
            for index, save in enumerate(output["saves"]):
                for switch, extension in [("-s", "SRO"), ("-x", "BUP")]:
                    arguments = [str(native), str(self.directory / output["file"])]
                    if output["type"] == "saroo":
                        arguments += ["-t", str(output["volume"])]
                    arguments += [switch, str(index)]
                    result = subprocess.run(arguments, cwd=self.directory, capture_output=True, text=True,
                                            timeout=10, env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1",
                                                             "ASAN_OPTIONS": "detect_leaks=0"})
                    self.assertEqual(result.returncode, 0,
                                     f"{output['file']} volume {output['volume']} save {index} {extension}:\n"
                                     +result.stdout+result.stderr)
                    self.assertNotIn("AddressSanitizer", result.stderr)
                    self.assertNotIn("runtime error:", result.stderr)
                    expected = self.directory / (save["prefix"]+"."+extension)
                    actual = self.directory / save[extension]
                    self.assertEqual(actual.read_bytes(), expected.read_bytes(), str(actual))
                    checked.append({"archive": output["file"], "volume": output["volume"], "index": index,
                                    "format": extension, "sha256": hashlib.sha256(actual.read_bytes()).hexdigest()})
        self.assertTrue(checked)
        maximum = (self.directory / "saroo-maximum-native.bin").read_bytes()
        expected, locations = slot_fixture([(b"MAXIMUM0001", bytes((i*7+27)%251 for i in range(65280)))])
        header = locations[0][0]
        expected[header+16:header+26] = bytes(10)
        expected[header+27:header+32] = bytes(5)
        self.assertEqual(maximum, expected)
        (BUILD / "native-interchange.json").write_text(json.dumps({"native_exports": checked}, indent=2)+"\n")

    def test_worker_ownership_and_atomic_dialog_changes(self):
        node_check("web_save_worker.test.mjs", {"SAROO_SAVE_WORKER_FIXTURES": str(self.directory),
                                               "SAROO_SAVE_SAX": str(SAX)}, BUILD / "worker.log")

    def test_reproducible_offline_html_and_linked_exports(self):
        outputs = [self.directory / "one.html", self.directory / "two.html"]
        receipts = []
        for output in outputs:
            result = subprocess.run([str(self.tool), "save", str(APP), str(output)],
                                    check=True, capture_output=True, text=True, timeout=20)
            receipts.append(json.loads(result.stdout))
        self.assertEqual(receipts[0], receipts[1])
        self.assertEqual(outputs[0].read_bytes(), outputs[1].read_bytes())
        self.assertEqual(outputs[0].read_bytes(), (APP / "index.html").read_bytes())
        self.assertEqual(receipts[0], json.loads((APP / "index.build.json").read_text()))
        html = outputs[0].read_text()
        self.assertNotIn("__SAROO_", html)
        self.assertIn("Blue Oak Model License", html)
        self.assertIn("GNU GENERAL PUBLIC LICENSE", html)
        worker = json.loads(re.search(r'<script id="worker-source" type="application/json">(.*?)</script>', html, re.S)[1])
        # Compare the exact linked module APIs with native ESM, then boot the real
        # bundled worker. No DOM mock is substituted for full browser acceptance.
        script = self.directory / "linked.mjs"
        script.write_text("import assert from 'node:assert/strict';\nimport vm from 'node:vm';\n"
                          "const sent=[];const context=vm.createContext({self:{postMessage:v=>sent.push(v)},"
                          "TextDecoder,TextEncoder,Uint8Array,Uint16Array,Uint32Array,ArrayBuffer,DataView,"
                          "Blob,File,AbortController,DOMException,performance,setTimeout,clearTimeout,atob,btoa});\n"
                          + "vm.runInContext("+json.dumps(worker)+",context);\n"
                          + "assert.equal(typeof context.self.onmessage,'function');\n"
                          + "context.self.onmessage({data:{op:'init',id:1}});await new Promise(r=>setTimeout(r,0));\n"
                          + "assert.equal(sent[0].id,1);assert.equal(sent[0].result.pageSize,24);\n"
                          + "\n".join("assert.deepEqual(Array.from(vm.runInContext('Object.keys("+namespace+").sort()',context)),"
                                       "Object.keys(await import("+json.dumps((APP / "src" / name).as_uri())+")).sort());"
                                       for name, namespace in [("save-fields.mjs", "Fields"), ("save-text.mjs", "Text"),
                                                               ("save-xml.mjs", "XML"), ("save-codec.mjs", "Archive"),
                                                               ("save-worker.mjs", "WorkerApp")]))
        result = subprocess.run(["node", str(script)], capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_bad_dependency_or_template_does_not_replace_output(self):
        copied = self.directory / "source"
        shutil.copytree(APP, copied)
        output = self.directory / "preserved.html"
        output.write_text("previous output")
        source = copied / "vendor/sax/lib/sax.js"
        original = source.read_bytes()
        source.write_bytes(original+b"\n// changed\n")
        result = subprocess.run([str(self.tool), "save", str(copied), str(output)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("identity mismatch", result.stderr)
        self.assertEqual(output.read_text(), "previous output")
        source.write_bytes(original)
        template = copied / "src/save.html"
        template.write_text(template.read_text().replace("__SAROO_APP__", "absent"))
        result = subprocess.run([str(self.tool), "save", str(copied), str(output)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exactly one", result.stderr)
        self.assertEqual(output.read_text(), "previous output")

    def test_vendored_sources_and_notices_match_the_pinned_revision(self):
        vendor = APP / "vendor/sax"
        record = json.loads((vendor / "sources.json").read_text())
        self.assertEqual(record["head"], "961ceeefcc5186005466f2085aeb36f31b161e37")
        self.assertEqual(set(record["files"]), {"lib/sax.js", "LICENSE.md", "README.md", "package.json"})
        for name, entry in record["files"].items():
            self.assertEqual(hashlib.sha256((vendor / name).read_bytes()).hexdigest(), entry["sha256"])


if __name__ == "__main__":
    unittest.main()
