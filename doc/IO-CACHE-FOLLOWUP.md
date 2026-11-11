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
