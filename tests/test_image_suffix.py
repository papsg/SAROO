"""Actual bounded ASCII suffix code with independent accepted-spelling oracle."""
# Adapted from paps-g 96f9329a; real discovery controls live in the MCU tests.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ImageSuffixTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ.get('SAROO_IMAGE_SUFFIX_SOURCE', ROOT/'Firm_MCU/Saturn/cdimg.c'))
        data = path.read_bytes()
        start = data.index(b'static int image_suffix(')
        end = data.index(b'\n}',start)+2
        build = ROOT/'tests/build/image-suffix'; build.mkdir(parents=True,exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-',dir=build)); cls.results=[]
        (cls.target/'suffix.inc').write_bytes(data[start:end]+b'\n')
        inputs = {'cdimg.c':data, 'image_suffix.c':(ROOT/'tests/image_suffix.c').read_bytes(),
                  'test_image_suffix.py':(ROOT/'tests/test_image_suffix.py').read_bytes()}
        for name,blob in inputs.items(): (cls.target/name).write_bytes(blob)
        (cls.target/'inputs.json').write_text(json.dumps({
            n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        flags = ['-O2'] if os.environ.get('SAROO_SHRINK_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'image-suffix'
        command=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra',
                 '-Werror',*flags,'-I',str(cls.target),str(cls.target/'image_suffix.c'),'-o',str(cls.binary)]
        r=subprocess.run(command,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(r.stdout+r.stderr)
        (cls.target/'command.json').write_text(json.dumps(command,indent=2)+'\n'); r.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def test_all_spellings_short_extents_and_byte_substitutions(self):
        r=subprocess.run([str(self.binary)],capture_output=True,text=True,timeout=30,
                         env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        self.assertIn('pass: 41130 suffix checks',r.stdout)


if __name__=='__main__':
    unittest.main()
