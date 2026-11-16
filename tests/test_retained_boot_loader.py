"""Existing BOOT admission with observable file/vector/CPU boundaries.
Adapted by papsg from paps-g 52632dd7/SSR-027. All boundaries are host substitutes.
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


class RetainedBootLoaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build=ROOT/'tests/build/retained-boot-loader';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        raw=Path(os.environ.get('SAROO_RETAINED_BOOT_SOURCE',ROOT/'Firm_MCU/Main/main.c')).read_bytes()
        inputs={'main.c':raw,'retained_boot_loader.c':(ROOT/'tests/retained_boot_loader.c').read_bytes(),
                'test_retained_boot_loader.py':Path(__file__).read_bytes(),
                'test_menu_cdplayer.py':(ROOT/'tests/test_menu_cdplayer.py').read_bytes()}
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(d).hexdigest() for n,d in inputs.items()},indent=2)+'\n')
        code=function(raw,'load_app')
        for old,new in [(b'*(u32*)(app_addr+4)',b'boot_word(app_addr+4)'),
                        (b'*(u32*)(app_addr+0)',b'boot_word(app_addr+0)')]:
            assert code.count(old)==1;code=code.replace(old,new)
        old=b'(void*)(boot_word(app_addr+4))';new=b'(void*)reset'
        assert code.count(old)+code.count(new)==1
        code=code.replace(old,b'boot_entry(boot_word(app_addr+4))').replace(new,b'boot_entry(reset)')
        (cls.target/'loader.inc').write_bytes(code)
        flags=['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        command=[os.environ.get('CC','cc'),'-std=gnu11','-g','-Wall','-Wextra','-Werror',*flags,
                 '-I',str(cls.target),str(cls.target/'retained_boot_loader.c'),'-o',str(cls.target/'loader')]
        (cls.target/'compile-command.json').write_text(json.dumps(command,indent=2)+'\n')
        result=subprocess.run(command,capture_output=True,timeout=30)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr);result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def invoke(self,case):
        result=subprocess.run([str(self.target/'loader'),case],capture_output=True,text=True,
                              timeout=10,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        row=dict(case=case,status=result.returncode,out=result.stdout,err=result.stderr)
        self.results.append(row);self.assertEqual(result.returncode,0,row);self.assertIn('pass',result.stdout)

    def test_complete_controls_and_open_refusal(self):
        for case in ('complete','minimum','maximum','open-error','stack-controls'):
            with self.subTest(case=case):self.invoke(case)

    def test_original_size_is_admitted_before_read_without_narrowing(self):
        for case in ('size-0','size-1','size-7','size-8','size-9','size-65537','size-65540','size-4294967304'):
            with self.subTest(case=case):self.invoke(case)

    def test_incomplete_failed_or_uncertain_io_never_enters_image(self):
        for case in ('error-zero','error-prefix','error-complete','short-zero','short-prefix','short-one','over-count','close-error'):
            with self.subTest(case=case):self.invoke(case)

    def test_stack_thumb_and_entry_halfword_are_required(self):self.invoke('vectors')
