"""Actual drive commands, helpers, pause branches and worker boundaries."""
# Adapted from paps-g 47ec46a4/cd83b371/3772326a; no private preparer or devices.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from test_cdc_copy import copy_source

ROOT=Path(__file__).resolve().parents[1]


class CDDriveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=Path(os.environ.get('SAROO_CDC_DRIVE_DIR',ROOT/'Firm_MCU/Saturn'))
        raw=(source/'saturn_cdc.c').read_bytes();main=(source/'saturn_main.c').read_bytes()
        header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-drive';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(data,signature):
            start=data.index(signature)
            return data[start:data.index(b'\n}',start)+2]+b'\n'
        code=b''.join(function(main,sig) for sig in (b'TRACK_INFO *get_track_info(',b'u32 track_to_fad(',b'int fad_to_track('))
        if b'u8 track_subindex(' in main:code=function(main,b'u8 track_subindex(')+code
        if b'int pause_drive(' in main:code+=function(main,b'int pause_drive(')
        code+=function(main,b'int pause_disc(')+function(raw,b'void set_report(')
        for sig in (b'static u32 drive_position(',b'static void set_drive_position(',b'static unsigned retained_position_type('):
            if sig in raw:code+=function(raw,sig)
        code+=function(raw,b'int init_cdblock(')
        code+=function(raw,b'int play_cd(')+function(raw,b'int seek_cd(')
        code+=function(raw,b'int get_subcode(')
        (cls.target/'drive.inc').write_bytes(code)
        dispatch = function(raw, b'void cdc_cmd_process(')
        cases = b''.join(dispatch[dispatch.index(start):dispatch.index(b'break;', dispatch.index(start)) + 6]
                         for start in (b'case 0x00:', b'case 0x11:'))
        start = dispatch.index(b'switch(cmd){') + len(b'switch(cmd){')
        end = dispatch.index(b'if(cdb.block_free!=MAX_BLOCKS)', start)
        code = function(raw, b'int get_cd_status(') + function(raw, b'char *cmd_str(')
        (cls.target/'seek-dispatch.inc').write_bytes(code + dispatch[:start] + cases +
                                                     b'default: abort();}\n' + dispatch[end:])
        start=main.index(b'_restart_nowait:')
        start=main.index(b'\t\tif(cdb.pause_request)',start)
        end=main.index(b'\n\t\t}',start)+4
        restart=main[start:end]
        start=main.index(b'\t\t\tif(cdb.pause_request)',end)
        end=main.index(b'\n\t\t\t}',start)+5
        active=main[start:end]
        ack=function(main,b'static void acknowledge_disc_pause(')
        (cls.target/'pause-worker.inc').write_bytes(ack+b'\nstatic void worker_pause_step(void) {\nif(pause_active) {\n'+active+b'\n} else {\n'+restart+b'\n}\n_restart_wait: return;\n}\n')
        guard=b'';start=main.find(b'        if (cdb.play_type == PLAYTYPE_SECTOR &&')
        if start>=0:guard=main[start:main.index(b'        int was_directory =',start)]
        start=main.index(b'\t\tSSLOG(_DTASK, "\\nplay end!');end=main.index(b'\n\t\tset_peri_report();',start)
        (cls.target/'phase.inc').write_bytes(b'static int worker_step(void) {\n'+guard+b'read_calls++;return 1;\n_play_end:\n'+main[start:end]+b'\nreturn 0;\n_restart_nowait: return 2;\n}\n')
        start=main.index(b'\t\t\tcdb.play_fad_start = track_to_fad(0x0100);');end=main.index(b'\n\t\t}',start)
        marker=b'            cdb.play_pos_types = 0;';prefix=main.rfind(marker,0,start)
        if prefix>=0 and not main[prefix+len(marker):start].strip():start=prefix
        (cls.target/'background.inc').write_bytes(b'static void background_start(int audio_repeat) {\n'+main[start:end]+b'\n}\n')
        (cls.target/'copy.inc').write_bytes(copy_source(raw,required=(b'free_block_list(',b'disconnect_filter_input(')))
        (cls.target/'file-start.inc').write_bytes(b''.join(function(raw,sig) for sig in (b'static void _set_filter(',b'void free_partition(',b'static void set_file_selector(',b'static void start_file_play(')))
        (cls.target/'reset.inc').write_bytes(function(raw,b'int reset_filter('))
        (cls.target/'types.h').write_bytes(header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')]+header[header.index(b'#define _BUFIO '):header.index(b'#define LOG_MASK_DEFAULT')])
        inputs={'saturn_cdc.c':raw,'saturn_main.c':main,'cdc.h':header}
        for name in ('cdc_drive.c','test_cdc_drive.py','cdc_copy.c','test_cdc_copy.py','track_fixture.h'):inputs[name]=(ROOT/'tests'/name).read_bytes()
        for name in ('ff.h','ffconf.h'):inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
        for name,data in inputs.items():(cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(v).hexdigest() for n,v in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'drive'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',
              '-Wno-unused-variable','-Wno-unused-function','-Wno-unused-label','-Wno-parentheses',*flags,
              '-I',str(cls.target),str(cls.target/'cdc_drive.c'),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=60,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_track_helper_bounds_and_index_storage(self):self.run_case(0)
    def test_all_track_index_bytes_for_play_start_end_and_seek(self):self.run_case(1)
    def test_fad_clamping_counts_and_high_bits(self):self.run_case(2)
    def test_mixed_forms_refuse_before_pause_or_mutation(self):self.run_case(3)
    def test_bounded_pause_failure_and_retry_all_worker_types(self):self.run_case(4)
    def test_real_worker_admission_completion_and_final_sector(self):self.run_case(5)
    def test_pause_stop_and_retained_compatibility_controls(self):self.run_case(6)
    def test_missing_media_and_invalid_tables(self):self.run_case(7)
    def test_actual_background_setup_includes_final_frame(self):self.run_case(8)
    def test_actual_pause_branches_preserve_same_reader_cache(self):self.run_case(9)
    def test_baseline_valid_position_metadata_and_exact_logs(self):self.run_case(10)
    def test_all_99_tracks_remain_addressable(self):self.run_case(11)
    def test_2880_repeat_counts_and_finite_budget_exhaustion(self):self.run_case(12)
    def test_changed_settings_and_failed_handoff_controls(self):self.run_case(13)
    def test_seek_stop_preserve_repeat_and_exact_invalid_report(self):self.run_case(14)
    def test_home_pause_and_direct_resume_restore_first_sector(self):self.run_case(15)
    def test_112_retained_endpoint_transition_pairs(self):self.run_case(16)
    def test_failed_handoff_retains_types_and_default_removes_constraint(self):self.run_case(17)
    def test_file_and_background_ranges_retire_play_type_constraints(self):self.run_case(18)
    def test_initialization_handoff_is_bounded_and_defers_all_mutation(self):self.run_case(19)
    def test_initialization_late_acknowledgement_and_final_wait(self):self.run_case(20)
    def test_all_initialization_flags_preserve_settings_and_reset_only_when_requested(self):self.run_case(21)
    def test_all_initialization_flags_without_media_preserve_drive_status(self):self.run_case(22)
    def test_selector_reset_handoff_preserves_ownership_and_retries(self):self.run_case(23)
    def test_all_selector_modes_preserve_existing_scope_and_pause_decisions(self):self.run_case(24)
    def test_selector_reset_preserves_exact_log_formats_and_masks(self):self.run_case(25)
    def test_stopped_drive_subcode_and_media_transitions(self):self.run_case(26)
    def test_subcode_track_bounds(self):self.run_case(27)
    def test_subcode_type_and_transfer_ownership(self):self.run_case(28)
    def test_subcode_valid_payloads(self):self.run_case(29)
    def test_seek_preserves_range_forms_and_repeats(self):self.run_case(30)
    def test_seek_resume_stays_in_retained_range(self):self.run_case(31)
    def test_seek_end_event_tracks_range_and_repeat_budget(self):self.run_case(32)
    def test_seek_dispatch_publishes_event_after_worker_handoff(self):self.run_case(33)
    def test_seek_busy_reply_and_following_settled_status(self):self.run_case(34)
    def test_seek_no_event_for_refusal_or_pending_repeat(self):self.run_case(35)


if __name__=='__main__':unittest.main()
