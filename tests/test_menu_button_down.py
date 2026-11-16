"""Actual full header; unsigned event oracle and signed caller masks (SSR-110)."""
# Adapted from paps-g 3ed0238f; no private preparer or device operations.
import json
import os
import shlex
import subprocess
import unittest

import test_menu_button_up as support


class MenuButtonDownTests(unittest.TestCase):
    fixture_name = 'menu-button-down'
    fixture_source = 'menu_button_down.c'
    driver_source = 'test_menu_button_down.py'
    setUpClass = classmethod(support.MenuButtonUpTests.setUpClass.__func__)
    tearDownClass = classmethod(support.MenuButtonUpTests.tearDownClass.__func__)

    def helper(self, case, count):
        flags = ['-O2'] if os.environ.get('SAROO_MENU_SAFETY_PROFILE') == 'release' else [
            '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        binary = self.target / ('press-' + str(case))
        command = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c11', '-g',
                   '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-function', *flags, '-I', str(self.target),
                   str(self.target / 'fixture.c'), '-o', str(binary),
                   # Let the signed parent reach UBSan. The native companion
                   # separately compiles both public interfaces strictly.
                   '-Wno-sign-compare']
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
        label = 'press-' + str(case)
        (self.target / (label + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        (self.target / (label + '-compile.log')).write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(binary), str(case)], capture_output=True, text=True, timeout=20,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        self.results.append(dict(case=case, returncode=result.returncode,
                                 stdout=result.stdout, stderr=result.stderr))
        (self.target / (label + '-run.log')).write_text(result.stdout + result.stderr)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f'{count} checks, 0 failures', result.stdout)
        self.assertEqual(result.stderr, '')

    def test_lower_half_signed_masks(self): self.helper(0, 229376)
    def test_right_signed_mask(self): self.helper(1, 7)
    def test_right_up_signed_mask(self): self.helper(2, 7)
    def test_all_defined_buttons_signed_mask(self): self.helper(3, 7)
    def test_all_16bit_masks_and_event_fields(self): self.helper(4, 458752)
    def test_arguments_are_evaluated_once(self): self.helper(5, 2)


if __name__ == '__main__': unittest.main()
