"""Check lossless browser save fields against independent Gregorian arithmetic."""
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def calendar(minutes):
    days, within = divmod(minutes, 1440)
    year = 1980
    leap = lambda y: y % 4 == 0 and (y % 100 != 0 or y % 400 == 0)
    while days >= 365 + leap(year): days -= 365 + leap(year); year += 1
    lengths = [31, 28 + leap(year), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    month = 1
    for length in lengths:
        if days < length: break
        days -= length; month += 1
    hour, minute = divmod(within, 60)
    return dict(year=year, month=month, day=days + 1, hour=hour, minute=minute)


class BrowserSaveTextTests(unittest.TestCase):
    def test_text_and_calendar(self):
        node = shutil.which('node')
        if not node: self.skipTest('Node.js is required for browser field checks')
        generator = random.Random(374)
        minutes = [0, 1, 59, 60, 1439, 1440, 0x7fffffff, 0xfffffffe, 0xffffffff]
        minutes += [generator.randrange(0x100000000) for _ in range(2048)]
        with tempfile.TemporaryDirectory(prefix='saroo-save-text-') as temp:
            fixture = Path(temp) / 'dates.json';fixture.write_text(json.dumps([(n,calendar(n)) for n in minutes]))
            result = subprocess.run([node,'--test','--test-reporter=tap',str(ROOT/'tests/web_save_text.test.mjs')],cwd=ROOT,
                env={**os.environ,'SAROO_SAVE_DATE_FIXTURE':str(fixture)},capture_output=True,text=True,timeout=60)
            evidence=ROOT/'tests/build/web-save-text-tests';evidence.mkdir(parents=True,exist_ok=True)
            (evidence/'node.log').write_text(result.stdout+result.stderr)
            shutil.copyfile(fixture,evidence/'dates.json')
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':unittest.main()
