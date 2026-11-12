# Startup, CD, and storage audit

## Validate the complete menu file

Adapted from paps-g `7f77b9d5` (AUD-MCU-002). The stock startup loader
validates the full `FSIZE_t` before converting the size to a read count.
The existing SDRAM reservation between `FIRM_ADDR` and `IMGINFO_ADDR`
allows 524,288 bytes. The lower bound is eight bytes. Out-of-range
files are closed before any payload read. A read error or a short read
also stops startup after closing the file, before save setup or menu
service publication. A successful read retains the existing handling
of the read-only close result.

The old signed 32-bit conversion can accept the low part of a larger
exFAT length. [FatFs R0.15 configuration, FF_FS_EXFAT](https://elm-chan.org/fsw/ff/doc/config.html#fs_exfat)
describes the 64-bit exFAT size option. The
[FatFs f_read contract](https://elm-chan.org/fsw/ff/doc/read.html)
requires checking the returned byte count even on success.
The [v0.8 loader](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_main.c)
and [address map](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/cdc.h)
define the startup path and the reservation.

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_menu_load.py -v
SAROO_TEST_CFLAGS=-O2 python3 -B -m unittest discover -s tests -p test_mcu_save_menu_load.py -v
```

The fixture executes the stock read block with the real size types and
limits. The parent fails 21 extent cases and nine incomplete reads.
All 43 cases pass under sanitizers and at O2, including valid boundary
lengths, missing files and the established close behavior. The GNU
MCU App links 63,752 load bytes, with 1,784 bytes of ITCM space and
unchanged static DTCM. The checks use a filesystem seam; physical
startup and real media are unqualified.

## Publish R–W subcode with accepted sectors

Adapted from paps-g `49b2bce5` (AUD-MCU-007/008). The disk task now
publishes four 24-byte packs after audio or data admission. A full
buffer leaves the queue unchanged so the next attempt cannot repeat
the packs. A deliberate data discard still advances playback. Failed
directory delivery retires without publishing; successful delivery
publishes before the directory handler can change the read position.

The disk task writes the spare slot before updating the cursors.
It saves and restores PRIMASK around each cursor update and the
track-change reset. The higher priority command task copies to its
FIFO without yielding. Capacity remains 63 packs, with the oldest
pack dropped on overflow. The original early Q report remains, and
the publisher raises SCDQ again when the packs become available so a
host that acknowledged the early report can still find the final
sector's packs.

[Sega CD Communication Interface ST-162-062094, §4.3, Fig. 4.4,
printed pp. 39–40](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=27)
describes Q timing, R–W buffering and WAIT on an empty queue.
[§7.2.5(2), printed p. 62](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=50)
specifies the pack size.
[Arm Cortex-M7 Generic User Guide DUI0646C, Priority Mask Register,
printed p. 2-8](https://developer.arm.com/documentation/dui0646/c)
defines the mask used to exclude task preemption here. SAROO's ring
capacity and the extra readiness hint are implementation choices.

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
```

The parent passes normal admission and fails five targeted methods.
The port passes all 223 methods in both runs. The new fixture varies
all ring positions and schedules a consumer at a producer cursor load
under the modeled mask. Linked Arm code keeps the pack copy and HIRQ
write outside the mask. The GNU MCU App links 63,800 load bytes, with
1,736 bytes of ITCM space and unchanged static DTCM. The host model
does not qualify scheduler latency or playback on the Saturn.

## Recover directory updates after storage errors

Adapted from paps-g `18ed08a6` (AUD-MCU-023/024/025/026) and its
`b483124c` directory initialization prerequisite. The original code
can link an uncleared directory cluster, leave an exFAT directory
length behind its chain, or leave part of a new name visible after
an I/O error. The port initializes a free directory cluster before
allocation and finishes each exFAT extension before starting another.

One record per mounted filesystem retains unfinished allocation,
parent header writes, or name cleanup. New names record each affected
slot before modifying it. Cleanup clears the InUse bit so later
entries in reused space stay visible. mkdir reserves the name before
allocating a child and releases a failed child's cluster after clearing
its name. Rename either retires an incomplete destination or finishes
retiring the old name after synchronizing the destination; it retains
the file data. Pending work retries through the existing filesystem
owner. A failed recovery leaves the mount registered. The port keeps
the existing read-only seek and retryable file synchronization rules.

[Microsoft exFAT §6.2.1, EntryType](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#621-entrytype-field)
distinguishes unused entries from the end-of-directory marker.
[§7.1, Allocation Bitmap](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#71-allocation-bitmap-directory-entry)
and [§7.6, Stream Extension](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#76-stream-extension-directory-entry)
describe allocation state and directory extent metadata.
[§8.1, Recommended Write Ordering](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#81-recommended-write-ordering)
gives the metadata ordering context. The recovery record lives in RAM.
The [FatFs f_mount contract](https://elm-chan.org/fsw/ff/doc/mount.html)
defines the registered filesystem work area.

```sh
make -C tests check-fatfs
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-fatfs
```

The exact parent fails 78 of its 214 directory cases, including the
one-sector cluster control for a long name that needs two extensions.
The port passes all 284 directory cases in both runs. It sweeps read,
partial write and sync failures, checks FAT12/16/32 directory clears,
and verifies allocation, live entries, data and retry across a
remount. It also models an unavailable volume with a pending update.
All 30 filesystem and Unicode methods pass in each run. The GNU MCU
App links 64,808 load bytes, with 728 bytes of ITCM space and 59,456
static DTCM bytes. All 13 enabled BootLoader C units compile. Recovery
requires the same mounted medium and power to stay on; the tests do
not qualify power-loss recovery, card replacement, or physical SD I/O.

## Validate FAT geometry before mount

Adapted from paps-g `8da2769d` (AUD-MCU-009/029). In classic FAT the
reserved sectors, all FAT copies and the fixed root directory are
counted at full width before calculating the first data sector.
The volume must leave space for data and its last LBA must fit the
configured address type. For exFAT the FAT must start after both boot
regions and cover every cluster entry, including the two reserved
entries. The existing FAT-to-heap and bitmap backing checks remain.

Microsoft's *FAT: General Overview of On-Disk Format*, v1.03,
“FAT Data Structure” and “FAT Type Determination,” printed pp. 13–15,
defines the per-copy FAT count and the full sum used to locate the data
region. [Microsoft exFAT §3.1.6, FatOffset](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#316-fatoffset-field)
and [§3.1.7, FatLength](https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification#317-fatlength-field)
give the start and minimum length for the exFAT table.

```sh
make -C tests check-fatfs
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-fatfs
```

The exact parent mounts five of the malformed geometries. The port
rejects them without data-area I/O, still refuses an overlapping
FAT and heap, and passes all five valid controls. Controls include the
last representable classic FAT LBA, one and two FATs, and allocation
with an exact-size exFAT table. All 32 filesystem and Unicode methods
pass in both profiles. The GNU MCU App links 64,872 load bytes,
with 664 bytes of ITCM space and unchanged static DTCM. These checks
cover declared filesystem geometry; physical media capacity and I/O
remain outside this test.

## Remove unused CD position history

Adapted from paps-g `1206dfc8` (AUD-RES-012). The only read of
`old_fad` was in an excluded distance-based delay experiment.
Remove that code, the private field, and its writes during seek,
file setup, sector advance and repeat. Active FAD, range and repeat
state, configured seek delay and pause handling are unchanged.

[Sega CD Communication Interface ST-162-062094, §4.2,
printed pp. 37–38](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=25)
defines the drive reports, range, repeat and full-buffer pause.
[§8.2.2 Nos. 2.1–2.2, printed pp. 82–83](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=70)
describes PLAY and SEEK. These behaviors depend on the live drive
state; the removed field was private to the MCU.

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-cdc-command-buffer
```

The exact parent and the port pass all 223 CD/CUE methods, including
the retained pause, repeat, seek timing and interleaved file controls.
Both port profiles pass. The linked `CDBLOCK` shrinks from 8,160 to
8,152 bytes. The GNU MCU App saves 64 load bytes, leaving 728 bytes
of ITCM space, and saves eight static DTCM bytes. Physical timing
and stack high-water usage remain unmeasured.

## Simplify CD partition updates

Adapted from paps-g `fb924aac` (AUD-RES-011/004). The partition
maintained a byte total that had no reader. Sector-count replies use
the list count, and actual-size and transfer replies use the selected
sector lengths. Drop the unused arithmetic and reuse the existing
append helper for filtered sectors. The outer saved interrupt mask
publishes the last destination and its new list entry together. The
helper restores the mask it received, leaving the outer owner to
restore interrupts.

[Sega CD Communication Interface ST-162-062094, §5.3.2 and
Fig. 5.5, printed p. 44](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=32)
defines partition ordering, sector units and the last store
destination. [§8.2.6 Nos. 6.2–6.4, printed pp. 92–93](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=80)
distinguishes sector-count and actual-size replies.
[Arm DUI0646C, Priority Mask Register, printed p. 2-8](https://developer.arm.com/documentation/dui0646/c)
defines the mask preserved by both callers.

The exact parent and the port pass all 224 CD/CUE methods. The port
passes under sanitizers and at O2. The new control observes list and
destination state at every unmask for all 24 partitions, including
empty and populated lists and an already-masked caller. Existing
controls cover GET, GETDEL, PUT, COPY, MOVE, actual-size replies and
the FIFO payload. Run `make -C tests check-cdc-command-buffer` in
both profiles as above. The old directory reproduction uses the
header from its historical source commit and still reproduces six
defects and one valid control.

The GNU MCU App saves 128 load bytes to 64,680, leaving 856 bytes of
ITCM space. It retains the 16-byte partition stride. A compact layout
saves 96 static DTCM bytes but costs 80 load bytes. A separate locked
helper avoids the nested mask operations at a cost of 16 load bytes.
The selected form leaves the most image space. Physical interrupt
latency and throughput remain unmeasured.
