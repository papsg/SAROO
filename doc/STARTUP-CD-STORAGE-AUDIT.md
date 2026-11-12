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
