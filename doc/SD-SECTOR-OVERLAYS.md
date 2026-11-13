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
