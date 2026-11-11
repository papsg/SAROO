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

[original-fatfs]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/FatFS/ff.c
[current-fatfs]: ../Firm_MCU/FatFS/ff.c
[sync]: https://elm-chan.org/fsw/ff/doc/sync.html
[close]: https://elm-chan.org/fsw/ff/doc/close.html
[ioctl]: https://elm-chan.org/fsw/ff/doc/dioctl.html
[diskio]: ../Firm_MCU/FatFS/diskio.c
[fatfs-fixture]: ../tests/fatfs_seek.c
[build]: ../tools/README.md#original-mcu-app-compilelink-check
