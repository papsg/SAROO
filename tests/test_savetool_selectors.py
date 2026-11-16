"""Actual CLI selector admission (paps-g bda37c83 / SSR-044).

Uses the public legacy slot geometry; source/binary/results and disposable
archive/output snapshots are retained under ignored tests/build/savetool.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

import gzip
import shutil
from save_fixtures import mems_fixture, raw_save, slot_fixture
from savetool_support import NativeSaveTest, BUILD


class SaveSelectorTests(NativeSaveTest):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.target = Path(tempfile.mkdtemp(prefix='selectors-', dir=BUILD))
        cls.binary_snapshot = cls.target / 'sst'
        shutil.copy2(cls.binary, cls.binary_snapshot)
        cls.binary = cls.binary_snapshot
        source = cls.target / 'source'; source.mkdir()
        for file in cls.source.iterdir():
            if file.suffix in ('.c', '.h'): shutil.copyfile(file, source / file.name)
        inputs = source / 'tests'; inputs.mkdir()
        for name in ('test_savetool_selectors.py', 'savetool_support.py', 'save_fixtures.py'):
            shutil.copyfile(Path(__file__).parent / name, inputs / name)
        entries = [(b'FIRST', b'first payload'), (b'SECOND', b'second payload')]
        for name, original in [('mems', mems_fixture([(i, n, d) for i, (n, d) in enumerate(entries)])[0]),
                               ('slot', slot_fixture(entries)[0])]:
            with gzip.open(cls.target / (name + '-original.gz'), 'wb', compresslevel=1) as out:
                out.write(original)
        (cls.target / 'inputs.json').write_text(json.dumps({str(p.relative_to(source)):
            hashlib.sha256(p.read_bytes()).hexdigest() for p in source.rglob('*') if p.is_file()}, indent=2) + '\n')
        cls.results = []

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, directory, args):
        result = subprocess.run([str(self.binary), 'archive.bin', *args], cwd=directory,
            capture_output=True, timeout=10, env={'PATH': os.defpath, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        capture = self.target / f'command-{len(self.results):03d}'; capture.mkdir()
        for file in directory.iterdir():
            if file.is_file():
                with gzip.open(capture / (file.name + '.gz'), 'wb', compresslevel=1) as out:
                    out.write(file.read_bytes())
        self.results.append(dict(args=args, returncode=result.returncode,
            stdout=result.stdout.decode(errors='replace'), stderr=result.stderr.decode(errors='replace'),
            archive_sha256=hashlib.sha256((directory / 'archive.bin').read_bytes()).hexdigest()))
        self.assertNotIn(b'AddressSanitizer', result.stderr)
        self.assertNotIn(b'runtime error:', result.stderr)
        return result

    def fixture(self, directory, slot=False):
        entries = [(b'FIRST', b'first payload'), (b'SECOND', b'second payload')]
        original = slot_fixture(entries)[0] if slot else mems_fixture(
            [(i, name, data) for i, (name, data) in enumerate(entries)])[0]
        (directory / 'archive.bin').write_bytes(original)
        (directory / 'incoming.SRO').write_bytes(raw_save(b'THIRD', b'incoming payload'))
        return original

    def test_malformed_save_selectors_refuse_delete_overwrite_and_exports(self):
        malformed = ['nonsense', '0junk', '4294967296', '2147483648', '9' * 80,
                     '', '-1', '-0', '+0', ' 0', '0 ', '0x0', '0.0', '\u0660']
        for flag, action in [('-s', ['-d']), ('-s', ['-i', 'incoming.SRO']),
                             ('-s', []), ('-x', [])]:
            for value in malformed:
                with self.subTest(flag=flag, action=action, value=value), tempfile.TemporaryDirectory() as temp:
                    directory = Path(temp); original = self.fixture(directory)
                    result = self.invoke(directory, [flag, value, *action])
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual((directory / 'archive.bin').read_bytes(), original)
                    self.assertNotIn(b'Updated archive:', result.stdout)
                    self.assertEqual({p.name for p in directory.iterdir()}, {'archive.bin', 'incoming.SRO'})

    def test_malformed_slot_selectors_refuse_before_mutation(self):
        for value in ['1junk', 'nonsense', '4294967297', '2147483648', '9' * 80,
                      '', '-1', '-0', '+1', ' 1', '1 ', '0x1', '1.0', '\u0661']:
            with self.subTest(value=value), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp); original = self.fixture(directory, slot=True)
                result = self.invoke(directory, ['-t', value, '-s', '0', '-d'])
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((directory / 'archive.bin').read_bytes(), original)
                self.assertNotIn(b'Updated archive:', result.stdout)

    def test_valid_decimal_selection_preserves_neighbor_and_export_payloads(self):
        for slot in [False, True]:
            with self.subTest(slot=slot), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp); self.fixture(directory, slot=slot)
                prefix = ['-t', '0001'] if slot else []
                result = self.invoke(directory, [*prefix, '-s', '01', '-d'])
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                listing = self.invoke(directory, prefix)
                self.assertEqual(listing.returncode, 0)
                self.assertIn(b'FIRST', listing.stdout); self.assertNotIn(b'SECOND', listing.stdout)
                for flag, extension in [('-s', 'SRO'), ('-x', 'BUP')]:
                    result = self.invoke(directory, [*prefix, flag, '000'])
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual((directory / ('FIRST.' + extension)).read_bytes()[64:], b'first payload')

    def test_unspecified_import_and_representable_missing_indices(self):
        for slot in [False, True]:
            with self.subTest(slot=slot), tempfile.TemporaryDirectory() as temp:
                directory = Path(temp); original = self.fixture(directory, slot=slot)
                prefix = ['-t', '1'] if slot else []
                for value in ['2147483647', '999']:
                    result = self.invoke(directory, [*prefix, '-s', value, '-d'])
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual((directory / 'archive.bin').read_bytes(), original)
                if slot:
                    for value in ['0', '2147483647']:
                        result = self.invoke(directory, ['-t', value, '-s', '0', '-d'])
                        self.assertNotEqual(result.returncode, 0)
                        self.assertEqual((directory / 'archive.bin').read_bytes(), original)
                result = self.invoke(directory, [*prefix, '-i', 'incoming.SRO'])
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                listing = self.invoke(directory, prefix)
                self.assertEqual(listing.returncode, 0); self.assertIn(b'THIRD', listing.stdout)
