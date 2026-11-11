"""Actual menu file completion with a modeled write-through SH7604 cache."""
# Adapted from paps-g 8078e501.
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from test_file_transfer import protocol_definitions


ROOT = Path(__file__).resolve().parents[1]


def host_cache_operations(code):
    """Substitute only the CPU status-register boundary; keep the C purge policy."""
    code = code.replace(b'__asm__ volatile ("stc sr,%0" : "=r" (sr) :: "memory");',
                        b'sr = cache_sr_read();')
    code = code.replace(b'__asm__ volatile ("ldc %0,sr" :: "r" (sr | 0xf0u) : "memory", "t");',
                        b'cache_sr_write(sr | 0xf0u);')
    code = code.replace(b'__asm__ volatile ("ldc %0,sr" :: "r" (sr) : "memory", "t");',
                        b'cache_sr_write(sr);')
    code = code.replace(b'u8 mode = REG8(0xfffffe92u);',
                        b'u8 mode = cache_control_read(0xfffffe92u);')
    return code


class MenuFileCacheTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(os.environ.get('SAROO_MENU_CACHE_SOURCE', ROOT / 'Firm_Saturn/main.c'))
        raw = source.read_bytes()
        start = raw.index(b'static u32 file_capacity(')
        end = raw.index(b'/**********************************************************/', start)
        words = raw.index(b'u32 BE32(')
        code = raw[words:raw.index(b'/**********************************************************/', words)] + raw[start:end]
        code = host_cache_operations(code).replace(b'(u32)buf', b'host_bus_address(buf)')
        code = code.replace(b'((TMPBUFF_ADDR & 0x1fffffff) + 0x100)', b'0x020c7100')
        if b'file_cache_purge(' not in raw:
            code += b'\nstatic void file_cache_purge(u32 a,u32 n) { (void)a; (void)n; }\n'
        args = b'a,n,0' if b'u32 size, int mirrored)' in code else b'a,n'
        code += b'\nstatic void test_cache_purge(u32 a,u32 n) { file_cache_purge(' + args + b'); }\n'
        output = ROOT / 'tests/build/menu-file-cache'
        output.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=output))
        (cls.target / 'cache.inc').write_bytes(code)
        (cls.target / 'main.c').write_bytes(raw)
        (cls.target / 'protocol.inc').write_bytes(
            protocol_definitions((ROOT / 'Firm_Saturn/main.h').read_bytes()))
        cls.tags = (0, 32) if b'static u16 file_ack;' in raw else (0,)
        cls.binary = cls.target / 'menu-cache'
        cls.results = []
        flags = ['-O2'] if os.environ.get('SAROO_FILE_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', *flags,
                   '-I', str(cls.target), str(ROOT / 'tests/menu_file_cache.c'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def case(self, mode, address, size, policy=1, outcome=0, tag=0, ram=0xb000):
        with self.subTest(mode=mode, address=hex(address), size=size, policy=policy,
                          outcome=outcome, tag=tag, ram=hex(ram)):
            args = (mode, address, size, policy, outcome, tag, ram)
            result = subprocess.run([str(self.binary), *map(str, args)],
                capture_output=True, timeout=15, env={**os.environ, 'UBSAN_OPTIONS':'halt_on_error=1'})
            self.results.append(dict(args=args, returncode=result.returncode,
                                     stderr=result.stderr.decode(errors='replace')))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_partial_lines_and_exact_end_preserve_other_cached_data(self):
        for alignment in range(16):
            for size in (0, 1, 2, 15, 16, 17, 31, 32, 255, 256):
                self.case(0, 0x02800000 + alignment, size)

    def test_large_and_window_end_reads_preserve_all_cache_modes_and_status(self):
        for policy in range(256):
            if policy & 0x30: continue  # CP reads zero; bit 5 is reserved.
            for address, size in ((0x02800001,4096), (0x02ffffef,17), (0x02800000,0x200000)):
                self.case(0, address, size, policy)

    def test_completion_invalidates_after_external_writes_including_failed_prefixes(self):
        for address in (0x02800003, 0x22800003):
            for size in (32, 4095, 4096, 0x200000):
                for outcome in range(7):
                    for tag in self.tags:
                        self.case(1, address, size, outcome=outcome, tag=tag)

    def test_unconfirmed_reads_invalidate_the_full_possible_write_extent(self):
        for address in (0x02800003, 0x22800003):
            for size in (32, 4095, 4096, 0x200000):
                for outcome in (10, 11):
                    self.case(1, address, size, outcome=outcome, tag=32)

    def test_mirrored_reads_purge_all_aliases_using_the_request_mode(self):
        for address in (0x02400003, 0x22400003, 0x02580003, 0x02600003, 0x22780003):
            for size in (1, 17, 32):
                for outcome in (0, 2, 3, 10, 11, 12):
                    self.case(1, address, size, outcome=outcome, tag=32, ram=0xa000)

    def test_other_modes_and_short_reads_before_the_mirror_stay_linear(self):
        for ram in (0x8000, 0x9000, 0xb000):
            self.case(1, 0x02400003, 32, tag=32, ram=ram)
        for address in (0x023fffe0, 0x023ffff0, 0x027ffff0):
            self.case(1, address, 64, outcome=1, tag=32, ram=0xa000)

    def test_rejected_requests_and_writes_leave_cache_untouched(self):
        for outcome in (7, 8, 9): self.case(1, 0x02800003, 32, outcome=outcome)

    def test_purge_work_is_bounded_for_large_reads(self):
        for address, size in ((0x02800000,4096), (0x02800001,4096),
                              (0x02800000,0x200000), (0x020c7100,0xf38f00)):
            self.case(2, address, size)


if __name__ == '__main__':
    unittest.main()
