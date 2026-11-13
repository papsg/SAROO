# SD sector overlays

The SSO1 format stores small edits for exact raw Mode 1/2352 data sectors.
The offline compiler checks the Saturn IP product number, the SHA-256 of
each edited CUE FILE, the target's raw sector address, the original bytes
and the complete original error detection and correction fields. It writes
a new `.SOV` file and never opens source media for writing. A pack contains
the original sector CRC and preimages, replacement bytes, and the rebuilt
Mode 1 EDC and ECC. Its filename is the eight uppercase hex digits of the
CRC32 of the disc lookup key, followed by `.SOV`. CRC32 detects mismatches
and accidental corruption. It is a checksum.

The decoder admits up to twelve strictly ordered FAD records
with up to four nonoverlapping edits of one, two or four bytes per record.
Offsets refer to the complete raw sector, with an allowed data range of
16–2063. The 24-byte header and 348-byte records use big-endian lengths,
offsets, FADs and CRC32 fields. The standard Mode 1 EDC within the raw
tail has its disc byte order. A pack is 372–4,200 bytes.

ABI 2 also admits a guard record with no edits. Its complete sector CRC must
match at mount and at delivery, and it returns the original sector unchanged.
ABI 3 admits a single contiguous edit of 5–44 bytes in one record. The compiler
checks its entire preimage, and the MCU checks the original complete sector CRC.
Both forms are used by the corrected Pocket Fighter recipe.

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
§4.2, printed pp. 25–27, places the ten-byte product number at IP offset
`20h` and the six-byte version at `2ah`.
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

## Capacity and disc revision

The compiler can target up to twelve distinct sectors in one pack. Its
earlier short record layouts and legacy output bytes are unchanged. This
raises the fixed pack buffer from 2,808 to 4,200 bytes. An earlier
eight-record MCU refuses a larger pack and leaves the built-in callback
available. The public MCU continues using the standalone CRC.

For a revision-specific pack, add `disc_id` to the recipe with all sixteen
printable bytes from the disc's IP. For example, `"MK-81210  V1.001"`
contains the space-padded ten-byte product followed by the six-byte version.
The compiler requires the prefix to match `product_id` and the whole field
to match the source disc. Only the filename changes; the header and sector
guards keep their existing format.

Mount first tries the full product/version filename. It tries the older
ten-byte product filename only if FatFs reports that the file or directory
is absent. A present but invalid pack leaves the built-in route available;
an I/O error fails the launch. Neither case falls back to another pack.
The name is a lookup key. The preflight still checks the ten-byte product,
each complete target sector and every short preimage before admission.

Adapted from paps-g `f8786c36` and `4c6b8ed8`; see the IP layout in Sega
ST-040 §4.2 above. The earlier private Manx TT and Japanese Die Hard
candidates motivated these changes; this checkpoint adds no new game
recipes. Its tests reproduce the old capacity and lookup limits and cover
the twelfth and thirteenth records, both IP versions, legacy fallback,
exact-pack refusal, and a final-sector mismatch that prevents the entire
pack and its callback replacement from taking effect.

## Stock callback recipes

The [reviewed recipes](../recipes/README.md) move the inherited menu writes
into the matching raw sectors. The original BIN/CUE stays available for
compilation and ordinary loading. Compile and copy only the pack for the exact
disc you intend to use. A fully admitted recipe replaces its built-in callback.
Without that admission, the menu uses its retained callback.

The first group adapts paps-g `5d5cd0e8`. Pocket Fighter includes the
`cdf6dba9` correction, restoring all three routine writes as well as the NOP,
and authenticates the neighboring sector containing its PC-relative literal.
Its earlier one-edit recipe fails the complete callback-memory comparison.
Shichuu uses the source reviewed in `3fafb587`. Its historical native-drive
emulator comparison leaves both paths waiting in BUSY, so physical acceptance
and its startup behavior remain open. The recipes preserve the attribution
to the original SAROO source in their `origin` field.

For one-sector alternatives such as Noel 3, the compiler's `bundle` command
checks each complete source and creates one pack under the shared lookup key.
Header byte 6 is one for this form. Records are ordered by FAD and original
CRC. At mount, exactly one complete original sector must match. Zero matches,
multiple matches and duplicate source records cannot arm the pack. A transport
error fails launch even if an earlier record matched. Runtime checks retain
the chosen record across bad reads and retries, and unload clears it.

The Dungeons & Dragons and Friends discs share their product IDs with siblings.
Their retained callbacks now check the complete original first-file CRC and
both original instructions before either write. D&D uses the 179,656-byte
Disc 2 executable with CRC `15e2d749`. Friends uses the 244,484-byte game
executable with CRC `f4bfcb42`. The corresponding preserved first-file
SHA-256 values are `503dba35ec64dba164a4a5446e9d2fa58c86394577a4bfd0240ff11d1094d67c`
and `22fbac5c6cd52c1193d56566614ebf77b72485caf9fba8549b0b17667e0b9bcb`.
The different D&D Disc 1 instructions and the Friends special disc remain
intact when their pack is absent or rejected.

These fallback guards use Sega ST-040 §4.3, printed p. 29, and §4.7,
printed p. 31, for the first-file destination and CD file identifier [2].
The IP's first-read size is ignored for CDs. CRC reads use the uncached P2
alias, and instruction writes use P1, following Hitachi SH7604 §8.4.2,
printed p. 219. The exact source executable and original instructions supply
the game-specific bounds and preimages.
[Sega Disc Format Standards](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf),
[Hitachi SH7604 manual](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=235).

`make -C tests check-sector-overlay` compares all 2,352 delivered bytes,
tests real FatFs mount/failure/retry paths and checks every recipe against
the retained C callback's memory effects using synthetic data and explicit
SH-2 byte order. `test_menu_vdp1_order.py` exercises the normal launch caller,
guarded sibling callbacks, user overrides and physical-disc branch.
These checks qualify the port's source delivery and memory operations.
Game execution, physical cache timing and whole-game acceptance require
separate hardware qualification.

## Recipe cartridge setup

The expansion RAM recipes adapt paps-g `4ac082e1` and preserve the original
callback's cartridge selection along with its instruction edits. Header byte
7 uses values 0, 1 and 4 for no action, 1 MB and 4 MB. ABI 3 can combine
those values with bit `80h` for callback replacement. Other values are refused
before source delivery. A bundle must use identical actions for every source.

After complete pack admission and successful save selection, the MCU publishes
`534f5601h` or `534f5604h`, or `534f5681h`/`534f5684h` when the callback
is also replaced. The menu accepts only these exact values and the existing
`534f5680h` replacement value on virtual-disc launches. It applies the recipe's
RAM mode after the callback stage and before explicit user configuration.
The existing `CS0_RAM1M` and `CS0_RAM4M` settings perform the selection.
These tags form SAROO's own MCU/menu contract.

Overlay capability 2 advertises cartridge actions. This menu also requests
overlays from capability 1 firmware, whose decoder refuses action packs.
An earlier menu, which requests overlays only for capability 1, uses the
retained callback route with capability 2 firmware. Missing, incompatible
and rejected packs retain that same fallback. A save or transport failure
publishes no action. Disc unload and the tray retirement entry clear it.
Automatic disc changes can retain the selected overlay policy without
publishing a new startup action.

The public tests cover both RAM settings, replacement and edit-only packs,
invalid action values, complete source admission, failure cleanup, both
menu/MCU capability directions and the override order. The Real Bout Special
two-source bundle checks its shared 1 MB action and both raw postimages.
All eight recipes match the retained callback's complete memory operations
and cartridge setting in the host fixture. Physical cartridge timing remains
unqualified.
[Original SAROO callbacks and cartridge setup](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).
