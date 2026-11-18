# Menu findings from the line-by-line port audit

These findings are separate from the five completed corrections in
[FIVE-SAFETY-PORTS.md](FIVE-SAFETY-PORTS.md). The original SAROO menu and its
contributors retain their attribution. The corrections below adapt work by
paps-g from the private development history; each entry names that source.
Host tests and target builds do not qualify behavior on a physical Saturn.

## MENU-ERROR-REDRAW: show persistent MCU failures after menu restart

Adapted from paps-g `139fd48f` (`saroo-menu-channel.patch`). The selected menu
build enables save generation. In the old public [`menu_run`](../Firm_Saturn/conio.c),
the warning code was initialized before `_restart`. `draw_menu_frame` clears the
visible warning, but an unchanged negative MCU status was then suppressed. The
correction resets that code at `_restart`, immediately before drawing the new
frame. Normal polling and warning deduplication within one frame are unchanged.

`python3 -m unittest discover -s tests -p test_menu_warning_redraw.py -v`
extracts the actual `menu_run` function and compiles it with ASan/UBSan. Before
the correction, only the persistent-error case fails; changed-error, healthy
and busy-status controls pass. Afterward all four pass. The same four pass with
`SAROO_MENU_PROFILE=release`. An isolated offline SuperH menu build succeeds:
96,696 code/data bytes, 9,540 BSS bytes and 8,452 bytes of body margin. The
test models a redraw and MCU replies, not VDP hardware or a physical MCU fault.

## LOAD-PLAYER-FAIL-OWNER: restore the menu pad owner on failure

Adapted from paps-g `4c918c9b` (`saroo-menu-scu-ist.patch`). The selected
[`my_cdplayer`](../Firm_Saturn/game_load.c) suspends menu pad handling before
switching to the BIOS CD player. If the bounded SMPC reset-disable request
fails, the old public path returned without `pad_resume`. The corrected error
return restores the same owner and interrupt state that the successful path
already restores. It still refuses to overwrite high work RAM after a failed
reset-disable request.

The existing `test_menu_clock_reset.py` extracts the actual player entry up to
its first destructive WRAM write. Its failure case now checks that the pad is
resumed exactly once for external-latch refusal, completion timeout and rejected
reply, while WRAM is untouched. The old public source fails the owner check;
the other nine methods are valid controls. All ten methods pass under
ASan/UBSan and `SAROO_MENU_CD_PROFILE=release`. The isolated offline SuperH menu
build succeeds with 96,696 code/data bytes, 9,540 BSS bytes and 8,452 bytes of
body margin. This does not establish how often reset-disable fails on hardware
or whether the old polling fallback was visibly impaired.
