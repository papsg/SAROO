"""Actual disc/ISO and automatic/menu callers coupled to save APIs and FatFs."""
# Adapted from paps-g 82125993; no private source preparation or device access.
import unittest
from mcu_save_support import MCUSaveTest

class DiscSaveSessionTests(MCUSaveTest):
    fixture = 'disc'

    def invoke(self, mode, value=0):
        return self.run_case([mode, value])

    def test_automatic_disc_change_preserves_live_and_pending_save_session(self):
        for state in range(6):
            with self.subTest(state=state): self.invoke('session',state)

    def test_explicit_menu_load_still_selects_requested_save(self):
        for index in (0,1):
            with self.subTest(index=index): self.invoke('menu',index)

    def test_failed_swap_waits_for_reinsertion_and_preserves_request(self):
        for fault in range(1,5):
            with self.subTest(fault=fault): self.invoke('swap-failure',fault)

    def test_load_and_preview_validate_index_and_allocation(self):
        for mode in ('index','preview-index'):
            for index in (-2,-1,2,65534,65535,2147483647):
                with self.subTest(mode=mode,index=index): self.invoke(mode,index)
        for mode in ('allocation','preview-allocation'):
            with self.subTest(mode=mode): self.invoke(mode)

    def test_optical_boot_stop_unloads_without_save_io_and_is_idempotent(self):
        self.invoke('stop')

    def test_explicit_actions_wait_for_ack_and_preserve_pending_saves_on_timeout(self):
        for value in range(8):
            with self.subTest(value=value):self.invoke('quiesce',value)

    def test_worker_idle_and_request_retirement_protect_new_publication(self):
        for value in range(6):
            with self.subTest(value=value):self.invoke('publication',value)

    def test_automatic_change_does_not_wait_on_its_own_worker(self):
        self.invoke('automatic-no-wait')

    def test_sd_reset_propagates_identification_results(self):
        for result in (0,-1,1,2,19):
            with self.subTest(result=result): self.invoke('reset',result)

if __name__ == '__main__': unittest.main()
