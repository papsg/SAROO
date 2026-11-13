# SD sector overlays

The SSO1 format stores small edits for exact raw Mode 1/2352 data sectors.
The offline compiler checks the Saturn IP product number, the SHA-256 of
each edited CUE FILE, the target's raw sector address, the original bytes
and the complete original error detection and correction fields. It writes
a new `.SOV` file and never opens source media for writing. A pack contains
the original sector CRC and preimages, replacement bytes, and the rebuilt
Mode 1 EDC and ECC. Its filename is the eight uppercase hex digits of the
CRC32 of the space-padded ten-byte product number, followed by `.SOV`.
CRC32 detects mismatches and accidental corruption. It is a checksum.

The initial ABI 1 decoder admits up to eight strictly ordered FAD records
with up to four nonoverlapping edits of one, two or four bytes per record.
Offsets refer to the complete raw sector, with an allowed data range of
16–2063. The 24-byte header and 348-byte records use big-endian lengths,
offsets, FADs and CRC32 fields. The standard Mode 1 EDC within the raw
tail has its disc byte order. A pack is 372–2,808 bytes.

The compiler accepts stored Mode 1 data at or after INDEX 01 and handles
shared CUE FILEs with different sector widths, generated gaps and stored
INDEX 00 offsets. It uses the firmware's bounded track, index, file-size
and FAD arithmetic. A mixed-width or later data track can carry a patch.
The product number comes from the original disc IP, even when the edit
targets another track.

Adapted from paps-g `f7791566` and the standalone checksum in `7afd1bea`.
[Sega CD Communication Interface ST-162-062094](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=36),
§§2.1 and 5.4, printed pp. 24–25 and 48, defines the frame address and
CD buffer sector layouts.
[Sega Disc Format Standards ST-040-R4-051795](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf#page=29),
§4.2, printed pp. 25–27, places the product number at IP offset `20h`.
[ECMA-130, 2nd edition](https://ecma-international.org/wp-content/uploads/ECMA-130_2nd_edition_june_1996.pdf),
§§14.1–14.6, printed pp. 15–16, and Annex A, printed p. 25 onward, define
the raw sector header, EDC and two sets of Reed-Solomon parity.

See the [compiler commands](../tools/README.md#sd-sector-overlay-compiler).
Run `make -C tests check-sector-overlay` for the compiler and actual C
decoder checks. The fixtures generate synthetic raw sectors, derive
parity independently, and compare all 2,352 bytes after delivery.

## Disc mount and sector delivery

An overlay-capable menu requests the feature when loading a game. The MCU
advertises capability 1 at SYSINFO offset `3ch`; the menu sets bit 15 of
the existing load-disc argument only when that capability is present. The
catalog's maximum index fits below this bit, and `ffffh` still unloads.
Automatic disc changes retain the current launch's overlay preference.

After identifying the disc and loading its configuration, the MCU opens one
bounded pack from `/SAROO/PATCHES/`. It checks every targeted sector before
arming the pack. Missing or incompatible packs leave the base disc available.
An open, read, seek or close I/O error fails the load. Preflight reuses the
existing sector workspace under the filesystem lock.

The CD worker edits the newly allocated CD block after copying from the
read cache and before publishing it. There are no additional SD reads or
writes for the overlay during playback. A runtime mismatch frees the
unpublished block and terminates the read with an error. The mounted pack
stays selected so a retry cannot silently deliver unpatched data. Disc
retirement, replacement and tray open clear the selection. Audio, Mode 2,
host PUT and subsequent CD-buffer copy or move are outside this hook.

The failure corrections come from paps-g `4da85af3`, `8eac58b1` and
`a3f25d5a`. The original private failure released the pack after a bad
sector, allowing a later PLAY to deliver the unpatched version. Host
fixtures exercise the actual CUE loader, FatFs, router and allocator,
including missing or oversized packs, I/O faults, changed-source retries,
disc replacement, source immutability and audio control. The public GNU
App build uses 57,944 load bytes and has 7,592 bytes free. Static DTCM spans
49,496 bytes. These are host and link checks.

## Pilot games and menu handoff

To use a reviewed recipe, build and use the matching MCU App and SH-2 menu.
Compile the recipe against your matching CUE/BIN source using the
[offline compiler](../tools/README.md#sd-sector-overlay-compiler) and place
the output in `/SAROO/PATCHES/`. The normal A-button game launch requests
overlays. The Z-button CD player uses its ordinary disc load.

The two pilot recipes declare that they replace the whole built-in callback.
The compiler encodes that declaration in SSO1 ABI 3. The MCU publishes
the replacement flag only after all target sectors match and save selection
succeeds. The menu accepts the exact flag on a virtual-disc boot, while the
existing callbacks remain available if the pack is absent, damaged or for
another source. Ordinary video initialization, backup support and configured
game writes still run. An ABI 1 pack edits sectors without replacing a callback.

| Recipe | Reviewed effect | Attribution |
| --- | --- | --- |
| [Terra Cresta 3D (Japan)](../recipes/terra-cresta-3d-jp-sector.json) | Preserve the BIOS language and keep the game using the virtual CD block. | [Original SAROO callback](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L695) and paps-g `d707cc0b`. |
| [Ultimate Mortal Kombat 3 (USA)](../recipes/umk3-us-sector.json) | Skip the two CDON calls reviewed for the US disc. | [Original issue #206](https://github.com/tpunix/SAROO/issues/206) and paps-g `f7791566`. |

The handoff adapts paps-g `5207cb9b`.
[Sega SMPC User's Manual ST-169-R1-072694](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf)
describes CDON and CDOFF on printed pp. 27–28 and SETSMEM on printed pp. 44–45.
The private Saturn trials exercised both titles with the early overlay
candidate. The public compiler reproduced both Terra records, including
their EDC/ECC, from the exact preserved first raw track. For UMK3, both
preimages and edits agree with the archived SOV and preserved first-file
instructions. Its full raw track was unavailable for a fresh compile.
The public port uses host regressions and target links for qualification.
