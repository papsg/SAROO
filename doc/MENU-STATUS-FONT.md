# Menu status and font safety

## SATURN-FONT-002 — Romanian s with comma below

The Romanian loading label uses `ș` (U+0219). At public baseline `9a9910a`,
both font tables lack that code point, so the Latin-first lookup returns the
replacement square. The added record draws the eight rows of the existing
`s`, a gap and a two-row comma. The advance, bearing and base-letter pixels
match the existing `s`. This adapts paps-g `f9e04ef9` using the Helvetica
`scommaaccent` source already in this repository. Two repeated comma-tail
rows are removed so the glyph fits the existing 16-pixel row.
[Romanian label](../Firm_Saturn/language.c) · [Original Helvetica BDF](../tools/bdfont/bdf/helvR14.bdf#L7581)

The existing converter builds the adapted record from a small licensed BDF
subset. The original font asset is the base for the merge. Removing U+0219
from the composed asset reconstructs that base exactly, preserving the 108
additions from [upstream PR #203](https://github.com/tpunix/SAROO/pull/203) and
all 59 Arabic records. `make -C tests check-font` verifies the missing-record
failure, source-row selection, the two-byte UTF-8 path and actual guarded
rasterization. The Arabic tests check their own unchanged stage and render
with the shipped font. [Rebuild commands](../tools/README.md#rebuild-the-shipped-font)

The pinned Yaul build keeps the ELF and 128 KiB body byte-identical. The
appended font grows by 19 bytes, including its five-byte index entry, for a
488,770-byte menu image. Host checks cover rendering into a model framebuffer.
Appearance and timing on hardware remain unqualified.

### Helvetica notice

Copyright 1984-1989, 1994 Adobe Systems Incorporated.

Copyright 1988, 1994 Digital Equipment Corporation.

Adobe is a trademark of Adobe Systems Incorporated which may be registered in
certain jurisdictions. Permission to use these trademarks is hereby granted only
in association with the images described in this file.

Permission to use, copy, modify, distribute and sell this software and its
documentation for any purpose and without fee is hereby granted, provided that
the above copyright notices appear in all copies and that both those copyright
notices and this permission notice appear in supporting documentation, and that
the names of Adobe Systems and Digital Equipment Corporation not be used in
advertising or publicity pertaining to distribution of the software without
specific, written prior permission. Adobe Systems and Digital Equipment
Corporation make no representations about the suitability of this software for
any purpose. It is provided "as is" without express or implied warranty.
[Adapted BDF and original notice](../tools/menu-assets/font-source/romanian14.bdf)
