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
        for fault in range(1,6):
            with self.subTest(fault=fault): self.invoke('swap-failure',fault)

    def test_real_configuration_failure_closes_readers_before_save_admission(self):
        for value in range(6):
            with self.subTest(value=value): self.invoke('configuration', value)

    def test_real_configuration_loads_reset_previous_game_defaults(self):
        self.invoke('configuration-defaults')

    def test_non_game_uses_runtime_global_configuration_and_preserves_saves(self):
        for pending in (0,1):
            with self.subTest(pending=pending): self.invoke('nongame',pending)

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

    def test_open_requires_an_observed_removal_and_remounts_requested_image(self):
        for next_disc in (0, 1):
            with self.subTest(next_disc=next_disc):
                self.invoke('tray-remount', next_disc)

    def test_sd_reset_propagates_identification_results(self):
        for result in (0,-1,1,2,19):
            with self.subTest(result=result): self.invoke('reset',result)

    def test_automatic_reidentification_holds_the_filesystem_lock(self):
        for result in (0,1):
            with self.subTest(result=result): self.invoke('reset-lock',result)

if __name__ == '__main__': unittest.main()
