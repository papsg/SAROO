"""Actual backup date functions checked against Python's independent Gregorian calendar."""
# Adapted from paps-g f5ed0ae1; fixture media and all outputs stay in public.
import datetime
import hashlib
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

EPOCH = datetime.datetime(1980, 1, 1)
END = datetime.datetime(2236, 1, 1)
LIMIT = int((END - EPOCH).total_seconds() // 60)


def record(kind, stamp, fields):
    return struct.pack('>BI6B', kind, stamp, *fields)


def calendar_record(date):
    stamp = int((date - EPOCH).total_seconds() // 60)
    return record(0, stamp, (date.year - 1980, date.month, date.day,
                             date.hour, date.minute, (date.weekday() + 1) % 7))


class MenuSaveDateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build=ROOT/'tests/build/save-date';build.mkdir(parents=True,exist_ok=True)
        cls.target=Path(tempfile.mkdtemp(prefix='case-',dir=build));cls.results=[]
        source=Path(os.environ.get('SAROO_GAME_SAVE_SOURCE',ROOT/'Firm_Saturn/game_save.c'))
        inputs={'game_save.c':source,'firmware_save_date.c':ROOT/'tests/firmware_save_date.c',
                'test_firmware_save_date.py':Path(__file__)}
        for name in ('main.h','smpc.h'):inputs[name]=ROOT/'tests/firmware-save-stubs'/name
        hashes={}
        for name,path in inputs.items():
            data=path.read_bytes();(cls.target/name).write_bytes(data);hashes[name]=hashlib.sha256(data).hexdigest()
        (cls.target/'inputs.json').write_text(json.dumps(hashes,indent=2)+'\n')
        flags=['-O2'] if os.environ.get('SAROO_FIRMWARE_SAVE_PROFILE')=='release' else [
            '-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
        cls.binary=cls.target/'date'
        command=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-fno-strict-aliasing',
                 '-Wno-int-to-pointer-cast','-Wno-pointer-to-int-cast',*flags,'-I',str(cls.target),
                 str(cls.target/'firmware_save_date.c'),'-o',str(cls.binary)]
        r=subprocess.run(command,capture_output=True,timeout=60)
        (cls.target/'compile.log').write_bytes(r.stdout+r.stderr);r.check_returncode()

    @classmethod
    def tearDownClass(cls):
        (cls.target/'results.json').write_text(json.dumps(cls.results,indent=2)+'\n')

    def invoke(self, argument):
        result=subprocess.run([str(self.binary),str(argument)],capture_output=True,timeout=30,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1','ASAN_OPTIONS':'detect_leaks=0'})
        self.results.append(dict(argument=Path(argument).name,returncode=result.returncode,
            stdout=result.stdout.decode(errors='replace'),stderr=result.stderr.decode(errors='replace')))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertNotIn(b'runtime error:',result.stderr)

    def test_packing_ignores_week_without_modifying_any_field(self):
        self.invoke('week')

    def test_every_representable_day_and_time_boundaries(self):
        path = self.target / 'days.bin'
        with path.open('wb') as stream:
            for day in range((END - EPOCH).days):
                base = EPOCH + datetime.timedelta(days=day)
                for minute in (0, 1439, (day * 37) % 1440):
                    stream.write(calendar_record(base + datetime.timedelta(minutes=minute)))
        self.invoke(path)

    def test_each_minute_around_leap_month_year_and_century_boundaries(self):
        path = self.target / 'boundaries.bin'
        dates = ((1980, 2, 29), (1981, 1, 1), (2000, 2, 29),
                 (2024, 2, 29), (2025, 3, 1), (2099, 12, 31),
                 (2100, 2, 28), (2100, 3, 1), (2200, 2, 28),
                 (2200, 3, 1), (2235, 12, 31))
        with path.open('wb') as stream:
            for fields in dates:
                base = datetime.datetime(*fields)
                for minute in range(1440):
                    stream.write(calendar_record(base + datetime.timedelta(minutes=minute)))
        self.invoke(path)

    def test_invalid_calendar_bytes_are_rejected_without_input_mutation(self):
        path = self.target / 'invalid-fields.bin'
        with path.open('wb') as stream:
            for year in (0, 1, 120, 220, 255):
                for month, day in ((2, 28), (4, 30), (12, 31)):
                    base = [year, month, day, 23, 59, 0]
                    for field in (1, 2, 3, 4):
                        for byte in range(256):
                            fields = base.copy()
                            fields[field] = byte
                            try:
                                value = datetime.datetime(fields[0] + 1980, *fields[1:5])
                            except ValueError:
                                stream.write(record(1, 0xffffffff, fields))
                            else:
                                stream.write(calendar_record(value))
        self.invoke(path)

    def test_unrepresentable_packed_dates_produce_invalid_zero_fields(self):
        path = self.target / 'invalid-stamps.bin'
        with path.open('wb') as stream:
            for stamp in (LIMIT, LIMIT + 1, LIMIT + 1439, 0x7fffffff, 0x80000000, 0xffffffff):
                stream.write(record(2, stamp, (0, 0, 0, 0, 0, 0)))
        self.invoke(path)

    def test_null_arguments(self):
        self.invoke('null')

    def test_previously_correct_epoch_day_and_time_fields_remain(self):
        self.invoke('compatible-fields')

    def test_native_backup_dispatch_uses_corrected_calendar(self):
        self.invoke('native-dispatch')

    def test_system_backup_dispatch_is_preserved(self):
        self.invoke('dispatch')


if __name__ == '__main__':
    unittest.main()
