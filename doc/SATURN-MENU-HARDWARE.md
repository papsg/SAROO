# Saturn menu hardware correctness

## Correct level-1 SCU DMA status

SATURN-SCU-001, adapted from paps-g `4b80469e`. The stock menu helper
derived every level's DMA status mask by shifting the level-0 fields.
Level 1 then waited on reserved bit 9 and missed D1WT at bit 19. A
queued level-1 transfer could be mistaken for an idle channel before
the helper rewrote its registers. Public menu callers currently use
levels 0 and 2, making this a latent helper defect.

Use the individual operation, wait and interruption masks defined in
[Sega SCU User's Manual ST-097-R5-072694, DMA Status Register, Figure 3.13,
printed pp. 48–49](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=64).
Level 0 remains `0x00010030`, level 1 uses `0x000a0100`, and level 2 remains
`0x00003000`. Transfer setup and callers are unchanged.

```sh
python3 -B -m unittest discover -s tests -p test_menu_scu_dma.py -v
SAROO_MENU_SCU_DMA_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_scu_dma.py -v
```

The fixture extracts the active helper from the stock source. The parent
fails the reserved-bit and level-1 wait cases. The other 28 register scenarios
pass. Both ASan/UBSan and optimized builds pass all 30 on the port, and the
adjacent VDP1, word-access and palette fixtures pass 13 methods. The complete
[Yaul/GCC menu build](ARABIC.md#build-qualification) retains 84,164 bytes of
code/data and a 488,770-byte image. Linked disassembly contains the corrected
status mask, and all 48 relocated ROM clock checks pass. Physical SCU
arbitration and any future menu use of level 1 remain unqualified.

## Bound the outer waits for physical-disc authentication

SATURN-CD-003 continuation, adapted from paps-g `25e04166`. Each outer
polling phase gets ten seconds from its first nonterminal report. If the
drive is still pending at the deadline, the menu returns the existing
timeout error. Expiry before authentication leaves sector size alone.
Expiry after it attempts to restore the normal sector size and preserves
the timeout even when restoration also fails. A changing nonterminal
status consumes the same phase budget.

[Sega CD Communication Interface ST-162-062094, §4.1, printed pp. 33–35](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21)
distinguishes ongoing drive states from terminal states. The ten-second
window is a firmware policy measured on returning polls. The existing
relocated operation, command sequence and terminal handling are retained.

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
```

Set `SAROO_MENU_CD_SOURCE` to the parent `Firm_Saturn` directory to reproduce
six stall failures. Its seven ordinary query/authentication methods pass.
The port passes all 64 CD methods in both profiles and three launch-error
controls. Cases cover counter wrap, failed size restoration, and two valid
slow phases with independent budgets. The SH-2 build links 84,292 code/data
bytes. The 504-byte relocated routine has identical object bytes and only
its resolved print and string addresses change at link time. Physical drive
timing, recovery and waits inside that routine remain unqualified.

## Initialize the VDP1 erase rectangle

SATURN-VDP1-001, adapted from paps-g `bfc3463f`. Menu startup selected
16-bit framebuffer mode and set the upper-left erase coordinate, leaving
the lower-right coordinate at its prior value. The write-only register
is undefined after power-on or reset. Setting EWRR to `0x0200` gives the
smallest valid rectangle for the existing origin and mode, eight pixels
on line zero. The menu still draws with VDP2, and a game that initializes
VDP1 supplies its own values.

[Sega VDP1 User's Manual ST-013-R3-061694, register initialization, printed p. 23](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf#page=38)
and [erase/write registers, printed pp. 46–49](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf#page=61)
define the write-only reset state, field encodings and coordinate constraints.

```sh
python3 -B -m unittest discover -s tests -p test_menu_palette.py -v
SAROO_MENU_SAFETY_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_palette.py -v
```

The parent fails the seeded EWRR case and passes all four existing palette
controls. Both profiles pass five methods on the port. The new case reseeds
three prior values across cold and logo startup. Seven VDP1 game-handoff
controls also pass. The SH-2 disassembly writes the 16-bit value to
`0x25d0000a`. The complete menu adds 16 code/data bytes to reach 84,308.
Physical erase behavior and software that inherits the register state
remain unqualified.

## Schedule VDP2 VRAM cycles for bitmap and CPU access

SATURN-VDP2-001, adapted from paps-g `50d580c8`. The two menu bitmaps
occupy the partitioned A0 and A1 banks. Each bank now schedules two
bitmap reads, one idle slot and five consecutive CPU slots. The CPU
slots align across the partitions. All slots in unused B0 and B1 select
No Access. The old CPU slots followed display fetches without the
required idle lead-in, and unused B-bank cycle registers retained prior
values.

[Sega VDP2 User's Manual ST-058-R2-060194, Table 3.3, printed p. 34](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=52)
gives the bitmap read counts. [The partitioned CPU access rule and Figure 3.7,
printed pp. 36–37](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=54)
define the shared idle lead-in. [Table 3.5, printed p. 40](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=58)
defines the cycle encodings.

The [palette fixture](#initialize-the-vdp1-erase-rectangle) decodes every
slot from actual initialization after cold and logo startup. The parent
fails the schedule case and passes the five other methods. Both profiles
pass all six on the port, and all nine cover display/hash methods pass.
The complete menu uses 84,324 code/data bytes, up 16, with the same
488,770-byte image and 9,488-byte static BSS. Linked disassembly contains
the A-bank values and all eight word writes. Physical arbitration and
visible redraw behavior remain unqualified.

## Enable the NTSC menu display during measured blanking

SSR-002, adapted from paps-g `370d5cbd` and the qualified counter helper
from `4d51eeae`. This supersedes the flag-only admission from `8cf9992e`.
The parent enables immediately after initializing video memory. Sega requires
the display-off to display-on transition during vertical blanking.
`TVSTAT.VBLANK` reads as set throughout display-off, so the flag alone cannot
identify that window.

See [Sega VDP2 User's Manual ST-058-R2-060194, §2.4, printed p. 16](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=34),
[§2.5, printed p. 19](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=37),
and [Technical Bulletin 37, items 3 and 6, PDF pp. 116–117](https://antime.kapsi.fi/sega/files/Sattechs.pdf#page=116).

On NTSC the menu prepares the 240-line off mode and selects the local latch.
The retained helper observes a new field and adjacent counter progression.
It masks interrupts, checks the current mode, latch ownership and fresh line,
then enables at counter line 242 or 243 and restores the full status register.
The attempt has a finite sample budget. The menu idle service can retry when
the counter resumes. Placing the retry there confines it to the menu and
binary browser. PAL retains the public baseline startup and latch ownership
while PAL phase qualification remains open.

Both [palette test profiles](#initialize-the-vdp1-erase-rectangle) pass all
ten methods. The parent fails three NTSC cases and passes seven controls,
including PAL legacy startup. The NTSC model covers every one of 263 starting
lines, skipped and stationary counters, foreign ownership and a phase change
before the locked write. Eleven peripheral and nine cover controls pass.
The linked menu uses 84,628 code/data bytes, up 304, with 20,568 bytes of
body margin. The target disassembly verifies the mask, fresh counter read,
word write and status-register restore in the two menu call paths.

Historical NTSC RAM captures yielded 660 samples and observed the 240-line
enable at line 242. They support the retained counter algorithm. The public
integration has host and linked-image validation. Physical validation of
this public build and PAL qualification remain open.

## Admit physical-drive power commands at a safe scan phase

SATURN-SMPC-001, adapted from the CDON/CDOFF portion of paps-g
`fa537859`. Public main already had timed controller admission and
[scanline-checked reset enable](CD-POWER-AND-TIMING.md#preserve-reset-enable-after-bios-clock-changes).
The physical-drive handoff still checked only the SMPC busy flag. It could
issue while SMPC was using the opening 300 microseconds of VBlank.

[Sega SMPC User's Manual ST-169-R1-072694, §2.2, printed pp. 17–19](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=27)
defines the command window. [CDON and CDOFF, printed pp. 27–28](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=37)
specify the power commands. [VDP2 §2.5, printed p. 19](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=37)
and [Technical Bulletin 37, items 3 and 6](https://antime.kapsi.fi/sega/files/Sattechs.pdf#page=116)
cover local counter latching and the flag's behavior while output is off.

With output on, require the end of blanking following a sample less than
one millisecond old from the same display mode. With output off, use
a fresh local latch on TV noninterlaced modes and accept lines 0–191.
External sync or latch ownership leaves the off-display command pending
within its existing deadline. After briefly masking interrupts, recheck
the phase, mode, time budget and SMPC idle state before changing routing
or writing the command. Restore the caller's full status register before
waiting for completion. The one-second shared budget and confirmation
before returning ownership to SAROO still apply.

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
```

The parent fails four timing methods and passes the 11 existing power
and caller methods. Both corrected profiles pass all 68 CD methods.
The new cases cover visible and disabled output, NTSC and PAL models,
timer wrap, stale phase, mode changes and expiry before changing routing.
Eight clock/launch controls and 48 linked ROM clock checks pass. The
complete menu links 84,900 code/data bytes, with 20,296 bytes of body
margin. Physical SMPC timing and real-disc behavior remain unqualified.

## Complete reset disable before handing off to the BIOS player

SATURN-SMPC-002 and SATURN-SMPC-004, adapted from paps-g `f822fc1e`
and `33014804`. The player handoff wrote RESDISA directly after the
BIOS clock call, then replaced high work RAM before confirming the
command. The public port uses the existing bounded reset-command
sequence. It waits for idle SF and a locally latched line from 0 through
191, sets SF before writing COMREG, waits for SF to clear, and checks
OREG31 for the completed command before changing high work RAM. The
scanline check works while output is disabled. An unavailable latch,
busy timeout, or unexpected result stops the handoff before replacement.

[Sega SMPC User's Manual ST-169-R1-072694, §2.2 and Figure 2.2, printed pp. 13–14](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=23)
defines Type-B issue and completion. [Command timing, printed pp. 17–19](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=27),
[OREG31, printed p. 21](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=31)
and [RESDISA, printed p. 34](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=44)
define the window, result code and reset-button NMI behavior.
[VDP2 §2.5, printed p. 19](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=37)
and [Technical Bulletin 37, items 3 and 6, PDF pp. 116–117](https://antime.kapsi.fi/sega/files/Sattechs.pdf#page=116)
cover the latch and counter available while output is disabled.

```sh
python3 -B -m unittest discover -s tests -p test_menu_clock_reset.py -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_clock_reset.py -v
```

The parent fails both player methods at command admission and passes
all five clock and installer controls. Both port profiles pass all seven
methods. The new failure controls cover unavailable latch, incomplete
command and mismatched OREG31. All 38 adjacent player, power, boot and
VDP1 methods pass. The SH-2 menu has 84,980 code/data bytes and 20,216
bytes of body margin. Both relocated clock fragments remain byte-identical
and pass all 48 linked checks. The historical private Astal trial reached
the BIOS player after two reset combinations. Hardware behavior of this
public build and timeout recovery after the BIOS clock change remain
unqualified.

## Stop the active VDP1 draw before replacing command RAM

SATURN-VDP1-003, adapted from paps-g `99806a35`. A change to PTMR
prevents the next automatic draw start. A draw already in flight can
continue reading the old command table. Menu initialization now writes
zero to ENDR, performs 64 serialized reads of EDSR, then replaces the
first command with draw-end. The fixed read count allows the documented
termination interval to elapse without waiting for a particular
completion flag.

[Sega VDP1 User's Manual ST-013-R3-061694, §4.3, printed p. 45](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf#page=60)
defines when the trigger change takes effect. [§4.5, printed p. 51](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf#page=66)
specifies zero for ENDR and approximately 30 VDP1 clocks for termination.

The [palette fixture](#initialize-the-vdp1-erase-rectangle) checks the
actual register and RAM access order across four starting EDSR values,
with cold and logo paths. The parent reaches the command RAM write
without terminating the draw and passes the other ten methods. Both
port profiles pass all eleven methods. Ten adjacent VDP1 handoff and
SCU DMA methods and 48 linked ROM clock cases pass. The SH-2 menu adds
32 code/data bytes, reaching 85,012. The linked sequence contains the
word store, 64-read loop and command replacement. Physical settling
and output behavior remain unqualified.

## Initialize both VDP2 menu layer transforms

SATURN-VDP2-003, adapted from paps-g `db0f31aa`. The menu previously
set only NBG0's scroll origin. It now sets the horizontal and vertical
origins of both NBG0 and NBG1 to zero and all four coordinate increments
to 1.0. Reset or inherited values could otherwise leave the layers
shifted, zoomed, or sampling a single source coordinate. The menu also
disables reduction and line/vertical-cell scrolling.

[Sega VDP2 User's Manual ST-058-R2-060194, §1 register access, printed p. 4](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=22)
permits word and longword access. [§5.1, printed pp. 122–125](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=140),
[§5.2, printed pp. 126–128](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=144)
and [§5.3, printed pp. 136–137](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=154)
define the scroll, coordinate increment and line/cell-scroll registers.

The parent fails the new seeded coordinate check and passes the other
eleven [palette methods](#initialize-the-vdp1-erase-rectangle). Both port
profiles pass all twelve, including cold and logo paths for both modeled
regions, with nine adjacent cover methods and 48 linked ROM clock checks.
The linked menu has 85,028 code/data bytes, up 16. Disassembly shows all
eight big-endian coordinate longword stores. Physical rendering of the
public build remains unqualified.

## Set VDP2 menu compositing defaults

SATURN-VDP2-004, adapted from paps-g `b0177efe`. The menu now initializes
mosaic, bitmap palette selection, windows, sprite and shadow interpretation,
line-color insertion, special priority, blending, and color-offset controls
while the display is off. It sets sprite priorities to zero before displaying
the menu. Inherited values could otherwise tint, clip, blend, or distort the
menu. The BIOS logo transition runs first, and the cover path still explicitly
sets its own window and blend ratio.

[Sega VDP2 User's Manual ST-058-R2-060194, bitmap palette and mosaic, printed pp. 110–119](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=128),
[windows, printed pp. 179–193](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=197),
[sprite control, printed p. 207](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=225),
and [priority, color calculation, color offsets and shadow, printed pp. 227–259](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=245)
define these controls. Writes stay within the selected registers and skip the
reserved word after the rotation priority register.

The parent fails the seeded compositing test and passes the other twelve
[palette methods](#initialize-the-vdp1-erase-rectangle). Both port profiles
pass all thirteen. The new test checks the affected registers, neighboring
canaries and intentional cover clipping and blend controls. All 20 adjacent
cover and peripheral methods and 48 linked ROM clock cases pass. The full
menu has 85,076 code/data bytes, up 48. Host checks model register values and
do not establish physical image quality.

## Initialize the stereo CD audio mixer with word writes

SATURN-SCSP-001 and SATURN-SCSP-002, adapted from paps-g `268ce1cc`
and `eb4826d3`. Menu startup wrote byte values to the SCSP external CD
audio inputs, which require aligned 16-bit writes from the main CPU.
The two complete words use send level 6 (−6 dB) and pan the inputs
fully left and right. The port uses the corrected left word `0x00df`
and right word `0x00cf`.

[Sega SCSP User's Manual ST-077-R2-052594, main CPU access, printed p. 19](https://antime.kapsi.fi/sega/files/ST-077-R2-052594.pdf#page=31)
specifies 16-bit access. [Figure 4.2, printed p. 25](https://antime.kapsi.fi/sega/files/ST-077-R2-052594.pdf#page=37)
and [Tables 4.29–4.31, printed pp. 87–88](https://antime.kapsi.fi/sega/files/ST-077-R2-052594.pdf#page=99)
define the register fields, levels, pan and external-input locations.
[Sega Technical Bulletin STN-29, CD-DA Playback Precautions](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/info/hon/stn29.htm)
specifies the stereo routing. Its left hexadecimal example disagrees
with the binary fields it describes. The port follows the manual
and the bulletin's field descriptions.

```sh
python3 -B -m unittest discover -s tests -p test_menu_scsp.py -v
SAROO_MENU_SAFETY_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_scsp.py -v
```

The parent fails width and field checks and passes the master volume and
neighbor control. An aligned-write control using `0x00fd` fails only the
stereo test. Both profiles pass all three methods on the port. Another
55 audio and startup methods and 48 linked ROM clock checks pass. The
linked menu stays at 85,076 code/data bytes and emits aligned word stores
with the corrected values. Physical CD audio balance and level on the
public build remain unqualified.

## Purge the SH-2 cache during BIOS player handoff

SATURN-CACHE-001, adapted from paps-g `aece1be2`. The old CD player
handoff wrote the purge and enable values through ordinary pointers.
The optimized SH-2 build emitted only the final enable. The handoff
now issues three volatile byte writes to CCR in order. It disables
the cache, purges tags and replacement state while the cache is off,
then enables the normal four-way cache before BIOS preparation.

[Hitachi SH7604 Hardware Manual, §8.4.6, printed p. 224](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=240),
[§8.5.1, printed p. 226](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=242)
and [§8.5.5, printed p. 229](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=245)
describe the control sequence and instruction-fetch considerations.

The parent fails the new order case and passes seven existing
[clock and player methods](#complete-reset-disable-before-handing-off-to-the-bios-player).
Both port profiles pass all eight, and all eight file-cache methods pass.
The linked function retains writes of 0, 16 and 1 at CCR, while the parent
contains only 1. All 48 linked ROM clock checks pass. The complete menu
remains 488,770 bytes with 85,076 code/data bytes and 20,120 bytes of body
margin. Physical cache and BIOS player behavior remain unqualified.

## Stop SH-2 DMA before replacing the channel state

SATURN-DMAC-001 and SATURN-DMAC-002, adapted from paps-g `33b51a8c`
and `59239df4`. The CD-player handoff now reads DMAOR before clearing
it, disabling both channels before reconfiguration. It reads both
CHCR registers before clearing their latched transfer-end flags.
Only then does it reset the request sources, addresses and counts.
The request registers are byte accesses at `0xfffffe71` and
`0xfffffe72`. The post-clock DMAOR clear also follows a read.

[Hitachi SH7604 Hardware Manual, §9.2.4, printed pp. 237–241](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=253),
[§§9.2.6–9.2.7, printed pp. 242–244](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=258)
and [§9.5 items 1–2, printed p. 285](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=301)
define the sticky flags, access widths and stop-before-rewrite rule.
The public port retains the surrounding UBC and divider setup.

The actual-source player fixture checks all 16 DMA accesses, including
register widths and both sides of the BIOS clock call. The parent fails
the order check. A control with the inherited DRCR definitions fails
at the request addresses. Both nine-method profiles and 15 adjacent
boot, launch and VDP1 controls pass. The SH-2 build grows by 16 code/data
bytes to 85,092 and all 48 linked ROM clock checks pass. Private reset
observations informed the address correction. The public build has
host and link verification.

## Restore cartridge timing from WRAM after a clock change

SATURN-SCU-003 and SATURN-SCU-005, adapted from paps-g `843c51bf` and
`d604fb88`. Menu startup and all BIOS clock paths complete an uncached
CS1 read before writing ASR0. That address returns open cartridge data
and is outside the cartridge ID window. The ordinary game tail runs
the read and timing restore in Saturn WRAM before fetching the
cartridge callback. The callback restores ASR0 again after BIOS SCU
initialization. The ROM-game path keeps its 96-byte entry and 36-byte
reset helper, with the existing bounded reset-enable sequence.

[Sega Saturn SCU Final Specifications ST-210-110194, precautions
09 and 11, printed p. 5](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf#page=9)
specify the ASR reserved bits and the completed-read requirement.
[Sega SMPC User's Manual ST-169-R1-072694, CKCHG352/320,
printed pp. 30–31](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=40)
specifies SCU reset and WRAM retention.
[Sega SCU User's Manual ST-097-R5-072694, §3.6, printed
pp. 62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=78)
defines the ASR0 timing fields and reset value.

```sh
python3 -B -m unittest discover -s tests -p test_menu_clock_reset.py -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_clock_reset.py -v
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
python3 -B tests/check_menu_clock_sh2.py Firm_Saturn/ssfirm.elf
```

The parent fails the new access-order case and both linked WRAM
checks. The port passes both ten-method clock profiles, all 15
adjacent VDP1 and CD-player methods, the linked ordinary tail and 48
relocated ROM clock cases. Omitting the tail read, ASR0 store or
return-register restore is rejected. The installed tail and callback
occupy 32 bytes through `0x060006d7` and leave the BIOS literal at
`0x060006dc` intact. The linked CD-player fragment occupies 40 of its
64 copied bytes. The image remains 488,770 bytes, with 85,132 code/data
bytes and 20,064 bytes of body margin. A prior development RAM trial
reached the game in *6 Inch My Darling* after the WRAM correction.
Physical timing and game behavior for this public build remain unqualified.

## Finish scrolling-selection DMA before buffer reuse

SATURN-SCU-004, adapted from paps-g `591f7c2b`. The scrolling selection
uses a scratch bitmap that the next redraw can overwrite and the menu exit
can clear. Each SCU DMA call waited for the previous row, so the last of
the sixteen rows could still own the scratch bitmap when the upload
returned. The menu now sends fifteen rows by level-0 DMA, waits for the
fifteenth to retire, and copies the last 304 bytes using the SH-2. The
copy uses the menu's existing CPU access slots in VDP2 bitmap VRAM.

[Sega SCU User's Manual ST-097-R5-072694, §3.4, Fig. 3.13, printed pp. 48–49](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=64)
defines the level-0 operation, wait and interrupted bits at 4, 5 and 16.
[Sega SCU Final Specifications ST-210-110194, precaution 23, printed p. 7](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf#page=11)
prohibits rewriting the setup registers of an active level.

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_*dma.py' -v
SAROO_MENU_SCU_DMA_PROFILE=release python3 -B -m unittest discover -s tests -p 'test_menu_*dma.py' -v
```

Set `SAROO_MENU_SELECTION_SOURCE` to the preceding checkout's
`Firm_Saturn` directory and select `-k waiting` with
`test_menu_selection_dma.py` to reproduce the ownership failure.
The fixture checks all three pending states, unrelated channel activity,
immediate reuse of the scratch bitmap and preservation of pixels outside
the selected row group. The SH-2 link places the status read at the
cache-through address `0x25fe007c`. The linked body grows by 32 bytes to
85,164, with 9,488 static bytes, 16,384 stack bytes and 20,032 bytes of
body margin. The complete image remains 488,770 bytes. Physical VBlank
copy timing and display behavior remain unqualified.

## Restore the cover window within the viewport

Adapted from paps-g `14b12636`. A large cover is clipped when its pixels
are drawn. Hiding the cover layer and showing it again used the original
width and height to restore the VDP2 window, so its end could extend beyond
the pixels drawn and the horizontal end could exceed the register field.
The restored inclusive end now stays within the same 320×240 viewport.
Cover placement, layer priority, transparency and pixel transfer are
unchanged.

[Sega VDP2 User's Manual ST-058-R2-060194 §8.1, printed pp. 180–181](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=198)
defines inclusive rectangular windows and their position registers.
[§8.2, printed p. 190](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=208)
defines the transparency operation.

Run `make -C tests check-cover-display check-gif check-menu-safety check-menu-binary`.
To reproduce, set `SAROO_COVER_MENU_SOURCE` to the parent's `Firm_Saturn/main.c`
and select `-k cover_layer_toggle` in `test_cover_display.py`. Four of
nine cases fail on the old window end; all nine now restore the actual
drawn window or keep the window disabled for non-cover images. The test
also checks the layer controls and that toggling adds no pixel transfers.
All 157 focused methods and all 10 optimized cover methods pass.
The linked stock menu passes the ordinary clock tail and 48 relocated ROM
clock cases. The menu remains 488,770 bytes, with 85,212 code/data bytes
(48 more), 19,988 bytes of body margin, and unchanged BSS and stack.
Physical display behavior remains unqualified.

## Release the BIOS player callback before restarting the menu

SATURN-MENU-013, adapted from paps-g `7f626020`. The BIOS CD player
installs a wrapper in the BIOS interrupt-mask slot. Pressing Start used
to enter the SAROO menu with that wrapper still installed. Startup clears
BSS, including the saved target, before controller initialization uses
the slot. The player now masks its SCU interrupts through the saved BIOS
function and restores that function before entering the menu.

[Sega System Program User's Manual ST-162-R1-092994, printed p. 2](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=7)
describes the BIOS-owned mask copy.
[Function 2.2, printed p. 10](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=15)
defines `SYS_CHGSCUIM` and the two mask arguments.

Run `make -C tests check-menu-safety check-peripherals`. To reproduce,
set `SAROO_MENU_CD_SOURCE` to the parent's `Firm_Saturn` directory and
select `-k start_retires` in `test_menu_cdplayer.py`. The parent fails
after modeled BSS clearing. Restoring the slot without masking the old
sources fails a separate assertion. The port passes Start from either
pad and keeps the A, C and idle paths working. All 143 focused methods
and all 10 optimized player methods pass. The stock menu adds 32
code/data bytes (85,244 total, 19,956 bytes of body margin) and passes
the linked clock tail and 48 ROM clock cases.

A historical private RAM trial reached SAROO after the Astal reset
sequence and BIOS player return. This public menu build has host and
link verification.

## Align the menu's initial SH-2 stack

Startup loads R15 from the end of the linker-reserved stack. With a
byte-aligned or halfword-aligned BSS tail, the inherited linker could place
that pointer at an address that faults on the first longword stack write.
The stock linker now aligns the stack to four bytes and checks both
boundaries, the 16 KiB reservation, and the end of the 128 KiB menu body.
The stock menu already linked at aligned addresses in the verified build.
Its ELF and image bytes are unchanged.

Adapted from paps-g `75919b29`. See [Hitachi SH7604 §2.2.2 and §4.8.1,
printed pp. 18 and 76](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=33).
Run `sh tests/check_menu_stack_sh2.sh` from the repository root with the SH-2
binutils on PATH. Set `SH2_PREFIX` for a different toolchain prefix. The check
links four BSS tails against the actual script and reports a misaligned R15
with the parent script. Removing the alignment while retaining the assertions
causes the linker itself to reject the same case. These are link checks.
