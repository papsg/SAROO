"""Actual retained line arithmetic and independent closed-form pixels.
Adapted by papsg from paps-g 77173c38/SSR-017. No devices.
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


class MenuLineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build=ROOT/'tests/build/menu-lines';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        source=Path(os.environ.get('SAROO_MENU_LINE_SOURCE',ROOT/'Firm_Saturn/conio.c')).read_bytes()
        inputs={'conio.c':source,'menu_lines.c':(ROOT/'tests/menu_lines.c').read_bytes(),
                'test_menu_lines.py':Path(__file__).read_bytes(),'test_menu_cdplayer.py':(ROOT/'tests/test_menu_cdplayer.py').read_bytes()}
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in inputs.items()},indent=2)+'\n')
        start=source.index(b'void put_line (')
        line=source[start:source.index(b'\n}',start)+3]
        (cls.target/'line.inc').write_bytes(function(source,'abs')+line)
        flags=['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-sanitize-recover=undefined','-fno-omit-frame-pointer']
        command=[os.environ.get('CC','cc'),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,
                 '-I',str(cls.target),str(cls.target/'menu_lines.c'),'-o',str(cls.target/'lines')]
        (cls.target/'compile-command.json').write_text(json.dumps(command,indent=2)+'\n')
        result=subprocess.run(command,capture_output=True,timeout=30)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr);result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def case(self,value):
        result=subprocess.run([str(self.target/'lines'),str(value)],capture_output=True,text=True,
                              timeout=20,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        row=dict(case=value,returncode=result.returncode,out=result.stdout,err=result.stderr)
        self.results.append(row);self.assertEqual(result.returncode,0,row)

    def test_orientations_reversal_zero_span_and_canvas_edges(self):
        for n in range(15):
            with self.subTest(case=n):self.case(n)

    def test_every_endpoint_pair_on_sixteen_pixel_grid(self):
        self.case('grid')
