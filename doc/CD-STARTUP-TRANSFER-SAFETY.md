# CD startup, transfer and save metadata safety

## Read subcode safely after a drive stop

Adapted from paps-g `555eb231` (MCU-CDC-018) for the public v0.8-derived
firmware. A seek to home reports track `0xff`. The preceding
[`dc5e079` implementation](https://github.com/papsg/SAROO/blob/dc5e0798b4992b4afb59641581da2daf5ebda615/Firm_MCU/Saturn/saturn_cdc.c)
used that byte to index the track table. It also signaled data-ready for
unsupported subcode types without starting a transfer.

Sega permits Q queries in every drive state and defines the invalid report
after a seek to home. Q carries ten bytes and R–W carries one twelve-word
pack. [CD Communication Interface User's Manual (CD Part), §§4.2–4.3,
archive page 4 of 19](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p04_10.htm),
[§§7.2.4–7.2.5, archive page 8 of 19](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p07_11.htm).

The MCU uses the existing checked track lookup. With no valid track it
returns `0xffffff` in the relative-position field and retains the other
report fields. Q and available R–W requests publish one transfer after
forming the reply. An active transfer or empty R–W queue still returns
WAIT. An idle unsupported request returns REJECT without signaling data-ready.

```sh
python3 -B -m unittest discover -s tests -p test_cdc_drive.py -k subcode -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_drive.py -k subcode -v
make -C tests check-cdc-command-buffer
```

Run the first command with `SAROO_CDC_DRIVE_DIR` pointing at the parent
commit's `Firm_MCU/Saturn` source to reproduce the failures. The parent
fails the stop transition, track validation and unsupported-type checks.
It passes the valid-payload control. The port passes all four under
ASan/UBSan and O2, including all track bytes for every table capacity
from 1 through 99, all type bytes with busy and empty ownership controls,
and 6,144 valid Q payloads. The full CD group passes 163 methods, including
1,179,648 complete replies in the register fixture.

The [GNU MCU App build](../tools/README.md#original-mcu-app-compilelink-check)
loads 63,212 bytes with 2,324 bytes of ITCM space and 60,216 bytes of
static DTCM. The tracked build inputs match the verified source. FIFO start
and scheduling are modeled in the host fixtures. Physical report timing,
multisession subcode and the original MDK build remain unqualified.

## Propagate CD query and authentication failures

Adapted from paps-g `544f2b10` (SATURN-CD-002). A CMOK timeout can
return before `cdc_cmd` fills its response. A WAIT or REJECT response
also carries a command error. Twelve menu queries now return that error
before reading the response or assigning caller outputs. Status and
hardware-info queries retain their optional output pointers. Existing
callers that consumed query values unconditionally now check the result.
Sega defines WAIT and REJECT as distinct from an accepted command and
describes report validity separately from command success. [CD Communication
Interface User's Manual (CD Part), §7.2.5(2), (4), archive page 8 of 19](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p07_11.htm#no5_0).

Disc authentication checks sector-size setup before calling the existing
relocated routine. After setup succeeds, it attempts to restore normal
sector size on success, status failure, or a report that the drive is
open or empty. It preserves the first error and returns a cleanup error
when the earlier steps succeed. The ordinary device-authentication wrapper
also returns command and status-query failures. The authentication command
sequence and successful query decoding follow the existing public firmware.

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
```

Set `SAROO_MENU_CD_SOURCE` to the preceding commit's `Firm_Saturn`
directory for the baseline. Four of the five query methods fail there
and the valid-decoding control passes. All 13 CD query and player methods
pass in both profiles. The fixtures include 3,072 valid decode
combinations, distinct command failures and cleanup precedence. They
script CD responses and model the relocated call.

The [SuperH menu build](../tools/README.md#isolated-firmware-qualification)
links 80,452 code/data bytes, up from 80,308, and 9,240 BSS bytes into the
488,770-byte padded image. Hardware authentication, recovery from a
failed size command and the duration of the existing waits remain unqualified.

## Refresh comment and language on save overwrite

Adapted from paps-g `d00d9efe` (SATURN-SAVE-007). The save writer updated
date and payload on an overwrite but set the comment and language only
on creation. It now copies all ten comment bytes and the requested
language on every admitted write, including a matching retry. Directory
link creation remains conditional. Sega defines both fields in the
directory input and describes overwriting through the `BUP_Write` flag.
[System Library User's Guide, ST-162-R1-092994, Backup Library §2.1,
printed p. 42](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=47),
[§2.3 function No. 5, printed p. 45](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=50).

The Policenauts report supplied a relevant comment change from
`Autosave  ` to `Autosave10`. The fixtures use generated data with
that comment shape. The report remains open and the game’s disc change
remains unqualified. [Upstream issue #71](https://github.com/tpunix/SAROO/issues/71).

```sh
make -C tests check-firmware-save
python3 -B -m unittest discover -s tests -p test_mcu_save_api.py -v
```

Repeat both commands with `SAROO_FIRMWARE_SAVE_PROFILE=release`. Use
`SAROO_GAME_SAVE_SOURCE` to run either fixture against the preceding
`game_save.c`. The parent fails 23 overwrite cases and ten
matching retries with changed metadata. Eleven rejected writes leave
both archive images untouched on the parent and port. The port covers
both backup devices, same-size and resized writes, full and padded
comments, neighboring records and preservation of every other byte on
metadata-only writes. The coupled fixture uses the public MCU and FatFs
code with scripted I/O failures. It verifies a retry through the
existing pending-write path.

The SH-2 build retains 80,452 code/data bytes, 9,240 BSS bytes, and
a 44-byte direct `write_pending` frame. Physical backup timing,
power-loss recovery and the reported game transition remain unqualified.
