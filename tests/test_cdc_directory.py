"""Actual streamed directory parser, commands, routes and worker fragments."""
# Adapted from paps-g selected8e465233; no private preparer or device operations.
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from cdc_reply_source import bswap_source
import test_cdc_bounds as bounds
from test_cdc_filter import OVERLAY, filter_source, overlay_source

ROOT=Path(__file__).resolve().parents[1]


class CDDirectoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=Path(os.environ.get('SAROO_CDC_DIRECTORY_SOURCE',ROOT/'Firm_MCU/Saturn/saturn_cdc.c'))
        sibling = source.with_name('saturn_main.c')
        main_source = Path(os.environ.get('SAROO_CDC_DIRECTORY_MAIN',
                              sibling if sibling.is_file() else ROOT/'Firm_MCU/Saturn/saturn_main.c'))
        with patch.dict(os.environ,{'SAROO_CDC_BOUNDS_SOURCE':str(source)}):
            bounds.CDBoundsTests.setUpClass()
        raw=source.read_bytes();main=main_source.read_bytes()
        image=Path(os.environ.get('SAROO_CDC_DIRECTORY_IMAGE',ROOT/'Firm_MCU/Saturn/cdimg.c')).read_bytes()
        header=(bounds.CDBoundsTests.target/'cdc.h').read_bytes()
        build=ROOT/'tests/build/cdc-directory';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        def function(data,signature):
            start=data.index(signature)
            return data[start:data.index(b'\n}',start)+2]+b'\n'
        (cls.target/'disc-unload.inc').write_bytes(b''.join(function(image,sig) for sig in (
            b'static int close_tracks(int count)\n{',b'int unload_disc(')))
        retired = b'int finish_read(' in raw
        code=overlay_source()+b''.join(function(raw,sig) for sig in (
            b'static u32 isonum_733(',b'static int read_directory_record(',
            b'int finish_read(' if retired else b'int finish_directory(',
            b'static __attribute__((noinline)) void log_directory_record(',
            b'static int fill_fileinfo(',b'int handle_diread(',b'int get_file_scope(',
            b'int get_file_info(',b'int abort_file(')) + filter_source(raw)
        if retired:
            code += function(main, b'static __attribute__((noinline)) void log_play_end(')
        else:
            code += b'#define finish_read finish_directory\n'
        if b'void fill_file_info_fifo(' not in raw:
            code = bswap_source(header, main) + code
        (cls.target/'directory.inc').write_bytes(code)
        init = function(main, b'void ss_sw_init(').replace(b'0x2400a000', b'TEST_CD_MEMORY')
        (cls.target/'directory-startup.inc').write_bytes(init)
        start=main.index(b'\t\t\t\tretv = filter_sector(')
        end=main.index(b'\n\t\t\t}\n\n\t\t\tcdb.',start)
        (cls.target/'dispatch.inc').write_bytes(b'static int dispatch(BLOCK wblk) {\nint retv;\n'+
            main[start:end]+b'\nreturn 1;\n_restart_wait: return 0;\n_restart_nowait: return 2;\n'+
            (b'_read_error: finish_read(0); return 0;\n' if b'goto _read_error;' in main[start:end] else b'')+b'}\n')
        if retired:
            error_start = main.index(b'\n_read_error:')
            error_exit = main[error_start:main.index(b'\n_delay_error:',error_start)]
            start=main.index(b'            retv = get_sector(cdb.fad, &wblk);')
        else:
            error_exit = b''
            start=main.index(b'            int reading_directory =')
        end=main.index(b'\n\t\t\tled_event(',start)
        code=b'static int input_error_step(void) {\nint retv; BLOCK wblk;\ndo {\n'
        code+=main[start:end]+b'\nreturn 1;\n} while (0);\nreturn 2;\n'+error_exit+b'\n_restart_wait: return 0;\n}\n'
        if retired:
            lookup = main.index(b'get_sector(cdb.fad, NULL)',main.index(b'\n_restart_nowait:'))
            start = main.rfind(b'\n',0,lookup)+1
            end = main.index(b'\n',lookup)
        else:
            start=main.index(b'        int was_directory =')
            end=main.index(b'\n\n\t\tif(cdb.play_fad_start',start)
        code+=b'static int initial_error_step(void) {\n'+main[start:end]+b'\nreturn 1;\n'+error_exit+b'\n_restart_wait: return 0;\n}\n'
        (cls.target/'input-error.inc').write_bytes(code)
        stop_signature = b'static void acknowledge_disc_pause('
        pause = (function(main, stop_signature) if stop_signature in main
                 else function(main, b'static int acknowledge_drive_request(') +
                 b'#define acknowledge_disc_pause acknowledge_drive_request\n')
        (cls.target/'pause.inc').write_bytes(pause)
        inputs={'saturn_cdc.c':raw,'saturn_main.c':main,'cdimg.c':image}
        for name in ('cdc_directory.c','test_cdc_directory.py','cdc_bounds.c','test_cdc_bounds.py','cdc_copy.c','test_cdc_copy.py','cdc_reply_source.py','test_cdc_filter.py'):
            inputs[name]=(ROOT/'tests'/name).read_bytes()
        for name in ('types.h','copy.inc','bounds.inc','ff.h','ffconf.h'):
            inputs[name]=(bounds.CDBoundsTests.target/name).read_bytes()
        inputs['cdc.h']=header
        inputs['types.h']+=header[header.index(b'#define _BUFIO '):header.index(b'#define LOG_MASK_DEFAULT')]
        for name,data in inputs.items(): (cls.target/name).write_bytes(data)
        (cls.target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(v).hexdigest() for n,v in inputs.items()},indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_CDC_PROFILE')=='release' else ['-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'directory'
        args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-Wall','-Wextra','-Werror',
              '-Wno-unused-variable','-Wno-unused-label','-Wno-parentheses',*flags,
              '-I',str(cls.target),str(cls.target/'cdc_directory.c'),'-I',str(OVERLAY.parent),str(OVERLAY),'-o',str(cls.binary)]
        result=subprocess.run(args,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(result.stdout+result.stderr)
        (cls.target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()

    @classmethod
    def tearDownClass(cls):
        bounds.CDBoundsTests.tearDownClass()
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def run_case(self,case):
        result=subprocess.run([str(self.binary),str(case)],capture_output=True,text=True,timeout=60,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        self.results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('pass:',result.stdout)

    def test_record_extents_names_padding_unsigned_values(self):self.run_case(0)
    def test_1512_selector_count_pages_and_one_free_block(self):self.run_case(1)
    def test_raw_mode1_descriptor_uses_its_original_payload(self):self.run_case(12)
    def test_image_fifo_lengths_and_audio_that_resembles_xa(self):self.run_case(13)
    def test_cooked_image_and_gap_wide_reads_clear_reused_buffer_tail(self):self.run_case(14)
    def test_mode2_header_bcd_boundaries_and_fifo_widths(self):self.run_case(15)
    def test_malformed_missing_empty_and_straddled_records(self):self.run_case(2)
    def test_descriptor_scan_root_bounds_and_transitions(self):self.run_case(3)
    def test_full_ids_self_parent_busy_queries_and_actual_fifo(self):self.run_case(4)
    def test_read_file_extents_selected_routes_and_refusals(self):self.run_case(5)
    def test_abort_pause_failure_and_buffer_preservation(self):self.run_case(6)
    def test_private_block_reuse_and_atomic_error_completion(self):self.run_case(7)
    def test_atomic_pause_and_pending_descriptor_transition(self):self.run_case(8)
    def test_bounded_readonly_names_and_preserved_traces(self):self.run_case(9)
    def test_wide_extent_arithmetic_and_state_preserving_refusals(self):self.run_case(10)
    def test_startup_places_directory_reservation_between_sector_pool_and_toc(self):self.run_case(11)
    def test_disc_replacement_reloads_the_new_directory_even_after_close_error(self):self.run_case(16)


if __name__=='__main__':unittest.main()
