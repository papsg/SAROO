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
