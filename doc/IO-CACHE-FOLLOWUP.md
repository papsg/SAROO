# Firmware I/O and cache follow-up

## Share CUE parser checks

The parser checks the byte after an indexed sector with the existing
offset helper, then subtracts one sector to recover its start. The same
bound covers partial sectors, exact EOF, and the 32-bit file window.
The minute limit keeps the endpoint addition in range. The seven accepted
track formats use the same sector sizes and file offsets. The shared
`MODE` prefix check also rejects short or malformed names before reading
past the token. [Existing CUE bounds and attribution](DISC-CUE-SAFETY.md#mcu-cue-001-bounded-cueraw-loading-and-reader-cleanup).

Adapted from paps-g's `8ef2ac7f`. The public GNU App load decreases by
8 bytes to 62,932. Static DTCM stays at 60,232 bytes. The [GNU App link
procedure](../tools/README.md#original-mcu-app-compilelink-check) uses the
same Arm GNU 14.3.Rel1 compiler and settings as the baseline.

The baseline and port pass all 37 CUE, header and sector reader methods.
The new controls cover 42 short, exact and overfull sector cases and 25
invalid mode strings. Run `python3 -m unittest discover -s tests -p
test_mcu_save_cue.py -v` and the CUE geometry, layout, subcode and
header fixtures. Repeat with `SAROO_FIRMWARE_SAVE_PROFILE=release` for
O2. Set `SAROO_MCU_SAVE_DIR` to the older checkout's
`Firm_MCU/Saturn` directory to rerun the baseline. These are host
parser and firmware link checks.

## Invalidate direct file-read cache lines

The MCU writes cartridge RAM directly. The calling SH-2 can keep old data
or instructions for those addresses in its write-through cache. The menu
now purges the returned range after a successful direct read. On a read
error or an invalid reply count it purges the bounded request because the
MCU may already have changed part of it. Both cartridge address aliases
purge the same physical lines. The existing file APIs and bounce copies
retain their behavior. [Hitachi SH7604 Hardware Manual, §§8.4.6–8.4.7,
printed p. 224](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=240) ·
[§§8.5.2–8.5.5, printed pp. 227–229](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=243).

Reads covering at most 256 lines use aligned associative purges. Larger
reads disable the cache before purging and restore its control and the
CPU's interrupt state afterward. Compiler barriers keep the shared request
and data accesses on the intended sides of the command boundary.

Adapted from paps-g's `8078e501`. The baseline fails 48 stale-line cases.
The port passes all 415 cache cases plus the coupled FatFs and BIN checks.
Run `make -C tests check-file-transfer check-menu-binary`. Set
`SAROO_FILE_PROFILE=release` for the O2 transfer checks. Set
`SAROO_MENU_CACHE_SOURCE` to the preceding `Firm_Saturn/main.c` to
repeat the baseline completion controls. The menu links 79,668 code/data
bytes (+80) with the same BSS and 488,770-byte image size. The host model
covers the calling CPU. Physical instruction fetch, interrupt timing,
and ownership on the other SH-2 remain unqualified.

## Share complete CD command replies

Seventeen handlers share the four consecutive CR1–CR4 stores. Each
store is still a volatile halfword and the register order is unchanged.
The internal argument combines CR3 and CR4 to keep the helper's register
use small. The handlers retain their existing admission, interrupt and
transfer paths. [Sega CD Communication Interface User's Manual,
ST-162, §8.2.5, printed pp. 87–90](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=75) ·
[Preceding public handlers](https://github.com/papsg/SAROO/blob/07424e17c16f5b7bf9cd33d119491015ac2fd0b9/Firm_MCU/Saturn/saturn_cdc.c).

Adapted from the CD reply portion of paps-g's `48f6a105`. The GNU App
load decreases by 64 bytes to 62,868, with static DTCM unchanged.

Run `make -C tests check-cdc-command-buffer` and repeat with
`SAROO_CDC_PROFILE=release`. All 157 methods pass in both profiles.
The baseline and port pass 1,179,648 ordered replies across 18 layouts.
The fixture rejects a control with the two tail writes reversed. Set
`SAROO_CDC_REPLY_SOURCE` to select a baseline or control source for
`test_cdc_replies.py`. Physical command timing remains unmeasured.

## Clear FPGA memory acknowledgments on reset

Reset can interrupt the arbiter after it sets an acknowledgment and before
its normal completion state clears that bit. A new write on the same port
can consume the old acknowledgment before its own SDRAM command issues.
Reset now clears both existing acknowledgment bits. Arbitration and byte
masks retain their existing behavior. [Original FPGA memory
arbiter](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/memhub.v#L126-L169) ·
[Original write consumer](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/cachebus.v#L116-L124).

Adapted from paps-g's `e345a327`. Run
`python3 -m unittest discover -s tests -p test_fpga_memory.py -v` with
Icarus Verilog. Set `SAROO_FPGA_MEMORY_BASELINE` to the preceding
checkout's `FPGA` directory to use the exact baseline. The actual memory
modules reproduce premature completion on both old ports and pass six
other old reset cases. The corrected source passes all 32 reset phase,
port and byte mask combinations. Removing either reset assignment
reproduces the failure on that port while the other port still passes.

The [original Quartus project comparison](../tools/README.md#original-fpga-project-comparison)
binds all 18 FPGA inputs and changes only `memhub.v`. Quartus 25.1 Lite
maps, fits, assembles, checks constrained timing and converts the result.
The fit uses 1,479 logic elements (+15), 816 registers and 32,768 memory
bits. Minimum reported setup slack is 0.361 ns versus 0.843 ns before the
change. Minimum hold slack is 0.179 ns versus 0.167 ns. MCU and menu
sources are unaffected. External bus timing and physical reset behavior
remain unqualified.

## Share filter condition resets and sector lengths

Software initialization, Set Filter Mode, and Reset Selector share the
nine condition-field clears. Each caller still owns connection updates,
pause and interrupt state. The four fixed sector lengths fit in read-only
16-bit entries. The get and put selectors retain their existing behavior,
including unchanged settings for out-of-range bytes. [Sega ST-162,
§8.2.5 functions 5.5 and 5.9, printed pp. 89 and 91](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=77) ·
[§8.2.7 function 7.1, printed p. 95](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=83).

Adapted from paps-g's `bc1f69cc` after the shared filter lookup and reply
ports. The public GNU App load decreases by 72 bytes to 62,796. Static
DTCM decreases by 16 bytes to 60,216.

The exact preceding source and the port pass the 131,072 filter
mode/selector calls and 65,536 get/put sector-length pairs. These checks
compare every filter byte, including adjacent fields and padding. All
159 CD command and buffer methods pass under ASan/UBSan and O2. Run
`make -C tests check-cdc-command-buffer` and repeat with
`SAROO_CDC_PROFILE=release`. For the parent control, set
`SAROO_CDC_CONNECTION_SOURCE` for `test_cdc_connections.py`.
Physical command timing remains unmeasured.
