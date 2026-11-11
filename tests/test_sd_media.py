"""Exercise actual SD card identification and sector admission with modeled replies."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SDMediaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = ROOT / 'tests/build/sd-media'
        build.mkdir(parents=True, exist_ok=True)
        cls.target = Path(tempfile.mkdtemp(prefix='case-', dir=build))
        cls.results = []
        source = Path(os.environ.get('SAROO_SD_SOURCE', ROOT / 'Firm_MCU/Main/sdio_h7.c'))
        data = source.read_bytes()
        state = data[data.index(b'#define SDCLK_LOW '):data.index(b'static osSemaphoreId_t cmd_sem;')]
        identify = data.index(b'int sdcard_identify(')
        sectors = data[data.index(b'#define SDBUF_SIZE'):identify]
        if b'static __attribute__((noinline)) int sd_sectors(' in sectors:
            state += b'#define HAS_SHARED_SECTOR_WRAPPER 1\n'
        sectors = sectors.replace(b'(u32)buf', b'sd_address(buf)')
        acmd = data.index(b'static int sd_acmd(')
        functions = data[acmd:data.index(b'\n}', acmd)+3]
        functions += data[identify:data.index(b'\n}', identify)+3] + sectors
        inputs = {'sdio_h7.c': data, 'state.inc': state, 'identify.inc': functions,
                  'RTX_Config.h': (ROOT / 'Firm_MCU/RTE/CMSIS/RTX_Config.h').read_bytes(),
                  'sd_model.h': (ROOT / 'tests/sd_model.h').read_bytes(),
                  'sd_media.c': (ROOT / 'tests/sd_media.c').read_bytes(),
                  'test_sd_media.py': Path(__file__).read_bytes()}
        for name, content in inputs.items():
            (cls.target / name).write_bytes(content)
        (cls.target / 'inputs.json').write_text(json.dumps({
            name: hashlib.sha256(content).hexdigest() for name, content in inputs.items()
        }, indent=2) + '\n')
        flags = ['-O2'] if os.environ.get('SAROO_SD_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cls.binary = cls.target / 'sd-media'
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11', '-g',
                   '-Wall', '-Wextra', '-Werror', *flags, str(cls.target / 'sd_media.c'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, timeout=60)
        (cls.target / 'compile.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode(errors='replace'))

    @classmethod
    def tearDownClass(cls):
        (cls.target / 'results.json').write_text(json.dumps(cls.results, indent=2) + '\n')

    def invoke(self, mode, value=0):
        result = subprocess.run([str(self.binary), mode, str(value)], capture_output=True,
                                text=True, timeout=10,
                                env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(mode=mode, value=value, returncode=result.returncode,
                                 stdout=result.stdout, stderr=result.stderr))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_same_card_can_reidentify_with_a_new_relative_address(self):
        for rca in (1, 0x1234, 0x7fff, 0x8000, 0xffff):
            with self.subTest(rca=rca): self.invoke('same', rca)

    def test_changed_card_cannot_accept_stale_filesystem_sector_io(self):
        for byte in range(16):
            with self.subTest(byte=byte): self.invoke('changed', byte)

    def test_failed_initialization_revokes_sector_access_and_preserves_identity(self):
        for command in (0, 41, 2, 3, 9, 7, 16, 55, 6, 13):
            with self.subTest(command=command): self.invoke('failure', command)

    def test_failed_first_attempt_does_not_bind_an_unusable_card(self):
        for command in (9, 7, 16, 6, 13):
            with self.subTest(command=command): self.invoke('first-failure', command)

    def test_direct_and_bounced_reads_and_writes_preserve_copy_and_error_order(self):
        self.invoke('sectors')

    def test_reserved_zero_relative_address_cannot_enable_io(self):
        self.invoke('zero')

    def test_identity_validity_is_separate_from_cid_contents(self):
        self.invoke('zero-cid')

    def test_sector_access_stays_closed_through_every_identification_command(self):
        for existing_rca in (0,1):
            with self.subTest(existing_rca=existing_rca): self.invoke('during',existing_rca)


if __name__ == '__main__':
    unittest.main()
