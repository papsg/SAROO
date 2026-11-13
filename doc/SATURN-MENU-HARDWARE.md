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
