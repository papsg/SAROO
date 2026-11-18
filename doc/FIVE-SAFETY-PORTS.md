# Five safety corrections from the private development history

This ledger tracks direct source ports onto this fork's `main`. Each row is
verified and published separately. The original implementation is credited to
paps-g by its private source commit; the baseline SAROO code remains credited to
tpunix and its contributors. Private research artifacts are references, not
runtime dependencies. These host checks and cross-builds do not establish
physical Saturn qualification.

| Order | Correction and source | Public commit | Verification and remaining limit |
| --- | --- | --- | --- |
| 1 | MCU formatter bounds; paps-g `c5c8df385f3f1d571a47e1885c3a4130784751e4`, adapted from [original formatter](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Main/printk.c) | [`3f1cd8e`](https://github.com/papsg/SAROO/commit/3f1cd8e) | See below. No device test. |
| 2 | Existing SDRAM self-test failure gate; paps-g `f3399085a262b9d6bcf5b050c99a63d591d63c92`, related to [upstream issue #79](https://github.com/tpunix/SAROO/issues/79) | [`6968ad3`](https://github.com/papsg/SAROO/commit/6968ad3) | See below. No device test. |
| 3 | FatFs `f_readdir` error-name clearing; paps-g `399d72df36d2c10305508c5c94d3d229e430bc1a`, adapting the [pinned upstream FatFs source](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/FatFS/ff.c) | [`4631c65`](https://github.com/papsg/SAROO/commit/4631c65) | See below. No device test. |
| 4 | Native physical-CD raw-auth HIRQ transaction; paps-g `232758b3f87fa0df9059c9f1e86f361e127bb474`, adapting the [original menu routine](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/cdblock.c) | [`2798b2e`](https://github.com/papsg/SAROO/commit/2798b2e) | See below. Physical disc unqualified. |
| 5 | Stock menu SCI initialization and receive-error recovery; paps-g `3c4823d6c13dfe3189d43a6420547a895fb26f57`, adapting the [original serial setup](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/main.c) | [`9149c07`](https://github.com/papsg/SAROO/commit/9149c07) | See below. Physical serial use unqualified. |

## 1. MCU formatter bounds

The pre-port source could fill every byte of `printk`'s 256-byte stack array
without a trailing NUL; the UART's `strlen` then read outside the array. The
same source also overflowed field-width parsing, negated `INT_MIN` as a signed
integer, and mishandled some signs. The port reserves the last byte for NUL,
stops padding at capacity, saturates parsed width, and computes negative
magnitudes with unsigned arithmetic. It retains the original formatter's
limited format vocabulary and its stored-byte count convention, including NUL.
The private UART and USB trace hunks are omitted because this fork has a
different UART implementation and this series excludes USB work.

`python3 tests/test_mcu_printk.py -v` compiles the actual MCU formatter twice,
for App and BootLoader, under ASan/UBSan. Before the source change, the short
message and hexadecimal control passed; the long path failed with an ASan
out-of-bounds `strlen` in both profiles, while signed minimum, huge width,
alignment and invalid-capacity checks failed. After the change, all six test
methods pass. The offline GNU App compile/link check passes with a 59,360-byte
load image and 6,176 ITCM bytes free; the BootLoader object check passes.
The GNU link and host sanitizer checks do not prove a Keil build or device
behavior.

## 2. Existing SDRAM self-test failure gate

The stock optional test scans the 8 MiB SDRAM region before loading the menu.
With the one-pass flag and a failed test, startup signaled the error but then
cleared it and continued to use memory that had just failed. The correction
returns after the loop if the last test failed. Continuous-test mode keeps its
existing repeat behavior even after errors, and disabled or passing tests
continue as before. It changes neither the test algorithm nor its flag meanings.
The source block comes from the [original upstream test](https://github.com/tpunix/SAROO/commit/19b2e5c180ad4716c3b1fbd46067cf4059a47fb3).

`python3 tests/test_mcu_sdram_gate.py -v` extracts that block from the current
MCU source and injects test results. Before correction, all four failed
one-pass cases fail; disabled, passing and continuous controls pass. After
correction, all four test methods pass under ASan/UBSan, covering 16 processes.
The offline GNU App compile/link passes with a 59,364-byte load image and
6,172 ITCM bytes free. Injected results do not measure the SDRAM hardware,
electrical cause, LED timing or a physical startup.

## 3. FatFs directory-error name policy

`f_readdir` can fill a file name and then return an error while advancing to
the next item. Errors before filling an item can leave a caller's old name in
place. The version-matched FatFs R0.15 documentation promises an empty name at
normal end of directory; it does **not** promise an empty name on error. Current
SAROO callers check `FRESULT` before using the name, so this is a stricter
repository policy, not a demonstrated catalog bug or upstream contract defect.
The correction clears only `fname[0]` when a nonnull output accompanies an error;
result, metadata, cursor and null-output rewind stay unchanged.

`python3 tests/test_fatfs_readdir.py -v` compiles the selected FAT32 and optional
multi-format provider with ASan/UBSan. It builds five sparse images, enumerates
40 files in each, injects each observed directory read failure in turn, retries
and checks all file contents and rewind behavior. Before the change, all five
cases finish those checks but fail the empty-on-error policy assertion. After
the change, all five pass. Offline GNU App compile/link and BootLoader object
checks pass. The App load image is 59,376 bytes with 6,160 ITCM bytes free.
These host block failures do not establish physical SD fault rates or Saturn
behavior.

## 4. Native physical-CD raw authentication

The stock high-WRAM authentication routine cleared an event and then waited
while that event was **set**, so the ordinary cleared state skipped the wait.
It did that for four command responses and two completions, sent raw-sector
bytes before data-ready, and issued authentication before sector movement
completed. The port waits for CMOK and each command's completion, acknowledges
the relevant old event, bounds each wait, and stops on REJECT or WAIT. The
caller restores sector size even when the copied routine fails, retaining the
first error. This follows [Sega ST-162-062094, register and HIRQ definitions,
printed pp. 27–32](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=15)
and [command completion rules, printed pp. 62–63](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=50).

`python3 tests/test_menu_cd_raw_hirq.py -v` extracts the current production
routine for a host event fixture. Before correction its required command/event
sequence fails; after correction the sequence, eight bounded-wait failures,
unavailable write buffer, and REJECT/WAIT at all four commands pass. An adjacent
actual-source caller fixture checks transfer-size restoration after a relocated
routine failure. Both ASan/UBSan and release-profile runs of all 77 menu CD
methods pass. The isolated SuperH menu build passes with 96,648 code/data bytes,
9,540 BSS bytes and 8,500 bytes of body margin. The linked copied routine is
four-byte aligned, 304 bytes long, contains its literals, and has no calls or
addresses to low-WRAM code; reproduce with `python3
tests/check_menu_cd_raw_copy.py tests/build/raw-hirq-menu-20261117/source/dump.txt`
after the [isolated menu build](../tools/README.md#isolated-firmware-qualification).
No optical disc, CD Block timing or physical recovery has been qualified.

## 5. Stock menu SCI initialization and receive-error recovery

The menu enabled SCI interrupt bits even though it installs no SCI interrupt
handler, then immediately enabled transmit and receive after setting the baud
rate. It also left receive errors set. The correction disables the channel,
discards retained receive state, selects the existing internal clock with
interrupts off, waits five microseconds, then enables transmit and receive.
When a valid byte and an error coexist, it returns that earlier valid byte and
clears the receive flags; an error without a byte is cleared before polling
continues. The existing bounded timeout-chunk correction remains in place.
The [Hitachi SH7604 Hardware Manual, §13.3.1, printed pp. 356–357](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=372)
requires the one-bit wait before TE/RE. Its [§13.2.6–13.2.7, printed pp.
340–345](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=356) describes
retained receive flags, clearing, and overrun behavior.

`python3 -B -m unittest discover -s tests -p 'test_menu_sci*.py' -v` runs
actual-source register fixtures under ASan/UBSan. Before the change, the
initialization test fails on the missing one-bit wait. Afterward, the register
contract and full signed timeout-domain tests pass, including error-only
recovery and preservation of a valid byte during overrun. The timeout methods
also pass in the release profile, and all 11 adjacent timer methods pass.
The isolated SuperH menu build passes with 96,696 code/data bytes, 9,540 BSS
bytes and 8,452 bytes of body margin. Its disassembly calls `usleep` with
argument five between SCR writes `0x01` and `0x31`. This does not qualify
physical serial startup, a noisy line or XMODEM transfer on a Saturn.
