# English developer notes for the v0.8 baseline

These notes and the accompanying ASCII source comments adapt the useful content
of [macshome's original PR #352](https://github.com/tpunix/SAROO/pull/352), following
[#343](https://github.com/tpunix/SAROO/pull/343), with the SD-layout request from
[#68](https://github.com/tpunix/SAROO/pull/68). They describe the source retained
in this fork, not a later board's measured electrical behavior.

## Source annotations

English comments are added to `cdblock.c`, `conio.c`, `crc32.c`, `game_load.c`,
`game_patch.c`, `game_save.c`, `main.c` and `sci_shell.c` in `Firm_Saturn/`.
Every original source byte is retained. Some Chinese comments use a
GB18030-compatible encoding; decoding them with replacement characters would
destroy information. The additions are ASCII line comments, not replacements
for the original text or changes to executable statements.

The comments explain input-polling states, bounded attempt counts, regional
variants and the original rationales for game patches. Where a source note is
tentative, its uncertainty is retained. Translating a patch rationale does not
demonstrate a new cause or successful retest of that game.

## Design generations and firmware versions

The [FPGA readme](../FPGA/readme.txt) compares the historical V1.2 and V1.1 designs:

1. Remove Qsys components whose SDRAM controller supported single accesses.
2. Use a simple SDRAM controller with four-access read bursts.
3. Provide two request ports with eight-byte caches.
4. Fill the CDC FIFO from the STM32, where the CD-block buffer resides.

These are the author's design-generation notes. They do not mean installed FPGA
firmware version 12, identify every later commercial board, or prove current
bus timing. Likewise, the [hardware change list](../HW/update.txt) is historical
V1.2/V1.3 design documentation rather than a universal component inventory.

## Reading the historical technical notebook

The [original notebook](SAROO技术点滴.txt) contains earlier register maps,
observations and tentative explanations. Several proposed English passages in
#343/#352 changed its technical meaning. The following retains that meaning
without treating the old notebook as a replacement for the Sega manuals:

| Topic | Meaning of the original note |
| --- | --- |
| Address | Asserted one cycle before CS and held for one or two cycles after CS ends. |
| RD | Asserted one cycle before CS and ends one cycle after CS ends. |
| FC0/FC1 | High during ordinary reads/writes. The author observes FC1 every 14.32 microseconds outside accesses and suspects refresh. |
| AAS | Has the same width as CS. |
| TIM0 | Starts with the address and ends one cycle before CS ends. |
| TIM1 | During writes, starts with the data and ends with TIM0; the author suggests a data-cycle indication. |
| TIM2 | Starts one cycle after TIM1 and ends with CS. |
| Write data | Appears one cycle after WR begins. |
| CS2 wait selection | The note attributes wait selection to address bits 18–15, not ASR. |
| CS2 32-bit mode | Describes two consecutive accesses returning a 32-bit value with A1 emitted. |
| CS2 16-bit mode | Describes access to the upper halfword, with the lower halfword inaccessible in that mode. |

The original high-work-RAM boundary is `0x06100000`, not the `0x06080000` substituted
by the proposed translation. The proposal also omitted the original HIRQ/status
constant lists and inserted `DATATRNS` offsets absent from that notebook.
Those are technical changes, not translations. The original itself retains
unresolved inconsistencies, including a "192K" label on the
`0x05c00000`–`0x05cc0000` span. No new timing measurement or complete register-map
qualification is claimed here.

## Historical auxiliary tools

- [`cdgplay`](../tools/cdgtools/) plays SUB/CDG files. Its readme describes
  full-speed playback by default and an optional speed argument.
- `cdgfix` repairs CD+G packets and reports corrected and uncorrectable packets.
  The current utility validates the input and stages the complete output before
  replacing the source. See [CD+G repair safety](CDG-REPAIR-SAFETY.md) for the
  standalone build, supported layouts and exit codes.
- [`cpmk`](../tools/cpktools/) describes a historical Windows pipeline using
  FFmpeg and `iccvid.dll`. In the source, `-key k` is the forced-keyframe interval,
  not the total number of keyframes. Its 32-bit build instructions and speed
  opinion concern that historical toolchain, not a current encoder benchmark.
- `cplayk` is the companion CPK player. No codec build or physical playback
  qualification is added by these notes.

The [English README](../README_EN.md) supplies the source-checked firmware/save
paths. This contribution contains no private development infrastructure,
proprietary codec payloads, device operations or new game-compatibility claims.
