# SAROO

SAROO loads Sega Saturn games from an SD card through the cartridge slot. Its
FPGA and MCU provide the CD-block interface, and it also supports 1 MiB/4 MiB
expansion-RAM operation. Expansion RAM does not increase the Saturn CPU clock.

This fork starts from the original
[v0.8 source](https://github.com/tpunix/SAROO/tree/ba387b083bb9d2a001ae6a22a24d6e6785e515bd).
Later changes are introduced individually. The original
[Chinese README](README.md) is preserved; its development history and broad
compatibility statements are historical author reports, rather than a new
compatibility certification for every game or commercial board.

## SD-card layout

These are the paths recognized by this source revision:

| Path | Purpose |
| --- | --- |
| `/SAROO/mcuapp.bin` | MCU application loaded by the MCU boot firmware. |
| `/SAROO/ssfirm.bin` | Saturn-side menu/firmware. |
| `/SAROO/saroocfg.txt` | Global and per-game configuration. |
| `/SAROO/SS_SAVE.BIN` | Per-game save archive. |
| `/SAROO/SS_MEMS.BIN` | Memory-cartridge save archive. |
| `/SAROO/cover.bin` | Cover-art database. |
| `/SAROO/ISO/Game/` | A game-image directory containing its CUE and referenced tracks. Configured categories add a directory level. |
| `/SAROO/BIN/program.bin` | A directly listed binary executable; this browser lists files, not one directory per program. |
| `/SAROO/update/SSMaster.rbf` | FPGA-update input recognized by the original updater. |
| `/SAROO/update/mcuboot.bin` | MCU-boot-firmware update input recognized by the original updater. |

The last two entries identify updater filenames, not an instruction to install
them on an unspecified board. `mcuapp.bin` and `mcuboot.bin` belong to different
loader stages. The old README's `/ramimage.bin` and MCU-update `ssmaster.bin`
names do not describe these v0.8 loaders.

Source references: the [MCU boot/application loader](Firm_MCU/Main/main.c),
[Saturn-menu loader](Firm_MCU/Saturn/saturn_main.c),
[image and binary discovery](Firm_MCU/Saturn/cdimg.c),
[save/cover access](Firm_MCU/Saturn/saturn_utils.c), and
[FPGA updater](Firm_MCU/Main/spi1_fpga.c).

## Images and configuration

This fork's MCU application requires a FAT32 SD card. It accepts partitioned
and unpartitioned FAT32 volumes. Each file can be at most 4 GiB minus one byte.
The original BootLoader still supports its earlier formats; being able to
load `mcuapp.bin` from an exFAT card does not mean the new App can use that card.

If your card uses exFAT, FAT12 or FAT16, copy everything to your computer and
verify the backup before formatting. Formatting erases the card. Format it
as FAT32 with your computer and restore the files in their original locations,
including `/SAROO/SS_SAVE.BIN` and `/SAROO/SS_MEMS.BIN` if present. For CUE/BIN
images, keep the track files together and preserve the filenames in the CUE.
See the [filesystem notes](doc/FATFS-SD-SAFETY.md#use-fat32-in-the-mcu-application)
for the firmware scope and verification.

The [configuration example](doc/saroocfg.txt) contains global and per-game
sections, language selection, category names, multi-disc markers, expansion-RAM
settings and timing overrides. Timing delays are expressed in microseconds in
the source's configuration documentation. Existing per-game delay values are
compatibility settings; their presence does not identify or resolve a shared
hardware-timing defect.

The baseline language IDs are Chinese 0, English 1, Brazilian Portuguese 2,
Japanese 3, French 4, Russian 5, Traditional Chinese 6, German 7, Spanish 8,
Italian 9, Polish 10, Swedish 11, Greek 12 and Romanian 13. The comment in the
original configuration example stops at Swedish; the actual
[language table](Firm_Saturn/language.c) also contains Greek and Romanian.
The [additional language list](doc/LANGUAGES.md) gives newer IDs and contributor
credits. Existing IDs retain their meanings.

The [controller protocol notes](doc/PERIPHERALS.md) describe continued input
collection, both ports/multitaps, primary Sega references and qualification limits.

## Source and tools

| Directory | Contents |
| --- | --- |
| `Firm_MCU/` | STM32H750 MCU firmware and the MDK project. |
| `Firm_Saturn/` | SH-2 Saturn menu, CD helpers, save support and its Makefile. |
| `FPGA/` | Quartus project, bus/cache logic and SDRAM controller. |
| `HW/` | Original hardware design files and change history. |
| `tools/savetool/` | Native save-archive utility. |
| `tools/covertool/` | Native cover-database utility. |
| [`tools/bdfont/`](tools/bdfont/README.md) | Font sources, selection lists, generator and verified coverage. |

The original menu instructions name SaturnOrbit's SH-ELF compiler; its Makefile
expects `sh-elf-gcc`, assembler and binutils, including an available target C
library. An arbitrary compiler-only cross-toolchain is not a complete substitute.
The FPGA project targets the historical Quartus 14.0 toolchain. The MCU project
references its own compiler and CMSIS/RTX dependencies. These are separate build
inputs, and building a component does not install it or qualify a board.

The [English developer notes](doc/DEVELOPMENT_EN.md) explain the source
annotations, historical design terminology and technical-notebook corrections.

## Attribution

The English documentation adapts the requests in
[original PR #68](https://github.com/tpunix/SAROO/pull/68) and
[Josh Wisenbaker/macshome's PR #352](https://github.com/tpunix/SAROO/pull/352),
which supersedes [#343](https://github.com/tpunix/SAROO/pull/343).
The earlier machine-translated paths and technical claims are checked against
the source rather than copied unchanged. Original copyright and license notices
remain in their files. This contribution adds no firmware feature or game test.
