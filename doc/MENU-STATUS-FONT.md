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

## SATURN-FONT-003 and SATURN-FONT-004 — glyphs that fit a menu row

The original renderer reads a four-bit height. The shipped CJK U+9515 record
holds sixteen bitmap rows but its height field is zero. The Latin U+046E record
holds seventeen rows but its height field is one. They render blank and
one-row respectively. Twelve other Latin records extend past the 16-pixel
row the menu clears or copies. The Romanian `ț` (U+021B) extends to row 18.
[Original rasterizer and menu item staging](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/conio.c)

The renderer reads zero height as sixteen rows when the record has a
nonzero width. U+9515 retains its entire original bitmap. The zero-width
space record stays empty. The curated Latin BDF retains all distinct
strokes of U+046E by removing a repeated stem row, then encodes its sixteen
rows using the same convention. Eleven comma-below glyphs lose two repeated
tail rows while preserving their base letters and gap. U+0375 keeps its bitmap
and moves up one row. These changes adapt paps-g `4c2cccb4` by editing the
public BDF source and regenerating its header with the original converter.
[Latin BDF and notice](../tools/bdfont/cbdf/helvR14c.bdf) · [WenQuanYi BDF and notice](../tools/bdfont/bdf/wqy13px.bdf)

All 637 Latin code points remain. The other 624 Latin records and the
appended 10,796-record asset retain their exact bytes. `make -C tests
check-font` regenerates the Latin header, checks all 11,433 record spans and
renders the fourteen repaired glyphs against their indexed bitmaps with
guarded rows. The preceding source fails the CJK raster, Cyrillic raster,
overhang and record geometry checks. The original CJK additions, Arabic
glyphs, Romanian `ș` and space are controls. The space test uses an exact
three-byte allocation to catch an accidental read past its header.
[Rebuild commands](../tools/README.md#rebuild-the-shipped-font)

The pinned menu build links 79,556 code/data bytes (16 fewer), with the same
9,232-byte BSS, 16 KiB stack reservation and 488,770-byte image. The
glyph rasterizer's direct frame remains 40 bytes. The packed height convention
is verified for the supplied font assets. Host raster checks do not qualify
physical Saturn output.

## SATURN-MENU-008 — match status drawing to its clear area

The original menu clears columns 16–303 on each status update and allows
drawing through column 319. A long message can write beyond the area its
replacement clears. Twelve inherited labels also exceed the 288-pixel
width, including the 368-pixel Romanian update-complete message.
[Original status implementation](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/conio.c)

The status draw and clear now share the same right edge. Shorter wording
fits the twelve French, German, Spanish, Swedish, Greek and Romanian
messages while retaining each error code, update warning and restart
instruction. This adapts paps-g `4c2cccb4` to the public catalog, retaining
the public Romanian disc-error format's explicit `%d` and its prior glyph
repairs. The [approved wording fixture](../tests/fixtures/menu-status-wording.json)
records the twelve changes.

`make -C tests check-language-catalog` checks 418 complete status variants
across all 19 languages, including both signed 32-bit error limits.
The preceding public source fails twelve width cases and both framebuffer
tests for overflow and stale pixels. Short and empty statuses pass on
both revisions. The C fixture runs the actual clear and draw routines against
a guarded framebuffer. The existing formatter and raster tests also pass
648 prior-language cases and 36 independent Arabic cases.
[Catalog checks](../tests/test_language_catalog.py) · [Framebuffer fixture](../tests/menu_status.c)

The pinned Yaul build links 79,380 code/data bytes (176 fewer than the glyph
checkpoint), with a 20-byte direct `menu_status` frame and a 488,770-byte
menu image. Native-speaker editorial review and physical Saturn appearance
remain unqualified.
