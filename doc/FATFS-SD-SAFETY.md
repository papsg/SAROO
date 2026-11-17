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

## AUD-MCU-014 — Reject non-shortest UTF-8 filenames

FatFs R0.15 patch 1 accepts some three- and four-byte encodings of
characters that have a shorter UTF-8 form. On both FAT32 and exFAT the
parent accepts four such names through `f_open`. The path decoder now
rejects those forms before constructing a name. Existing checks for
bad continuation bytes, surrogates and values above U+10FFFF remain in
place. This targets the public `FF_LFN_UNICODE=2` path; FatFs string I/O
is disabled. Adapted from paps-g `6e52b09c`; ChaN's notice is retained.

[RFC 3629 §§3–4](https://www.rfc-editor.org/rfc/rfc3629.html#section-3)
specifies a single encoding length per range and the allowed second
bytes after `E0` and `F0`. See also the [original FatFs source][original-fatfs]
and [FatFs filename configuration](https://elm-chan.org/fsw/ff/doc/config.html#lfn_unicode).

Run `make -C tests check-fatfs` and repeat with
`SAROO_FIRMWARE_SAVE_PROFILE=release` for the O2 build. For the parent
reproduction set `SAROO_FATFS_DIR` to its `Firm_MCU/FatFS` directory and
run `python3 -B -m unittest discover -s tests -p test_fatfs_seek.py -k utf8 -v`.
All 35 FatFs methods pass in both profiles. The UTF-8 cases include
eight canonical names per filesystem that survive a remount, the four
overlong forms and eight other malformed names per filesystem. Every
rejection returns `FR_INVALID_NAME` with no disk writes. The GNU App
adds 32 load bytes (64,712 used, 824 free), with unchanged static DTCM
and compiler direct stack frames. This verifies the host media fixture
and the GNU build; no physical SD or game validation was performed.

## Reject SD card status errors and failed recovery STOP

MCU-SD-007, adapted from paps-g `3f0e15ae`. The controller could report
completion even when the card rejected an address, write or other
operation. The driver now rejects the R1/R1b error and locked-card
bits and the three R6 error bits before publishing the response. The
R7 interface condition, R3 OCR and long responses retain their
separate formats. Recovery CMD12 requires a valid controller
completion after the command and data engines and busy signal settle.
The existing retry can reset the controller once and try STOP again.
A second failure revokes sector access until reidentification.
The recovery check does not treat retained card-status bits from the
failed transfer as fresh STOP transport errors.

[SD Physical Layer Simplified Specification 3.01, §4.9, printed pp. 71–73](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=83),
[§4.10.1, Tables 4-36 and 4-37, printed pp. 75–77](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=87)
and [STM32 RM0433 Rev 8, §§55.10.4 and 55.10.11, printed pp. 2438–2440 and 2445–2447][rm0433]
define the response formats and controller status.

Run `make -C tests check-mcu-diskio` and repeat with
`SAROO_SD_PROFILE=release SAROO_FIRMWARE_SAVE_PROFILE=release`.
To reproduce the failures, point `SAROO_SD_SOURCE` at the parent
`sdio_h7.c` and select `-k card_errors` in `test_sd_safety.py` and
`-k requires_valid_controller` in `test_sd_transfer.py`. The parent
accepts all 179 injected card-error replies and misses the recovery
retry after STOP failure. Both corrected 40-method profiles pass.
The GNU App links 65,448 bytes, with 88 bytes of ITCM space and
unchanged 50,768-byte DTCM data and BSS span. Physical card recovery,
save durability and the original MDK build remain unqualified.

## Complete pending SD programming before sync and reads

MCU-SD-008, adapted from paps-g `3b44556a` and its production-path
follow-up `5c15670e`. The shared block submission marks a write
pending before the data command. FatFs `CTRL_SYNC` holds the disk
semaphore and polls CMD13 until the card reports transfer state and
ready for data. Reads take the same path when a write is pending.
A failed write can retry only after a successful check. Status
errors, unexpected states or an expired five-second polling budget
return failure and keep the pending marker for retry. A command
already in progress can finish at the boundary. Card reidentification
starts a new command session and clears the marker.

[SD Physical Layer Simplified Specification 3.01, §§4.3.4 and 4.6.2.2, printed pp. 30–31 and 56–57](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=43),
[STM32 RM0433 Rev 8, §55.10.11, printed pp. 2445–2447][rm0433]
and the [FatFs `CTRL_SYNC` contract](https://elm-chan.org/fsw/ff/doc/dioctl.html)
distinguish end of data transfer from completion at the medium.

Run `make -C tests check-mcu-diskio check-fatfs` and repeat the
disk tests with `SAROO_SD_PROFILE=release SAROO_FIRMWARE_SAVE_PROFILE=release`.
The parent launches a read without waiting and reports sync success
without acquiring the disk semaphore. The corrected fixture covers
direct and bounced writes, receive and programming states, timeout
across tick wrap, a later successful sync and failed-write retry.
Both storage profiles pass 44 methods.

The first port exceeded the 64 KiB ITCM limit by 144 bytes. The
public port drops verbose startup response and capacity prints and
the CSD arithmetic used only by those prints. The command sequence,
CID checks, addressing and error logs are preserved. The resulting
GNU App uses 65,036 load bytes, with 500 bytes free and 50,736 bytes
of DTCM data and BSS span. Card programming is tested in a model.
Physical media timing, power-loss durability and MDK remain open.

## Resume a retained filesystem after card removal

MCU-SD-R09, adapted from paps-g `de274082` and its filesystem
test `371f5369`. Once PA15 reports removal, sector I/O and write
sync remain unavailable until the driver completes identification
and matches the retained 16-byte CID. The disc task samples PA15
at the menu too. The drive's semaphore continues to serialize
reidentification and transfer. It can exist while the drive is
uninitialized.

FatFs retries identification before touching a retained mount or
file object. A missing or different card leaves the dirty file,
directory recovery state and mount ID intact. The same card can
resume through the original objects. Mounting a different card
requires a new MCU/filesystem session.

[SD Physical Layer Simplified Specification 3.01, §4.2 and §5.2, printed pp. 19–22 and 93](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=31),
[FatFs drive status](https://elm-chan.org/fsw/ff/doc/dstat.html)
and [drive initialization](https://elm-chan.org/fsw/ff/doc/dinit.html)
describe card identity, relative addressing and readiness.

Run `make -C tests check-mcu-diskio check-fatfs check-mcu-save`.
The parent fails the injected removal, drive-status and open-file
retention tests. The corrected tests cover a removal sampled during
identification, write sync and multi-block pre-erase, full-CID
refusal and dirty data readback after remount on FAT32 and exFAT.
The GNU App uses 65,184 load bytes, with 352 bytes of ITCM free
and a 50,736-byte DTCM data and BSS span. Removal and reinsertion
entirely between samples, cloned CIDs, interrupted physical writes
and MDK remain unqualified.

## Check the SD interface-condition echo before initialization

MCU-SD-009, adapted from paps-g `8e5ea8ce`. A completed CMD8 must
echo both the requested voltage code and the eight-bit pattern.
Reject a mismatch before issuing CMD55 or ACMD41. The reserved
upper response bits are outside the echo comparison.

[SD Physical Layer Simplified Specification 3.01, §4.3.13, printed p. 51](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=63)
defines the request and R7 echo.

Run `make -C tests check-mcu-diskio`. To reproduce the defect,
point `SAROO_SD_SOURCE` at the parent's driver and select
`interface_echo` in `test_sd_safety.py`. The parent accepts six
mismatches. The corrected build also accepts both valid echoes,
including the control with a reserved upper bit set. Both
48-method disk profiles pass. The GNU App uses 65,424 load bytes,
with 112 bytes free. Physical negotiation and MDK remain unqualified.

## Keep legacy SD cards on byte addressing

MCU-SD-010, adapted from paps-g `76c94c6c`. Interpret the ready
ACMD41 capacity bit only after a successful CMD8. A card that
skipped CMD8 confirmation continues to use byte addresses even
if the response has that bit set.

[SD Physical Layer Simplified Specification 3.01, §4.2.3, printed p. 21](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=33)
defines when the capacity bit is valid.

Run `make -C tests check-mcu-diskio`. The
`capacity_flag_requires` test in `test_sd_safety.py` checks
both response values with and without CMD8 confirmation. With
`SAROO_SD_SOURCE` set to the parent driver, the legacy case with
the capacity bit set incorrectly selects block addressing. Both
corrected 49-method disk profiles pass, including standard- and
high-capacity controls. The GNU App uses 65,436 load bytes, with
100 bytes free. Physical addressing and MDK remain unqualified.

## Require acknowledgment of SD application commands

MCU-SD-011, adapted from paps-g `56e9f763`. Check the APP_CMD
status bit in the R1 replies to CMD55, ACMD6 and ACMD23.
A missing acknowledgment uses the existing command cleanup.
Initialization then stops before issuing the unaccepted application
command or changing to four-bit mode. Multi-block write setup stops
before DMA starts. ACMD41 carries R3 and has no APP_CMD bit.

[SD Physical Layer Simplified Specification 3.01, §4.3.9.1, printed p. 40](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=52)
defines the acknowledgment in the prefix and application response.

Run `make -C tests check-mcu-diskio`. The parent accepts replies
without the bit at the command, initialization and write boundaries.
Both 52-method profiles pass, including the acknowledged controls
and the ACMD41 R3 response. The GNU App uses 65,464 load bytes
and leaves 72 bytes free. Physical card behavior and MDK remain
unqualified.

## Reserve legacy CMD8 fallback for a controller response timeout

MCU-SD-012, adapted from paps-g `b25cbc7e`. The legacy ACMD41
sequence is allowed when the only controller error is CMD8's
response timeout. Ignore unrelated status bits. Propagate CRC,
data and host semaphore errors before capacity negotiation.

[SD Physical Layer Simplified Specification 3.01, §4.2.3 and figure 4.1, printed pp. 21–22](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/kinetis/28816/1/Simplified_Physical_Layer_Spec_3.01.pdf#page=33)
defines the no-response branch. [STM32 RM0433 Rev 8, §55.10.11, printed p. 2447][rm0433]
defines separate timeout and CRC status flags.

Run `make -C tests check-mcu-diskio`. The parent accepts nine
injected command, data and host-wait failures as legacy startup.
The two response-timeout controls remain valid. Both corrected
53-method profiles pass. The GNU App uses 65,484 load bytes and
leaves 52 bytes free. Physical card timing and MDK remain unqualified.

## Honor the selected FAT32 copy

FAT32 volumes may disable mirroring and select one allocation table. The
inherited mount always read from FAT0 and wrote both tables when two were
present. That could follow stale cluster links or change the inactive copy.
Mount now validates the selector against the physical copy count. It uses
the selected table for cluster links, allocation and free-space scans, and
updates only that table. Mirrored volumes still use FAT0 and keep the existing
second-write failure and retry path.

Adapted from paps-g `a3ca5238` (SSR-011). Microsoft *FAT: General Overview
of On-Disk Format*, version 1.03 (6 December 2000), p. 12, defines
`BPB_ExtFlags` at byte 40, including the active index and mirroring bit.
The public port uses the physical copy count to validate the volume geometry
before reusing the existing FatFs field as the number of write targets.
This preserves the filesystem object layout and other supported formats.
The pinned GNU App link uses all 65,536 ITCM load bytes.

Run `make -C tests check-fatfs`. Generated FAT32 images cover active FAT0
and FAT1, stale inactive chains, free-space scans, exact appended data,
out-of-range selectors, single-table volumes and remounting the same
filesystem object with mirroring restored. They check every inactive FAT
sector for writes. Existing mirrored-write retry and FAT12/16/exFAT
controls also run. These tests and the GNU target link do not qualify
physical media or game behavior.

## Use FAT32 in the MCU application

The App now builds FatFs for FAT32 only. It recognizes partitioned and
unpartitioned FAT32 from the boot sector and cluster geometry, including cards
without the optional filesystem type string. It refuses FAT12, FAT16 and exFAT
without writing to those volumes. Long UTF-8 names, FAT32 active copy and
mirroring rules, and the full 4 GiB minus one byte file limit are retained.
The BootLoader still selects the original multi-format configuration. See
the [card migration instructions](../README_EN.md#images-and-configuration).

Adapted from paps-g `205e0f61` and `495794eb` (MCU-FS-005/006). A failed
allocation retains the previous tail and new cluster in the mounted FAT32
volume until the rollback can be synchronized. A later allocation failure
during a write-mode seek leaves the successfully grown prefix eligible for
sync or close. Both paths work when exFAT is compiled out. This recovery
state lives in RAM; it is not an on-disk journal. The public adaptation also
checks the CD header's track offset and sector prefix before adding them, so
the narrower file position cannot wrap to another part of the image.

Microsoft *FAT: General Overview of On-Disk Format*, version 1.03
(6 December 2000), printed pp. 10–12 and 14–15, defines the FAT32 BPB,
`BPB_ExtFlags`, and the cluster calculation used to distinguish FAT types.
The port retains the FatFs driver's existing cluster-count limits.
See also [FatFs f_lseek](https://elm-chan.org/fsw/ff/doc/lseek.html) and
[FatFs f_close][close].

Run `make -C tests check-fatfs check-mcu-save check-file-transfer check-mcu-diskio`.
The App profile verifies generated media, directory growth, an unlabelled
FAT32 volume, the last byte of a maximum-size file and one- and two-FAT
recovery under injected I/O failures. Generic-format fixtures remain available
for comparison. The pinned GNU App uses 60,512 of 65,536 ITCM load bytes
and 46,688 bytes of static DTCM span, down from 65,536 and 50,744 bytes.
A separate GNU `BOOT` link retains the multi-format configuration and the
same 29,112-byte image size; the linked bytes differ in `mount_volume`.
Host tests and GNU links do not qualify physical media, gameplay or MDK output.

## SSR-106: do not use a persisted free-count hint to deny allocation

A valid FAT32 FSInfo sector can contain a stale but in-range free count. The
public parent loads it into the mounted authoritative count; zero prevents
allocation, and one permits only one cluster. On generated media with 128,988
actually free clusters, these hints allow 0/512 of a 1,024-byte write. Actual
public save-startup C then returns `FR_DISK_ERR` when its header write is short.
It correctly refuses readiness, but cannot create the missing archive despite
available space. Accurate/unknown counts and a genuinely full FAT are controls.

Microsoft *FAT: General Overview of On-Disk Format*, v1.03 (6 December 2000),
**FAT32 FSInfo Sector Structure and Backup Boot Sector**, printed p.21,
defines `FSI_Free_Count` at byte 488 as last-known and potentially inaccurate
even when in range. `FSI_Nxt_Free` at 492 is a separately validated search hint.
The locally preserved primary PDF SHA-256 is
`1a2c7e13a7b246a5581ab8cdf5c79c016bedf25be7a3c6739eede560423e3354`.
The version-matched [FatFs R0.15 archive](https://elm-chan.org/fsw/ff/arc/ff15.zip),
`documents/doc/config.html`, **FF_FS_NOFSINFO**, defines bit0 as distrust of the
free count and bit1 as distrust of the next-cluster hint. These establish the
input's limitations and the configuration choice, not a unique required
implementation or a Sega-specific MCU rule. No new online research was used.

Adapted from paps-g `8f8b63c5`/SSR-106, with papsg's public-layout tests and
independent raw-volume checks. Set only `FF_FS_NOFSINFO` bit0. On mount, free
count stays unknown; ordinary allocation searches real FAT entries without
first recounting the whole volume. Bit1 stays clear: the allocator retains
and range-checks the search hint at use. Explicit `f_getfree` scans once, then
reuses its mounted count; existing allocation/freeing updates maintain a known
count and leave an unknown count unknown. Persisting the unknown marker is
valid. No new buffer, cache, scan algorithm, retry or API call is added.
ChaN's notices and the selected/generic providers remain intact. No original
tpunix issue/PR is identified for this independent finding. Private diagnostic
configuration and runtime diagnostic code are outside this port.

Reproduce using public files and a C compiler with ASan/UBSan:

```sh
PYTHONPATH=tests python3 -B -m unittest -v test_fatfs_fsinfo
SAROO_FIRMWARE_SAVE_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_fatfs_fsinfo
```

The selected FAT32 and retained generic/64-bit providers pass 12 methods over
38 API cases plus two image preparations in each build mode. ASan/UBSan and O2 both
pass; the generic provider is exercised on FAT32, not exFAT. Independent image
construction supplies real FAT32 geometry, two tables, backup boot sectors and
valid FSInfo signatures. Cases change only the primary/backup count fields or
the explicitly selected next hint; the full-volume controls mark free entries
as unavailable. Prepared media are byte-identical across current/inverse and
selected/generic variants. Images, input copies/hashes, results and failures
remain under `tests/build`.

To reproduce the inverse, copy `Firm_MCU/FatFS`, change only
`FF_FS_NOFSINFO` bit0 back to zero and set `SAROO_FATFS_DIR` to that copy before
running the first command. It produces 26 failed subcases, with 12 other API
controls and both preparations valid. These are related manifestations of one
cause, not 26 findings. Stale zero/one allocation is short, explicit queries
return stale counts, the refused-query test cannot observe a FAT read, and
save startup fails. Its partial/empty files are retained in the evidence.

Selected cases complete all 1,024 bytes and remount/read the exact payload.
Six actual save-startup cases create both 65,536-/8,388,608-byte archives,
validate their headers and reopen after remount without new writes. Query
read refusal preserves the output sentinel and succeeds on retry. Six search
hint values per provider cover 0, 1, occupied 2, last-valid, first-invalid and
unknown; all allocate safely despite a stale count of one. Independent raw
audits of every resulting volume check table equality, unique live cluster
ownership, chain/size agreement, neighboring/target payloads and archive header
bytes. They never require persisted FSInfo counts to equal actual free space.

The I/O tradeoff is explicit on the 64 MiB fixture: ordinary 1,024-byte allocation
needs one FAT read, including accurate/unknown controls. A first explicit query
reads 1,008 FAT sectors and the next adds none. With a truly full FAT and an
accurate stored zero, the inverse safely refuses with zero allocation reads;
the selected path uses 1,009 on its first attempt and 1,008 on the immediate
repeat, whose initial FAT window is already cached. Unknown-count failures can
repeat a search. High-count full-FAT controls also refuse safely. These are
fixture I/O counts, not SD timing or universal bounds. `f_getfree` is absent
from both standard App links; no automatic startup recount was introduced.

All 28 relevant existing startup, contiguous-allocation, FAT32 growth/profile,
active-copy/mirroring and FSInfo-error regression methods pass. The optional
contiguous-allocation oracle already explicitly requests a current count before
checking exact bookkeeping; its raw ownership/payload assertions are retained.
Both fresh 166-vector standard App links pass, with only `ffconf.h` changed in
all 70 public MCU input pins. LTO saves 12 load bytes to 59,524 (6,012 free);
non-LTO saves 8 to 63,784 (1,752 free). Data/BSS stay 200/50,696 and 204/50,688;
static DTCM span 50,896 and stack-address gap 80,176 remain. Direct frames stay 64
for mount and 32 for chain creation in both profiles; callee/IRQ peaks are not
measured. Menu/FPGA source is unchanged; the preceding paired menu build remains
current by its complete source pins, without claiming a fresh menu build here.

These are actual filesystem/save C and GNU target links over synchronous host
block/shared-address/mutex adapters. MCU instructions, physical SD performance,
durability, power loss, concurrent scheduling, full game launch and Saturn/game
saves remain unqualified. Existing short/damaged archives are not repaired by
this change; their established refusal policy remains intact.

## SSR-107: retain a failed child directory until cleanup is synchronized

A failed FAT directory clear can leave its allocation flushed but unpublished.
The parent `91bd560` clears the allocation only in its dirty FAT window;
immediate unmount discards that rollback and leaves a lost cluster. An
intervening lookup accidentally flushes it and hides the defect. This is a
retained/shared-library cleanup defect, with no identified normal game/save
failure: `f_mkdir` and `mkdir_p` are absent from both standard GNU App links.

Adapted from paps-g `99d0b565` and retained-format qualification `4842feaa`
(SSR-107), with papsg's public recovery integration, fixtures and independent
raw-volume audits. The public provider already tracks parent-extension
rollback; it must not reuse that state for a separately allocated child.
One new DWORD owns the child. Bit31 records completed count credit, safe for
FAT12/16/32 cluster numbers. Cleanup clears the FAT entry, credits a known
count once, leaves an unknown count unknown, and retains ownership until
`sync_fs` succeeds. Existing mount, object-validation and unmount/replacement
recovery gates retry it before parent recovery. Failures retain both owners
and refuse new operations. Empty state adds a check without device I/O.
ChaN's notices, feature selections and existing exFAT recovery stay intact.
No new normal mkdir caller, diagnostic provider or allocation buffer is added.

This is **project cleanup policy**, not a promise required by upstream FatFs
or a Sega transaction rule. The locally preserved [FatFs R0.15 archive](https://elm-chan.org/fsw/ff/arc/ff15.zip)
(SHA-256 `e0d76654d877e6c74be5ea3c395808794d495169514e98cbf6046168b8f4f070`)
contains `documents/doc/mount.html`, **Description**, which permits
unregistration with opt0 without physical I/O. Its `sync.html`, **Description**,
and `appnote.html`, **Critical Section**, describe interrupted-write damage
and risk reduction, not atomic directory transactions. The archive is the
versioned citation; these claims were checked locally, without online research.
No original tpunix issue/PR is identified for this independent finding.

A pending cleanup can therefore make this fork's opt0 unregister/replacement
perform I/O and fail. The caller must honor failure, keep the old FATFS object
alive, restore access to the same media and retry. Replacement storage stays
untouched on refusal. Volume-management calls require external serialization,
even in the reentrant configuration (R0.15 `appnote.html`, **Re-entrancy**).
There is no new force-discard policy or physical card-identity protection.

Run the two commands in [the public test instructions](../tests/README.md#failed-directory-creation-and-immediate-unmount).
Fourteen methods execute 72 cases per mode across selected FAT32/static LFN
and retained generic FAT12/16/32/stack LFN with a volume mutex. Both ASan/UBSan
and O2 pass. The exact parent fails all 60 fault cases and retains 12 valid
healthy/lookup controls. Those 60 final images have an independently identified
orphan each; corrected images have none. This is one cause, not 60 findings.

The cases exercise one/four-sector clusters, observable 256-byte partial clears,
known/unknown counts, FAT reads, both FAT writes and synchronization refusal.
Repeated unmount, allocation, existing-file read/close and replacement attempts
retain ownership; modeled not-ready/write-protect responses preserve state and
release the test mutex. Restored access permits close/unmount and remount.
The existing open-file not-ready return convention is preserved. Four cases
also force simultaneous child127 and uncounted parent-extension128 obligations
through actual allocation APIs, without fabricating recovery fields. Final raw
walks verify FAT-copy equality, packed FAT12 neighbors, unique live ownership,
chain sizes and exact neighboring/anchor payloads, without trusting FSInfo.
All eight healthy cases have identical parent/corrected read/write/sync counts;
these counts do not measure CPU cost or physical SD timing.

Both fresh 166-vector standard GNU links pass. LTO load grows 59,524→59,616
(5,920 free ITCM bytes); non-LTO grows 63,784→63,904 (1,632 free).
The native `sdfs` object grows 576→580 bytes. Alignment keeps LTO total BSS
50,696/static DTCM span50,896 unchanged; non-LTO BSS grows50,688→50,696,
span50,896→50,904 and stack-address gap80,176→80,168. Data stays200/204.
Direct recovery frame grows8→16 bytes; non-LTO child helper is16. Mount-volume,
chain creation, mount and retained mkdir direct frames stay64/32/40/112.
These compiler frames are not complete call-chain, IRQ or runtime stack peaks.
Only `ff.c`/`ff.h` differ in the matched 70-input MCU cohorts. All40 source
pins of the preceding paired menu build remain exact; no fresh menu build is
claimed. Menu/FPGA source and standard optional-feature settings are unchanged.
All37 focused existing directory/growth/expand/FSInfo/save-startup/common-pool
methods pass, including the actual native allocator check in local M7 QEMU.
The host builder's19 unit/two RTL tests, formatting and strict Clippy pass.
Its reusable preservation check matches all3,272 tracked private paths and
complete Git state; that host-only tool is outside firmware.

These tests use actual filesystem C over synchronous host block/status/mutex
adapters. They do not establish real-media durability, power-loss recovery,
card replacement, complete directory transactions after entry publication,
concurrent scheduling, repair of existing damage or Saturn/game acceptance.

## SSR-115: account for the current-directory terminator in TCHAR units

The retained R0.15 `f_getcwd` can use the complete supplied capacity for its
path and then write the final NUL past it. A zero-capacity root also underflows
the unsigned reverse-stack index. This needs no damaged media. It is an
optional library defect: standard `FF_FS_RPATH=1` excludes this API, which
requires 2. No normal game/save failure is established or new caller enabled.

Adapted from paps-g `097ad56b` and strengthened guard follow-up `6e9c9e63`, with
papsg's public fixtures and provider/native joins. Reject capacities below 2;
zero remains untouched and a one-unit buffer receives an empty string. Reserve
one TCHAR for NUL before path construction. Numeric and string-prefix admission
must use the remaining budget: two units for a numeric prefix, name length + 1
for either string style. Keeping the old gates after reservation would reject
valid exact fits. No bytes outside `f_getcwd` change. Standard configuration,
ChaN notices, allocation, filename decoding and other recovery stay intact.

The versioned [FatFs R0.15 archive](https://elm-chan.org/fsw/ff/arc/ff15.zip)
(identity recorded above) contains `documents/doc/getcwd.html`, **Arguments**,
**Return Values**, **Description**, and **QuickInfo**: length is in TCHARs,
prefixes depend on volume configuration, and this revision returns root for
exFAT. `rc.html`, **FR_NOT_ENOUGH_CORE**, identifies insufficient supplied
capacity; `config.html`, **FF_FS_RPATH**, **FF_LFN_UNICODE**, and
**FF_STR_VOLUME_ID**, define these retained profiles. Its function body exactly
matches the public parent. These primary files were checked locally, with no
online research or invented Sega buffer rule. No original tpunix issue/PR is
identified for this independent finding.

```sh
PYTHONPATH=tests python3 -B -m unittest -v test_fatfs_getcwd
SAROO_FIRMWARE_SAVE_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_fatfs_getcwd
# Optional standalone native objects (requires an existing local Arm toolchain):
SAROO_ARM_CC=/path/to/arm-none-eabi-gcc PYTHONPATH=tests \
  python3 -B -m unittest -v test_fatfs_getcwd
```

Each mode compiles 33 public-source profiles: selected FAT32 and retained generic
providers, four API encodings, single/numeric/DOS-string/Unix-string prefixes,
plus selected UTF-8 with its original static LFN storage. The other 32 profiles
use per-call LFN storage. Only copied configurations enable RPATH=2/formatting;
no diagnostic provider or private preparation framework is used. Host
file-backed disk/mutex endpoints support actual format, mkdir, chdir and getcwd
calls. Only volume 0 is mounted; declaring two volumes tests prefix construction,
not multi-device arbitration.

All 181 API processes and 1,150 buffer checks pass with ASan/UBSan and O2. Cases
cover root, short/nested/long ASCII paths and a non-ASCII BMP name in Unicode
profiles, larger/exact/path-only capacities, zero/one capacity, independent
canaries and actual short heap allocations. OEM uses supported ASCII names.
Guard changes are counted even when the return result is already wrong.
Sixteen generic exFAT cases preserve its documented root result below root.
Commands, sources, hashes, images and logs remain under `tests/build/fatfs-getcwd`;
default repeats use separate output directories.

The parent full matrix passes 130 controls and fails 51 cases: 33 zero-capacity
faults, nine ASan heap overflows and nine guarded wrong-success processes
with 45 explicit one-TCHAR guard overwrites. These are manifestations of one
capacity cause. Reproduce using the five FatFs inputs from `fa90658` in a
separate directory selected by `SAROO_FATFS_DIR`. A separate copied provider
that reserves NUL but retains the old prefix gates fails numeric/DOS/Unix exact
fits while all three larger-buffer controls pass. `SAROO_GETCWD_VARIANTS` can
select those three known profile names as a JSON array; unknown/duplicate
selections refuse.

Twelve standalone Cortex-M7 object builds compare before/after in six optional
profiles. Inspected instructions branch around zero/one-capacity writes and
filesystem admission correctly. Direct frames remain 920 bytes for selected
static-LFN UTF-8; per-call LFN OEM/UTF-8/UTF-16/UTF-32 frames remain
1,432/1,432/2,232/3,840, and generic UTF-8 remains 2,072. These are non-LTO direct
compiler frames, including the prologue reservation before the early guard,
not complete callee/IRQ peaks or proof a firmware thread can enable this API.
Host-tested and native providers/configurations match exactly.

Both fresh 166-vector standard GNU App links pass and their entire flat images
are byte-identical to matched parents: 59,616 LTO and 63,904 non-LTO. Resources,
loaded extent and BSS layout stay unchanged. Complete stripped ELFs and defined
symbol tables also match; unstripped source-checksum/DWARF metadata differs. All 70 public MCU inputs join with
only `ff.c` changed; all 40 inputs of the preceding menu build remain current.
The maintained builder now retains `qualification.bin`, checks its length
against the qualified load extent and records its hash, replacing repeated
manual image extraction. Rust unit/RTL tests, format, strict Clippy,
source/privacy/whitespace and private-preservation checks pass.

These are actual optional filesystem C, host block fixtures and target compile/
link checks, not physical storage, MCU/Saturn execution or game qualification.
Arbitrary I/O faults, every path/encoding, supplementary Unicode, other active
volume IDs and full task/interrupt stack peaks remain unqualified. Optional
API enablement still requires appropriate caller storage and stack.

## SSR-116: bound optional numeric output and reject finite exponent overflow

The retained floating formatter has two related defects. Its public owner is
`f_printf`'s 32-byte numeric array. Zero bypasses the old exponent/precision
admission and can overrun it. Separately, rounding `DBL_MAX` in exponent
notation can create infinity before exponent admission; `ilog10` then stops
converging and eventually overflows its loop counter. Standard
`FF_USE_STRFUNC=0` excludes this code. No shipping game failure is established.

Port of paps-g `1326a62c` plus final `31151513`, with papsg's independent public
fixtures, containment and source/native joins. Count actual integer/suffix,
separator and minus-sign characters, reserving NUL before zero/nonzero output.
Recheck extra integer digits after fixed rounding. Determine a finite exponent
before rounding and reject values above +99 immediately; retain the check
after rounding for carries across that boundary. Reuse existing digit scratch;
no array, state, task or disk call is added. All bytes outside `ftoa` stay exact.
Rounding, special-value spellings, padding and feature selections are retained.

The locally checked [FatFs R0.15 archive](https://elm-chan.org/fsw/ff/arc/ff15.zip)
(identity above), `documents/doc/printf.html`, **Description** (format-control directives),
defines precisions 0–99, default 6, e/E/f numeric strings through 31 characters,
signed overflow markers beyond that size or exponent +99, and NaN/infinity
spellings. `config.html`, **FF_USE_STRFUNC** and **FF_PRINT_FLOAT**, defines
plain/CRLF and dot/comma options. The parent's complete `ftoa` body matches
that versioned archive. No Sega rule or original tpunix issue/PR is asserted;
these are optional library contracts checked without online research.

```sh
PYTHONPATH=tests python3 -B -m unittest -v test_fatfs_float
SAROO_FIRMWARE_SAVE_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_fatfs_float
# Also compile the optional native functions with an existing local toolchain:
SAROO_ARM_CC=/path/to/arm-none-eabi-gcc PYTHONPATH=tests \
  python3 -B -m unittest -v test_fatfs_float
```

Eight public-source profiles cover selected FAT32/generic providers, dot/comma
and plain/CRLF output. Both sanitizer and O2 modes pass all 64 fresh API/helper
processes and 34,776 checks with empty stderr. Each profile sweeps 2,100 exact
small-integer cases across e/E/f and all precisions, observes the actual helper
with independent 32-byte guards, and closes/reopens/reads actual `f_printf`
files. Exact 31-character sign/notation fits, ordinary rounding, padding,
defaults, negative zero, NaN/infinities, newline conversion, largest finite
values, +99 admission and rounding across it are checked. Exact small-integer
strings are authored independently; controlled rounding examples also compare
with host libc. Complete actual FatFs executes over host disk/mutex endpoints.

Per parent profile, the direct sweep reports 148 guard violations and 158 content
mismatches; these sets overlap. Ten of 21 valid exact fits are falsely rejected.
All 106 ordinary controls pass. Across the full parent cohort, 32 public cases
hit ASan stack-buffer overflow. Six extreme-value processes reach UBSan loop-
counter overflow and two reach the two-second containment deadline. Entry
markers identify the helper; the runner kills/reaps each exact expired process
group, and subsequent PID checks confirm no timed-out child remains. These
are host observations, not firmware deadlines or physical CPU timing.

Reproduce with the five pre-correction FatFs inputs from `0b725cc` selected by
`SAROO_FATFS_DIR`. The bounded runner intentionally reports these failures.
A separate capacity-only copy passes all 2,100 guard/content checks and 21 exact
fits but still reaches the contained finite-value failure. A deliberately
stricter plus-sign reservation keeps guards intact and 106 ordinary controls
valid, yet rejects eight exact fits. Neither intermediate is published as the
fix. `SAROO_FLOAT_VARIANTS` selects known profile names for focused controls;
unknown/duplicate names refuse. Repeated defaults create separate evidence
folders. All sources/configs, commands, logs, images and containment records
remain under `tests/build/fatfs-float`.

Sixteen standalone M7 before/after objects retain 172-byte direct `f_printf`
frames in all eight profiles; `ftoa` is inlined there and emitted math helpers
have zero direct frames. This is non-LTO compile accounting, not execution or
complete callee/interrupt stack fit. Two adjacent UTF-8 current-directory
profiles pass 11 API processes/70 buffer checks. Both fresh 166-vector standard
App links retain exact flat bytes, data/BSS symbols and resources: 59,616 LTO
and 63,904 non-LTO loads. `ftoa`/`f_printf` remain absent. All 70 MCU input pins
join with only `ff.c` changed; all 40 preceding-menu pins remain current.
Standard emitted code/storage has no cost from this disabled API. Enabled
output still uses the original buffered writer; device I/O timing/costs were
not measured. Source/privacy/whitespace and private preservation checks pass.

This does not certify arbitrary-value rounding, every parser/encoding/fenv
case, I/O failures, enabled task-stack fit, physical CPU/SD/Saturn execution
or games. No diagnostic provider, USB integration, installed firmware change
or device operation is included. ChaN notices and earlier evidence remain.

## SSR-117: reject an invalid exFAT volume-label count before decoding

The optional `f_getlabel` reader used an on-disk exFAT `CharacterCount` as an
unbounded loop limit. A count above 11 makes it read reserved bytes or another
directory entry and can write past the API's documented caller buffer. This is
an optional malformed-media defect: standard `FF_USE_LABEL=0` and FAT32-only
builds cannot reach this branch. No normal game, installed firmware or physical
failure is established.

Port of paps-g `d063f6db`, adapted and independently reproduced in the public
layout. Reject counts above 11 with `FR_INT_ERR` before decoding or publishing a
label. The existing `LEAVE_FF` route releases a held volume grant. The guard
stays in `f_getlabel`, so serial-only requests bypass malformed labels and
`f_setlabel` can still repair one. The directory walker, valid decoding, FAT32
path and configuration are unchanged. No task, storage or I/O call is added.

The locally preserved [Microsoft exFAT specification, §7.3 Table 26 and
§7.3.2](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#73-volume-label-directory-entry)
assigns 22 bytes to the UTF-16 label and limits `CharacterCount` to 0–11; the
last eight bytes of the 32-byte entry are reserved. The locally checked
[FatFs R0.15 archive](https://elm-chan.org/fsw/ff/arc/ff15.zip) includes
`documents/doc/getlabel.html`, **Parameters** and **Return Values**, which give
minimum `TCHAR` capacities of 34 for UTF-8, 12 for UTF-16/UTF-32 and 23 for
OEM with exFAT, and include `FR_INT_ERR`. Its `config.html` defines the optional
label/exFAT/encoding settings. The vulnerable exFAT loop matches that release;
the public parent also retains earlier SAROO FAT32-only serial selection.
These are filesystem/API contracts, not Sega hardware rules. No original
tpunix issue or PR for this defect is identified.

```sh
PYTHONPATH=tests python3 -B -m unittest -v test_fatfs_label
SAROO_FIRMWARE_SAVE_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_fatfs_label
# Optional standalone Cortex-M7 objects with an existing local toolchain:
SAROO_ARM_CC=/path/to/arm-none-eabi-gcc PYTHONPATH=tests \
  python3 -B -m unittest -v test_fatfs_label
# Reproduce the three expected invalid-count failures against the public parent:
parent_dir=$(mktemp -d)
git archive c61ff31 Firm_MCU/FatFS | tar -x -C "$parent_dir"
SAROO_FATFS_DIR="$parent_dir/Firm_MCU/FatFS" \
  SAROO_LABEL_VARIANTS='["generic-u2","fat32-u2"]' \
  PYTHONPATH=tests python3 -B -m unittest -v test_fatfs_label
```

The public fixture uses the complete selected and generic FatFs providers,
real formatting/mount/label calls and file-backed block media. Only copied
test configurations enable label/formatting; generic copies enable exFAT and
re-entrancy. Four API encodings run in each provider with CP437 conversion.
Valid controls keep unused label words and reserved bytes zero; malformed
entries poison the reserved bytes. It tests every valid count 0–11, ASCII/BMP/
supplementary encodings, the first/final root-sector slots, and exact documented
heap capacities. Every invalid byte count 12–255 is exercised at both slots,
twice after remount. Reader operations make no disk writes; generic trials
check that the volume grant is released. Absent-label, null-output,
serial-only, repair and FAT32 remount controls remain valid.

The two complete host modes pass **32 API processes and 22,780 checks each**
under ASan/UBSan and optimized O2, with empty sanitizer stderr. The public
parent, using the same final fixture in a copied two-profile run, retains its
valid/repair/FAT32 controls but fails three invalid-count methods. Count 12/13
returns `FR_OK` and changes three/six bytes beyond a guarded 34-item UTF-8
buffer, including repeated reads. A real 34-byte allocation separately
triggers ASan heap-buffer-overflow in actual `put_utf`/`f_getlabel`.
`SAROO_FATFS_DIR` selects copied pre-correction provider inputs, and
`SAROO_LABEL_VARIANTS` accepts a JSON list of known profile names for a scoped
reproduction. Sources, hashes, commands, outputs and sparse media are retained
under `tests/build/fatfs-label`; repeated runs use fresh directories.

Eight optional Arm object builds before and after preserve direct `f_getlabel`
frames: generic OEM/UTF-8/UTF-16 128 bytes, generic UTF-32 120 bytes;
FAT32-only profiles 80–88 bytes. Generic function text grows 18/20/18/18
bytes across OEM/UTF-16/UTF-8/UTF-32, respectively; FAT32-only text is
unchanged. The generic UTF-8 disassembly loads the count, compares it with 11,
and routes rejection through `unlock_volume` before `put_utf`. These are direct
non-LTO object costs, not complete call-chain or task/interrupt stack peaks.

Both fresh 166-vector standard GNU App links succeed, retain their previous
complete flat-image bytes and resource accounting at 59,616 LTO and 63,904
non-LTO load bytes, and contain no `f_getlabel`/`f_setlabel` symbol. The 155
builder source pins differ only in `ff.c`. Adjacent focused getcwd and float
regressions pass 11 processes/70 checks and 16 processes/8,694 checks. No
diagnostic provider, installed firmware change or device operation is part of
this port. These host and target-build observations do not qualify physical
storage/Saturn timing, all code pages, malformed Unicode, arbitrary I/O faults
or games. Callers must still provide the documented output capacity.

## SSR-163: use each exFAT volume's up-case table

The optional exFAT reader previously used FatFs's global Unicode upper-case
conversion for file-name hashes, comparisons and creation. A valid volume can
carry a different mapping. On the public parent `be4b589`, a file named
`Ɐ.BIN` on a volume mapping `ɐ` to `Ɐ` opens by its stored name but an
open by `ɐ.BIN` returns `FR_NO_FILE`. The independent raw-media oracle verifies
both boot regions, allocation, table checksum, name hash and payload on that
input; the original image is unchanged by the lookup.

The correction adapts paps-g `f3714b9e319a8721325dc80521e43c090228e95a`
to the public tree. Mount validates the unique active up-case entry, bounded
cluster chain, allocation, full 65,536-code mapping, mandatory ASCII mappings
and checksum before publishing the volume. A sparse per-volume index is used
for name hashes and comparisons; failures propagate before directory entry
allocation. The shared filesystem window is restored after comparison. The
standard FAT32-only configuration remains excluded from this optional path.
The [Microsoft exFAT specification §§7.2, 7.2.2, 7.2.5 and 7.6.4](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
requires the table checksum before use, supports compressed and uncompressed
tables, and defines NameHash over the volume's up-cased name. The original
[FatFs configuration documentation](https://elm-chan.org/fsw/ff/doc/config.html)
identifies exFAT as an optional build feature. ChaN's source notices remain.

Run `python3 -B -m unittest discover -s tests -p test_fatfs_upcase.py -v`.
Eight corrected methods pass under ASan/UBSan with 1,140 actual compile/API
processes. They cover complete mapping vectors, read-only and concurrent
two-volume reads, create/rename/directory/unlink, malformed table refusal and
each injected mount/lookup/creation read failure. The baseline witness is
publicly reproducible by exporting `Firm_MCU/FatFS` from `be4b589` into a
temporary directory, setting `SAROO_FATFS_DIR` to that exported folder and
`SAROO_UPCASE_BASELINE=1`, then running the same test command. The baseline
method confirms the valid alias is missing while the stored-name control works.
The host fixture uses file-backed sectors and pthread grants.

Both standard GNU App builds pass and match the prior checkpoint's flat image
byte for byte: LTO SHA-256 `e4c577489865f7d6f22ece2f2d1d318b924b4a3497082792b5a771dc9c71e122`,
non-LTO SHA-256 `22f9e9e5c782d5f7dc65bbe6be39bdb857ed257fab4e0457b64772115b5bdabb`.
An isolated Cortex-M7 exFAT `ff.c` object grows from 16,453 to 18,509 text
bytes; its BSS is unchanged. In the selected host configuration, `FATFS`
grows from 688 to 1,720 bytes per volume. These are object and host-structure
measurements, not an optional linked firmware admission or complete stack and
timing qualification. Physical SD, Saturn and game behavior remain untested.

## SSR-164: validate the selected exFAT boot region before using it

On the public parent `556a980`, the optional exFAT path mounts an image whose
main and backup Boot Checksum sectors have both been altered. The valid control
also mounts, and the two images differ at only byte offsets 5,632 and 11,776.
The parent therefore uses volume geometry without verifying the selected main
region. [Microsoft's exFAT specification §§3.1 and 3.4](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification)
requires checksum validation before boot-region contents are used. Section 3.4
covers eleven sectors, excludes first-sector offsets 106, 107 and 112, and
requires the checksum to fill every four-byte word of sector eleven.

This port adapts paps-g `a817842e929820e5879a30c9103fb2c5f2a88e67`.
It validates the main boot region in the existing SFD, MBR and GPT discovery
paths before mount consumes the VBR parameters. The check uses configured
sector size, bounds the LBA and known partition span, checks every checksum
word and restores the VBR window. An invalid main region is refused even when
the backup is valid; backup recovery is outside the existing selection policy.
FAT32 and standard FAT32-only firmware remain on their previous path.

Run `python3 -B -m unittest discover -s tests -p test_fatfs_boot_checksum.py -v`.
Seven sanitizer-backed methods make 97 compile/API calls and check 3,840
individual checksum-word corruptions at 512, 1,024, 2,048 and 4,096-byte
sector sizes. They also cover covered bytes and exclusions, read errors and
remount, SFD/MBR/GPT bases, short extents, 32/64-bit LBA bounds, read-only
mounts and the retained main/backup policy. To reproduce the parent, export
`Firm_MCU/FatFS` from `556a980` into a temporary directory, set
`SAROO_BOOT_CHECKSUM_SOURCE` to that folder and
`SAROO_BOOT_CHECKSUM_BASELINE=1`, then run the same test command. The baseline
method verifies a damaged main and backup still mount and read `KEEP.BIN`.
The fixture supplies file-backed sectors and host mutexes; it does not model
power loss or every possible on-disk validity rule.

The existing exFAT geometry and bitmap-refusal tests now expect thirteen
reads: recognition plus the twelve-sector main boot validation. Their refusal
and zero-write checks remain; the FAT32 one-read control is unchanged.

Both standard GNU App builds pass and their LTO/non-LTO flat images remain
byte-identical to SSR-163. The isolated optional Cortex-M7 `ff.c` object grows
from 18,509 to 18,733 text bytes, with BSS unchanged. An optional linked
image, whole-call-chain stack and physical SD/Saturn timing are unqualified.
