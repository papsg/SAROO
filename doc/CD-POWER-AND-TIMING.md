# CD power and timing

## Bound physical CD power and cartridge takeover

Adapted from paps-g `ce4de2ff` (SATURN-CD-003) for the stock menu and
BIOS callbacks. CDON and CDOFF share a one-second budget for waiting
for SMPC admission and completion. CDON releases the emulated CD
interface after admission. CDOFF gives SAROO control after SF clears
and OREG31 confirms the command. CDON can separately wait up to ten
seconds for a non-BUSY drive reply, reading all four response registers.
The deadlines are firmware policy. A timed-out command can finish later.
[Sega SMPC User's Manual ST-169-R1-072694, §2.2, printed pp. 13–14](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=23),
[OREG31, printed p. 21](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=31),
[CDON and CDOFF, printed pp. 27–28](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=37).

The menu checks CD power before entering the physical player, starting
disc authentication or selecting a binary or image player. A virtual
BIOS boot checks takeover before initializing the CD interface or changing
boot state. Physical BIOS callbacks keep a power retry pending through
another initialization callback, then retry OFF and ON before reading
the drive status.

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
python3 -B -m unittest discover -s tests -p test_menu_launch_errors.py -v
```

Use the tests from this checkpoint and point `SAROO_MENU_CD_SOURCE` and
`SAROO_MENU_LAUNCH_SOURCE` at the parent's `Firm_Saturn` directory. Twelve
methods reproduce stalled commands, ignored replies or caller failures.
The two ordinary power and physical menu controls pass. The port checks
counter wrap, late completion, a wrong completion code, the ready timeout
and retry before stale status can be consumed. The pinned SH-2 menu links
81,172 code/data bytes with 24,272 bytes of body space. Physical SMPC timing
and disc recovery remain unqualified.

## Bound a stalled game sector read

Adapted from the game-facing portion of paps-g `63e0e2b4` (SATURN-CD-003).
After the first empty buffer reply, the reader has ten seconds to observe
another sector. A newly empty buffer starts a fresh budget. Already buffered
sectors take the normal path without reading the timer. Terminal states and
command errors return their existing errors. A timeout returns `-3` and
retains any bytes the reader has already copied. The timeout is a firmware
policy for an empty buffer and depends on the timer and commands returning.
[Sega CD Communication Interface ST-162-062094, §4.1, printed pp. 33–35](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21),
[§8.2.6 No. 6.2, printed p. 92](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=80).

Run `test_menu_cd_transfer.py` with the parent `Firm_Saturn` path in
`SAROO_MENU_CD_SOURCE`. The parent issues another count query after the
deadline and passes the other 15 methods. The port checks stalls with
and without copied output, terminal status, wrap, and a read that makes
progress across 27 seconds of modeled time. Both CD profiles pass 54
methods. The SH-2 menu links 81,236 code/data bytes with 24,208 bytes of
body space. Physical drive timing and game behavior remain unqualified.

## Preserve reset enable after BIOS clock changes

Adapted from paps-g `cc2cafa2` and the clock-hook portion of `ff3f9d2f`
(SATURN-SMPC-005). Both BIOS clock hooks wait for an idle SMPC before
issuing RESENAB, then wait for completion. Each hook uses one shared
poll budget across admission and completion. It reads EXTEN to latch a
fresh VDP2 counter and issues the command in an early scanline with
external latching disabled. The check uses the latched counter when
display output is disabled. Elapsed time varies with the clock and bus.
[Sega System Program User's Manual, clock switching](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/prog/hon/p01_10.htm),
[SMPC command issue and timing, printed pp. 13–14 and 17–19](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=23),
[RESENAB, printed p. 33](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=43),
[VDP2 User's Manual ST-058-R2-060194, §2.5, printed p. 19](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=37).

The KOF95 and Ultraman cartridge hook retains its 96-byte WRAM slot.
The 36-byte issue routine occupies the unused BIOS clock tail ending
before `0x060006dc`. The installer copies both pieces before publishing
the hook vector. The relocated code keeps the existing BIOS calls and
SCU write order and uses only its own code, literals, and fixed MMIO.

```sh
python3 -B -m unittest discover -s tests -p test_menu_clock_reset.py -v
python3 -B tests/check_menu_clock_sh2.py /path/to/ssfirm.elf
```

With `SAROO_MENU_CD_SOURCE` pointing at the parent, four host methods
fail and the ordinary BIOS/SCU control passes. All five methods pass
under sanitizers and O2. The linked SH-2 fragments pass 48 cases at
their relocated addresses, covering register/stack preservation, the
cross-fragment branch, line bounds and timeout behavior. The pinned
menu links 81,380 code/data bytes with 24,064 bytes of body space.
Physical SMPC timing, reset-button behavior and BIOS variants remain
unqualified.

## Use TIM2 for CD worker pacing

Adapted from paps-g `77b8f027` (buffered prescaler) and `dca425eb`
(timer width and worker ownership). The 16-bit TIM6 could start its
first delay before its prescaler took effect. A requested delay of
655,351 microseconds rounded to 65,536 counts and wrote zero to its
reload register. The menu startup command could also start TIM6
outside the worker's wait.

The worker uses 32-bit TIM2 in one-pulse mode. It loads the 100 kHz
prescaler and zeroes the counter through UG with URS set, so a
software update cannot complete the wait. The counter still counts
from zero through the reload value, preserving ordinary periods.
Only an update flag can release the waiter. The bounded wait leaves
room for the inclusive count and the configured RTX tick phase.
[STM32H750 reference manual RM0433 Rev 8, §§39.3.1–39.3.2,
printed pp. 1633–1635](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=1633),
[§39.4.1, printed pp. 1676–1677](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=1676),
[§§39.4.14–39.4.15, printed pp. 1694–1695](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=1694).

The public port releases partially allocated synchronization objects
before CD initialization on failure. If a timer wait fails, it stops
the timer, clears pending completion and drains any late token before
publishing the existing CD error status. A subsequent command can
retry. Directory work uses the existing directory failure response.
The independent TIM7 audio recovery timer retains its handler.

```sh
python3 -B -m unittest discover -s tests -p test_cd_pacing_timer.py -v
python3 -B -m unittest discover -s tests -p test_cdc_play_continuity.py -v
python3 -B -m unittest discover -s tests -p test_menu_command_completion.py -v
```

To use the parent firmware with these tests, point
`SAROO_CD_TIMER_SOURCE`, `SAROO_CDC_DRIVE_DIR` and
`SAROO_MENU_COMPLETION_SOURCE` at its `Firm_MCU/Saturn` directory.
The timer model passes two parent controls and reproduces 14 failing
cases, including an inactive programmed prescaler, reload truncation,
signed overflow and an unbounded wait. The parent also fails the
worker timeout and startup ownership checks. The port passes all
three suites under sanitizers and O2. The GNU MCU link verifies the
TIM2 vector and uses 64,460 load bytes, leaving 1,076 bytes of ITCM
space. Physical timer latency and game pacing remain unqualified.
