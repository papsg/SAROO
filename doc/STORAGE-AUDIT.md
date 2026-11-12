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
