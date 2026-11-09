"""Actual menu display function; synthetic memory and asynchronous DMA stubs."""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class CoverDisplayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build=ROOT/'tests/build/cover-display';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        source=Path(os.environ.get('SAROO_COVER_MENU_SOURCE',ROOT/'Firm_Saturn/main.c')).read_bytes()
        (cls.target/'main.c').write_bytes(source)
        begin=source.index(b'static int last_w, last_h, last_f;')
        selected=source[begin:source.index(b'static void change_cover_layer(',begin)]
        selected=re.sub(rb'0x224([0-9a-fA-F]{5})',lambda m:b'(TEST_GFX_BASE + 0x'+m[1]+b')',selected)
        (cls.target/'cover-display.inc').write_bytes(selected)
        inputs={'main.c':hashlib.sha256(source).hexdigest()}
        for name in ('cover_display.c','test_cover_display.py'):
            data=(ROOT/'tests'/name).read_bytes();(cls.target/name).write_bytes(data)
            inputs[name]=hashlib.sha256(data).hexdigest()
        (cls.target/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'display'
        r=subprocess.run([*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g',*flags,
            '-I',str(cls.target),str(cls.target/'cover_display.c'),'-o',str(cls.binary)],capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(r.stdout+r.stderr);r.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def invoke(self,value):
        r=subprocess.run([str(self.binary),str(value)],capture_output=True,timeout=10,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1','ASAN_OPTIONS':'detect_leaks=0'})
        self.results.append(dict(value=value,returncode=r.returncode,stdout=r.stdout.decode(errors='replace'),stderr=r.stderr.decode(errors='replace')))
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)

    def test_cover_sizes_and_final_dma_ownership(self):
        for value in range(5):
            with self.subTest(value=value):self.invoke(value)

    def test_gif_stride_remains_512(self):
        for value in (5,6):
            with self.subTest(value=value):self.invoke(value)

    def test_partial_clipping_uses_the_same_source_stride(self):
        for value in (7,8,9):
            with self.subTest(value=value):self.invoke(value)

    def test_non_vblank_preserves_pending_ownership(self):self.invoke(10)


if __name__=='__main__':unittest.main()
