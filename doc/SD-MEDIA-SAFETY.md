# SD transfer, media launch and storage safety

## MCU-SD-004 — Recover transfer ownership

The original driver issues STOP after a data error and spins until the SDMMC
command and data engines go idle. A failed wait can instead clear DMA controls
while the controller still owns them. A late completion can also leave a
semaphore token for the next command. [Original driver][original-sd] ·
[Earlier command and initialization fixes][sd-initialization]

The port adapts paps-g `433866e8dc3e28875de59f93b0017a2404458fd2`, with final
historical verification at `33916ab5`. It qualifies each IRQ against the enabled
flags, keeps errors enabled while waiting for card busy release, and drains a
late token with the IRQ masked. RTX releases the token before deferred waiter
processing. [Pinned CMSIS-RTX source][semaphore] · [Public driver][driver]

On a failed transfer the driver waits for command ownership before sending
STOP. It waits for both engines, the command-enable bit and the card busy
signal before releasing the DMA buffer. Each STOP attempt has a one-second
RTOS budget. If it fails, the driver resets SDMMC1 and makes one more bounded
attempt. A second failure resets the controller and clears the card address
to prevent further sector access. A receive IDMA error also resets residual
controller state. The retained Rev 15 errata excerpt identifies revisions Y/W.
The reset path does not depend on identifying the chip revision. The shared setup keeps the existing command order, sector
addresses and read/write timeouts on the healthy path.
[STM32 RM0433 Rev 8, pp. 2393, 2398–2403, 2438–2440 and 2454][rm0433] ·
[STM32 ES0392 Rev 15, section 2.8.3, p. 25][errata] ·
[ST HAL SD abort implementation][hal]

The public adaptation explicitly checks CPSMEN at STOP completion and
keeps the existing log strings. Twenty of 23 modeled scenarios fail on
baseline `946ca0c`. All 23 pass with ASan/UBSan and O2, including delayed
STOP, persistent busy, stale and unrelated IRQs, failed pre-erase and
the original healthy read/write sequences. A compiled variant that skips
the STOP enable check fails the delayed-enable case. The previous 68
command and initialization cases, four disk I/O methods and 11 coupled
disc/save methods also pass. [Event fixture][transfer-test]

Run `make -C tests check-mcu-diskio` and
`python3 -B tests/test_mcu_save_disc.py -v`. Set `SAROO_SD_PROFILE=release`
for the optimized SD fixtures. The GNU App gains 144 load bytes for
62,364 bytes total and 3,172 bytes of ITCM margin. Static DTCM remains
60,176 bytes. The compiler-reported direct command and recovery frames
are 24 and 32 bytes. Historical hardware evidence reached startup.
Physical SD fault recovery and game behavior remain unqualified.
[Build instructions][build]

[original-sd]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Main/sdio_h7.c
[sd-initialization]: FATFS-SD-SAFETY.md
[driver]: ../Firm_MCU/Main/sdio_h7.c
[semaphore]: https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_semaphore.c
[rm0433]: https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743-753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
[errata]: https://www.st.com/content/ccc/resource/technical/document/errata_sheet/group0/b8/f4/b7/a3/d1/a0/44/a6/DM00368411/files/DM00368411.pdf/jcr:content/translations/en.DM00368411.pdf
[hal]: https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/e3c518ebda3e00c8e52b1625ca44fba65da3b63e/Src/stm32h7xx_hal_sd.c#L2895
[transfer-test]: ../tests/sd_transfer.cpp
[build]: ../tools/README.md#original-mcu-app-compilelink-check
