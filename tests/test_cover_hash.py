"""Actual firmware title hashes versus zlib, including exact modular boundaries."""
import hashlib
import json
import os
from pathlib import Path
import random
import struct
import subprocess
import shlex
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]


def hash_source(data):
    """Retain the production bounds and function; also exercise older sources."""
    if b'static u32 cover_title_hash(' in data:
        start = data.index(b'#define COVER_TITLE_BYTES ')
    else:
        start = data.index(b'u32 adler32(')
    return data[start:data.index(b'/******************************************************************************/', start)]


class CoverHashTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_COVER_HASH_SOURCE',
                                     ROOT / 'Firm_MCU/Saturn/saturn_utils.c'))
        code = hash_source(source.read_bytes())
        build = ROOT / 'tests/build/cover-hash'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        cls.source_sha256 = hashlib.sha256(code).hexdigest()
        call = b'cover_title_hash(data)' if b'cover_title_hash(' in code else b'adler32(data, 64)'
        code += b'\nstatic u32 test_title_hash(u8 *data) { return ' + call + b'; }\n'
        (cls.target / 'title-hash.inc').write_bytes(code)
        cls.binary = cls.target / 'test-hash'
        flags = (['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE') == 'release'
                 else ['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
                   str(ROOT / 'tests/cover_hash.c'), '-o', str(cls.binary)]
        p = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(p.stdout + p.stderr)
        p.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(
            dict(source_sha256=cls.source_sha256, cases=cls.results), indent=2) + '\n')

    def check_titles(self, name, titles):
        corpus = self.target / (name + '.bin')
        count = 0
        with corpus.open('wb') as out:
            for title in titles:
                self.assertEqual(len(title), 64)
                out.write(title + struct.pack('<I', zlib.adler32(title)))
                count += 1
        p = subprocess.run([str(self.binary), str(corpus)], capture_output=True, timeout=30,
                           env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(name=name, titles=count, returncode=p.returncode,
                                 stdout=p.stdout.decode(errors='replace'),
                                 stderr=p.stderr.decode(errors='replace')))
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(p.stdout, f'{count} titles, {count * 8} aligned/unaligned calls\n'.encode())

    def test_every_uniform_byte_and_extreme_sum(self):
        self.check_titles('uniform', (bytes([byte]) * 64 for byte in range(256)))

    def test_every_position_and_byte_value(self):
        self.check_titles('positions', (bytes(position) + bytes([byte]) + bytes(63 - position)
                                       for position in range(64) for byte in range(256)))

    def test_both_sides_of_every_reachable_modulus_multiple(self):
        titles = []
        for multiple in range(1, 9):
            for delta in (-1, 0, 1):
                remaining = 65521 * multiple + delta - 64
                title = bytearray(64)
                for i in range(64):
                    title[i] = min(255, remaining // (64 - i))
                    remaining -= title[i] * (64 - i)
                self.assertEqual(remaining, 0)
                self.assertEqual(64 + sum((64 - i) * b for i, b in enumerate(title)),
                                 65521 * multiple + delta)
                titles.append(title)
        self.check_titles('modulus', titles)

    def test_deterministic_mixed_titles_including_embedded_zeroes(self):
        rng = random.Random(0x5341524f4f)
        self.check_titles('mixed', (rng.randbytes(64) for _ in range(4096)))


if __name__ == '__main__':
    unittest.main()
