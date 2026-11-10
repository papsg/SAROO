"""Reproduce six isolated old-parser defects plus a valid control.

Usage: python3 tests/check_cdc_directory_baseline.py /path/to/old/saturn_cdc.c
Use the source from public commit 525dc55. This intentionally differs from
the current streamed fixture; it preserves the old sector-count units.
"""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]


def main():
    raw=Path(sys.argv[1]).read_bytes()
    header=(ROOT/'Firm_MCU/Saturn/cdc.h').read_bytes()
    build=ROOT/'tests/build/cdc-directory-baseline';build.mkdir(parents=True,exist_ok=True)
    target=Path(tempfile.mkdtemp(prefix='case-',dir=build))
    def function(sig):
        start=raw.index(sig)
        return raw[start:raw.index(b'\n}',start)+2]+b'\n'
    code=b''.join(function(sig) for sig in (b'static void _set_filter(',b'static u32 isonum_711(',
        b'static u32 isonum_733(',b'void fill_fileinfo(',b'int handle_diread(',
        b'static __attribute__((noinline)) void start_directory_read(',b'int read_dir('))
    inputs={'saturn_cdc.c':raw,'cdc.h':header,'legacy.inc':code,
            'types.h':header[header.index(b'#define REG8('):header.index(b'extern CDBLOCK cdb;')]}
    for name in ('cdc_directory_legacy.c','check_cdc_directory_baseline.py'):
        inputs[name]=(ROOT/'tests'/name).read_bytes()
    for name in ('ff.h','ffconf.h'): inputs[name]=(ROOT/'Firm_MCU/FatFS'/name).read_bytes()
    for name,data in inputs.items(): (target/name).write_bytes(data)
    (target/'inputs.json').write_text(json.dumps({n:hashlib.sha256(v).hexdigest() for n,v in inputs.items()},indent=2)+'\n')
    args=[*shlex.split(os.environ.get('CC','cc')),'-std=gnu11','-g','-O1','-Wall','-Wextra','-Werror',
          '-Wno-unused-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer',
          '-I',str(target),str(target/'cdc_directory_legacy.c'),'-o',str(target/'legacy')]
    result=subprocess.run(args,capture_output=True,timeout=60)
    (target/'compile.log').write_bytes(result.stdout+result.stderr)
    (target/'command.json').write_text(json.dumps(args,indent=2)+'\n');result.check_returncode()
    results=[]
    for case in range(7):
        result=subprocess.run([str(target/'legacy'),str(case)],capture_output=True,text=True,timeout=30,
            env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
        results.append(dict(case=case,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
        (target/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        if case<6:
            assert result.returncode and any(s in result.stderr for s in ('Assertion failed:','runtime error:','AddressSanitizer')),results[-1]
        else: assert not result.returncode and 'pass:' in result.stdout,results[-1]
    print('pass: six compiled old-source failures and one valid control')


if __name__=='__main__':main()
