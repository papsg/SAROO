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
