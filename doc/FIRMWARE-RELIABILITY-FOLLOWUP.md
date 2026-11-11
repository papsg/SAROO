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
