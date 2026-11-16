"""Actual public dictionary, copied keys and an independently found full collision.
Adapted by papsg from paps-g f7634ce3/SSR-021. No device operations.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]


class LanguageCollisionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build=ROOT/'tests/build/language-collision';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        source=Path(os.environ.get('SAROO_MENU_LANGUAGE_SOURCE',ROOT/'Firm_Saturn/language.c')).read_bytes()
        inputs={'Firm_Saturn/language.c':source,'Firm_Saturn/main.h':(ROOT/'Firm_Saturn/main.h').read_bytes(),
                'tests/language.c':(ROOT/'tests/language.c').read_bytes(),
                'tests/test_language_collision.py':Path(__file__).read_bytes()}
        for name,data in inputs.items():
            path=cls.target/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        command=[os.environ.get('CC','cc'),'-std=c99','-g','-Wall','-Wextra','-Werror',*flags,
                 str(cls.target/'tests/language.c'),'-o',str(cls.target/'language')]
        (cls.target/'compile-command.json').write_text(json.dumps(command,indent=2)+'\n')
        result=subprocess.run(command,capture_output=True,timeout=30)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr);result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def case(self,case):
        run=subprocess.run([str(self.target/'language'),case],capture_output=True,text=True,timeout=30,
                           env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        row=dict(case=case,status=run.returncode,out=run.stdout,err=run.stderr)
        self.results.append(row);self.assertEqual(run.returncode,0,row)

    def test_existing_language_wrap_and_startup_controls(self):self.case('languages')
    def test_full_hash_collision_and_copied_key_identity(self):self.case('collision')
