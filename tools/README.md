# Host asset tools

## SD sector overlay compiler

Use a reviewed JSON recipe and its matching local CUE/BIN files. The
compiler checks complete source FILE hashes and writes a new pack to an
existing output directory. The `name` command gives the expected filename.

```sh
cargo build --offline --locked --manifest-path tools/Cargo.toml -p sector-overlay
tools/target/debug/sector-overlay name recipe.json
tools/target/debug/sector-overlay compile recipe.json disc.cue output/1234ABCD.SOV
make -C tests check-sector-overlay
```

Use the actual name returned by `name` in place of `1234ABCD.SOV`.
See the [overlay format and source checks](../doc/SD-SECTOR-OVERLAYS.md).

## Isolated firmware qualification

`firmware-build` runs the original public menu Makefile on a source copy under
`tests/build`, using the cached pinned Yaul image documented in
[ARABIC.md](../doc/ARABIC.md#build-qualification). It never pulls an image or
accesses a device. The ordinary firmware Makefile remains usable directly.

```sh
cargo build --offline --locked --manifest-path tools/Cargo.toml -p firmware-build
tools/target/debug/firmware-build menu --repo "$PWD" --out tests/build/menu-check
```

The repository is explicit; relative output paths resolve against it. Output
must be a new directory inside that repository's `tests/build`. Existing results
are refused. The tool copies bounded public menu inputs without changing their
bytes, records source SHA-256 identities, disables container networking and
admits only a local Unix-socket Docker endpoint. Builds have a five-minute
deadline, with bounded removal of the tool's own container on timeout.
Compiler stdout/stderr and the complete source/object/ELF/image/map cohort remain
in the output directory. A JSON receipt records success or build verification
failure. Setup failures can leave the new directory for inspection.

Qualification checks the big-endian SuperH executable, entry, original 128 KiB
body and 16 KiB stack, code/BSS/margin and exact appended font. This is target
compile/link qualification, with no physical Saturn, bus or storage acceptance.
GCC's `-fstack-usage` files and receipt rows record direct function frames and
their static/dynamic qualifiers. They exclude transitive callees and interrupt
use and do not prove a game's available stack headroom.
The host builder reuses the workspace's pinned dependencies; it
adds no Rust code to either processor's firmware. Tests are available with
`cargo test --offline --locked --manifest-path tools/Cargo.toml -p firmware-build`.

### Original MCU App compile/link check

The `mcu` command stages public MCU inputs unchanged and offline CMSIS/STM32/RTX
dependencies supplied explicitly by the caller. It compiles the MDK App
target's C units, configured `RTX_Config.c`, the RTX sources/tick provider
and assembly into `qualification.elf`. It checks the 64 KiB ITCM/128 KiB DTCM
limits from the original [App memory settings](../Firm_MCU/ssmaster.uvprojx) and all
166 entries from the [ST startup table](../Firm_MCU/Startup/startup_stm32h750xx.s), including strong RTX
and storage IRQ overrides. The original MDK project remains the standard build.

```sh
tools/target/debug/firmware-build mcu --repo "$PWD" \
  --out tests/build/mcu-link-check \
  --toolchain /path/to/arm-gnu-toolchain-14.3.rel1 \
  --cmsis /path/to/CMSIS \
  --stm32 /path/to/STM32/Include \
  --rtx /path/to/CMSIS/RTOS2
```

The GNU App build uses link-time optimization with one partition
(`-flto=4 -flto-partition=one`), keeping `-Os` and the same source units.
For a comparison, run the same command with a fresh output directory and
append `--no-lto` after the RTX argument. The receipt records the selected
partition policy and collects the linked partition's stack report. The
original MDK project does not use these GNU flags.

Adapted from paps-g `2aaf465c` (AUD-RES-028); see the [GCC 14.3
optimization options](https://gcc.gnu.org/onlinedocs/gcc-14.3.0/gcc/Optimize-Options.html#index-flto-partition)
for `-flto` and `-flto-partition=one`. With the same FAT32-only inputs and
Arm GNU 14.3.Rel1, the App's load image goes from 60,512 to 56,412 bytes,
leaving 9,124 bytes free in ITCM. The RTX common pool remains 29,184 bytes.
Inlining increases some direct frames: `load_cover` goes from 336 to
568 bytes while its caller `disk_task` stays at 88. These are compiler
reports for those functions, not measurements of full call chains.

Use a complete Arm GNU bare-metal toolchain including newlib (the compiler-only
Homebrew package is insufficient). New output paths are required inside this
checkout's `tests/build`; prior outputs and failures are preserved. Staging
bounds files/cohorts/nesting and rejects symlinks/escapes. Commands use explicit
arguments, a separate process group and sixty-second per-command/five-minute
compile/link deadlines. No download, network, device, private preparation,
installation or boot-file update is performed.

The receipt records copied source/SDK and generated-glue hashes, compiler and
builder identities, command results, vectors, load/BSS/data bounds and compiler
direct frames. The GNU verification CRT supplies data/BSS initialization and
adapts the original ST vector/HardFault sequence; a preinclude supplies the
prior-mask return used by the [existing source](../Firm_MCU/Main/main.c), where
GCC's `__disable_irq` intrinsic returns void. It contains
no additional runtime diagnostic or USB integration. The GNU glue is a build
prerequisite, not evidence of byte equivalence to MDK or qualification for
installation. Direct frames/address gaps do not prove RTOS/IRQ peak stack use.

The existing public sources also need the standards-compatible `malloc(size_t)`
declaration/definition when compiled against GNU newlib. C11 N1570 §7.22.3.4
paragraph 1 (printed p. 349) specifies that signature; the target's `uint32_t`
and `size_t` have the same width but different C types under this toolchain.
See [the recorded prerequisite](../doc/FIRMWARE-SAVE-SAFETY.md#gnu-mcu-build-prerequisite-malloc-size-type).
Original ST and Arm notices remain in their source/dependency files. The host
Rust code is MIT; the embedded ST startup adaptation also retains its BSD
3-Clause notice and [license](crates/firmware-build/LICENSE-ST-startup).

This is GNU target compile/link accounting. It does not qualify the MDK image,
cache/bus/clock behavior, physical endian transfers, concurrent access, SD
durability, games or installed firmware.

To check RTX common pool admission in QEMU with either profile, set
`SAROO_ARM_CC` to that toolchain's `arm-none-eabi-gcc` and set
`SAROO_RTX_BUILD` to the corresponding output directory, then run
`python3 -B -m unittest discover -s tests -p test_rtx_pool.py`. This runs
the compiled RTX allocation provider in a Cortex-M7 model.

### Original FPGA project comparison

The `fpga` command builds a new copy of the original [Quartus project](../FPGA/SSMaster.qsf)
for EP4CE6F17C8, using an explicitly supplied, already installed Quartus Prime
Lite25.1std.0.1129 Linux compiler and the already cached local amd64 runtime
`sha256:d60907a0e1139b4c866760fbc76007971f7330ba1f5d47e99494acf1f8119c29`.
This is a source-only verification environment, adapted from paps-g's offline
Quartus/QEMU procedure. It never downloads a compiler/image or programs hardware.
The original project remains usable through the ordinary Quartus flow; the
specific cached runtime is a prerequisite for this pinned comparison command.

```sh
cargo build --offline --locked --release --manifest-path tools/Cargo.toml -p firmware-build
tools/target/release/firmware-build fpga --repo "$PWD" \
  --out tests/build/fpga-comparison --quartus /path/to/quartus-25.1
```

Only a local Unix-socket Docker endpoint is accepted. Compiler and original JRE
mounts are read-only; the authored Java wrapper is overlaid inside the container,
leaving the installed compiler unchanged. The pinned runtime supplies its existing
launcher/QEMU. All new build orchestration is Rust. Each map/fit/assembly/timing/
conversion phase has a recorded unique container identity, one-hour deadline,
two-CPU/four-GiB limits, networking disabled and bounded logs. On a deadline the
tool bounds removal of its own container. If the host tool is interrupted, inspect
the last recorded container before retrying. New output directories are required;
failures and prior source/ELF/bitstream/report cohorts are retained.

Public project inputs are bounded regular files. The receipt records their
SHA-256 values, exact copied-source cohort, generated setting additions, wrapper/
builder identities, runtime ID, phase commands and artifact hashes. A streaming
manifest binds every installed compiler regular file and internal symlink before
and after the build, with count/nesting/size bounds; escaping links are rejected.
No compiler or runtime binaries are distributed by this repository. Their vendor
and distribution notices remain in the caller's unchanged installation/image.

Both compared builds use the same verification-only additions to the copied QSF:
two parallel processors, Standard Fit and an additional `derive_clock_uncertainty`
SDC. Original project bytes in the checkout are unchanged. Required nonempty SOF,
RBF, JIC, fit/STA reports and finite timing rows are checked. Java/helper fatal
markers are rejected even after a zero exit status. Negative constrained slack
is reported separately from complete artifacts and returns failure. Unconstrained
external Saturn/MCU/SDRAM interfaces remain unqualified even when reported slack
passes. No hardware, clock-domain, signal-integrity or game acceptance follows.
Host-tool checks remain part of `make -C tests check-host-tools`.

## Browser and font assets

The [offline save editor](savetool/web/README.md) and
[cover editor](covertool/web/README.md) are separate host applications.
Their checked-in HTML runs directly without Rust or a server. `web-assets`
rebuilds the self-contained files from readable source modules (and the save
editor's pinned XML parser):

```sh
cargo run --locked --manifest-path tools/Cargo.toml -p web-assets -- save tools/savetool/web tools/savetool/web/index.html > tools/savetool/web/index.build.json
cargo run --locked --manifest-path tools/Cargo.toml -p web-assets -- cover tools/covertool/web tools/covertool/web/index.html > tools/covertool/web/index.build.json
```

The command prints a JSON receipt, bounds each input to 1 MiB and output to
2 MiB, preserves destinations on rejected input, and rejects input/output aliasing.
It accepts only the authored module conventions; it is not a general JavaScript
transpiler. The pinned parser and dependency-license identities are checked at build
time. The Rust linker is MIT-licensed; the application and XML dependency retain
their own licenses. Public browser/format checks are documented with the editor.

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
tools/target/debug/menu-assets merge-font tests/build/cjk-only.bin tests/build/arabic-only.bin tests/build/cjk-arabic.bin
tools/bdfont/bdfont tools/menu-assets/font-source/romanian14.bdf -adj 0 -c tests/build/romanian-only.bin
tools/target/debug/menu-assets merge-font tests/build/cjk-arabic.bin tests/build/romanian-only.bin tests/build/font_cjk.bin
cmp tests/build/font_cjk.bin Firm_Saturn/font_cjk.bin
tools/bdfont/bdfont tools/bdfont/cbdf/helvR14c.bdf -l tools/bdfont/latin.txt -h tests/build/font_latin.h font_latin
cmp tests/build/font_latin.h Firm_Saturn/font_latin.h
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
