"""Public menu decoder against Python Unicode scalars and malformed subparts.

Adapted from paps-g SSR-158 / 20c97a9d for the original source layout.
"""
import io
import json
import os
from pathlib import Path
import random
import re
import struct
import subprocess
import unittest
from test_font_coverage import build_renderer

ROOT = Path(__file__).resolve().parents[1]


class MenuUtf8Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_UTF8_SOURCE', ROOT / 'Firm_Saturn/conio.c'))
        cls.target = Path(os.environ.get('SAROO_MENU_UTF8_OUTPUT',
                                        ROOT / 'tests/build/menu-utf8'))
        cls.target.mkdir(parents=True, exist_ok=True)
        match = re.search(r'static int utf8_to_ucs\([^\n]*\)\n\{.*?\n\}',
                          source.read_text(), re.S)
        if not match:
            raise AssertionError('actual menu decoder was not found')
        decoder = match[0]
        (cls.target / 'decoder.inc').write_text(decoder)
        cls.binary = cls.target / 'test-utf8'
        command = [os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                   '-Wall', '-Wextra', '-Werror', '-Wno-pointer-sign',
                   '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                   '-I' + str(cls.target), str(ROOT / 'tests/menu_utf8.c'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=30)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        result.check_returncode()

    def run_decoder(self, mode, *paths):
        result = subprocess.run([str(self.binary), mode, *map(str, paths)],
                                capture_output=True, timeout=30,
                                env={**os.environ, 'ASAN_OPTIONS': 'halt_on_error=1',
                                     'UBSAN_OPTIONS': 'halt_on_error=1'})
        (self.target / (mode + '.log')).write_bytes(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b'pass', result.stdout)
        self.assertEqual(result.stderr, b'')

    def test_valid_bmp_and_supplementary_scalar_controls(self):
        self.run_decoder('controls')

    def test_supplementary_label_renders_one_fallback_square(self):
        renderer = build_renderer(self.target, fixture='menu_utf8_render.c',
                                  source_path=os.environ.get('SAROO_MENU_UTF8_SOURCE'))
        result = subprocess.run([str(renderer), str(ROOT / 'Firm_Saturn/font_cjk.bin')],
                                capture_output=True, timeout=15,
                                env={**os.environ, 'ASAN_OPTIONS': 'halt_on_error=1',
                                     'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(b'one supplementary scalar', result.stdout)

    def test_every_nonzero_unicode_scalar_and_exact_byte_extent(self):
        # The reference encoder is Python's standard Unicode codec, not C's
        # inverse bit arithmetic. Surrogates are not Unicode scalar values.
        path = self.target / 'all-scalars.utf8'
        with path.open('wb') as output:
            for low, high in ((1, 0xd800), (0xe000, 0x110000)):
                output.write(''.join(map(chr, range(low, high))).encode('utf-8'))
        self.run_decoder('scalars', path)

    def test_malformed_subparts_preserve_following_text_and_nul_bounds(self):
        # Independent strict-codec error extents define a maximal ill-formed
        # subpart. Menu policy displays '?' for each, preserving later text.
        def reference(data):
            pairs = []
            while data:
                try:
                    text = data.decode('utf-8')
                except UnicodeDecodeError as error:
                    text = data[:error.start].decode('utf-8')
                    pairs.extend((ord(ch), len(ch.encode('utf-8'))) for ch in text)
                    pairs.append((ord('?'), error.end - error.start))
                    data = data[error.end:]
                else:
                    pairs.extend((ord(ch), len(ch.encode('utf-8'))) for ch in text)
                    break
            return pairs

        corpus = io.BytesIO()
        count = 0
        def add(data):
            nonlocal count
            assert b'\0' not in data
            expected = reference(data)
            corpus.write(struct.pack('>II', len(data), len(expected)))
            corpus.write(data)
            for scalar, extent in expected:
                corpus.write(struct.pack('>II', scalar, extent))
            count += 1

        add(b'')
        for first in range(1, 256):
            add(bytes([first]))
            for second in range(1, 256):
                add(bytes([first, second]) + b'Z')
        # All prefixes at length/range boundaries, including genuine U+FFFD
        # and valid noncharacters. No reject policy for assignedness is added.
        for scalar in (0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xfffd, 0xffff,
                       0x10000, 0x1f600, 0x20000, 0x10ffff):
            encoded = chr(scalar).encode('utf-8')
            for length in range(len(encoded) + 1):
                add(encoded[:length])
                add(b'A' + encoded[:length] + b'Z')
        rng = random.Random(158)
        for _ in range(4096):
            add(bytes(rng.randrange(1, 256) for _ in range(rng.randrange(1, 33))) + b'Z')
        path = self.target / 'malformed.bin'
        path.write_bytes(corpus.getvalue())
        (self.target / 'reference.json').write_text(json.dumps({
            'cases': count, 'seed': 158,
            'oracle': 'Python strict UTF-8 codec values and error start/end extents',
            'policy': 'one question mark per maximal ill-formed subpart',
            'scope': 'NUL-terminated menu strings; no embedded-NUL API'
        }, indent=2) + '\n')
        self.run_decoder('malformed', path)


if __name__ == '__main__':
    unittest.main()
