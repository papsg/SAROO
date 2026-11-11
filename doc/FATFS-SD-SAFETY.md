# FatFs write safety and SD initialization

## MCU-FS-002 — Keep a failed sync pending

SAROO's FatFs R0.15 patch 1 clears `FA_MODIFIED` in the FAT and exFAT
branches of `f_sync` even when `sync_fs` returns an error. Another sync
can report success without attempting the failed work. Since `f_close`
calls `f_sync`, a close retry can also report success and invalidate the
handle. [Original FatFs source][original-fatfs] · [FatFs f_sync][sync] ·
[FatFs f_close][close]

The shared flag clear now runs after successful synchronization.
A failed sync or close keeps the handle and pending state for a
caller-requested retry. The port adapts paps-g
`1816d52c5a90821cc4a3b18b3687f5e1758cde57` directly in the public tree
and preserves ChaN's notice. [Public FatFs source][current-fatfs]

Generated FAT32 and exFAT volumes exercise directory-write failures, repeated
sync and close, successful retries, and clean syncs with zero additional I/O.
The fixture remounts both volumes and compares the complete 9,217-byte file
and its 2,048-byte neighbor. Baseline `3f02379` loses the pending sync in all
eight injected cases. Both clean controls pass. All ten scenarios and the
existing geometry and seek tests pass with ASan/UBSan and O2.
[Actual FatFs fixture][fatfs-fixture]

The test also injects an error from the lower-layer `CTRL_SYNC` call.
Public SAROO's disk adapter currently returns success for this command.
Rejected directory-sector writes exercise the failure path reachable on
the current firmware. [FatFs disk_ioctl][ioctl] · [SAROO disk adapter][diskio]

Run `make -C tests check-fatfs` from the public checkout. Set
`SAROO_FIRMWARE_SAVE_PROFILE=release` for the O2 build. The GNU App build
has 62,156 load bytes, 3,380 bytes of ITCM margin, and 60,176 bytes of
static DTCM. The direct `f_sync` frame is 112 bytes. Those values match
the preceding checkpoint. The host tests and GNU link checks leave
physical SD durability and the MDK build unqualified. [Build instructions][build]

## MCU-FS-003 — Retry failed FAT mirrors and FSInfo writes

FatFs reports success when a write to the second FAT or the FAT32 FSInfo
sector fails. It also clears the corresponding pending state. A failed
mirror can leave the backup allocation table stale. FSInfo holds the
free-cluster count and allocation hint. A failure there leaves those hints
stale. [Original FatFs source][original-fatfs] · [FatFs f_sync][sync]

The port adapts paps-g `a81828e45f480a803e8fa41d41c934c9eed2f69f`.
The shared sector stays dirty until every required FAT copy succeeds.
A primary error stops the write before the mirror. Retrying a mirror
error can write both copies again. The FSInfo update remains pending on
error, and the shared sector cache remains available to readers because
the counters can rebuild the FSInfo sector. [Public FatFs source][current-fatfs]

Generated FAT12, FAT16 and FAT32 volumes reach the production allocation
and sync paths. The 14 scenarios cover rejected and partial writes,
repeated failures, successful retry, exact copy counts, clean syncs, and
byte comparisons of the files, mirrored tables and FAT32 hints after
remount. The preceding `669edd8` baseline ignores the failure in seven
mirror and FSInfo scenarios. Seven primary-error and single-FAT controls
pass on that baseline. All 14 pass with ASan/UBSan and O2 on this port.
[Actual FatFs fixture][fatfs-fixture]

The GNU App build adds 16 load bytes for a total of 62,172, with 3,364 bytes
of ITCM margin and 60,176 bytes of static DTCM. The direct `sync_fs` frame
falls from 24 to 16 bytes. The tests establish retry behavior after a
reported device error. Physical SD durability remains unqualified.
[Build instructions][build]

## MCU-SD-003 — Reject all failed command waits

The SD driver's `sd_cmd` treats `osErrorTimeout` as failure and falls through
on other failed semaphore waits. With a clean-looking controller status,
it can return success and copy stale responses. The pinned RTX source returns
`osErrorParameter` for a null semaphore, including one left by failed
allocation. [Original SD driver][original-sd] · [CMSIS semaphore contract][semaphore] ·
[Pinned RTX implementation][rtx-semaphore]

The port adapts paps-g `dc3468dee53505c41aa577ac31810f8aa5628585`.
Only `osOK` reaches controller-status interpretation and response
copying. Failed waits take the existing cleanup path. The public port
retains the original timeout, log text and register sequence.
This is the controller command semaphore. The earlier disk-access lock
is handled in `f2b44e7`. [Public SD driver][current-sd] ·
[Disk-access lock](SAVE-STORAGE-FOLLOWUP.md)

The fixture compiles actual `sd_cmd` and models the semaphore result
and SDMMC registers. It verifies five command/response shapes, every
modeled error status, null-handle rejection, hardware errors, the no-CRC
response and register cleanup. Baseline `62d993c` accepts 26 failed waits.
All 43 scenarios pass with ASan/UBSan and O2 on the port. The model's
register bits follow RM0433 Rev 8 sections 55.10.4 (pp. 2438–2440),
55.10.11 (pp. 2445–2447), and 55.10.12 (p. 2448).
[STM32 RM0433][rm0433] · [Command fixture][sd-fixture]

Run `make -C tests check-mcu-diskio`. Set `SAROO_SD_PROFILE=release`
for the optimized command tests. The GNU App remains at 62,172 load bytes,
3,364 bytes of ITCM margin, 60,176 bytes of static DTCM and a 16-byte
direct `sd_cmd` frame. Physical IRQ/DMA completion remains unqualified.
[Build instructions][build]

## MCU-SD-004 — Bound card-ready polling

A card can answer ACMD41 successfully while keeping OCR bit 31 clear.
The command wait returns, but the old identification loop tries again
indefinitely. The port adapts paps-g
`96c9dd85a9e0f56b5c052103a8a2ba6920ad0efb` and starts a two-second
retry budget at the first ACMD41, after its initial CMD55 prefix.
Later command time counts toward the budget. Unsigned elapsed ticks
handle wraparound. The driver accepts a ready response from a command
already in progress and reports timeout when a response remains
unready at or beyond the budget. [Original SD driver][original-sd] ·
[Public driver][current-sd] · [RTX tick configuration][rtx-config]

The retained SD Association excerpt identifies the Physical Layer
Simplified Specification v5.00, p. 27. It calls for at least one second
of ready polling and recommends a longer host timeout. The reference
URL is labeled v5.10. The two-second policy preserves that minimum.
It bounds repeat polling when commands return. An in-progress command
pair can finish after the deadline. Controller abort and DMA ownership
have separate requirements. [SD Association reference][sd-spec] ·
[STM32 RM0433, pp. 2398–2403 and section 55.10.16 (p. 2454)][rm0433]

The public model runs actual `sdcard_identify`, `sd_cmd`, command-prefix,
and clock/bus setup code. It checks both legacy and high-capacity cards,
ready responses at the boundary and from a command finishing after it,
tick wrap, command latency, and CMD55/ACMD41 errors before card selection.
All 25 initialization scenarios pass with ASan/UBSan and O2, including
a bounded exit for a card that keeps returning an unready response.
Four scenarios hit the fixture's command-count guard on baseline
`12093f0`. The other 21 pass there. All 43 command scenarios still pass.
The same suite passes at a modeled 37 Hz, exercising the configured
tick rate alongside the production 100 Hz. Compiled controls fail when
they compare absolute ticks, charge the first CMD55 against the budget,
or fix the budget at 200 ticks. [Initialization fixture][sd-fixture]

The GNU App build adds 48 load bytes at this checkpoint, for a total of
62,220 bytes and 3,316 bytes of ITCM margin. Static DTCM stays at 60,176
bytes and the direct `sdcard_identify` frame is 40 bytes. The menu ELF and
488,770-byte image match the starting baseline. Physical initialization
timing, SD-card compatibility and hardware recovery remain unqualified.
[Build instructions][build]

[original-fatfs]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/FatFS/ff.c
[original-sd]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Main/sdio_h7.c
[current-sd]: ../Firm_MCU/Main/sdio_h7.c
[current-fatfs]: ../Firm_MCU/FatFS/ff.c
[sync]: https://elm-chan.org/fsw/ff/doc/sync.html
[close]: https://elm-chan.org/fsw/ff/doc/close.html
[ioctl]: https://elm-chan.org/fsw/ff/doc/dioctl.html
[diskio]: ../Firm_MCU/FatFS/diskio.c
[fatfs-fixture]: ../tests/fatfs_seek.c
[sd-fixture]: ../tests/sd_safety.c
[semaphore]: https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__SemaphoreMgmt.html
[rtx-semaphore]: https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_semaphore.c
[rtx-config]: ../Firm_MCU/RTE/CMSIS/RTX_Config.h
[rm0433]: https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743-753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
[sd-spec]: https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver5.10.pdf
[build]: ../tools/README.md#original-mcu-app-compilelink-check
