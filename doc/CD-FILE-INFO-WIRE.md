# Native CD file-information byte order

SSR-037 ports paps-g's `acf616d6` correction, independently adapted and verified
by papsg against this public implementation. The underlying parser, FIFO and
menu API originate in tpunix/SAROO. This is our source investigation; no
original upstream issue or pull request is asserted.

## Requirement and correction

Sega [ST-162-062094, data 6.8, printed p.72/PDF p.60](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=60)
distinguishes file-unit and interleave-gap sizes.
[Section 8.4, printed p.100/PDF p.88](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=88)
returns a 12-byte record, or up to254 records for the all-files query.
[ECMA-119 second edition, §§9.1.7–9.1.8, printed p.29](https://ecma-international.org/wp-content/uploads/ECMA-119_2nd_edition_december_1987.pdf)
places the separate unit/gap values at directory byte positions27/28. The ISO
semantics alone do not establish the CD command's native byte layout.

The cached original Sega SBL6.01 `SEGA_CDC.H` declares unit/gap/file-number/
attributes at offsets8/9/10/11 after two 32-bit fields. Its individually
rehashed SHA-256 is
`26a1aedb37f7a8f3456535fb70292ea9e6c31f79184a593b4cfb28ac76632898`.
The corresponding `SEGA_CDC.A` hash is
`ab5c8dad0a84ab954e590f6765a547a2e6c8dd0a5bad6dcb00c4d815340db654`.
Preserved disassembly of `_CDC_TgetFileInfo`/`_CDREG_TgetData` shows the
destination passed unchanged to direct halfword stores. This is static evidence,
not fresh execution or authentication of Sega's original distribution archive.
Neither proprietary artifact is distributed here.

The actual public `FPGA/SSMaster.v` swaps each FIFO halfword's bytes for the
Saturn. The current `CdcFile` also has unit at8 and gap at9. Previously the MCU
wrote `(unit << 8) | gap`; unit2/gap3 consequently arrived as unit3/gap2. Write
`(gap << 8) | unit` so the existing bus swap and native stores produce the
required unit-first record. Parser/interleave arithmetic, FPGA, native transfer,
record sizes, query cleanup and all other fields remain unchanged.

## Reproduction

```sh
python3 -B -m unittest discover -s tests -p test_cdc_file_info_wire.py -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_file_info_wire.py -v
python3 -B -m unittest discover -s tests -p test_cdc_directory.py -v
python3 -B -m unittest discover -s tests -p test_cdc_xa.py -v
make -C tests check-cdc-command-buffer
```

The five new methods extract the complete actual producer, both native transfer
branches, public query wrapper and actual type declarations. Only the host
pointer-width alignment cast and register/command/FIFO boundaries are seams.
The fixture checks the actual RTL permutation and models SH-2 big-endian stores;
it also supplies an independent halfword-store reference and fixed literal
`000000fa0000200002032528`. No vendor SDK is needed to reproduce it.

Against the preceding public source, four methods fail with semantic exit42:
all five distinct2/3 paths,65,025 unequal pairs in the65,281-pair domain,
32,256 of32,385 batch records, and all four literal-record alignments. The256
equal-field controls pass. Corrected sanitizer and optimized profiles pass all
five methods, including every batch length1–254, all other record bytes, buffer
guards, exact FIFO consumption and command cleanup. The declaration domain is
unit1–255/gap0–255 plus0/0; it does not assert every pair describes a valid ISO
extent. Wide FAD/size patterns test byte preservation, not valid complete files.

The existing directory fixture's former gap-first expectation is corrected from
the independent wire contract. All17 directory and13 XA methods pass, exercising
actual parser/command/streaming/range paths separately from the new command seam.
Complete public parent sources, failures, generated fixtures and source hashes
remain in ignored build evidence. A mistakenly named range-test discovery
selected zero tests; it is excluded from validation. Initial build argument-order
refusals are likewise preserved separately from successful builds.

## Target resources and limits

Fresh GNU14.3 builds with the accepted SDK and source pins pass all166 vector
checks. LTO load59,488→59,484 leaves6,052 ITCM bytes; non-LTO63,760→63,756 leaves
1,780. Data/BSS stay200/50,696 and204/50,688 respectively. Both static DTCM spans
remain50,896, with unchanged reserved stack/heap and80,176 address gap.
The producer's compiler-reported direct frame stays20 bytes in both profiles;
this excludes callees and interrupt use.
The normal menu and FPGA inputs do not change.

These are source/API tests, explicit transport models and target compilation.
Native FIFO execution, physical bus timing, named-game use of interleave fields
and complete hardware/game compatibility remain unqualified. No runtime probe,
USB integration, device operation, overlay recipe or installed firmware changes.
