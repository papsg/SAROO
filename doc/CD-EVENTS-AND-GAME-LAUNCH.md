# CD events and game launch

## Exit CD waits on terminal drive states

Adapted from the sector reader and physical disc authentication portions of
paps-g `80364f8f` (SATURN-CD-003). If a sector query returns zero available
sectors, the reader checks the drive status and returns `-1` for OPEN, NODISC,
ERROR or FATAL. It still consumes sectors already in the buffer and retains any
bytes it copied before a later failure. RETRY can still produce data, so the
reader keeps waiting. The disc authentication path now returns on ERROR after
attempting to restore the original sector size. Its existing FATAL-to-status
path retains the original result and cleanup behavior. Sega describes the
drive states and their recovery conditions in the CD library manual.
[ST-162-062094, §4.1, printed pp. 33–35](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21).

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
```

Run with `-k terminal` and set `SAROO_MENU_CD_SOURCE` to the parent
`Firm_Saturn` directory to reproduce the two failures. Both parent fixtures
observe an extra poll where the caller should return or restore sector size.
All 35 CD methods pass under ASan/UBSan and O2 after the change, including
status and flag combinations, partial output, buffered sectors, and cleanup
failure precedence.

The SH-2 menu links 80,820 code/data bytes (+32) and 9,244 BSS bytes.
Physical disc recovery and elapsed wait time remain unqualified.

## Preserve VDP1 writes during game launch

Adapted from paps-g `74bee684` (SATURN-VDP1-001). The game launch used to
clear VDP1 memory after applying game patches, erasing Croc's final-word
workaround and configured writes. The dispatcher now initializes video
memory before applying the selected callback and then the configuration.
The same clear extent and command markers are used for unpatched launches.
An explicit configuration write still takes precedence over a built-in
value. [Original Croc callback](https://github.com/tpunix/SAROO/commit/f6a35a4d1d5a32af507f7d6d02edc8b7ab169fbc),
[upstream issues #181](https://github.com/tpunix/SAROO/issues/181)
and [#359](https://github.com/tpunix/SAROO/issues/359).

Ultraman can transfer control directly to its ROM. Its selected callback runs
before the normal clear, which occurs once if the callback returns. KOF95
retains its deferred callback. Sega defines `0x8000` in CMDCTRL as Draw End.
[VDP1 User's Manual ST-013-R3-061694, §7.10, printed p. 132](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf#page=147).

```sh
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
SAROO_MENU_VDP1_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
```

Set `SAROO_MENU_VDP1_SOURCE` to the parent `Firm_Saturn` directory
to reproduce the Croc and configuration failures. The parent passes five
other controls. The port passes all seven methods under ASan/UBSan and O2,
with whole-VRAM comparisons, patch bypass, game selection and ROM takeover
checks. Ten additional companion ROM and BIOS handoff methods pass. The
SH-2 menu links 80,852 code/data bytes (+32) and 9,244 BSS bytes. Croc's
physical drawing behavior and the reported game symptoms remain unqualified.
