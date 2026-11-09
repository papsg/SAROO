# Menu palette and timing safety

Direct source ports start at public `19bbd76`. Original findings are revalidated
against public source; historical host/build results are not reused. The two
changes have separate verification/publication checkpoints. Later clock, GIF,
controller additions and video-register corrections remain outside this series.

## SATURN-MENU-001: initialize the defined palette without overreading

The [current baseline initialization](https://github.com/papsg/SAROO/blob/19bbd76/Firm_Saturn/conio.c#L85)
reads 256 words from `vga_pal[16]`. Entry16 is already outside the array. Native
sanitizers reproduce the defect in three initialization cases; the independent
logo-only control passes. This establishes a source defect, not the cause of a
reported hardware fault, freeze or particular visual symptom.

Copy only the actual table extent, then explicitly initialize the rest of the
256-entry text bank to black. Keep volatile 32-bit destination stores and all
16 defined colors. SAROO's cover writer starts at CRAM offset `0x400`; all later
palette entries retain their previous bytes. The logo path and other video
register values are unchanged. No font/framebuffer format or allocation changes.

The [Sega VDP2 User's Manual, ST-058-R2-060194](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf),
§3.4 (printed43–46), Figure3.9 (44) and Figure3.10 (45), defines mode2's
RGB8/32-bit color storage. Its Color RAM Address Offset Register description
(217) defines per-screen offset fields. Those pages were read from the local
primary snapshot and the layout/offset diagrams rendered for inspection.
The specific text/cover bank boundary and fixed 16-color table are project
contracts in `vdp_init`, `nbg1_set_cram` and the text drawing code; the manual
does not prescribe this palette or require these unused text entries to be black.
WG14 [N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
§6.5.6 p8 (printed93) defines array-pointer bounds and forbids dereferencing
one-past pointers. Clearing unused entries is an explicit deterministic policy.

Original implementation/fixture: **paps-g**,
`9d0cf19951a1fe6e3e89f79cdd92bee50fd3959b` (2026-09-06). Public adaptation removes
the private source-preparer dependency, captures its own source/input artifacts
and applies the fixture aliasing correction recorded at `b06621a8`. Existing
public source credits and CC BY-SA3.0 notices remain unchanged. No individual
upstream request is claimed as the origin of this self-reported correction.

Four public methods check the complete palette, two reseeded initializations,
logo/no-logo paths, exact original logo palette/tiles, VRAM/register guards and
first/last cover colors. ASan/UBSan and O2 pass; baseline fails three cases at
`vga_pal[16]`, while the logo-only control passes. Omitting tail clearing or
clearing into the cover bank fails three cases each. Actual source functions
and original register macros are compiled, with only hardware backing replaced
by host storage. The fixture uses `-fno-strict-aliasing` for its register views.

```sh
make -C tests check-menu-safety
SAROO_MENU_SAFETY_PROFILE=release make -C tests check-menu-safety
# Baseline/mutation comparisons: SAROO_MENU_PALETTE_SOURCE=/path/to/conio.c
```

The complete menu link adds **16 code/data bytes** (78,836→78,852). BSS/reserved
stack stay **9,232/16,384**, leaving **26,604** body bytes. The image/font allocation
sizes stay **488,751/357,679**; image contents change with the corrected code.
Compiler direct `vdp_init` / `logo_trans` frames remain **4/4 bytes**, excluding
callees/interrupts. MCU source inputs are unchanged. Source-level palette work
reads 16 defined words instead of 256, avoiding 240 invalid words/960 bytes;
all 256 color writes remain. This is not a measured bus-traffic or speed claim.

These checks establish bounded access, preserved bytes and register intent.
They do not emulate VDP rasterization, logo duration, physical store timing,
complete video initialization or games. Nothing is installed on the cartridge.
The timing correction is the separate checkpoint below.

## SATURN-MENU-004: preserve repeat intervals and short deadlines across wrap

The public baseline at `7c97088` still assigns the absolute repeat deadline the
bare value `100` after the first repeat. This is neither a future deadline nor
100 milliseconds in timer ticks. Valid controller samples can then repeat on
every poll. Schedule the next repeat from its actual emission time plus
`MS2TICK(PAD_REPEAT_RATE)`. A late poll emits once and starts a new interval;
it does not replay missed repeats. Keep the initial 500 ms delay, 100 ms repeat
interval, four-sample press debounce, release handling and no-data behavior.

Use unsigned repeat/scroll deadlines and the existing `timer_reached` helper
for the two 1-second title-scroll waits and held-repeat check. The unsigned
timer carry and helper were already public in
[86c7caf](https://github.com/papsg/SAROO/commit/86c7caf); they are preserved,
not ported twice. The baseline scrolling deadline addition can overflow signed
`int`; absolute comparisons also misorder a deadline spanning unsigned wrap.
WG14 [N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf),
§6.2.5 p9 (printed40) defines unsigned modulo arithmetic, while §6.5 p5
(printed76) defines out-of-range arithmetic as undefined. The half-range
comparison is a project algorithm for short deadlines, not a hardware feature.

Reset also discards an old overflow flag after clearing the counter and carry.
Without that clearing, the next read can add a stale 65,536 ticks, about 314 ms
at the inherited configured frequency. The
[Hitachi SH7604 Hardware Manual](https://antime.kapsi.fi/sega/files/sh7604.pdf),
§11.2.5 (printed300–302, specifically301), specifies reading OVF as one then
writing zero to clear it. Read-modify-write clears only OVF and retains the
other status/control bits. Section11.3 (304) specifies upper-byte then lower-byte
accesses through TEMP. Reset and getter retain that order, and the getter's
existing status/counter/status retry sequence is unchanged. The cited local
manual pages were independently read and rendered for this port.

The delay values and late-poll policy come from the menu's existing constants
and intended navigation behavior; Sega/Hitachi does not mandate this user
interface. Current controller collection, admission and interrupt ownership are
unchanged; their separate primary requirements are in [PERIPHERALS.md](PERIPHERALS.md).
The getter's existing overflow acknowledgement is also unchanged: preservation
of unrelated status bits here specifically describes the new reset operation.

Original implementation/fixtures: **paps-g**,
`90db7fe2056528bf276d7505b16bb3c68f419325` (2026-09-06). The public port extracts
its own actual source, deduplicates the existing carry/helper, and adds register
reference-order and controller-to-repeat integration controls. Existing credits
and notices remain. This is a self-reported source correction; no unrelated
upstream issue or PR is claimed as its origin.

Seven public timer methods execute 21 native cases. The baseline fails13:
five normal repeats, five late repeats, a signed-overflow scrolling case, stale
reset, and the reset part of the register-order check. Three existing carry
cases, four other scrolling origins and the no-overflow reset control pass.
Corrected source passes all21 under ASan/UBSan and O2. Five origins include
signed-boundary and unsigned-wrap crossings. Checks cover just-before/exact
deadlines, 40 successive repeats, unavailable/duplicate samples, release
cancellation, both scroll waits and VBlank-gated drawing/uploads. The register
fixture also injects an overflow between counter references to require retry.
It checks C register references and stored values, not silicon event timing or
TEMP-latch emulation. Target disassembly independently retains byte-access
order and the new status-register read/mask/write.

A separate public integration method runs ten normal/late cases through the
actual interrupt-driven SMPC collector and actual `conio_getc`. Explicit wire
packets pass the existing admission, debounce, no-new-sample, same-blank and
release gates at the same five timer origins. All ten pass with sanitizers/O2;
all ten baseline cases fail at an early repeat. This fixture supplies a
synthetic monotonic modulo clock; actual clock code is checked separately.
It does not qualify electrical polling, physical interrupt latency or a device.

Eight targeted mutations are rejected: bare repeat rate (10 failures), rate
without tick conversion (10), catching up from the old deadline (5), absolute
repeat comparison (2), absolute scroll comparison (1), omitted reset clearing
(2), clearing all status bits (3), and reversed counter reads (1). The absolute
scroll mutation exposed a fixture gap; an early observation before counter
wrap was added to both scrolling waits before final validation.

```sh
make -C tests check-menu-safety
SAROO_MENU_SAFETY_PROFILE=release make -C tests check-menu-safety
# Baseline/mutations: point SAROO_MENU_TIMER_DIR to a directory containing
# main.c, main.h and conio.c.
cargo run --locked --manifest-path tools/Cargo.toml -p firmware-build -- \
  menu --repo . --out tests/build/menu-timing-target
make -C tests check
```

The timer checkpoint adds **32 code/data bytes** (78,852→78,884); the whole
series adds48 versus `19bbd76`. BSS/reserved stack remain **9,232/16,384**,
with **26,572** body bytes free. Image/font sizes stay **488,751/357,679**.
Direct compiler frames for reset/getter/repeat/scroll remain **0/4/12/8 bytes**;
these exclude callees and interrupts. MCU inputs are unchanged. No allocation,
file I/O, buffer or extra clock poll is added. Reset gains one status read and
write; repeat interval conversion is constant-folded and schedules one future
deadline. This is source/disassembly accounting, not a measured speed claim.

The inherited FRT getter has one overflow flag and requires timely polling:
multiple unobserved 16-bit wraps cannot be recovered. Short-deadline comparison
requires a consistent clock epoch and observations within half the 32-bit range.
The later clock/sleep-duration redesign remains separate. These checks resolve
the demonstrated source defects; they establish neither long-idle freeze cause
nor physical timing, regional/peripheral compatibility or game qualification.
The target build and integrated suite are host/build verification only. Nothing
is installed on the cartridge.

## Series verification

The complete public `make -C tests check` passes **243 Python methods** in
21 groups, **14 Rust checks**, and its native C checks. The palette/timer/
controller integration controls also pass uninstrumented O2. The target menu
link succeeds; its existing warning count falls26→25 for the timing change,
with no added warning. All36 menu build inputs match the verified source;
all56 MCU project inputs match the unchanged preceding MCU build receipt.

Publication review covers full new/changed files, commit text and approved
public Git identities. A secret scan and private-PII checks find no leaks;
raw host paths/logs and private provenance snapshots remain local and ignored.
These are verification records, not additional firmware instrumentation.
