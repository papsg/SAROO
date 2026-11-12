"""Reproduce common-pool admission using the compiled Arm RTX provider in QEMU."""
# Adapted from paps-g e4953e40.
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RtxPoolTests(unittest.TestCase):
    def test_led_resize_preserves_later_common_pool_admission(self):
        name = os.environ.get('SAROO_RTX_BUILD')
        if not name:
            self.skipTest('set SAROO_RTX_BUILD to a completed GNU MCU build')
        build = Path(name).resolve(strict=True)
        cc = os.environ.get('SAROO_ARM_CC', 'arm-none-eabi-gcc')
        qemu = os.environ.get('SAROO_ARM_QEMU', 'qemu-system-arm')
        self.assertIsNotNone(shutil.which(cc))
        self.assertIsNotNone(shutil.which(qemu))
        main = (build / 'Firm_MCU/Main/main.c').read_bytes().replace(b'\r\n', b'\n')
        # The startup function is the final definition in the staged source.
        startup, count = re.subn(rb'\A[\s\S]*?\n(?=int main\(void\)\n)', b'', main)
        self.assertEqual(count, 1)
        parents = ROOT / 'tests/build'
        parents.mkdir(parents=True, exist_ok=True)
        out = Path(tempfile.mkdtemp(prefix='rtx-pool-', dir=parents))
        (out / 'startup.inc').write_bytes(startup)
        (out / 'pool.ld').write_text("""ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 4M
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}
SECTIONS {
  .vectors : { KEEP(*(.vectors)) } >FLASH
  .text : { *(.text*) *(.rodata*) } >FLASH
  .ARM.exidx : { *(.ARM.exidx*) } >FLASH
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } >RAM
}
""")
        sdk = build / 'sdk'
        includes = [out, build / 'Firm_MCU/FatFS', build / 'Firm_MCU/RTE/CMSIS',
                    build / 'Firm_MCU/RTE/_App', sdk / 'core', sdk / 'stm32',
                    sdk / 'rtos/Include', sdk / 'rtos/RTX/Include', sdk / 'rtos/RTX/Source']
        args = [cc, '-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16', '-mfloat-abi=softfp',
                '-DSTM32H750xx', '-Os', '-ffunction-sections', '-fdata-sections',
                '-nostartfiles', '-Wl,--gc-sections', '-T', str(out / 'pool.ld')]
        args += [part for path in includes for part in ('-I', str(path))]
        elf = out / 'pool.elf'
        for unit in ('rtx_memory', 'rtx_evr'):
            obj, = build.glob(f'*-{unit}.o')
            args.append(str(obj))
        args += [str(ROOT / 'tests/rtx_pool.c'), '-o', str(elf)]
        compile_result = subprocess.run(args, capture_output=True, timeout=60)
        (out / 'compile.log').write_bytes(compile_result.stdout + compile_result.stderr)
        self.assertEqual(compile_result.returncode, 0, compile_result.stderr.decode())
        run = subprocess.run([qemu, '-M', 'mps2-an500', '-nographic', '-monitor', 'none',
                              '-serial', 'none', '-semihosting-config', 'enable=on,target=native',
                              '-kernel', str(elf)], capture_output=True, timeout=30)
        (out / 'qemu.log').write_bytes(run.stdout + run.stderr)
        self.assertEqual(run.returncode, 0, run.stderr.decode())
        self.assertIn(b'PASS: startup, directory depth, hole reuse and exhaustion match the parent',
                      run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
