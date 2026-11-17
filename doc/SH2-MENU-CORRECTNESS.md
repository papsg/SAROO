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


## SSR-066: callable unsigned button-release helper

Port of paps-g `fac81eb1`, independently adapted/verified by papsg against the
public header and current unsigned event producer. This is our source finding;
no original upstream issue/PR or observed normal-menu/game failure is asserted.

The old `#define BUTTON_UP  (stat, bt)` puts whitespace before the parenthesis,
so it defines an object-like macro. [C11 N1570 §6.10 lparen grammar, printed
p.160/PDF p.178](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=178)
and [§6.10.3p9–10, printed pp.166–167/PDF pp.184–185](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=185)
distinguish these forms. An ordinary two-argument release call cannot compile.
Removing only the whitespace also leaves an inverted predicate: the independent
PAD_A control reports release0/held1 instead of release1/held0.

Keep the public spelling `BUTTON_UP(event, buttons)` and call an unsigned inline
predicate. SAROO's own producer puts changed buttons in the high16 bits and
current presses in the low16. Every selected button must have changed and none
may remain pressed. Each argument evaluates once; RIGHT's bit31 event uses
logical unsigned extraction. Empty selection is vacuously true; selections
outside the16-bit button field are false. This is the local helper contract,
not a Sega-defined event-word API. The active press helper is unchanged.

```sh
python3 -B -m unittest discover -s tests -p test_menu_button_up.py -v
SAROO_MENU_SAFETY_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_button_up.py -v
make -C tests check-menu-safety
# Compile future generic/RIGHT callers using an existing SH-2 cross compiler:
sh2eb-elf-gcc -m2 -mb -Os -std=c99 -ffreestanding -fno-builtin -Wall -Wextra -Werror -I Firm_Saturn -c tests/menu_button_up_native.c -o /tmp/menu_button_up_native.o
```

The complete parent header passes four existing-press/independent-reference
controls but fails translation of the release interface. A separately retained
whitespace-only header compiles under strict freestanding sanitizer flags and
returns semantic42 for release0/held1. The earlier probe's host-builtin warnings
are preserved separately; they do not qualify the wider legacy header ABI.

Corrected sanitizer/O2 profiles pass393,224 release checks: all65,536 selections
at six event patterns, signed/unsigned RIGHT boundaries, unsupported bits and
single-evaluation side effects. The oracle visits individual bits independently
of the helper's aggregate expression; some masks are deliberately synthetic.
The four existing controls also pass. Header/fixture/driver hashes, compile logs,
binaries, results, parent and competing-implementation failures remain archived.

Forced generic and RIGHT callers compile strictly to big-endian ELF32 SuperH
using the same local pinned/network-disabled compiler as the normal menu.
Objects/disassembly and the exact matching header are preserved. An initial
container-image admission check expected a bare digest rather than the pinned
name@digest form and refused before compilation; the corrected check retains
immutable digest and local-only endpoint requirements. No code is executed on
an SH-2 and neither test caller is linked into production.

The source census finds only the helper definitions, with no ordinary caller.
A fresh full menu build retains the complete ELF/image hashes, symbols/font,
resources and every direct-frame record. Code/data remain87,912, BSS9,508,
reserved stack16,384, margin17,268 and image488,770 bytes. All40 current menu
source pins and the forced-call header match; zero standard-program cost is
measured. The complete menu safety target passes181 methods after supplying the
current paired ELF for its one optional linked-ROM check. That initial skip and
the passing actual-linked replay remain separately recorded. MCU/FPGA sources
and feature selections are unchanged.

C translation, scripted predicates, static target compilation and current-build
parity do not qualify physical controller transactions, target execution, input
latency, feasible physical masks or games. No new caller, press-helper redesign,
runtime diagnostic, USB feature, overlay recipe or device operation is added.

## SSR-110: unsigned, single-evaluation button presses

Port of paps-g `3ed0238f`, with papsg's direct public-layout adaptation and
independent tests. This is a conditional retained-API defect; no normal menu
or game input failure is established. Current production call sites use lower
masks. No new RIGHT caller or release caller is enabled.

`PAD_RIGHT` is an `int` containing bit15. The old `BUTTON_DOWN` macro shifts
that signed mask by16 before the event's unsigned comparison can affect its
type. With 32-bit `int`, the result is not representable. [WG14 N1570
§6.5.7 paragraphs3–4, printed pp.94–95/PDF pp.112–113](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=112)
defines promotions and the signed/unsigned shift domains. The selected menu is
C99; the relevant rule is the same. [Sega ST-169-R1-072694 §3.3, Table3.18,
printed p.82/PDF p.92](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=92)
places active-low Right in bit7 of the first pad byte. SAROO maps that byte to
the high byte of its selection. Changed/current event halves are SAROO's
software convention, not Sega's event-word format. Both primary documents
were read from preserved local copies; no online research was used.

Keep `BUTTON_DOWN(event, buttons)` and use an unsigned-32-bit inline predicate.
All selected changed bits and current pressed bits must be present. Each
argument is evaluated once. Selections occupy16 bits; empty selection remains
true. The independent release helper and event producer remain unchanged.
No original tpunix issue/PR is identified for this source finding.

```sh
PYTHONPATH=tests python3 -B -m unittest -v test_menu_button_down test_menu_button_up test_menu_timer test_menu_repeat_transport
SAROO_MENU_SAFETY_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_menu_button_down test_menu_button_up test_menu_timer test_menu_repeat_transport
# With the already cached pinned compiler image; never downloads or installs:
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/button-contract --button-objects
```

The full parent header passes229,376 lower-mask comparisons. RIGHT, RIGHT+UP,
all thirteen buttons and the full16-bit sweep separately reach nonrecovering
UBSan signed-shift failures. The once-evaluated-argument control also fails.
These are five failed methods for related expressions, not five findings.
All15 other host controls pass. To reproduce, set `SAROO_MENU_BUTTON_HEADER`
to `git show 8d8ec6e:Firm_Saturn/main.h` saved outside the source tree. The
native parent's generic/RIGHT press callers additionally fail strict C99
signedness warnings; that translation refusal is separate from the runtime
UBSan witnesses. Failed logs, source hashes and setups are retained.

Both corrected profiles pass all20 methods. The press oracle visits buttons
individually and covers every65,536 selection at seven event patterns
(458,752 comparisons), plus the separate lower range, RIGHT combinations and
argument counters. Release retains393,224 comparisons. Existing producer,
repeat, wrap and actual controller-transport controls remain valid. Synthetic
masks exceed what a physical standard pad can simultaneously produce.

The maintained `--button-objects` option reuses the pinned, network-disabled
menu compiler and records strict compile-only press/release companions.
It extends only the staged Makefile's build targets; neither object enters
OBJ or the production link. Both are big-endian ELF32 SuperH with verified
32-bit integer widths. Preserved disassembly shows unsigned field construction
and RIGHT's `0x80008000` constant; all four forced functions have zero direct
compiler stack bytes. They are not executed on an SH-2.

Fresh complete before/after builds preserve the exact entire ELF and
488,770-byte image, SHA-256
`e66b16396ab22614c8e33552852be24eb24027bc933d43c9f01883e83c1dbf46`.
Code/data87,928, BSS9,508, reserved stack16,384 and body margin17,252 stay equal,
as do production direct frames. Only `main.h` changes among40 menu source
pins. Both preceding standard MCU builds retain all70 input pins; no fresh
MCU build or physical test is claimed. Rust unit/RTL tests, strict Clippy,
formatting, repeated-option refusal and private preservation checks pass.
These checks do not qualify physical pad sampling, SMPC timing, latency,
Saturn execution, games or every optional build.

## SSR-111: initialize translations before the existing early console

Port of paps-g `6491b42b`, with papsg's independent public stock-path fixture.
With debug bits0/1 set, `_main` enters the existing `sci_shell` before the old
`lang_init`. Its existing `menu` command calls `menu_init`, which translates
labels before installing/running `main_handle`. That handler's LT branch calls
`lang_next`. Empty startup buckets give wrong text for valid configured
languages; invalid IDs can reach array-bound or increment undefined behavior
before the existing initializer admits them. No ordinary post-console
menu/game failure or normal source of the invalid counterexamples is asserted.

Move the existing single call immediately before the console conditional,
still after CD takeover and raw debug printing. Complete `main.c` differs
only by that call reorder. The language module, its admission/bounds, existing
shell, input handlers and selection on reentry are unchanged. This neither
adds a console feature nor ports private diagnostic/command-channel code.

The requirement is the project's translation/startup contract. [WG14 N1570
§6.5p5, printed p.76/PDF p.94](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=94)
and [§6.5.6p8, printed p.93/PDF p.111](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf#page=111)
bound signed arithmetic and array access; they explain the invalid-ID
consequences, not a Sega electrical initialization rule. These are locally
checked primary citations. No original tpunix issue/PR is identified.

```sh
PYTHONPATH=tests python3 -B -m unittest -v test_menu_language_startup test_language_collision
SAROO_MENU_SAFETY_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_menu_language_startup test_language_collision
```

The fixture copies the complete actual language C unchanged and extracts the
actual startup tail, shell `menu` clause and handler's LT clause. It checks
the source-traced translate-before-handler UI order. Only the complete UI,
transport, CD takeover, boot prefix and input endpoints are host substitutes;
this is not execution of the full UART/controller/UI. A wrapper counts actual
initializer calls. Each startup scenario is a fresh process. Normal admission,
all19 languages, invalid boundary/reported values, early lookup/cycling,
debug-bit combinations, repeated UI entry and selection after console return
are covered. All43 startup processes and both complete language/collision
controls pass under ASan/UBSan and O2 (six methods per mode).

The parent passes31 startup controls but fails eight wrong-text scenarios and
four nonrecovering UBSan cases: three array bounds and `INT_MAX` increment.
Reproduce it by saving `git show cc8f6db:Firm_Saturn/main.c` outside the source
and setting `SAROO_MENU_STARTUP_SOURCE` to that file. The current source
initializes exactly once before the early UI. The actual language controls
retain their65,541 startup inputs and copied-key/full-hash collision checks.
Synthetic invalid metadata is a boundary test, not evidence that normal
configuration produces it. Source copies, extracted clauses, compile commands,
per-process results and failures remain under `tests/build`.

The full native build and all40 menu inputs join with only `main.c` changed.
Inspected linked calls move initialization from0x02003a8a to0x02003a7a, before
the conditional console at0x02003a88 (formerly0x02003a82); CD takeover remains
at0x02003a74. The normal488,770-byte image changes to SHA-256
`5df4296ec40cfbd99820e12f6b71071181ebde3efdaac37e7327fbf94b3e6170`.
Code/data87,928, BSS9,508, reserved stack16,384, body margin17,252 and font
bytes/hash stay unchanged. Direct `_main`/`lang_init`/`sci_shell` frames remain
24/24/172 bytes, not complete stack or IRQ peaks. Prior standard MCU input
pins remain exact. Source/privacy/whitespace and private preservation checks
pass. No device, installed firmware, MCU/FPGA source or physical gameplay is
changed or qualified.

## SSR-158: decode complete UTF-8 scalars in menu labels

[RFC 3629 §§3–4](https://www.rfc-editor.org/rfc/rfc3629#section-3) defines
one-to-four-byte UTF-8 encodings and excludes overlong sequences, surrogates
and values above U+10FFFF. The [public menu decoder](../Firm_Saturn/conio.c)
handled a valid four-byte sequence with its three-byte calculation: for example
U+1F600 became U+07D8, leaving a second character. Filename and menu-label
paths can carry UTF-8, so this is a conditional display defect. No physical
occurrence or file-selection error has been established.

This ports paps-g `20c97a9d` / SSR-158 as a direct edit of
`utf8_to_ucs`. The decoder now returns the complete scalar and advances past
its full encoding. Malformed or truncated input consumes the maximal valid
prefix and displays one `?`, leaving the next byte or NUL for the following
call. That question mark is SAROO display policy; RFC 3629 does not prescribe
it. Existing NUL-terminated string behavior remains. Supplementary scalars
are not added to the font: the existing lookup renders one fallback square.
No original tpunix issue or PR was identified.

The exact public parent fails four maintained methods: complete scalar value
and byte extent, malformed subparts, named supplementary controls and the
real lookup/raster fallback. The corrected decoder passes all four. The test
compares every 1,112,063 nonzero Unicode scalar against 4,382,591 bytes from
Python's independent encoder, plus 69,469 guarded strings and 260,381 exact
scalar/subpart calls from its strict decoder. Seven named controls cover ASCII,
BMP, supplementary and maximum values. The real font lookup and guarded raster
produce exactly one square for a supplementary label while retaining a distinct
BMP glyph. Three existing menu text tests and nine font coverage tests pass.
Run `make -C tests check-menu-text` or the standalone
[`test_menu_utf8.py`](../tests/test_menu_utf8.py); set
`SAROO_MENU_UTF8_SOURCE` to an original `conio.c` copy for the parent witness.

Paired isolated SH-2 menu builds differ in exactly `Firm_Saturn/conio.c` among
their complete source inputs. Both retain the 488,770-byte image, unchanged
357,698-byte font/hash and 9,508-byte BSS. Linked code/data grows
87,928→88,040 bytes; body margin falls 17,252→17,140 bytes. The compiler's
direct decoder frame grows 0→32 bytes, which does not measure complete stack
high-water. Host sanitizers and target linking do not qualify Saturn rendering,
timing or games. No device was touched.
