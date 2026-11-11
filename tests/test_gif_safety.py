"""Actual GIF loader/decoder, generated wire records and independent RGB output."""
import os
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import json
import shlex
import tempfile
import unittest

# Adapted from paps-g 3e62998c; no private preparation or runtime diagnostics.
ROOT = Path(__file__).resolve().parents[1]


def pack_codes(codes, minimum):
    """GIF LSB-first code stream, including growth and deferred table clears."""
    clear = 1 << minimum
    width, next_code, previous = minimum + 1, clear + 2, False
    packed, bits, count = bytearray(), 0, 0
    for code in codes:
        assert code < 1 << width
        bits |= code << count
        count += width
        while count >= 8:
            packed.append(bits & 255)
            bits >>= 8
            count -= 8
        if code == clear:
            width, next_code, previous = minimum + 1, clear + 2, False
        elif code != clear + 1:
            if previous and next_code < 4096:
                next_code += 1
                if next_code == 1 << width and width < 12:
                    width += 1
            previous = True
    if count:
        packed.append(bits)
    return packed


def blocks(data, split):
    return b''.join(bytes([len(data[i:i + split])]) + data[i:i + split]
                    for i in range(0, len(data), split)) + b'\0'


PALETTE = bytes([0, 0, 0, 255, 16, 32, 32, 255, 48, 48, 64, 255])


def image(w, h, pixels, *, split=255, codes=None, minimum=2,
          x=0, y=0, local=False, interlaced=False):
    if codes is None:
        sequence = list(pixels)
        if interlaced:
            sequence = [pixels[row * w + col]
                        for start, step in [(0, 8), (4, 8), (2, 4), (1, 2)]
                        for row in range(start, h, step) for col in range(w)]
        codes = [1 << minimum, *sequence, (1 << minimum) + 1]
    descriptor = b',' + struct.pack('<4HB', x, y, w, h,
                                    (0x81 if local else 0) | (0x40 if interlaced else 0))
    return descriptor + (PALETTE if local else b'') + bytes([minimum]) + blocks(pack_codes(codes, minimum), split)


def header(w, h, *, global_palette=True, version=b'89a'):
    return b'GIF' + version + struct.pack('<HHBBB', w, h, 0x81 if global_palette else 0, 0, 0) + (PALETTE if global_palette else b'')


def rgb(indices):
    return b''.join(PALETTE[v * 3:v * 3 + 3] for v in indices)


def expected(w, h, frames):
    return struct.pack('<3H', w, h, len(frames)) + b''.join(
        struct.pack('<5H', *rect, delay) + rgb(pixels)
        for rect, delay, pixels in frames)


class GifSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source_path = Path(os.environ.get('SAROO_GIF_SOURCE', ROOT / 'Firm_MCU/Saturn/tinygif.c'))
        main_path = Path(os.environ.get('SAROO_GIF_MAIN', ROOT / 'Firm_MCU/Saturn/saturn_main.c'))
        build = ROOT / 'tests/build/gif-safety'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        inputs = {'tinygif.c': source_path.read_bytes(), 'saturn_main.c': main_path.read_bytes(),
                  'cdc.h': (ROOT / 'Firm_MCU/Saturn/cdc.h').read_bytes()}
        for name in ('test_gif_safety.py', 'gif_safety.c'):
            inputs[name] = (ROOT / 'tests' / name).read_bytes()
        for name, data in inputs.items():
            (cls.target / name).write_bytes(data)
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(data).hexdigest() for name, data in inputs.items()}, indent=2)+'\n')
        source = re.sub(rb'^#include[^\n]*\n', b'', inputs['tinygif.c'], flags=re.M)
        source = re.sub(rb'0x240([0-9a-fA-F]{5})',
                        lambda m: b'(GIF_TEST_AXI + 0x' + m[1] + b')', source)
        source = re.sub(rb'0x614([0-9a-fA-F]{5})',
                        lambda m: b'(GIF_TEST_GFX + 0x' + m[1] + b')', source)
        layout = b'\n'.join(line for line in inputs['cdc.h'].splitlines()
                            if line.startswith(b'#define COVER_INDEX_OFFSET '))
        (cls.target / 'gif.inc').write_bytes(layout + b'\n' + source)
        main = inputs['saturn_main.c']
        case = main[main.index(b'case SSCMD_LOADDISC:'):main.index(b'case SSCMD_CHECK:')]
        (cls.target / 'handoff.inc').write_bytes(
            b'static void dispatch_load(void) { int retv; switch (SS_CMD) {\n' + case + b'\n} }\n')
        cls.binary = cls.target / 'test-gif'
        flags = ['-O2'] if os.environ.get('SAROO_GIF_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g', '-Wall',
                   '-fno-strict-aliasing', *flags, '-I', str(cls.target),
                   str(cls.target / 'gif_safety.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(command, indent=2)+'\n')
        result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2)+'\n')

    def invoke(self, mode, data, output=None):
        path = self.target / 'input.gif'
        path.write_bytes(data)
        args = [str(self.binary), mode, str(path)]
        if isinstance(output, bytes):
            result_path = self.target / 'expected.rgb'
            result_path.write_bytes(output)
            args.append(str(result_path))
        elif output is not None:
            args.append(str(output))
        try:
            result = subprocess.run(args, capture_output=True, text=True, timeout=5,
                                    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        except subprocess.TimeoutExpired:
            self.results.append(dict(mode=mode, input_sha256=hashlib.sha256(data).hexdigest(),
                                     timeout=True))
            raise
        self.results.append(dict(mode=mode, input_sha256=hashlib.sha256(data).hexdigest(),
                                 returncode=result.returncode, stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('pass', result.stdout)
        if mode == 'valid':
            corpus = self.target / 'corpus'
            corpus.mkdir(exist_ok=True)
            name = hashlib.sha256(data).hexdigest()
            (corpus / (name + '.gif')).write_bytes(data)
            (corpus / (name + '.rgb')).write_bytes(output)

    def check_image(self, w, h, pixels, **options):
        self.invoke('valid', header(w, h, global_palette=not options.get('local')) +
                    image(w, h, pixels, **options) + b';',
                    expected(w, h, [((w, h, 0, 0), 0, pixels)]))

    def test_supported_first_frame_controls(self):
        for version in (b'87a', b'89a'):
            with self.subTest(version=version):
                self.invoke('first', header(1, 1, version=version) + image(1, 1, [1]) + b';',
                            expected(1, 1, [((1, 1, 0, 0), 0, [1])]))

    def test_cancellation_at_publication_and_clean_reinitialization(self):
        self.invoke('cancel', header(1, 1) + image(1, 1, [1]) + b';')

    def test_lzw_width_growth_full_dictionary_and_subblock_splits(self):
        for split in [1, 2, 3, 7, 255]:
            with self.subTest(split=split):
                pixels = [i % 4 for i in range(128 * 64)]
                self.check_image(128, 64, pixels, split=split)
        self.check_image(3, 1, [1] * 3, codes=[4, 1, 6, 5])
        self.check_image(4, 1, [1, 1, 2, 3], codes=[4, 1, 1, 4, 2, 3, 5])

    def test_all_code_sizes_and_256_palette_entries(self):
        for minimum in range(2, 9):
            with self.subTest(minimum=minimum):
                pixels = [i % 4 for i in range(1024)]
                self.check_image(32, 32, pixels, minimum=minimum)
        palette = bytes(channel for i in range(256)
                        for channel in (i, 255 - i, i ^ 0x5a))
        pixels = [i % 256 for i in range(128 * 64)]
        data = b'GIF89a' + struct.pack('<HHBBB', 128, 64, 0x87, 0, 0) + palette
        data += image(128, 64, pixels, minimum=8) + b';'
        output = struct.pack('<8H', 128, 64, 1, 128, 64, 0, 0, 0)
        output += b''.join(palette[i * 3:i * 3 + 3] for i in pixels)
        self.invoke('valid', data, output)

    def test_palette_locations_interlacing_and_small_dimensions(self):
        for h in [1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17]:
            for local in [False, True]:
                with self.subTest(height=h, local=local):
                    pixels = [(i * 3 + i // 7) % 4 for i in range(7 * h)]
                    self.check_image(7, h, pixels, local=local, interlaced=True)
        # Repeated-symbol dictionary references keep this maximum canvas within
        # the independent compressed-file capacity.
        self.check_image(512, 512, [1] * (512 * 512),
                         codes=[4, 1, *range(6, 728), 422, 5])

    def test_transparency_control_scope_subrectangles_and_wrapping_delay(self):
        first = [1] * 12
        second = [1, 2, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1]
        third = [0] * 12
        data = header(4, 3) + b'!\xf9\x04\x00\x05\x00\x01\0' + image(4, 3, first)
        data += b'!\xfe' + blocks(b'comment', 3)
        data += b'!\xf9\x04\x01\x04\0\0\0' + image(2, 2, [2, 0, 3, 0], x=1)
        data += image(4, 3, third) + b';'
        self.invoke('valid', data, expected(4, 3, [((4, 3, 0, 0), 5, first),
                    ((2, 2, 1, 0), 4, second), ((4, 3, 0, 0), 0, third)]))

    def test_every_truncation_before_a_complete_frame_is_rejected(self):
        data = header(3, 2) + b'!\xf9\x04\x01\x01\0\0\0' + image(3, 2, [0, 1, 2, 3, 2, 1], split=1) + b';'
        for size in range(len(data) - 1):
            with self.subTest(size=size):
                self.invoke('reject', data[:size])

    def test_invalid_header_rectangles_codes_and_pixel_counts(self):
        examples = [b'GIF88a' + header(1, 1)[6:], header(0, 1), header(1, 0),
                    header(513, 1), header(1, 513), header(65535, 65535),
                    header(1, 1, global_palette=False) + image(1, 1, [1]) + b';',
                    header(1, 1) + image(2, 1, [1, 1]) + b';',
                    header(1, 1) + image(1, 1, [1], x=1) + b';',
                    header(1, 1) + image(1, 1, [1], y=1) + b';',
                    header(1, 1) + image(0, 1, []) + b';',
                    header(1, 1) + image(1, 0, []) + b';',
                    header(1, 1) + image(1, 1, [], codes=[4, 7, 5]) + b';',
                    header(1, 1) + image(1, 1, [], codes=[4, 5]) + b';',
                    header(1, 1) + image(1, 1, [1, 2]) + b';',
                    header(1, 1) + image(1, 1, [], codes=[8, 7, 9], minimum=3) + b';',
                    header(1, 1) + b'!\xf9\x03\x00\0\0\0\0',
                    header(1, 1) + b'!\xf9\x04\x00\0\0\0\x01']
        base = header(1, 1) + image(1, 1, [1]) + b';'
        for minimum in [0, 1, 9, 12, 31, 32, 255]:
            examples.append(base[:35] + bytes([minimum]) + base[36:])
        for i, data in enumerate(examples):
            with self.subTest(case=i):
                self.invoke('reject', data)

    def test_stop_reader_before_disc_workspace_handoff(self):
        data = header(1, 1) + image(1, 1, [1]) + b';'
        for mode in range(3):
            with self.subTest(mode=mode):
                self.invoke('handoff', data, mode)

    def test_file_capacity_read_failures_short_reads_and_close_failures(self):
        data = header(1, 1) + image(1, 1, [1]) + b';'
        for fault in range(1, 8):
            with self.subTest(fault=fault):
                self.invoke('io', data, fault)


if __name__ == '__main__':
    unittest.main()
