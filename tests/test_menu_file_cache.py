"""Actual menu file completion with a modeled write-through SH7604 cache."""
# Adapted from paps-g 8078e501.
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


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
        code += b'\nstatic void test_cache_purge(u32 a,u32 n) { file_cache_purge(a,n); }\n'
        output = ROOT / 'tests/build/menu-file-cache'
        output.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=output))
        (cls.target / 'cache.inc').write_bytes(code)
        (cls.target / 'main.c').write_bytes(raw)
        cls.binary = cls.target / 'menu-cache'
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

    def case(self, mode, address, size, policy=1, outcome=0):
        with self.subTest(mode=mode, address=hex(address), size=size, policy=policy, outcome=outcome):
            result = subprocess.run([str(self.binary), *map(str, (mode,address,size,policy,outcome))],
                capture_output=True, timeout=15, env={**os.environ, 'UBSAN_OPTIONS':'halt_on_error=1'})
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
                    self.case(1, address, size, outcome=outcome)

    def test_rejected_requests_and_writes_leave_cache_untouched(self):
        for outcome in (7, 8, 9): self.case(1, 0x02800003, 32, outcome=outcome)

    def test_purge_work_is_bounded_for_large_reads(self):
        for address, size in ((0x02800000,4096), (0x02800001,4096),
                              (0x02800000,0x200000), (0x020c7100,0xf38f00)):
            self.case(2, address, size)


if __name__ == '__main__':
    unittest.main()
