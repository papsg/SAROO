# Storage and catalog audit

## Align direct SD transfers

Adapted from paps-g `b483124c` (AUD-MCU-001). The sector wrapper now
checks word alignment before giving the caller's buffer to SDMMC
IDMA. An unaligned byte buffer in AXI SRAM or cartridge SDRAM uses
the existing eight-sector bounce buffer. Aligned buffers in those
regions keep the direct path. The bounce path keeps completed reads,
copies writes before each command, and stops at the first error.

The buffer base register stores bits 31–2. Its low two bits read as
zero, so a direct transfer at an unaligned address cannot start
at the caller's requested byte. The original driver supplies the
existing memory regions and bounce capacity.
[STM32 RM0433 Rev. 8 §55.10.18, printed p. 2455](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743-753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf),
[SAROO v0.8 SD driver](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Main/sdio_h7.c).

```sh
make -C tests check-mcu-diskio
SAROO_SD_PROFILE=release SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-mcu-diskio
```

The parent fails both modeled DMA regions on the new alignment check
and passes the existing sector controls. The port covers all three
unaligned residues in both regions and directions, with 48 successful
or faulted transfer cases and 16 existing controls. Each case checks
payloads, guard bytes, chunk order and the completed prefix after
an error. Both storage runs pass 30 methods. Set `SAROO_SD_SOURCE` to
the parent's driver and select `-k unaligned` in `test_sd_media.py`
to reproduce the refusal.

The GNU MCU App links 63,864 load bytes with 1,672 bytes of ITCM
headroom. All 738 direct compiler frames match the parent. The
checks model the sector and controller boundaries. Physical DMA
timing remains unqualified.

## Share disk read and write retries

Adapted from paps-g `b483124c` (AUD-RES-001). The FatFs entry points
share one retry loop. The helper carries the direction in a high bit
of its existing drive argument and preserves all eight drive bits.
It acquires the disk semaphore once, makes at most four attempts,
then releases the semaphore once. Failed acquisition returns before
the first transfer. The existing failure and recovery messages keep
their arguments and order. Read-only builds link without a write
provider.
[Original SAROO disk adapter](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/FatFS/diskio.c),
[CMSIS-RTOS2 semaphore acquire and release](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__SemaphoreMgmt.html).

```sh
python3 -B -m unittest discover -s tests -p test_mcu_diskio.py -v
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B -m unittest discover -s tests -p test_mcu_diskio.py -v
```

Both revisions pass the same nine disk adapter methods. The matrix
checks 2,560 drive, direction and retry combinations and another
1,280 read-only cases, including exact return codes, payloads, lock
use and message order. Set `SAROO_DISKIO_SOURCE` to compare the
parent. Both complete storage runs pass 32 methods on the port.

The public GNU App saves 64 load bytes, from 63,864 to 63,800.
Static DTCM stays at 60,208 bytes. The two wrappers tail-call
the 40-byte helper frame, compared with 32-byte frames on the
parent. This measures an eight-byte increase while the shared
transfer function runs. Physical timing and peak stack use remain
unqualified.

## Validate exFAT allocation bitmap backing

Adapted from paps-g `b483124c` (AUD-MCU-021). Mount now verifies
that the allocation bitmap declares enough bytes for the cluster
heap and that its contiguous FAT chain holds the declared length.
Each link must be inside the declared FAT and cluster heap. The
public port validates volume-relative bounds before forming the
root and FAT addresses, and computes the heap product at full width.
It publishes the bitmap sector only after these checks. A malformed
bitmap returns `FR_NO_FILESYSTEM`. A failed read returns
`FR_DISK_ERR` and permits a new mount. The existing single-FAT,
contiguous-bitmap support and valid padding are preserved.
[Microsoft exFAT §§3.1.6–3.1.9, FAT and cluster heap bounds](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#316-fatoffset-field),
[§§7.1.3–7.1.5 and Table 22, bitmap chain and length](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#71-allocation-bitmap-directory-entry).

The exact parent accepted a bitmap backed by one sector on a volume
that needed two. An ordinary file write changed the first byte of the
neighboring file from zero to one. Five other parent subcases fail the
mount or read bound. The port rejects all seven invalid layouts with
zero writes. Valid exact and padded bitmaps, partitioned exFAT, three
formatted exFAT cluster sizes and FAT32 retain both files after remount.
All three injected mount read errors permit retry.

```sh
python3 -B -m unittest discover -s tests -p test_fatfs_seek.py -k bitmap -v
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B -m unittest discover -s tests -p test_fatfs_seek.py -k bitmap -v
```

Set `SAROO_FATFS_SOURCE` to the parent's `ff.c` for the comparison.
The fixture uses generated files as block devices. The port adds
96 GNU MCU App load bytes for a total of 63,896 and leaves 1,640
bytes in ITCM. The direct mount frame grows from 40 to 48 bytes.
Static DTCM stays at 60,208 bytes. Physical media and power-loss
behavior remain unqualified.

## Bound game catalog sorting

Adapted from paps-g `b483124c` (AUD-MCU-006 and AUD-RES-021).
The game catalog sorts its existing offset array with in-place
heapsort. The old endpoint partition compared every remaining pair
on an ordered or equal catalog. The heap bounds the number of
comparison steps by its height and needs only local indices and one
temporary offset. The catalog retains the same filename comparator,
forward and reverse modes, enumeration order when sorting is off,
and each offset's file and cover association.
[Original SAROO sorter](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_utils.c),
[Public parent comparator and catalog publication](https://github.com/papsg/SAROO/blob/73a9ce65b2ea26fb66dab6dded0335b85c70d89b/Firm_MCU/Saturn/cdimg.c#L349-L427).

The exact parent uses 3,272,961 comparisons for each of the three
ordered, reversed and equal 2,559-entry cases checked in ascending
mode. The port uses at most 52,436 across those shapes in both
directions. Both versions pass 100 reference ordering and offset
conservation cases, including the full catalog and 255-byte names.
The port passes all 24 discovery and cover methods in each host
profile, including file lookup, scan failures and catalog reuse.
These are comparison counts, with physical startup latency open.

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_discovery.py -v
python3 -B -m unittest discover -s tests -p test_mcu_save_cover.py -v
```

Set `SAROO_MCU_SAVE_DIR` to the parent's `Firm_MCU/Saturn` directory
and select `-k catalog_sort` to compare the exact old sorter.
The GNU MCU App saves 96 load bytes, reaching 63,800 bytes, and
eight static DTCM bytes. The sorter's direct frame falls from 48
to 40 bytes.

## Refuse allocation during a read-only seek

Adapted from paps-g `b483124c` (AUD-MCU-015). A nonempty file with
no starting cluster has inconsistent allocation metadata. If a
positive seek on a read-only handle reaches the missing-chain
branch, FatFs now returns `FR_INT_ERR` before allocating. The
existing abort path records the error on the handle and leaves the
filesystem through its normal cleanup. A writable handle can still
allocate its first cluster. Empty reads and zero-offset seeks retain
their existing behavior.
[FatFs `f_lseek` behavior and access modes](https://elm-chan.org/fsw/ff/doc/lseek.html),
[Microsoft exFAT §§6.3.5–6.3.6, FirstCluster and DataLength](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#635-firstcluster-field).

On FAT12, FAT16, FAT32 and exFAT, the exact parent allocates and
persists a cluster after a read-only seek on the malformed file.
The FAT32 fixture reads the allocated FAT entry directly because
FSInfo can retain the previous free count. The port refuses all
12 malformed seeks while preserving the allocation state and
neighboring file. Both revisions pass 16 valid controls covering
zero offsets, empty files, writable growth and reads from an
allocated chain.

```sh
make -C tests check-fatfs
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-fatfs
```

Set `SAROO_FATFS_SOURCE` to the parent's `ff.c` and select
`-k readonly_seek` in `test_fatfs_seek.py` to reproduce the
comparison. Both port profiles pass all 24 FatFs methods. The
GNU MCU App links 63,816 bytes, up 16, with 1,720 ITCM bytes
free. Static DTCM and all 739 direct compiler frames match the
parent. The filesystem tests use generated host media.

## Reduce padding in FatFs and CD state

Adapted from paps-g `b483124c` (AUD-RES-007). Move the 64-bit
`FFOBJID.objsize` to the naturally aligned offset after the
eight-byte prefix. `FFOBJID` shrinks from 48 to 40 bytes, and
`FIL` and `DIR` each shrink from 80 to 72. Move the subcode file
object behind the fixed CD tables, which brings the ordinary
playback controls closer to the start of `CDBLOCK`.
The linked ARM layout measures `CDBLOCK` at 8,160 bytes, down
eight. `play_type` moves from byte 196 to 116 and `play_wait`
from 200 to 120. Member widths, capacities and on-disk encodings
are preserved.
[Arm AAPCS32 2025Q1 §§5.1 and 5.3.1, fundamental types and aggregate alignment](https://github.com/ARM-software/abi-aa/blob/2025Q1/aapcs32/aapcs32.rst#aggregates),
[Original SAROO FatFs header](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/FatFS/ff.h).

Rebuilding the full public GNU MCU App saves 112 load bytes,
from 63,816 to 63,704, and leaves 1,832 bytes of ITCM headroom.
Static DTCM drops from 60,200 to 59,368 bytes. The 832-byte saving
comes from 100 track file objects, two save file objects, the
cover file and the CD state. Twenty-two direct compiler frames
shrink by 8 to 24 bytes each. The other 717 frames retain their
sizes. These are individual frames and static allocations.
Peak stack use and physical timing remain unmeasured.

```sh
make -C tests check-cdc-command-buffer check-fatfs check-mcu-save
SAROO_FIRMWARE_SAVE_PROFILE=release SAROO_CDC_PROFILE=release \
  make -C tests check-cdc-command-buffer check-fatfs check-mcu-save
```

Both host profiles pass 351 methods, including subcode, CDC
commands, wide seeks and save ownership. The exact parent also
passes the 217 CD methods. The GNU App rebuild uses the new
headers for all consumers, and all 13 enabled BootLoader C
units compile with the original BootLoader defines and RTE.
Use the matching compiled objects and ELF for the new in-memory
layout.
