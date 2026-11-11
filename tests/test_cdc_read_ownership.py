"""Actual read ownership and explicit modeled preemption; no device operations."""
# Adapted from paps-g 3d2c405e; count semantics are verified separately.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import unittest
from unittest.mock import patch
import test_cdc_bounds as bounds

ROOT=Path(__file__).resolve().parents[1]


class CDReadOwnershipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=os.environ.get('SAROO_CDC_READ_SOURCE',str(ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        with patch.dict(os.environ,{'SAROO_CDC_BOUNDS_SOURCE':source}):bounds.CDBoundsTests.setUpClass()
        cls.target=bounds.CDBoundsTests.target;cls.results=[]
        raw=Path(source).read_bytes();header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        def function(signature):
            start=raw.index(signature)
            return raw[start:raw.index(b'\n}',start)+2]+b'\n'
        (cls.target/'ownership.inc').write_bytes(function(b'int reset_filter(')+function(b'int filter_sector('))
        types=cls.target/'types.h'
        types.write_bytes(types.read_bytes()+header[header.index(b'#define _BUFIO '):header.index(b'#define LOG_MASK_DEFAULT')])
        inputs=json.loads((cls.target/'inputs.json').read_text())
        for name in ('cdc_read_ownership.c','test_cdc_read_ownership.py'):
            data=(ROOT/'tests'/name).read_bytes();(cls.target/name).write_bytes(data);inputs[name]=hashlib.sha256(data).hexdigest()
        (cls.target/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n')
        (cls.target/'bounds-command.json').write_bytes((cls.target/'command.json').read_bytes())
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'read-ownership'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',
              '-Wno-unused-variable','-Wno-parentheses',*flags,'-I',str(cls.target),str(cls.target/'cdc_read_ownership.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'ownership-compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=45,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_720_getdel_ranges_and_scrambled_full_pool(self):self.run_case(0)
    def test_same_partition_output_cancellation_retains_private_blocks(self):self.run_case(1)
    def test_other_partitions_and_condition_resets_preserve_reads(self):self.run_case(2)
    def test_actual_incoming_appends_preserve_selected_payload(self):self.run_case(3)
    def test_release_detaches_before_preemption_and_next_before_reuse(self):self.run_case(4)
    def test_buffer_and_transfer_logs_precede_selection(self):self.run_case(5)
    def test_10368_read_admission_boundaries(self):self.run_case(6)
    def test_software_and_nonreset_private_ownership(self):self.run_case(7)
    def test_baseline_valid_payload_and_exact_log_controls(self):self.run_case(8)
    def test_incomplete_getdel_is_refused_without_partial_detachment(self):self.run_case(9)
    def test_first_window_counts_ignore_host_consumption(self):self.run_case(10)
    def test_serviced_mixed_and_completed_windows_count_once(self):self.run_case(11)
    def test_fixed_reply_production_and_fifo_counter_full_flags(self):self.run_case(12)
    def test_valid_zero_and_idle_error_with_retained_write_controls(self):self.run_case(13)
    def test_sector_read_completion_follows_data_end(self):self.run_case(14)


if __name__=='__main__':unittest.main()
