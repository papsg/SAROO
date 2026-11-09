"""Actual MCU ID matcher: strict versions, bounded fields and legacy joining."""
import itertools
import json
import os
from pathlib import Path
import struct
import subprocess
import hashlib
import shlex
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def pair(left, right, expected):
    return left + b'\0', right + b'\0', expected


def identity(value):
    """Preserved format policy expressed as immutable byte-string tokens."""
    if len(value) >= 24:
        return None
    tokens = value.split(b' ', 1)
    if len(tokens) == 1:
        return tokens[0], None
    serial, rest = tokens
    version = rest.lstrip(b' ').split(b' ', 1)[0]
    if len(serial + version) == 16:
        return serial + version, None
    return serial, version


class ConfigMatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_CONFIG_MATCH_SOURCE', ROOT / 'Firm_MCU/Saturn/saturn_utils.c'))
        data = source.read_bytes()
        start = data.index(b'static char *gameid_split(')
        end = data.index(b'/******************************************************************************/', start)
        build = ROOT / 'tests/build/config-match'; build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        (cls.target / 'saturn_utils.c').write_bytes(data)
        (cls.target / 'config-match.inc').write_bytes(data[start:end])
        inputs = {'saturn_utils.c': hashlib.sha256(data).hexdigest()}
        for name in ('config_match.c', 'test_config_match.py'):
            fixture = (ROOT / 'tests' / name).read_bytes()
            (cls.target / name).write_bytes(fixture)
            inputs[name] = hashlib.sha256(fixture).hexdigest()
        (cls.target / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
        cls.binary = cls.target / 'config-match'
        flags = ['-O2'] if os.environ.get('SAROO_CONFIG_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, '-I', str(cls.target),
                   str(cls.target / 'config_match.c'), '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        (cls.target / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
        result.check_returncode()
        cls.results = []

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, name, cases):
        path = self.target / (name + '.bin')
        with path.open('wb') as stream:
            for left, right, expected in cases:
                stream.write(struct.pack('<HHH', len(left), len(right), expected))
                stream.write(left + right)
        result = subprocess.run([str(self.binary), str(path)], capture_output=True, timeout=15,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(name=name, pairs=len(cases), returncode=result.returncode,
                                 stdout=result.stdout.decode(errors='replace'),
                                 stderr=result.stderr.decode(errors='replace')))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stdout), {'pairs': len(cases), 'calls': len(cases) * 2})

    def test_version_presence_and_exact_tokens(self):
        examples = [(b'T-12345H V1.000', b'T-12345H', 0),
                    (b'T-12345H V1.000', b'T-12345H V1.000', 1),
                    (b'T-12345H V1.000', b'T-12345H V1.001', 0),
                    (b'T-12345H', b'T-12345H', 1), (b'A', b'B', 0),
                    (b'A  V1 ', b'A V1', 1), (b'A V1 ignored', b'A V1', 1),
                    (b'A\tV1', b'A V1', 0), (b'A ', b'A', 0),
                    (b'A  ', b'A ', 1), (b'', b'', 1), (b' ', b'', 0),
                    (b'a', b'A', 0), (b'\xff V1', b'\xff V1', 1)]
        for i, example in enumerate(examples):
            with self.subTest(case=i): self.invoke('version-' + str(i), [pair(*example)])

    def test_legacy_sixteen_byte_joining_uses_safe_overlap(self):
        examples = [(b'ABCDEFGHIJKLMNO P', b'ABCDEFGHIJKLMNOP', 1),
                    (b'ABCDEFGH IJKLMNOP', b'ABCDEFGHIJKLMNOP', 1),
                    (b'ABCDEFGH  IJKLMNOP tail', b'ABCDEFGHIJKLMNOP', 1),
                    (b'123456789012345 6', b'1234567890123456', 1),
                    (b'ABCDEFGHIJKLMNO Q', b'ABCDEFGHIJKLMNOP', 0),
                    (b'ABCDEFGHIJKLMNOP ', b'ABCDEFGHIJKLMNOP', 1)]
        for i, example in enumerate(examples):
            with self.subTest(case=i): self.invoke('join-' + str(i), [pair(*example)])

    def test_field_capacity_rejects_without_copy_or_truncation(self):
        for length in (0, 1, 15, 16, 17, 22, 23, 24, 25, 1024):
            with self.subTest(length=length):
                field = b'A' * length
                self.invoke('length-' + str(length), [pair(field, field, int(length < 24))])
        self.invoke('long-prefix', [pair(b'A' * 24, b'A' * 23, 0)])

    def test_unterminated_twenty_four_byte_fields_are_rejected(self):
        self.invoke('unterminated', [(b'A' * 24, b'A\0', 0), (b'B' * 24, b'B' * 24, 0)])

    def test_token_corpus_matches_independent_policy_in_both_directions(self):
        values = {b'', b' ', b'A\tV1', b'ABCDEFGHIJKLMNOP', b'A' * 24}
        for base in (b'A', b'AB', b'ABCDEFGH', b'ABCDEFGHIJKLMNO'):
            values.add(base)
            for spaces, version, tail in itertools.product((b' ', b'  '),
                                                           (b'', b'V1', b'V2', b'IJKLMNOP'),
                                                           (b'', b' ignored')):
                values.add(base + spaces + version + tail)
        cases = []
        for left, right in itertools.product(sorted(values), repeat=2):
            normalized = identity(left)
            cases.append(pair(left, right, int(normalized is not None and normalized == identity(right))))
        self.invoke('corpus', cases)


if __name__ == '__main__':
    unittest.main()
