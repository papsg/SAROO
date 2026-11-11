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

## Preserve readiness when DataEnd fails

Adapted from paps-g `3d10ea73` (SATURN-CD-003). DataEnd now clears DRDY
and writes the optional 24-bit count after a successful reply. A timeout,
REJECT or WAIT returns its original error without acknowledging DRDY. This
also preserves a ready event arriving during the existing cleanup attempt
if that attempt times out. Sega treats data readiness and transfer end as
separate events and specifies the count returned by DataEnd.
[ST-162-062094, §3.4, printed p. 32](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=20),
[§8.2.1 No. 1.10, printed p. 81](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=69).

Run the CD tests above with `-k data_end` and set `SAROO_MENU_CD_SOURCE`
to the parent `Firm_Saturn` directory to reproduce the failure. The parent
fails three methods and passes the successful-count control. All 38 CD
methods pass ASan/UBSan and O2 on the port. The fixture checks the event
bits and optional output on failure, success, and arrival of DRDY during
cleanup. The SH-2 menu retains 80,852 code/data and 9,244 BSS bytes.
Physical interrupt timing and recovery remain unqualified.

## Signal sector-read completion at DataEnd

Adapted from paps-g `35cf0b88` (MCU-CDC-019). The MCU used to return
EHST while GET or GETDEL still owned the transfer. Both commands now
publish DRDY at FIFO setup. DataEnd and cancellation retain their
completion events. Sega places EHST after DataEnd for these two commands.
[CD Communication Interface User's Manual, §8.2.7, functions 7.2 and 7.4](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p08_17.htm).

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
```

Run `test_cdc_read_ownership.py` with `-k completion_follows` and set
`SAROO_CDC_READ_SOURCE` to the parent's `saturn_cdc.c` to observe
the extra EHST bit. The focused case checks 12 early and full transfers
for GET and GETDEL, including overlapping requests, FIFO service, the
public firmware's produced-word counts, and buffer ownership. Both runs
of the full CD group pass 164 methods. The GNU MCU App loads 63,212
bytes with 2,324 bytes of ITCM space. Physical
event timing and game behavior remain unqualified.
