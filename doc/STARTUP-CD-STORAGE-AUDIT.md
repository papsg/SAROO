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
