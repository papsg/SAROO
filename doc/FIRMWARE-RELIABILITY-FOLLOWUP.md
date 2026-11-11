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
