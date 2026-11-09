"""Compile actual native cover source into retained, public test artifacts."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CoverToolBuild(unittest.TestCase):
    faults = False

    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/native-cover'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='faults-' if cls.faults else 'cli-', dir=build))
        cls.results = []
        source = Path(os.environ.get('SAROO_COVERTOOL_SOURCE', ROOT / 'tools/covertool/main.c'))
        data = source.read_bytes()
        (cls.target / 'main.c').write_bytes(data)
        inputs = {'main.c': hashlib.sha256(data).hexdigest()}
        for name in ('covertool_support.py', 'test_covertool.py', 'test_covertool_io.py',
                     'cover_fixtures.py', 'covertool_faults.c', 'covertool_faults.h'):
            data = (ROOT / 'tests' / name).read_bytes()
            (cls.target / name).write_bytes(data)
            inputs[name] = hashlib.sha256(data).hexdigest()
        (cls.target / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_COVERTOOL_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        common = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                  '-Wall', '-Wextra', '-Werror', '-fstack-usage', *flags]
        cls.binary = cls.target / 'covertool'
        if cls.faults:
            commands = [[*common, '-include', str(cls.target / 'covertool_faults.h'),
                         '-c', str(cls.target / 'main.c'), '-o', str(cls.target / 'main.o')],
                        [*common, str(cls.target / 'main.o'), str(cls.target / 'covertool_faults.c'),
                         '-o', str(cls.binary)]]
        else:
            commands = [[*common, str(cls.target / 'main.c'), '-o', str(cls.binary)]]
        (cls.target / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        for command in commands:
            result = subprocess.run(command, capture_output=True, timeout=60)
            with (cls.target / 'compile.log').open('ab') as log:
                log.write(result.stdout + result.stderr)
            result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')
