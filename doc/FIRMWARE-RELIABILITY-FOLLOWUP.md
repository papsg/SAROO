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
