# Five safety corrections from the private development history

This ledger tracks direct source ports onto this fork's `main`. Each row is
verified and published separately. The original implementation is credited to
paps-g by its private source commit; the baseline SAROO code remains credited to
tpunix and its contributors. Private research artifacts are references, not
runtime dependencies. These host checks and cross-builds do not establish
physical Saturn qualification.

| Order | Correction and source | Public commit | Verification and remaining limit |
| --- | --- | --- | --- |
| 1 | MCU formatter bounds; paps-g `c5c8df385f3f1d571a47e1885c3a4130784751e4`, adapted from [original formatter](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Main/printk.c) | This change | See below. No device test. |
| 2 | Existing SDRAM self-test failure gate; paps-g `f3399085` | Pending | Pending. |
| 3 | FatFs `f_readdir` error-name clearing; paps-g `399d72df` | Pending | Pending. |
| 4 | Native physical-CD raw-auth HIRQ transaction; paps-g `232758b3` | Pending | Pending. |
| 5 | Stock menu SCI initialization and receive-error recovery; paps-g `3c4823d6` | Pending | Pending. |

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
