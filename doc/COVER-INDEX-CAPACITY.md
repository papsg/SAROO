# Full cover-index lookup

## MCU-COVER-002

The v0.8 desktop writer reserves 16,384 records of 32 bytes in
cover.bin. The firmware read and searched only the first 2,048. A cover
past that point could be missing even with a single installed game.
[Original writer][writer] · [Original reader][reader]

This port adapts paps-g's `babf1f4ca4fb5cd0a4e6163ea69dba3d23e33e53`.
The cutoff was found while reviewing the cover-tool history.
The original artwork feature and Bucanero's tool contributions are already
in v0.8. [Upstream issue #215][issue215] · [PR #321][pr321] · [PR #332][pr332]

The MCU still reads one 64 KiB page at startup. A lookup reads up to seven
additional pages as needed and retains each complete page until the file
reopens. It matches the same normalized serial and IP checksum. An empty
record or the earliest payload offset found while scanning bounds the table.
That preserves a full compact 64 KiB table and the 512 KiB layout written
by the desktop and browser tools. The format has no independent record
count or layout tag. The reader uses these bounds for lookup and checks
the selected payload's dimensions and extent. [Firmware reader][current] ·
[Native format][native] · [Browser format][browser]

A failed extension keeps the preceding pages usable. The catalog cache
stores confirmed misses. A failed seek, read error or short transfer leaves
the entry eligible for another lookup. A disk error latched by FatFs still
requires reopening the file through `cover_init`.
[Checked I/O][current] · [FatFs f_lseek][seek] · [FatFs f_read][read] ·
[FatFs f_close][close]

## Memory ownership

The port builds on the checked cover reader and catalog ownership in
87ddb00 and the menu's final-DMA wait in 0812ccd. The public adaptation puts
the index/cache offsets in the MCU header so the GIF canvas and catalog
cache bounds are checked against the same index address.
[Existing ownership changes and Sega SCU DMA references][artwork] ·
[MCU layout][layout] · [GIF canvas][gif]

| MCU SDRAM range | Use |
| --- | --- |
| 0x61400000–0x61400fff | Graphics control, palette and staging prefix |
| 0x61401000–0x61440fff | Maximum 512×512 GIF canvas |
| 0x61441000–0x6145ffff | Gap before the cover index |
| 0x61460000–0x614dffff | Full 512 KiB cover index |
| 0x614e0000–0x614effff | Existing catalog cache reservation, with 5,118 bytes used |
| 0x614f0000–0x614fffff | Former 64 KiB cover index |

The index uses 448 KiB more shared SDRAM. The menu's file-transfer bounce
buffer ends below graphics. Direct file transfers use caller-owned cartridge
addresses, including existing game-boot staging. The menu's startup command
initializes the index again. [Menu file and startup paths][menu] ·
[MCU startup and selection commands][dispatch] · [Game-boot staging][games]

## Verification

On public baseline `a8f2961`, zero-based records 0 and 2,047 render correctly.
Records 2,048, 4,095, 4,096, 8,192 and 16,383 show the missing-cover placeholder
for the same generated input. With the fix, all seven render the expected
palette and pixels through the actual MCU reader, FatFs and host-compiled
SH-2 display code. The final record is also checked in the final catalog slot.

Run the focused host checks from the repository root.

    python3 -B tests/test_mcu_save_cover.py -v
    SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B tests/test_mcu_save_cover.py -v
    make -C tests check-cover-display check-gif

The 76 cover scenarios pass with ASan/UBSan and O2. They include compact
and partial-page layouts, an early terminator, truncated index and payload
extents, checksum selection across a page boundary, cache reuse and reopen,
seek/read faults with recovery, and an unchanged busy 512×512 GIF canvas.
A full cold lookup requests eight 64 KiB index reads including startup.
Subsequent lookups use the pages already in memory. Those are file-level
counts from the fixture. [Generated-media tests][fixture]

The pinned GNU App build grows from 62,012 to 62,156 loaded bytes, leaving
3,380 bytes of ITCM. Static DTCM stays at 60,176 bytes. The compiler-reported
direct load_cover frame grows from 320 to 336 bytes. The menu ELF and
488,770-byte image match the baseline byte for byte. The builds use the
existing public cover/display fixes and the documented offline build
dependencies. The checks cover host behavior and target compile/link
accounting. Physical SD timing, menu pacing and hardware playback remain
unmeasured. [Build instructions][build]

[writer]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/tools/covertool/main.c
[reader]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_utils.c#L649-L698
[issue215]: https://github.com/tpunix/SAROO/issues/215
[pr321]: https://github.com/tpunix/SAROO/pull/321
[pr332]: https://github.com/tpunix/SAROO/pull/332
[native]: ../tools/covertool/main.c
[browser]: ../tools/covertool/web/src/cover-codec.mjs
[current]: ../Firm_MCU/Saturn/saturn_utils.c
[seek]: https://elm-chan.org/fsw/ff/doc/lseek.html
[read]: https://elm-chan.org/fsw/ff/doc/read.html
[close]: https://elm-chan.org/fsw/ff/doc/close.html
[artwork]: COVER-ARTWORK-SAFETY.md
[layout]: ../Firm_MCU/Saturn/cdc.h
[gif]: ../Firm_MCU/Saturn/tinygif.c
[menu]: ../Firm_Saturn/main.c
[dispatch]: ../Firm_MCU/Saturn/saturn_main.c
[games]: ../Firm_Saturn/game_patch.c
[fixture]: ../tests/mcu_save_cover.c
[build]: ../tools/README.md#isolated-firmware-qualification
