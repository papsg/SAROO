"""Actual XA parser, file commands, router and worker advancement."""
# Adapted from paps-g selected05852d90; iterative and wide arithmetic oracles.
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import test_cdc_directory as directory

ROOT=Path(__file__).resolve().parents[1]


class CDXATests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=Path(os.environ.get('SAROO_CDC_XA_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        sibling = source.with_name('saturn_main.c')
        main_source = Path(os.environ.get('SAROO_CDC_XA_MAIN',
                              sibling if sibling.is_file() else ROOT/'Firm_MCU/Saturn/saturn_main.c'))
        with patch.dict(os.environ,{'SAROO_CDC_DIRECTORY_SOURCE':str(source),'SAROO_CDC_DIRECTORY_MAIN':str(main_source)}):
            directory.CDDirectoryTests.setUpClass()
        build=ROOT/'tests/build/cdc-xa';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        for f in directory.CDDirectoryTests.target.iterdir():
            if f.is_file() and f.suffix in ('.c','.h','.inc'): (cls.target/f.name).write_bytes(f.read_bytes())
        raw=source.read_bytes();main=main_source.read_bytes()
        data=(ROOT/'tests/cdc_directory.c').read_bytes()
        (cls.target/'directory-harness.inc').write_bytes(data[:data.index(b'int main(')])
        # A host word only holds the last write. Record each event issued by
        # the worker so an EFLS write followed by PEND is still observable.
        def observe_hirq(code):
            return re.sub(rb'\bHIRQ\s*=\s*(HIRQ_\w+);', rb'file_hirq(\1);', code)

        dispatch = cls.target / 'dispatch.inc'
        dispatch.write_bytes(observe_hirq(dispatch.read_bytes()))
        start = main.index(b'\n_play_end:') + len(b'\n_play_end:')
        end = main.index(b'\n\t\tset_peri_report();', start)
        range_start = main.find(b'int outside_play_range(')
        range_helper = main[range_start:main.index(b'\n}', range_start)+2] if range_start >= 0 else b''
        (cls.target / 'file-end.inc').write_bytes(
            range_helper + b'\nstatic int finish_step(void) {\n' + observe_hirq(main[start:end]) +
            b'\nreturn 0;\n_restart_nowait: return 2;\n_play_checkpoint: abort();\n_delay_error: abort();\n}\n')
        start=main.index(b'\t\t\tcdb.fad++;')
        end=main.index(b'\n\t\t}',start)
        (cls.target/'advance.inc').write_bytes(b'static void advance(void) {\n'+main[start:end]+b'\n}\n')
        for name in ('cdc_xa.c','test_cdc_xa.py'): (cls.target/name).write_bytes((ROOT/'tests'/name).read_bytes())
        inputs={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in cls.target.iterdir() if f.is_file()}
        inputs.update({'saturn_cdc.c':hashlib.sha256(raw).hexdigest(),'saturn_main.c':hashlib.sha256(main).hexdigest()})
        (cls.target/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'xa'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',
              '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label','-Wno-parentheses',*flags,
              '-I',str(cls.target),str(cls.target/'cdc_xa.c'),'-I',str(directory.OVERLAY.parent),
              str(directory.OVERLAY),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        directory.CDDirectoryTests.tearDownClass()
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=60,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_suffix_bounds_name_padding_and_extended_attributes(self):self.run_case(0)
    def test_65536_iso_xa_attributes_and_exact_replies(self):self.run_case(1)
    def test_xa_directory_precedence_and_streamed_self_parent(self):self.run_case(2)
    def test_all_unit_gap_values_against_iterative_sector_lists(self):self.run_case(3)
    def test_1680_wide_extent_and_offset_boundaries(self):self.run_case(4)
    def test_all_file_numbers_range_and_payload_routing(self):self.run_case(5)
    def test_full_pool_retries_preserve_unit_position(self):self.run_case(6)
    def test_full_metadata_snapshot_and_sector_playback_control(self):self.run_case(7)
    def test_contiguous_and_directory_setup_retire_old_repeat_and_cursor(self):self.run_case(8)
    def test_file_end_follows_full_buffer_retries(self):self.run_case(9)
    def test_file_end_preserves_interleaved_payload(self):self.run_case(10)
    def test_file_end_on_abort_preserves_buffered_sectors(self):self.run_case(11)
    def test_file_end_still_signals_without_buffer_pressure(self):self.run_case(12)


if __name__=='__main__':unittest.main()
