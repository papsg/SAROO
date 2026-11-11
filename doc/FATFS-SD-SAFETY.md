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

[original-fatfs]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/FatFS/ff.c
[current-fatfs]: ../Firm_MCU/FatFS/ff.c
[sync]: https://elm-chan.org/fsw/ff/doc/sync.html
[close]: https://elm-chan.org/fsw/ff/doc/close.html
[ioctl]: https://elm-chan.org/fsw/ff/doc/dioctl.html
[diskio]: ../Firm_MCU/FatFS/diskio.c
[fatfs-fixture]: ../tests/fatfs_seek.c
[build]: ../tools/README.md#original-mcu-app-compilelink-check
