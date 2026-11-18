"""Actual legacy SH-2 SCI receive arithmetic with modeled register readiness.
Adapted by papsg from paps-g 67f025a6/SSR-015. No device operations.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_menu_cdplayer import function
ROOT=Path(__file__).resolve().parents[1]


class MenuSCITimeoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build=ROOT/'tests/build/menu-sci-timeout';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        source=Path(os.environ.get('SAROO_MENU_SCI_SOURCE',ROOT/'Firm_Saturn/main.c')).read_bytes()
        fixture=(ROOT/'tests/menu_sci_timeout.c').read_bytes()
        inputs={'main.c':source,'menu_sci_timeout.c':fixture,'test_menu_sci_timeout.py':Path(__file__).read_bytes(),
                'test_menu_cdplayer.py':(ROOT/'tests/test_menu_cdplayer.py').read_bytes()}
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in inputs.items()},indent=2)+'\n')
        (cls.target/'sci-timeout.inc').write_bytes(function(source,'sci_getc'))
        flags=['-O2'] if os.environ.get('SAROO_FILE_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer']
        command=[os.environ.get('CC','cc'),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,
                 '-I',str(cls.target),str(cls.target/'menu_sci_timeout.c'),'-o',str(cls.target/'timeout')]
        (cls.target/'compile-command.json').write_text(json.dumps(command,indent=2)+'\n')
        result=subprocess.run(command,capture_output=True,timeout=30)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr);result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def case(self,timeout,ready_at,seed=0):
        run=subprocess.run([str(self.target/'timeout'),str(timeout),str(ready_at),str(seed)],
                           capture_output=True,text=True,timeout=10,
                           env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        row=dict(timeout=timeout,ready_at=ready_at,seed=seed,status=run.returncode,out=run.stdout,err=run.stderr)
        self.results.append(row);self.assertEqual(run.returncode,0,row)

    def test_full_int_domain_and_exact_readiness_budget(self):
        cases=[(0,1),(1,1),(1000,1),(1431655,1),(1431656,1),(2147483647,1),
               (-1,1),(-1431655,1),(-1431656,1),(-2147483648,1),
               (1,0),(2,0),(1,1500),(1,1501),(2,1501),(2,3000),(2,3001)]
        for timeout,ready in cases:
            with self.subTest(timeout=timeout,ready=ready):self.case(timeout,ready)

    def test_original_safe_domain_recovers_receive_errors(self):
        for timeout,ready in [(1,0),(1,1),(1,2),(1,1500),(2,3000),(0,1)]:
            with self.subTest(timeout=timeout,ready=ready):self.case(timeout,ready,0x38)
