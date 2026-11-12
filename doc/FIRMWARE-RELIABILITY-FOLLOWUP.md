# Firmware reliability follow-up

## Shared CD command filter lookup

The range, subheader and mode handlers and the connection query reuse one
bounds check and obtain the selected filter once. Each command retains its
own status, reply and interrupt result. Connection setters still validate
their separate endpoint selectors, including disconnected outputs. The
condition reset preserves existing connections. [Sega CD Communication
Interface User's Manual, ST-162, §8.2.5, functions 5.1–5.8, printed pp.
87–90](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=75).

Adapted from the CD portion of paps-g's SAROO maintenance checkpoint
`31326563`. This is a code size change. The GNU App load decreases from
62,620 to 62,540 bytes and static DTCM stays at 60,184 bytes.

Run `make -C tests check-cdc-command-buffer` and repeat with
`SAROO_CDC_PROFILE=release`. The baseline and port pass all 156 host
methods. The controls cover all 24 filters, invalid selector bytes,
connection and reset semantics, and complete subheader replies. The
standard MCU link uses the [GNU App procedure](../tools/README.md#original-mcu-app-compilelink-check)
and Arm GNU 14.3.Rel1. Command timing on a Saturn remains unmeasured.

## Shared SD sector transfers

Read and write requests share the eight-sector bounce buffer loop. Eligible
AXI and cartridge SDRAM buffers still go directly to the lower transfer
routine. The shared loop copies a write before its command, copies a read
only after success, and returns the first error. Controller commands and
media ownership follow the existing driver rules. [STM32H7 RM0433 Rev. 8,
§55.10.18, p. 2455, IDMA buffer 0 base address](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=2455).

Adapted from the SD portion of paps-g's `31326563`. The GNU App load
decreases from 62,540 to 62,476 bytes. Static DTCM remains 60,184 bytes
and the bounce buffer remains 4,096 bytes.

Run `make -C tests check-mcu-diskio` and repeat with
`SAROO_SD_PROFILE=release`. Both profiles pass 28 host methods.
The baseline and port pass all 16 added transfer cases. The fixture
checks complete payloads on the two direct paths and the bounce path,
and checks failure at each chunk. To compare the wrappers with a
separate baseline checkout, set `SAROO_SD_SOURCE` to its
`Firm_MCU/Main/sdio_h7.c` and run
`python3 -m unittest discover -s tests -p test_sd_media.py -v`.
Physical SD timing and fault recovery remain unmeasured by these checks.

### Reject a sector span that the card address cannot represent

Adapted from paps-g `bf429abf` (AUD-MCU-046). The
[SD Association Physical Layer Simplified Specification, v3.01,
§4.3.14, printed p. 52](https://www.sdcard.org/downloads/pls/archives/)
defines a 32-bit byte address for SDSC and a 32-bit block address
with 512-byte blocks for SDHC/SDXC. The old driver converted an
out-of-range SDSC sector number to bytes after transfer setup.
That conversion could wrap and direct a read or write at a lower
sector. A bounce request crossing the boundary could start with a
valid chunk even though the complete request could not succeed.

The driver now checks a positive count and the inclusive final sector
before it copies data, starts pre-erase or sets up DMA. SDSC can
represent sectors through `0x7fffff` in its 32-bit byte argument;
SDHC/SDXC retain the full 32-bit sector argument. These are address
representation limits. They do not substitute for a card's reported
physical capacity. The same check covers the lower block entry points.
Readiness, direct eligibility, bounce chunk size and transfer recovery
are unchanged.

```sh
make -C tests check-fatfs check-mcu-diskio
SAROO_SD_PROFILE=release make -C tests check-fatfs check-mcu-diskio
```

With `SAROO_SD_SOURCE` pointing at the parent's `sdio_h7.c`, the
`unrepresentable_span` test reaches the modeled controller while
the valid boundary control passes. The port passes 204 new transfer
cases across both directions, the direct and bounce routes, and both
address units. Invalid spans leave all registers and both buffers
untouched. Valid endpoints preserve the emitted SD commands and
arguments. All 70 FatFs and SD methods pass in both runs. The GNU App
load grows from 64,912 to 64,960 bytes (576 free); static DTCM stays
at 50,752 bytes. These are host and build results. Physical card
capacity, command timing and media behavior are unqualified.

### Bound the size of direct SD commands

Adapted from paps-g `4c24b1d8` (AUD-MCU-018). The stock SD clock
configuration gives 25 MHz in four-bit SDR mode, or at most
12,500,000 data bytes per second. FatFs can pass a 16 MiB direct
request in an exFAT cluster. The old driver issued it as one data
command with a one-second wait. The payload alone takes at least
1.34 seconds under that clock configuration.
[STM32 RM0433 Rev. 8 §55.10.2 p. 2436, §55.10.8 p. 2442,
§55.10.11 p. 2445](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=2436).

Admit the complete address span before issuing any command, then
submit aligned direct transfers in chunks of at most 4,096 sectors
(2 MiB). The nominal wire time for a full chunk is about 168 ms.
Advance the address and buffer only after success. The first error
returns to the existing disk retry path. The eight-sector bounce
path, card address units and complete request capacity stay the same.
The wire-time calculation leaves room for card and protocol delays.
Actual completion time depends on the card and bus.

```sh
make -C tests check-mcu-diskio
SAROO_SD_PROFILE=release make -C tests check-mcu-diskio
```

Point `SAROO_SD_SOURCE` at the parent's `sdio_h7.c` and select
`-k respects_wire_rate` to observe the old 16 MiB command. The
512-sector and 4,096-sector controls reach their expected commands.
The port passes 83 new cases. They check address and DMA progression,
lengths of 4,095 to 32,768 sectors, reads and writes in both card
address modes, refusal before I/O for invalid spans, and termination
at each failed chunk. Both 37-method disk and SD runs pass. The GNU
App adds 36 load bytes, leaving 364 free, with unchanged static DTCM.
The GNU BootLoader links with all 166 vectors. The controller model
does not measure physical transfer or card-busy timing.

## Recover aborted save handles through ordinary requests

FatFs latches a disk error on an open file. Subsequent requests on that
handle keep failing after the card becomes usable. The save service now
lets a valid slot or MEMS request recover under the filesystem mutex.
If the SD address was revoked, the existing disk semaphore excludes an
in-flight transfer while the CID-checked driver reidentifies the card.
Ready media uses the existing handle and issues no reidentification
commands. [FatFs result codes](https://elm-chan.org/fsw/ff/doc/rc.html).

Recovery checks close before replacing the handle and keeps the original
file identity across failed opens. It opens the existing file, verifies its
size, first cluster, mount generation and canonical signature, then retries
the ordinary operation. A failed close leaves cached data pending. The
signature is read into local storage, leaving the shared save buffers
available for a retry. Files with a structural error remain rejected.
[FatFs f_close](https://elm-chan.org/fsw/ff/doc/close.html) ·
[FatFs f_open](https://elm-chan.org/fsw/ff/doc/open.html).

Adapted from paps-g's `d5cb72ea`. The port retains the public save extent
rules and SD ownership fixes. The GNU App load is 62,876 bytes (+400), with
2,660 bytes of ITCM margin. Static DTCM is 60,232 bytes (+48). SDIO and
diskio also compile as objects with the BootLoader configuration.

Run `make -C tests check-mcu-save check-mcu-diskio` and repeat with
`SAROO_FIRMWARE_SAVE_PROFILE=release SAROO_SD_PROFILE=release`.
The 60 recovery cases use the actual save code and FatFs on generated
FAT32 and exFAT volumes. The baseline fails 56 cases and passes the four
structural-error controls. To repeat the baseline comparison, set
`SAROO_MCU_SAVE_DIR` to the older checkout's `Firm_MCU/Saturn` and run
`python3 -m unittest discover -s tests -p test_mcu_save_reopen.py -v`.

The identity checks cover the file reopened after a successful close.
The host cases exercise retry behavior and preservation of neighboring
files. Physical SD recovery, writes interrupted by power loss and external
changes to a still-mounted volume remain unqualified.

## Check companion ROMs before BIOS handoff

Stock European and Japanese KOF ’95 and Ultraman need a 2 MiB companion ROM.
The original callbacks installed cartridge hooks and entered the ROM even
after a failed or short read. The menu now requires the complete read before
BIOS handoff. The existing product numbers and IP predicates still select the
ROM. ROM-free variants and unrelated titles follow their existing paths.
[Sega Disc Format Standards, ST-040-R4, §§4.1–4.2, printed pp. 25–26](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf#page=29) ·
[Upstream KOF ’95 reports #261](https://github.com/tpunix/SAROO/issues/261)
and [#276](https://github.com/tpunix/SAROO/issues/276).

The preflight stages the ROM in the existing file transfer workspace before
the artwork region. The relocated SH-2 helper copies forward from that
workspace into the lower, overlapping destination. The standard menu
verifier checks alignment and the two 96-byte relocation slots. The
linked copy and boot helpers occupy 36 and 80 bytes.

Adapted from paps-g's `198a079f` on top of the public file transfer and
CD player safety ports. The menu links 79,684 code/data bytes (+176) and
9,236 BSS bytes (+4). Its complete image remains 488,770 bytes.

Run `make -C tests check-menu-safety check-cover-display` and repeat with
`SAROO_MENU_SAFETY_PROFILE=release SAROO_MENU_CD_PROFILE=release`.
All 29 methods pass in both profiles, including 39 ROM scenarios and 15
preflight cases across the ordinary and CD player launch paths. Set
`SAROO_MENU_ROM_SOURCE` to the baseline `Firm_Saturn/game_patch.c` and
run `python3 -m unittest discover -s tests -p test_menu_rom.py -v` to
reproduce 21 unsafe baseline handoffs and three valid full reads.
Actual game launch and the individual upstream reports remain unqualified.

## Use the FPGA counter for menu timing

The old SH-2 free-running timer reports only one pending overflow. After
two unobserved wraps, the original getter reports 65,569 instead of
131,105 ticks in the host control. Sleep helpers also reset the shared
epoch. The menu now uses the cartridge's existing 32-bit, nominal 1 MHz
counter. It initializes the epoch on menu entry and retains it during
ordinary navigation. Long sleeps advance one cumulative deadline in
bounded steps. The controller path converts the previous nominal
90-tick admission delay to 432 microseconds and keeps its late-field
and timeout checks. [Sega SMPC User's Manual, ST-169-R1, Command Issue
Timing, printed p. 17](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=27).

Adapted from paps-g's `fb67ac50` together with its correction in
`86e2af4e`. The high/low/high read retries on a carry. The low word must
use the CS2 two-transfer alias `0x2581700e`. The earlier
`0x2589700e` access returned the high word on the tested Saturn.
Historical testing with the corrected normal menu restored controller
navigation after Reset. The port uses the original FPGA registers.
[Sega SCU final specifications, ST-210, precaution 32, printed p. 9](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf#page=13) ·
[CS2 transfer measurements](https://segaxtreme.net/threads/a-bus-cs2.24940/).

Run `make -C tests check-menu-safety check-peripherals`. The host runs
cover the corrected access, rollover, full-width sleeps, and controller
and menu deadlines. With Icarus Verilog they also verify the existing
FPGA 05 divider and register decode. Changing only `SS_TIMER_LO` back
to `0x2589700e` in a test copy of `Firm_Saturn` fails four counter cases.
Use `SAROO_MENU_TIMER_DIR` to select that copy for `test_menu_timer.py`.
The standard menu links 79,588 code/data bytes (-96) and 9,232 BSS bytes
(-4). The image remains 488,770 bytes. Physical timing across regions
and cartridge revisions remains unqualified.

## Acknowledge completed saves explicitly

The original FPGA clears both command and result registers on reset. A
Saturn caller that survives the reset could interpret the two zeroes as a
successful save or load. The host control reproduces this with the original
menu. [Original SAROO FPGA register logic](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L526-L548).

The MCU advertises a capability in the existing system information area.
The menu latches it once at entry. The four negotiated save and load
commands carry a tag. The MCU writes the tagged FatFs result before
clearing the command, and the menu accepts only the tagged success value.
A cleared result leaves the dirty bitmap and the pending continuation
available for retry. The argument remains 16 bits, including the MEMS
header flag. Either firmware component can still pair with an older
component using the original command and result values.

Adapted from paps-g's `74999ab0`. The GNU App load is 62,940 bytes (+64),
with 2,596 bytes of ITCM margin and 60,232 bytes of static DTCM. The menu
retains 79,588 code/data bytes and adds 4 BSS bytes to 9,236. No new FPGA
image is needed.

Run `make -C tests check-firmware-save check-file-transfer` and repeat
with `SAROO_FIRMWARE_SAVE_PROFILE=release`. The protocol tests exercise
all 65,536 dispatch values, the complete result range, full-width MEMS
arguments, optical save-load status and reset at the register boundary.
Icarus Verilog checks the public FPGA 05 register transport. To reproduce
the baseline reset failure, point `SAROO_ACK_MENU_DIR` and
`SAROO_ACK_MCU_DIR` at the corresponding directories of the preceding
checkout and run the reset case in `test_save_ack.py`.
Physical reset propagation, durability and recovery of a full running
session remain unqualified.

## Set the required FMC reserved bit during boot

Adapted from paps-g `8345fa51` (MCU-FMC-001). The BootLoader sets up
the cartridge FPGA's multiplexed NOR bus in `device_init`. The
control value inherited from upstream clears FMC_BCR1 bit 7.
ST specifies one for that reserved bit in muxed mode.
[Upstream commit `4eef4054`](https://github.com/tpunix/SAROO/commit/4eef40541e2e3139e7cfdf57addf0de1ac2bc89f) ·
[STM32 RM0433 Rev. 8 Tables 184–185, pp. 829–830](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=829).

Change FMC_BCR1 from `0x8000b05b` to `0x8000b0db`. The 16-bit NOR
mode, NWAIT configuration and FMC_BTR1 timing value stay the same.

```sh
make -C tests check-mcu-startup
# For the parent, set SAROO_FMC_SOURCE to its Firm_MCU/Main/main.c.
```

The parent fails the reserved-bit check and passes the mode and
timing controls. All three checks pass with the correction. The
28,964-byte GNU BootLoader load differs at just the byte containing
bit 7. The GNU App load matches byte for byte because it inherits
the boot configuration. The source and image checks establish the
register contract. No physical failure is attributed to this bit.

## Limit AXI SRAM read issuing on STM32 revisions Y and W

Adapted from paps-g `cb70560a` (STM32-AXI-001). On Y/W silicon,
concurrent reads with delayed acceptance can return incorrect AXI
SRAM data. ST prescribes setting target 7's read issuing override
to allow one outstanding read. Revisions X and V do not have this
erratum. [ST ES0392 Rev. 15 Table 2 and §2.2.10, pp. 13–14](https://www.st.com/resource/en/errata_sheet/es0392-stm32h742xig-stm32h743xig-stm32h750xb-stm32h753xi-device-errata-stmicroelectronics.pdf) ·
[STM32 RM0433 Rev. 8 p. 118, AXI_TARGx_FN_MOD](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=118).

Both public targets use `Startup/system_init.c`. At entry to
`SystemInit`, check `DBGMCU_IDCODE.REV_ID` for `0x1003` and set
`READ_ISS_OVERRIDE` at `0x51008108` while preserving the other
register bits. Both reset handlers call this function before
the rest of firmware startup.

```sh
make -C tests check-mcu-startup
SAROO_STARTUP_PROFILE=release make -C tests check-mcu-startup
# Set SAROO_SYSTEM_INIT_SOURCE to the parent's Startup/system_init.c
# and select -k AXIStartupTests to reproduce the original omission.
```

The parent fails all 16 affected revision cases and passes all
64 other revision cases. The port passes all 80 in both profiles.
The fixture runs the actual App and BootLoader startup bodies with
modeled registers and verifies that the update precedes clock and
vector setup. Both linked images contain the revision comparison and
conditional read, OR and write. Each adds 36 load bytes. The GNU App
uses 65,208 bytes with 328 free and unchanged static DTCM. The GNU
BootLoader uses 29,000 bytes. This verifies the code and ST contract.
No physical Y/W silicon result is attributed to this public build.
