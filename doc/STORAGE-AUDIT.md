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
