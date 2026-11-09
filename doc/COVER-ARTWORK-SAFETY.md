# Cover and artwork safety

This series follows public `19de4f3`. Findings are independently revalidated
against public source; historical results do not qualify the port. Firmware
producer/consumer changes and native host-tool work have separate checkpoints.
The existing browser editor is preserved.

## SATURN-MENU-003: preserve frame ownership through final DMA

The row-copy helper waits for the preceding channel-2 transfer, then starts
another one. Returning from the final call therefore does not mean the last
row is complete. The old menu clears the shared frame flag immediately,
allowing its producer to reuse memory still read by DMA. Wait until channel-2
standby/operation bits clear before releasing the flag through a volatile store.

The [SCU User's Manual, ST-097-R5-072694, DMA Status Register (DSTA),
Figure 3.13 and D2WT/D2MV descriptions, printed pages 48–49](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf)
defines bits 13 and 12 and their stopped condition. The original page was
rendered/inspected; PDF SHA-256 is
`d56a86087f10c61d0bafaf5dfa0de134f92fe413e6e40f69a656df5192ccc8d3`.
Sega's [DMA Library, §2.4 function specifications 2–3,
DMA_ScuMemCopy / DMA_ScuResult](https://www.infochunk.com/saturn/segahtml_en/prgg/prg3/dmal/hon/p02_13.htm)
also distinguishes transfer start from completion for non-work-RAM destinations.
This is unpaginated HTML, inspected from the local preserved primary snapshot.
SAROO uses its own [row-copy helper](https://github.com/papsg/SAROO/blob/19de4f3/Firm_Saturn/main.c#L537);
that source supplies the channel-2 mask `0x3000` and ownership contract.

Cover rows now advance by their actual width; GIF rows retain their existing
512-byte stride. Upward clipping uses that same selected stride. The latter
is a narrow public refinement: the original correction changed row advancement
but retained `512` in the upward-clipping adjustment. New partial-clipping
controls reproduce its wrong source offset for cover widths below 512.

Original row-width/final-wait implementation and display fixture: `paps-g`,
`681387bc9dae2d8f7c73c1943df8bec7a7c968dc` (2026-09-06). This port separates the
menu prerequisite before the MCU producer, adds independent clipping/GIF/
non-VBlank controls and preserves unrelated source bytes/comments. No new
runtime diagnostic is added.

Four public methods / **eleven scenarios** compile actual `gif_timer`, rebinding
only native addresses. A deterministic asynchronous DMA seam retains its source
until completion; an independent rectangular pixel oracle checks the complete
512×240 destination, palette and guards. Five cover dimensions include one pixel,
odd width and 512×240 staging; GIF stride, partial left/right/top clipping and
non-VBlank pending ownership are checked. Baseline fails **ten** cases; its
non-VBlank control passes. ASan/UBSan and O2 candidates pass all eleven.
Separate missing-wait/fixed-cover-stride/fixed-clipping-stride/wrong-GIF-stride
controls fail **10/2/2/2** cases. Existing binary-selection and bounded-menu-text
checks pass.

```sh
make -C tests check-cover-display
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-cover-display
```

The complete SH-2 menu build adds **16 code/data bytes**, reaching **78,836**.
BSS/reserved stack remain 9,232/16,384; body margin is **26,620**. The 488,751-byte
image and font allocation sizes are unchanged. Direct `gif_timer` frame remains
56 bytes, excluding callees/interrupts. MCU inputs are unchanged. Native fixtures
model DMA completion, not SCU byte lanes, bus timing, cache/VDP rasterization,
physical hangs or game acceptance. The final wait has the same hardware-progress
assumption as the existing per-row helper; no physical timeout policy is added.

## MCU-COVER-001: bound catalog ownership and publish complete artwork

The original allocator advances one cache pointer for each category without a
limit. Twelve full 2,559-entry catalogs require 122,832 bytes, exceeding the
64 KiB cache region and reaching the header table. Only the first 10,240 bytes
are initialized. Invalid indices/categories are used before checks, and cached
positions survive reordered or replaced catalogs. The cache now contains 2,559
16-bit record numbers for **one current catalog**, using 5,118 bytes. Zero means
unresolved; 1–2,048 identifies a header; `0xffff` is a completed absent lookup.
The shared catalog producer invalidates it before every replacement attempt,
including failed, empty and BIN scans. Rescans clear 5,118 bytes and revisiting a
category performs fresh identity lookup. An unchanged, repeated selection still
performs no preview or payload I/O. The surrounding 64 KiB reservation remains.

The reader enables lookup only after opening and reading the complete 64 KiB
header table. A failed close retains file ownership; an explicit `cover_init`
can retry. It checks catalog/category/record bounds, fixed 12-byte serials,
nonzero dimensions within the existing 512×240 staging allocation, and complete
file extents beyond the header table. It reuses the existing checked `save_io`,
including exact seek position and byte count. Normalized preview errors are
retryable, rather than becoming cached absence. Failed payload reads publish a
complete 128×128 blank; partial palette/pixels are never advertised as artwork.

Before reusing shared frame/palette storage, the producer waits for both flags
to clear. Ten one-tick delays bound the wait; exhaustion leaves pending data
unchanged and a later call can retry. Completed pixel/palette/metadata stores
precede a memory barrier and volatile ownership publication. This requires the
preceding **SATURN-MENU-003** final-DMA completion fix. Initialization no longer
clears 77,760 shared graphics bytes; blank publication clears 16,384 pixels
instead of 24,576. No heap allocation or new runtime logger is added.

The format, staging allocation and flag values are SAROO source contracts,
not Sega-defined artwork requirements. FatFs R0.15's
[`f_read`, Description](https://elm-chan.org/fsw/ff/doc/read.html) requires checking
the returned byte count, [`f_lseek`, Description](https://elm-chan.org/fsw/ff/doc/lseek.html)
documents clipped seeks and checking the resulting position, and
[`f_close`, Description](https://elm-chan.org/fsw/ff/doc/close.html) invalidates an
object after successful closure. These unpaginated primary references were read
from the preserved local R0.15 snapshot. The current
[`save_io` implementation](https://github.com/papsg/SAROO/blob/0812ccd/Firm_MCU/Saturn/saturn_utils.c#L522)
implements those checks. The SCU ownership evidence is cited above; the ten-tick
exit is a bounded local policy, not a manual-prescribed timeout.

Original implementation/fixtures: **paps-g**, commit
`681387bc9dae2d8f7c73c1943df8bec7a7c968dc` (2026-09-06). This public port applies
source changes directly, retains the normalized disc-preview prerequisite and
adds explicit empty/BIN/failed-BIN replacement controls. It omits the original
new error print to keep runtime diagnostics outside the port. Related original
requests are [issue #215](https://github.com/tpunix/SAROO/issues/215),
[Bucanero's PR #321](https://github.com/tpunix/SAROO/pull/321) and
[PR #332](https://github.com/tpunix/SAROO/pull/332). Their cached descriptions,
comments and changes establish the tool's history; they do not establish that
these defects explain a particular user's missing artwork. Their existing tool
features are already in v0.8 and are not reimported or claimed as these fixes.

Seven public methods / **55 scenarios** compile the actual catalog scanner,
cache/reader, checked I/O, FatFs and corrected menu display. Independent complete
palette/pixel oracles and guards cover valid dimensions, hot/forced reloads,
12 maximum catalogs, same-count changes, failed/empty/BIN scans, malformed
records/selections, incomplete initialization, exact/short/error/partial I/O,
failed closure, latched disk errors and delayed/stalled consumers. On public
`0812ccd`, **47 scenarios fail and eight valid controls pass**. All 55 pass under
ASan/UBSan and O2. Removing catalog invalidation, ignoring payload errors,
ignoring ownership, and restoring an unbounded serial comparison fails
**7/6/3/1** targeted scenarios respectively.

```sh
make -C tests check-mcu-save
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -m unittest discover -s tests -p test_mcu_save_cover.py -v
```

All **66** public MCU save/disc/catalog methods pass in the integrated sanitizer
run (134.992 seconds). The complete original-project GNU MCU build has 166 vector entries and uses
**59,812 load bytes** (4 fewer), **5,724 ITCM bytes free**, and **61,112 static
DTCM bytes** (48 fewer), leaving a 69,960-byte address gap to the configured
stack/heap boundary. Compiler direct frames are `load_cover` 304→320,
`cover_init` 24→16, new `cover_reset` 8, and unchanged `list_entries` 176 /
`ss_cmd_handle` 112 bytes. Frames exclude callees/interrupt nesting. The build is
a linked GNU qualification artifact; it is not an MDK-equivalence or install
claim. The preceding menu artifact remains unchanged.

Tests model publication/completion, not physical memory ordering, RTOS timing,
SCU bus lanes/cache behavior, VDP output or games. Normalized preview bytes are a
fixture; actual disc parsing retains its separate regressions. Stalled requests
need another caller attempt; latched file errors need explicit reinitialization.
Concurrent catalog generations, live media replacement and GIF decoder bounds
are separate. The later 16,384-record MCU capacity expansion is deliberately
outside this series: firmware lookup remains limited to the first 2,048 records.
