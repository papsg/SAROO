# Menu startup, GIF display, and CD handoff

## Validate the startup language ID

Adapted from paps-g `1ad9fb40` (SATURN-MENU-010). The SH-2 menu
reads the 32-bit language ID from startup metadata. It checks both
ends of the signed range before indexing the language table. Values
outside the table select the existing default, ID 0. The 19 valid
selections and language cycling retain their behavior.

The upstream report records invalid startup metadata. Its firmware
predates the language table indexing in this fix, so the original
report's cause remains open.
[Upstream issue #9](https://github.com/tpunix/SAROO/issues/9),
[v0.8 menu language source](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/language.c).

```sh
make -C tests check-language
make -C tests check-language BUILD_DIR=build/language-release \
  CFLAGS='-std=c99 -O2 -g -Wall -Wextra -Werror' SANITIZERS=
```

Run this test against the parent `Firm_Saturn/language.c` to reproduce
the out-of-bounds access for `INT_MIN`. The parent's 19-language
lookup and cycling controls pass. The port passes 65,540 signed ID
cases and all 2,280 existing lookups in both profiles. The linked
menu contains 81,380 code/data bytes and 24,064 bytes of body space.
Malformed startup metadata has been exercised on the host.

## Skip row DMA for a fully clipped GIF

Adapted from paps-g `a3a14724` (SATURN-MENU-011). Horizontal clipping
can reduce the row width to zero or below while the frame still has
visible rows. The menu only forms the row destination and starts DMA
for a positive width and height. Empty frames still release their
frame and palette flags. The existing final channel-2 completion
check preserves ownership for visible rows.
[Sega SCU User's Manual ST-097-R5-072694, DMA transfer byte count,
Figure 3.4, printed p. 42](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=58),
[upstream artwork discussion #217](https://github.com/tpunix/SAROO/issues/217).

```sh
make -C tests check-cover-display
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-cover-display
```

Set `SAROO_COVER_MENU_SOURCE` to the parent `Firm_Saturn/main.c` and
run `test_cover_display.py`. The parent submits four zero or negative
counts on the left and right edges and passes 13 controls. The
port passes all 17 display scenarios, including top and bottom
clipping, exact visible pixels, palette publication and the next
frame after an invisible one. All nine cover/display methods pass
under sanitizers and O2. The pinned menu links 81,396 code/data
bytes with 24,048 bytes of body space. Physical SCU/VDP behavior
remains unqualified.

## Retire original-drive commands in the emulated registers

Adapted from paps-g `4e024188` (MCU-CDC-034). The FPGA observes the
Saturn's HIRQ clear and CR4 write while the physical drive owns the
interface. The MCU command worker acknowledges the request. With
emulation disabled, it now sets only CMOK in the emulated register
through the FPGA's set-mask port and returns. Other pending events,
command responses and asynchronous reader ownership are preserved.
This prepares the register state for the later menu admission gate.
[Sega CD Communication Interface ST-162-062094, §3.1 HIRQREQ,
printed p. 28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=16),
[v0.8 FPGA register logic](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v).

```sh
python3 -B -m unittest discover -s tests -p test_cdc_drive.py -v
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
```

Set `SAROO_CDC_DRIVE_DIR` to the parent `Firm_MCU/Saturn` directory
to reproduce both failures. The parent passes 44 methods. The
port covers 1,024 disabled-mode opcode/control cases with the
production dispatcher, then uses the actual MCU mask with the
stock FPGA decode, HIRQ, request and enable logic under Icarus
Verilog. Takeover restores CMOK and preserves the unrelated
events in that trace. Both CD profiles pass 207 methods. The
GNU MCU App links 64,460 load bytes with 1,076 bytes of ITCM
space. Physical bus timing and cold-start game behavior remain
unqualified.

## Protect an unanswered menu CD command

Adapted from paps-g `b782c9b7` (SATURN-CD-003). The menu checks
CMOK before clearing events or writing any command word. If CMOK
is clear, it returns the existing local error value `-2` for
non-admission. A response timeout still returns `-3`. The old
command owns the registers until it publishes its reply. A later
caller can try again once CMOK returns. Original-drive command
retirement is the prerequisite for takeover to present a ready bit.
[Sega CD Communication Interface ST-162-062094, §3.1 HIRQREQ,
printed p. 28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=16),
[§7.2.6 command return codes, printed p. 63](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=51).

```sh
make -C tests check-menu-safety
SAROO_MENU_CD_PROFILE=release make -C tests check-menu-safety
```

Point `SAROO_MENU_CD_SOURCE` at the parent `Firm_Saturn` directory
and run `test_menu_cd_transfer.py` to reproduce both admission
failures. The other 16 methods pass. The port checks 65,536 busy
states across two wait masks. It also checks a timed-out command,
an attempted retry, its late reply and the distinct next response.
The broader menu checks cover modeled physical/virtual startup,
player, boot and recovery paths. The group passes 90 methods in
both runs, with the CD methods exercised under sanitizers and O2.
The pinned menu links 81,396 code/data bytes with 24,048 bytes of
body space. Physical CD and game behavior remain unqualified.
