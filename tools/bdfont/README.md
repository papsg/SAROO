# CJK font coverage

The reviewed selection from [Bowen Cui/notcbw's original PR #203](https://github.com/tpunix/SAROO/pull/203)
adds 108 records omitted from the v0.8 compiled font. Its five newly drawn
Japanese punctuation records are already present in v0.8 and are not new fixes
here. We preserve all 10,628 original records, including seven which differ
from the older PR binary. The Latin font and production rendering code stay
unchanged. Romanian additions and unrelated legacy geometry repairs are outside
this contribution.

The glyphs come from the already retained [WenQuanYi BDF](bdf/wqy13px.bdf).
Its original copyright/contributor notice and GPL v2 font embedding exception
remain in that source. [cjk-pr203.json](cjk-pr203.json) records its source hash,
the 108 codepoints, baseline identity and an independent digest of the complete
records from the original supplied firmware archive. That archive was inspected
as data; it is not required to build or test this font.

From the repository root:

```sh
make -C tools/bdfont
mkdir -p tests/build
tools/bdfont/bdfont tools/bdfont/bdf/wqy13px.bdf -l tools/bdfont/cjk.txt -adj 1 -c tests/build/cjk-only.bin
make -C tests check-font
```

The original converter reproduces the shipped baseline exactly after explicitly
selecting its existing U+000C form-feed record, which was absent from the source
list. The updated list retains that record and adds the 108 missing selections.
No replacement converter, new font format or runtime allocation is introduced.
The asset grows from 353,174 to 356,463 bytes: 3,289 additional loaded bytes.
The 128 KiB body plus font occupies 487,535 bytes, below the 512 KiB menu/catalog
boundary. MCU, FPGA and menu code are unchanged.

The tests rebuild through the actual C converter with ASan/UBSan, reconstruct
and hash the entire original font, and verify the added-record digest. They
also execute the actual lookup, UTF-8 decoder and raster functions for all
108 additions with an independent pixel oracle and framebuffer guards. Only
the two-byte count load is adapted to model the SH-2's big-endian access on a
little-endian host. Existing decoder pointer-sign warnings are suppressed in
that host fixture; the production source is untouched. Leak reporting is
disabled only for the original converter's process-lifetime allocations.

The same checks fail against the original asset on missing glyphs. This is
source/host qualification, not native Saturn rendering or physical timing.
The separate trailing-period filename discussion in the original PR remains
unresolved; ASCII period was already present, and this font addition does not
establish a filename fix.

The shipped font also includes [59 Arabic records](../../doc/ARABIC.md) and
[Romanian U+0219](../../doc/MENU-STATUS-FONT.md#saturn-font-002--romanian-s-with-comma-below).
The command above regenerates the CJK-only stage. Removing the Arabic and
Romanian additions reconstructs that stage exactly. The [complete merge commands](../README.md#rebuild-the-shipped-font)
regenerate the shipped asset.
