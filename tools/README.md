# Host asset tools

The existing C tools retain their original licenses and layout. The Rust
`menu-assets` crate is a host-only asset compiler under its own MIT license.
Its lockfile pins serde/serde_json and their dependencies. It does not add Rust
or a layout engine to Saturn firmware.

Build with Rust 1.85 or newer, from the repository root:

```sh
cargo build --locked --manifest-path tools/Cargo.toml
tools/target/debug/menu-assets --help
```

The three commands accept explicit input and output paths, independent of the
current directory. `arabic-table` compiles twenty authored logical labels to a
C table. `arabic-glyphs` selects their required presentation forms. `merge-font`
combines two existing SAROO indexed fonts, preserving every baseline record
and rejecting overlaps, malformed indices and new out-of-cell geometry.

Inputs are bounded: 64 KiB JSON and 384 KiB fonts. The merged font must remain
within the 512 KiB menu window after its 128 KiB body. Outputs use a sibling
temporary file followed by atomic rename; failures preserve the destination.
An input cannot also be the output. A colliding temporary file causes an error
and is left intact. These operations do not access a device or network.

## Rebuild the shipped font

```sh
make -C tools/bdfont
mkdir -p tests/build
tools/target/debug/menu-assets arabic-table tools/menu-assets/arabic.json tests/build/arabic.inc
tools/target/debug/menu-assets arabic-glyphs tools/menu-assets/arabic.json tests/build/arabic.txt
tools/bdfont/bdfont tools/bdfont/bdf/wqy13px.bdf -l tools/bdfont/cjk.txt -adj 1 -c tests/build/cjk-only.bin
tools/bdfont/bdfont tools/menu-assets/font-source/arabic12.bdf -l tests/build/arabic.txt -adj 0 -c tests/build/arabic-only.bin
tools/target/debug/menu-assets merge-font tests/build/cjk-only.bin tests/build/arabic-only.bin tests/build/font_cjk.bin
cmp tests/build/font_cjk.bin Firm_Saturn/font_cjk.bin
```

Normal menu builds consume the checked-in C table and font. To edit Arabic,
change `menu-assets/arabic.json`, regenerate the table and font, then update the
`lang_ar` table in `Firm_Saturn/language.c` and the font asset. The tests enforce
an exact table/asset match. Do not paste visual-order strings into the logical
source or change font expectations without checking independent reference pixels.

`joining-forms.json` pins 32 stable presentation-form decomposition mappings
from Unicode 16.0.0 (CPython's database). Joining follows
[Unicode 17 §9.2, Arabic](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-9/#G20596).
The authored directional runs preserve numbers and Latin text without a runtime
bidirectional algorithm. This supports the fixed unvocalized labels only;
unsupported letters, diacritics, joiner controls and format strings are rejected.
See [Arabic scope and attribution](../doc/ARABIC.md).

The Arabic drawings have separate OFL-1.1 terms in
[`menu-assets/font-source`](menu-assets/font-source/FONT-NOTICE.txt).
Original source atlases, README, font/library licenses and hashes are retained
there. The BDF's `PIXEL_SIZE` declaration is a header adaptation for the original
C converter; no glyph pixels, advances or bearings changed.

## Public checks

Use a Python environment with the test-only PNG decoder:

```sh
python3 -m venv tests/build/venv
tests/build/venv/bin/python -m pip install -r tests/requirements.txt
make -C tests PYTHON=build/venv/bin/python check
```

The suite runs the actual C translation/formatting/raster functions under
ASan/UBSan, exact font regeneration, independent bitmap comparisons, language
controls and Rust formatting/lint/tests. Pillow only decodes the original PNG
for its independent pixel oracle; it is unnecessary for firmware compilation or
asset generation. No oracle package, network service or Saturn is used at test
runtime after these development dependencies are installed.

For a complete menu build with a cached Yaul GCC 13.2.0 environment, see
[the build qualification](../doc/ARABIC.md#build-qualification). The original
`sh-elf` toolchain with libc remains supported by the existing menu Makefile.
