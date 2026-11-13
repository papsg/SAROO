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

## Accept valid completion counts on partial sector reads

SATURN-CD-004, adapted from paps-g `482d27d8`. The menu copies just
the requested bytes from the final sector. It used to reject any
DataEnd count other than 1,024 or 1,162 words, even after the requested
bytes had arrived. It now accepts at least the words it consumed
and at most 1,162. This preserves the odd-byte tail and the original
setup, transfer and DataEnd errors.

[Sega ST-162-062094 §8.2.1, function 1.10, printed p. 81](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=69)
allows a CD block to report read-ahead on an interrupted transfer.
[§8.2.7, function 7.1, printed p. 95](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=83)
defines the 2,048-byte and 2,324-byte payloads in size mode zero.
The public MCU already reports the words it produced.
[MCU producer-count verification](CD-READ-SAFETY.md#mcu-cdc-012-cd-produced-read-counts-and-pending-windows).

With `SAROO_MENU_CD_SOURCE` set to the parent's `Firm_Saturn`,
select `-k bounded_completion` in `test_menu_cd_transfer.py` to
reproduce a valid copied read being rejected. The fixture covers
96 accepted and rejected count cases, destination bounds, and the
existing Kaitei Daisensou music preload with its 896-word final
reply. All 143 focused menu and peripheral methods and 70 optimized
CD menu methods pass. The stock menu still links with 85,244
code/data bytes and 19,956 bytes of body margin, and passes the
linked clock tail and 48 ROM clock cases. Physical playback has
not been verified for this port.

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

## Preserve signed disc-launch errors

Adapted from the normal menu portion of paps-g `218abca2`
(SATURN-MENU-009). The A and Z launch paths now interpret the completed
16-bit mailbox result as signed, so the MCU error `-6` reaches the
error dialog as `-6`. A successful A launch still calls the BIOS and a
successful Z launch still enters the CD player. The original report shows
the previous unsigned value of `65530`.
[Upstream SAROO issue #227](https://github.com/tpunix/SAROO/issues/227#issuecomment-2770044683).

```sh
python3 -B -m unittest discover -s tests -p test_menu_launch_errors.py -v
SAROO_MENU_LAUNCH_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_launch_errors.py -v
```

Set `SAROO_MENU_LAUNCH_SOURCE` to the parent `Firm_Saturn` directory
to reproduce the displayed value in both paths. The fixture runs the real
selector for all 65,536 possible mailbox words on each path and verifies
launch admission, player state and BIOS error propagation. Both methods
pass under ASan/UBSan and O2. The SH-2 menu links 80,836 code/data bytes
(−16) and 9,244 BSS bytes. Physical disc loading and the causes of the
reported launch failures remain unqualified.

## Keep a file read pending while the CD buffer is full

Adapted from paps-g `a506699c` (MCU-CDC-020). A full CD buffer pauses
the file reader with the current sector and interleave position still
pending. The pause now signals buffer full through BFUL. The reader can
resume when capacity returns, and EFLS still reports completion and
explicit abort. Sega distinguishes a buffer-full pause from the end of
file access.
[CD Communication Interface User's Manual, §§6.2.2(4–5) and 6.2.3(4)](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p06_10.htm).

```sh
python3 -B -m unittest discover -s tests -p test_cdc_xa.py -k file_end -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_xa.py -v
```

Set `SAROO_CDC_XA_MAIN` to the parent's `saturn_main.c` to reproduce
the early EFLS in three methods. The no-pressure control passes on both
versions. The port checks 1,864 scenarios with 38,208 full-buffer pauses
across all selectors, interleaved files, normal completion and abort.
The harness records each worker event write and models the retry boundary.
The 168-method CD group and all 13 XA methods at O2 pass.
The GNU MCU App loads 63,196 bytes (−16), with 2,340 bytes of ITCM space.
Physical scheduling and game behavior remain unqualified.

## Avoid the prohibited DMA force-stop write in Command & Conquer

Adapted from paps-g `6bcc069b` (GAME-REG-004). The US v1.000 game
writes to the removed SCU DMA force-stop register during soft reset.
Both discs share the checked executable. The existing game dispatcher
checks the product and version, then the callback checks the loaded
executable CRC and original instruction. It replaces just that
instruction with a NOP. The game keeps its subsequent DMA cleanup
and return trap. The checksum scan runs once at launch for the
matching game.

[Sega SCU Final Specifications ST-210-110194, precaution 14, printed p. 5](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf#page=9)
prohibits writes to that register.
[Hitachi SH7604 Hardware Manual, §8.4.2, printed p. 219](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=235)
describes the cached write used to update the instruction and any
resident line.

Run `python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v`.
Set `SAROO_MENU_VDP1_SOURCE` to the parent menu sources and select
`-k cnc_reset_patch` to see the unchanged instruction. The fixture
uses authored bytes and the real CRC and dispatch code. It checks the
matching executable and refuses a different version, disabled patches,
four different instructions and four changed executable positions.
All 144 focused methods and eight optimized dispatch methods pass.
The stock menu adds 108 code/data bytes (85,352 total, 19,848 bytes
of body margin) and passes the linked clock tail and 48 ROM clock cases.

The historical private menu passed the reported soft-reset sequence
on the development Saturn. This port has host and link verification.

## Declare the complete Vandal Hearts and Bio Hazard callback payload

The built-in callback shared by the Chinese Vandal Hearts and Bio Hazard
entries checks the original instruction before changing game RAM. Its copy
uses twelve longwords, but the inherited literal declared eleven. In the
stock Yaul/GCC build the next word happened to be the compiler's RTS/NOP.
The callback now declares that word as part of the payload, preserving all
48 bytes and both existing guards. The abandoned 44-byte variant changed
the RAM output.

The original [Vandal Hearts addition](https://github.com/tpunix/SAROO/commit/f6a35a4)
credits yzb. This port adapts paps-g `a1f65f2a` (MENU-GAME-VANDAL-STUB-001),
the successor to `55ed020f`. See the
[Hitachi SH7604 instruction tables](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=48),
tables 2.12 and 2.16, printed pp. 33 and 39.

Build the parent and current stock menus with
[`firmware-build menu`](../tools/README.md#isolated-firmware-qualification).
Run `python3 -B tests/check_vandal_stub_sh2.py CURRENT_ELF --baseline PARENT_ELF`.
The parent fails the declared-payload check. The corrected ELF declares all
48 bytes, with identical linked guards, copy instructions and historical RAM
payload. This verifies the compiled change. Behavior in the exact modified
game revisions remains unverified.
