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
