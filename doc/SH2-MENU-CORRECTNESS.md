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

## SSR-016: pack controller events as unsigned fields

Cast the change mask to`u32` before its16-bit shift. Valid Right transitions
use bit15; shifting the signed value could exceed INT_MAX before assignment
to the unsigned event. Missing-input handling and all debounce/repeat/release
control flow are unchanged. High16 bits still describe changes and low16 bits
still describe current buttons. This adapts paps-g `342fc309`/SSR-016, with
papsg's public fixture/integration work identified.

The language requirement is C11 N1570 **§6.5.7p4**, printedp95/PDFp113:
[WG14 shift operators](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=113).
Sega **ST-169-R1, Table3.18, printedp82/PDFp92** defines Right in bit7 of the
first active-low standard-pad data byte; the existing parser maps that byte
to the menu mask's high byte. Thus this is valid input, not malformed hardware
data: [SMPC User's Manual](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=92).
Both locally retained sources were checked; no online research. No original
tpunix issue/PR is identified for this independent finding.

```sh
python3 -B -m unittest discover -s tests -p test_menu_timer.py -v
SAROO_MENU_SAFETY_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_timer.py -v
```

The actual producer/clock/scroll code passes all11 methods with sanitizers
and optimization. An independent field-layout oracle checks all8,192 masks
across the13 defined bits through press, repeat, missing-sample and release,
including the0x80000000 Right-release event. Mixed Right/Up transitions run
at five timer origins, including signed/unsigned wrap. Existing clock/carry/
reset/sleep/repeat/scroll controls pass. The parent has nine precise UBSan
failures in Right-bearing cases, while its earlier eight methods pass. An
initial extraction refused an unsupported fixture helper return type before
adding the cases; that unchanged-source run is not a defect witness.

Fresh target menu builds have the same complete image/hash, symbols, font
and resource records:87,896 code/data,9,508 BSS,16KiB reserved stack and17,284
margin. The compiler already emitted the intended target shift; no earlier
wrong physical event is asserted. Nonloaded ELF debug metadata may differ.
MCU and FPGA are unchanged. Scripted masks/clock/registers do not qualify
SMPC transactions, all combinations on a physical pad, SH-2 execution, input
latency or games. No device access or new diagnostic feature occurs.

## SSR-017: define negative line slopes

The retained `put_line` converted signed slope deltas with left shifts.
Negative slopes make that undefined in C11. Replace only those two conversions
with multiplication by65,536 before division. All scaled differences fit
signed32-bit arithmetic in the established nonnegative512×256 canvas: maximum
magnitude33,488,896. Endpoint orientation/reversal, quantization, stepping and
zero-span handling remain unchanged. This adds no clipping or arbitrary-integer
coordinate contract. Source searches find no ordinary caller beyond the
declaration/definition; no visible menu failure is inferred.

This adapts paps-g `77173c38`/SSR-017, retaining the source's attribution, with
papsg's public integration/fixture work identified. The requirement is C11
N1570 **§6.5.7p4**, printedp95/PDFp113, the signed-left-shift domain:
[WG14 shift operators](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=113).
The local primary text and original limits were checked; no Sega-specific
rasterization requirement or original tpunix issue/PR is identified.

```sh
python3 -B -m unittest discover -s tests -p test_menu_lines.py -v
SAROO_MENU_SAFETY_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_lines.py -v
```

The actual function receives the original endpoint order. A bounds-checked
pixel callback uses an independent64-bit closed-form oracle, preserving the
existing16.16 truncation. It checks15 explicit orientation/reversal/point/
canvas-edge cases and all65,536 endpoint pairs on a16×16 grid, including553,504
grid pixels, color and exact major-span emission count. Both methods pass
ASan/UBSan and optimization. The parent has eight negative-shift failures;
eight control invocations pass. The initial function-extractor signature
refusal remains separate from the complete parent defect witness.

Host pixels are not framebuffer/SH-2 execution, physical rendering or game
qualification. No device operation or added diagnostic feature occurs.

Fresh paired menu links retain the complete image/hash, symbols, font and
resources from SSR-016:87,896 code/data,9,508 BSS,16KiB reserved stack and
17,284 margin. The individual line frame remains24 bytes. The locked compiler
reduces the multiplication to its prior target shifts; this defines the source
without asserting previously incorrect target pixels. Nonloaded ELF metadata
may differ. MCU/FPGA sources and feature selections are unchanged.

## SSR-021: compare the actual translation key

Equal32-bit hashes do not establish equal source text. Keep the hash as a
candidate filter and require exact`strcmp` equality before returning that
entry's translation; otherwise continue through the existing chain and
return the original unknown label. Languages,64-bucket layout, stored keys,
hash arithmetic, initialization and formatting strings remain unchanged.
This adapts paps-g `f7634ce3`/SSR-021, with papsg's public integration and
new public-table collision witness identified.

The requirement is the original dictionary's key/fallback contract, visible
in [original TT and stored source keys](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/language.c).
C11 N1570 **§7.24.4.2**, printedp365/PDFp383, defines the exact string comparison
used by this repair: [WG14 strcmp](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=383).
This does not invent a Sega translation rule. No original issue/PR is identified
for this separate discovery. Cached/local evidence was used without online research.

The private witness targets a later formatted label, so it is not reused as
public proof. A bounded ASCII search found`probe_uCuB4ef`, whose actual public
hash`0x95eb3646` equals that of`选择游戏`. Independent assertions in the compiled
actual module verify both hashes. The parent returns`Select Game` for that
unknown input (status42, empty sanitizer diagnostics); its ordinary language
control passes. The corrected source preserves unknown-pointer identity in
all19 languages and retains content equality for380 copied keys at distinct
addresses. A one-character noncolliding neighbor remains unchanged too.

```sh
python3 -B -m unittest discover -s tests -p test_language_collision.py -v
SAROO_MENU_SAFETY_PROFILE=release python3 -B -m unittest discover -s tests -p test_language_collision.py -v
make -C tests check-language
```

Both new methods pass under sanitizers/optimization; the existing default C
fixture also runs the collision checks. The old19-language repeated lookup/
selection/wrap controls and65,540 signed startup-ID cases are retained. This
is an API/dictionary defect; no ordinary menu label or game is claimed to have
produced the adversarial input. Existing initialization and valid terminated-
string requirements are unchanged; no NULL admission or separate early-console
lifecycle fix is imported. Physical rendering and SH-2 execution are unqualified.

Fresh target code/data grows16 bytes to87,912, leaving17,268 margin. BSS9,508,
16KiB stack reservation,488,770-byte padded image size and357,698-byte font/hash
are unchanged. The individual TT direct frame grows8→20 bytes, excluding callees
and interrupt/full-stack peaks. Matching-hash candidates now incur an exact
string comparison; no timing claim is made. MCU/FPGA are unchanged.
