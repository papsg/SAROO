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

## SATURN-CD-001 — Check menu CD transfers and identify audio first

The menu previously continued after the second HIRQ wait failed and
read TOC words after a rejected command. It also tried to read 16 game
header sectors from an audio disc. This port adapts paps-g
`0b9dc270174e58c47248cd0decccdfd3545be2e5`, the menu part of upstream
issue #175. [Original report][issue175] · [Original CD helpers][original-menu-cd]

The menu now reads the 408-byte TOC into the existing IP workspace and
checks its first and last track numbers. An audio-first disc goes to
the existing player. A data-disc boot requires successful setup,
PLAY, GET/DELETE and DataEnd with the expected 16,384 CD-block words.
The old PEND is cleared before PLAY so a fast new completion remains
visible. Failed TOC or DataEnd operations return an error without
publishing an unavailable response count. The optical and virtual
player paths keep their corresponding BIOS callbacks.
[Sega ST-162 section 3.1, printed p. 28][sega-hirq] ·
[Section 8.2.1, functions 1.5 and 1.10, printed pp. 77 and 81][sega-toc] ·
[Public menu source][menu-loader]

Twenty of 33 scenarios fail on `9e53888`. All 33 pass with ASan/UBSan
and O2, including invalid TOCs, immediate PEND, complete IP data,
failed waits and the two callback modes. Run
`make -C tests check-menu-safety check-cover-display` or set
`SAROO_MENU_CD_PROFILE=release` and run
`python3 -B tests/test_menu_cdplayer.py -v`.
[Menu CD fixture][menu-fixture]

The pinned menu build adds 128 bytes of linked code/data to 79,508.
Its body margin is 25,948 bytes. BSS remains 9,232 bytes and the image
remains 488,770 bytes. The model checks command admission and menu
handoffs. Physical disc playback and BIOS timing remain unqualified.
[Menu build instructions][menu-build]

## MCU-DISC-005 — Classify images before selecting a game save

The MCU used the first track's bytes at offset 0x20 as a save identity
even for audio and foreign data. The port adapts paps-g
`ec0c0e930f145296e5b0f0d07c29a813c528e195`. Audio bypasses the IP read.
A data track must supply a complete normalized header and the Saturn
hardware identifier before its product/version bytes identify a game.
Read errors still fail the load. A signed game with an empty product
identity still reaches save validation. [Public image loader][image-loader] ·
[Sega ST-040-R4 section 4.3, printed pp. 26–27][sega-ip] · [Related issue #175][issue175]

Non-game media uses an empty runtime identity. This applies global
playback settings through the existing locked parser and ends the
config scan before per-game sections. The existing save slot, window
owners and pending writes remain available. [Public configuration][config-source]

Five CUE/ISO and two coupled save cases reproduce the old behavior on
`8bebab7`. Two signed-game controls pass there. The public cases cover
audio-first and game-first mixed mode, a coincidental PCM signature,
foreign data, locked global settings, zero archive I/O and successful
retry of a save retained across media selection. Both 12-method image
and save suites pass with ASan/UBSan and O2. Header, runtime save and
configuration regressions pass. Run
`python3 -B tests/test_mcu_save_cue.py -v` and
`python3 -B tests/test_mcu_save_disc.py -v`.
[CUE fixture][cue-fixture] · [Save-session fixture][session-fixture]

The GNU App adds 48 load bytes for a total of 62,412, with 3,124 bytes
of ITCM margin and 60,176 bytes of static DTCM. The direct loader and
identity frames remain 312 and 24 bytes. Physical player entry and
save survival across a console reset remain unqualified.
[Build instructions][build]

## MCU-FS-004 — Keep completed writes pending after an abort

FatFs can write a prefix of the requested bytes or change the cluster
chain before a device error aborts the file. The old `f_write` reached
`FA_MODIFIED` only on normal exit. A subsequent close could skip the
pending work and lose cached data or the new file length.
[Original FatFs source][original-fatfs] · [FatFs f_write][fatfs-write]

The port adapts paps-g `0e259a3a75c8dd77c79aa6eb0e3da6fc5aabf5a0`.
It marks the file modified after input and access checks, before the
write loop can change data or allocation. The previously published
sync retry remains responsible for the pending work. The existing
error latch still rejects another write to the aborted handle.
[FatFs disk error and close contracts][fatfs-error] ·
[FatFs f_close][fatfs-close] · [Public FatFs source][fatfs-source]

Six of 12 generated FAT32 and exFAT cases fail on `8e23954`. The port
retains the completed bytes after contiguous and fragmented growth,
retries a persistently failing cache write through close, and preserves
neighboring files after remount. The first-transfer failure, read-only
and zero-length controls still pass. Run `make -C tests check-fatfs`.
All 16 methods pass with ASan/UBSan and O2. Twelve MCU save-runtime
methods also pass. [Generated-media fixture][fatfs-fixture]

The GNU App stays at 62,412 load bytes, with 3,124 bytes of ITCM margin
and 60,176 bytes of static DTCM. The direct `f_write` frame stays at
56 bytes. Physical SD durability remains unqualified.
[Build instructions][build]

## MCU-SD-005 and MCU-SD-006 — Bind sector access to an initialized card

The old driver overwrote the remembered card ID during reidentification.
It also published a signed relative card address before several required
commands had succeeded. A later error could leave sector I/O enabled,
and shifting a valid address with bit 15 set could overflow a signed
integer. [Original driver][original-sd] · [Public driver][driver]

The port adapts paps-g `613efd17b74bec535f3f94cf1beab8766cce03c1`.
It retains the first successfully initialized card's complete 16-byte CID.
A different CID is refused before requesting a relative address. The
candidate address stays local and unsigned until all required commands
complete, so sector I/O remains disabled throughout identification and
after any failure. The same card can return with a different address.
An absent card, failed identification or missing semaphore returns an
error at startup. [Public driver][driver]

Automatic reidentification holds the existing filesystem mutex. The
public adaptation also takes the disk I/O semaphore because public
callers can reach sector I/O outside the filesystem mutex. Acquisition
failure returns an error before the controller is touched. Both locks
are released before loading the next disc. A different card requires a
fresh MCU/filesystem session. Live card removal and changes made to the
same card outside SAROO remain unqualified.
[Public disk adapter][disk-adapter] · [Disc worker][disc-worker] ·
[CMSIS-RTOS2 semaphore contract][semaphore-contract]

Thirty-five of 40 identity cases fail on `4e4bdf0`. Three startup
scenarios and both modeled filesystem-lock paths also expose the old
behavior. All 40 identity cases, 25 transfer scenarios, seven new
disk-lock controls and 13 disc/save methods pass in both profiles.
Removing the disk lock fails the in-flight read and write controls.
Publishing the address early fails both in-progress initialization
controls. Run `make -C tests check-mcu-diskio` and
`python3 -B tests/test_mcu_save_disc.py -v`.
[Identity fixture][media-fixture] · [Disk I/O fixture][disk-fixture]

The GNU App adds 208 load bytes for 62,620 total and 2,916 bytes of
ITCM margin. Static DTCM grows by eight bytes to 60,184. Direct
identification and disk-reidentification frames are 72 and 16 bytes.
The tests check identity, command failure, and ownership at modeled
boundaries. Physical card replacement, bus timing and durability remain
unqualified. [Build instructions][build]

[original-sd]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Main/sdio_h7.c
[sd-initialization]: FATFS-SD-SAFETY.md
[driver]: ../Firm_MCU/Main/sdio_h7.c
[semaphore]: https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_semaphore.c
[rm0433]: https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743-753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
[errata]: https://www.st.com/content/ccc/resource/technical/document/errata_sheet/group0/b8/f4/b7/a3/d1/a0/44/a6/DM00368411/files/DM00368411.pdf/jcr:content/translations/en.DM00368411.pdf
[hal]: https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/e3c518ebda3e00c8e52b1625ca44fba65da3b63e/Src/stm32h7xx_hal_sd.c#L2895
[transfer-test]: ../tests/sd_transfer.cpp
[issue175]: https://github.com/tpunix/SAROO/issues/175
[original-menu-cd]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/cdblock.c
[menu-loader]: ../Firm_Saturn/game_load.c
[menu-fixture]: ../tests/menu_cdplayer.c
[sega-hirq]: https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=16
[sega-toc]: https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=65
[sega-ip]: https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf#page=30
[image-loader]: ../Firm_MCU/Saturn/cdimg.c
[config-source]: ../Firm_MCU/Saturn/saturn_utils.c
[cue-fixture]: ../tests/mcu_save_cue.c
[session-fixture]: ../tests/mcu_save_disc.c
[original-fatfs]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/FatFS/ff.c
[fatfs-source]: ../Firm_MCU/FatFS/ff.c
[fatfs-write]: https://elm-chan.org/fsw/ff/doc/write.html
[fatfs-error]: https://elm-chan.org/fsw/ff/doc/rc.html#de
[fatfs-close]: https://elm-chan.org/fsw/ff/doc/close.html
[fatfs-fixture]: ../tests/fatfs_seek.c
[disk-adapter]: ../Firm_MCU/FatFS/diskio.c
[disc-worker]: ../Firm_MCU/Saturn/saturn_main.c
[semaphore-contract]: https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__SemaphoreMgmt.html
[media-fixture]: ../tests/sd_media.c
[disk-fixture]: ../tests/mcu_diskio.c
[menu-build]: ../tools/README.md#isolated-firmware-qualification
[build]: ../tools/README.md#original-mcu-app-compilelink-check
