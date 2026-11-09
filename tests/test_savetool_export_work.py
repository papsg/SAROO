"""Poisoned actual-CLI exports: complete independent bytes, work and round trips."""
# Original optimization/evidence: paps-g 99127a6a. All media are synthetic files.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile
import unittest
import zlib

from save_fixtures import bup_save, raw_save, mems_fixture, slot_fixture, saturn_fixture, disk_bytes

ROOT = Path(__file__).resolve().parents[1]
NAME = b'SAVE_A00001'
DATE = bytes.fromhex('01020304')
WORKLOADS = [('mems', (0, 1, 960, 961, 32768, 479 * 1024)),
             ('slot', (0, 1, 127, 128, 129, 32768, 65280)),
             ('plain', (0, 28, 29, 840, 841, 29550)),
             ('odd', (0, 28, 29, 840, 841, 29550))]


def fixture(kind, payload, empty=False):
    if kind == 'mems': return mems_fixture([] if empty else [(0, NAME, payload)])[0]
    if kind == 'slot': return slot_fixture([] if empty else [(NAME, payload)])[0]
    return disk_bytes(saturn_fixture([(NAME, payload)])[0], kind == 'odd')


def expected_file(payload, fmt, date=DATE, comment=b'Generated!', language=1):
    out = raw_save(NAME, payload, date) if fmt == 'SRO' else bup_save(NAME, payload, date)
    if fmt == 'SRO':
        out[32:42] = comment; out[43] = language
        struct.pack_into('>I', out, 12, zlib.crc32(out[16:]))
    else:
        out[28:38] = comment; out[39] = language; out[52:56] = date
    return out


class ExportWorkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base = ROOT / 'tests/build/export-work'; base.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=base)); cls.results = []
        source = Path(os.environ.get('SAROO_SAVETOOL_SOURCE', ROOT / 'tools/savetool'))
        staged = cls.target / 'source'; staged.mkdir(); hashes = {}
        for path in sorted(source.iterdir()):
            if path.suffix not in ('.c', '.h'): continue
            data = path.read_bytes(); hashes[path.name] = hashlib.sha256(data).hexdigest()
            original = cls.target / 'inputs' / path.name; original.parent.mkdir(exist_ok=True)
            original.write_bytes(data)
            if path.name == 'main.c':
                old = b'u32 crc32b(u8 *data, int size)'; assert data.count(old) == 1
                data = data.replace(old, b'u32 export_work_crc_impl(u8 *data, int size)')
                old = b'int main(int argc, char *argv[])'; assert data.count(old) == 1
                data = data.replace(old, b'int savetool_cli_main(int argc, char *argv[])')
            else:
                # Only replace the export's actual scratch clear; other memsets stay real.
                data, count = re.subn(rb'memset\(save_buf, 0, ([^;]+)\);',
                                     rb'export_work_clear(save_buf, 0, \1);', data)
                if path.suffix == '.c': assert count == 1, path.name
            (staged / path.name).write_bytes(data)
        for name in ('savetool_export_work.c', 'savetool_export_work.h', 'test_savetool_export_work.py', 'save_fixtures.py'):
            data = (ROOT / 'tests' / name).read_bytes(); (cls.target / name).write_bytes(data)
            hashes['tests/' + name] = hashlib.sha256(data).hexdigest()
        (cls.target / 'inputs.json').write_text(json.dumps(hashes, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_SAVETOOL_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'sst'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall',
                   '-fno-strict-aliasing', *flags, '-I', str(staged), '-I', str(cls.target),
                   '-include', str(cls.target / 'savetool_export_work.h'),
                   *map(str, sorted(staged.glob('*.c'))), str(cls.target / 'savetool_export_work.c'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def run_cli(self, directory, kind, *args):
        command = [str(self.binary), 'archive.bin', *(['-t', '1'] if kind == 'slot' else []), *map(str, args)]
        result = subprocess.run(command, cwd=directory, capture_output=True, timeout=20,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'ASAN_OPTIONS': 'detect_leaks=0'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn(b'runtime error:', result.stderr)
        self.assertNotIn(b'AddressSanitizer', result.stderr)
        match = re.search(rb'EXPORT_WORK clear_calls=(\d+) clear_bytes=(\d+) crc_calls=(\d+) crc_bytes=(\d+)', result.stderr)
        self.assertIsNotNone(match, result.stderr)
        return list(map(int, match.groups()))

    def export_case(self, kind, size, fmt, check_work):
        payload = bytes((i * 13 + 29) % 251 for i in range(size))
        image = fixture(kind, payload)
        with tempfile.TemporaryDirectory(dir=self.target) as tmp:
            directory = Path(tmp); archive = directory / 'archive.bin'; archive.write_bytes(image)
            # Independent expected bytes include all reserved fields, not just payload/CRC.
            metrics = self.run_cli(directory, kind, '-s' if fmt == 'SRO' else '-x', 0)
            out = (directory / (NAME.decode() + '.' + fmt)).read_bytes()
            self.assertEqual(out, expected_file(payload, fmt))
            self.assertEqual(archive.read_bytes(), image)
            self.results.append(dict(kind=kind, size=size, format=fmt, check_work=check_work,
                metrics=metrics, sha256=hashlib.sha256(out).hexdigest(), input_unchanged=True))
            if check_work:
                self.assertEqual(metrics, [1, 64, int(fmt == 'SRO'), size + 48 if fmt == 'SRO' else 0])

    def test_poisoned_exports_match_every_expected_byte(self):
        for kind, sizes in WORKLOADS:
            for size in sizes:
                for fmt in ('SRO', 'BUP'):
                    with self.subTest(kind=kind, size=size, fmt=fmt):self.export_case(kind, size, fmt, False)

    def test_export_work_is_bounded_to_its_header_and_retained_crc(self):
        for kind, sizes in WORKLOADS:
            for size in sizes:
                for fmt in ('SRO', 'BUP'):
                    with self.subTest(kind=kind, size=size, fmt=fmt):self.export_case(kind, size, fmt, True)

    def test_bup_import_reexports_complete_metadata_and_payload(self):
        for kind, sizes in [('mems', (0, 1, 961, 479 * 1024)), ('slot', (0, 1, 129, 65280))]:
            for size in sizes:
                with self.subTest(kind=kind, size=size), tempfile.TemporaryDirectory(dir=self.target) as tmp:
                    directory = Path(tmp); archive = directory / 'archive.bin'
                    archive.write_bytes(fixture(kind, b'', empty=True))
                    data = bytes((i * 11 + 43) % 251 for i in range(size))
                    date = bytes.fromhex('0102ccdd'); comment = b'Alt meta!!'
                    incoming = bup_save(NAME, data, date); incoming[28:38] = comment; incoming[39] = 2
                    (directory / 'incoming.BUP').write_bytes(incoming)
                    self.run_cli(directory, kind, '-i', 'incoming.BUP')
                    saved = archive.read_bytes()
                    for fmt in ('SRO', 'BUP'):
                        self.run_cli(directory, kind, '-s' if fmt == 'SRO' else '-x', 0)
                        self.assertEqual((directory / (NAME.decode() + '.' + fmt)).read_bytes(),
                                         expected_file(data, fmt, date, comment, 2))
                        self.assertEqual(archive.read_bytes(), saved)
                    self.results.append(dict(kind=kind, size=size, imported_bup_roundtrip=True))


if __name__ == '__main__': unittest.main()
