"""Actual menu byte/word helpers on high-bit and unaligned inputs."""
# Adapted from paps-g 4df2e67a; independent baseline-valid cohort added.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class MenuWordAccessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path=Path(os.environ.get('SAROO_MENU_WORD_SOURCE',ROOT/'Firm_Saturn/main.c'))
        raw=path.read_bytes();start=raw.index(b'u32 BE32(')
        build=ROOT/'tests/build/menu-word-access';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        (cls.target/'words.inc').write_bytes(raw[start:raw.index(b'/**********************************************************/',start)])
        inputs={'main.c':raw,'menu_word_access.c':(ROOT/'tests/menu_word_access.c').read_bytes(),'test_menu_word_access.py':(ROOT/'tests/test_menu_word_access.py').read_bytes()}
        for name,blob in inputs.items():(cls.target/name).write_bytes(blob)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in inputs.items()},indent=2)+'\n')
        cls.binary=cls.target/'word-access';flags=['-O2'] if os.environ.get('SAROO_FILE_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,'-I',str(cls.target),str(cls.target/'menu_word_access.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60);(cls.target/'compile.log').write_bytes(result.stdout+result.stderr);(cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def invoke(self,mode,checks):
        result=subprocess.run([str(self.binary),mode],capture_output=True,text=True,timeout=20,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(mode=mode,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr));self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn(f'pass: {checks} word checks',result.stdout)

    def test_full_width_words_in_both_orders_and_four_alignments(self):
        for mode in ('big','little'):
            with self.subTest(mode=mode):self.invoke(mode,4128)

    def test_baseline_valid_low_byte_words_are_preserved(self):
        for mode in ('big-low','little-low'):
            with self.subTest(mode=mode):self.invoke(mode,2064)


if __name__=='__main__':unittest.main()
