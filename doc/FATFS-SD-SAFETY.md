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
