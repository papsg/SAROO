# SH-2 menu correctness follow-up

## SSR-015: keep serial receive budgets representable

The existing `sci_getc(int)` multiplied its signed argument by1,500 before
checking readiness. That overflows for valid large inputs, even with a byte
already waiting. Use a1,500-probe inner counter and a positive unit counter
instead. The complete positive mathematical budget is retained without a
clamp or multiplication. Zero still expires immediately; negative inputs now
consistently expire before touching SCI instead of having inconsistent ready/
nonterminating/undefined outcomes. The existing caller's1,000 units are within
the parent's safe range. No ordinary serial/game failure is inferred.

This adapts paps-g `67f025a6`/SSR-015. Public implementation and fixture work are
by papsg. The original public readiness test, RDR access and `SSR &=0xbf` order
are retained; no unrelated private SCI initialization/recovery change is imported.
This is existing serial functionality, with no new console/diagnostic feature.

The requirement is ISO C11 N1570 **§6.5p5**, printedp76 (PDFp94): an arithmetic result
outside its type's representable range is undefined. See the locally retained
[WG14 draft](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=94).
The operation's polling budget is a software contract; no Sega/Hitachi rule
establishes calibrated elapsed time for these loop units. No original tpunix
issue/PR is identified for this independent finding.

```sh
python3 -B -m unittest discover -s tests -p test_menu_sci_timeout.py -v
SAROO_FILE_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_sci_timeout.py -v
```

The actual production getter is compiled with modeled volatile register
readiness. Twenty-two invocations cover the full signed input boundaries,
original safe budgets, exhaustion/final-ready/just-late boundaries, delayed
readiness and retained error bits. Large positive cases return early; the
complete enormous budget follows from the bounded counters, not physical
exhaustion. Both sanitizer and optimized methods pass. The parent gives four
signed-overflow reports and two formerly inconsistent negative-input results;
its separate original-domain/error-bit control passes. The first literal edit
refused a mixed-newline preimage; those unchanged-provider tests/build remain
preserved as parent controls. The successful edit preserves original line endings.

Fresh paired target menu links grow code/data87,880→87,896 bytes, with17,284
body-margin bytes remaining,9,508 BSS and unchanged16KiB stack reservation.
The getter's compiler direct frame remains0. Padded image size488,770 and the
357,698-byte font/hash are unchanged; code/image identity is not claimed.
MCU/FPGA sources and their standard feature selection remain unchanged.

Host register addresses stand in for hardware. Equal probe counts are not
equal elapsed time. Physical SCI/noisy-line/transfers, SH-2 execution, complete
stack high-water and game behavior remain unqualified. No device was accessed.
