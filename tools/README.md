# Host asset tools

## SD sector overlay compiler

Use a reviewed JSON recipe and its matching local CUE/BIN files. The
compiler checks complete source FILE hashes and writes a new pack to an
existing output directory. The `name` command gives the expected filename.

```sh
cargo build --offline --locked --manifest-path tools/Cargo.toml -p sector-overlay
tools/target/debug/sector-overlay name recipe.json
tools/target/debug/sector-overlay compile recipe.json disc.cue output/1234ABCD.SOV
tools/target/debug/sector-overlay bundle output/1234ABCD.SOV first.json first.cue second.json second.cue
make -C tests check-sector-overlay
```

Use the actual name returned by `name` in place of `1234ABCD.SOV`.
An optional `disc_id` field gives the exact sixteen IP product/version
bytes, including spaces. Omit it to retain the older product-only filename.
The `bundle` command accepts two to twelve reviewed recipe/CUE pairs. Each
must fit one sector record and share the same product, optional revision and launch
actions. Mount selects exactly one matching alternative.
Short and contiguous edits can share a bundle, which uses the newest
record ABI required by its inputs.
See the [overlay format and source checks](../doc/SD-SECTOR-OVERLAYS.md).

### Offline PCA/PRS check

For the reviewed PCA container, `sector-overlay decode-pca INPUT.PCA NEW-OUTPUT.BIN`
checks the complete PRS stream and prints input/output hashes and declared
destination segments. Input and output are bounded to 1 MiB; at most sixteen
disjoint, aligned high-Work-RAM segments are accepted, with lengths in
longwords. Header count zero denotes a flat stream without declared destinations.
Truncation, an invalid back-reference, trailing bytes, declared-length mismatch
or output overflow refuses before creating output. Existing output is never
overwritten. This reader adapts paps-g's native-source review; it adds no codec
or execution hook to the firmware. A format decode does not establish native
CPU state or hardware timing. Compare original and edited complete outputs,
with source/hash pins, rather than accepting a matching edited window alone.

### Logical-source verification

`sector-overlay verify-source MANIFEST.json NEW-OUTPUT-DIRECTORY` replaces
manual logical-file extraction comparisons and repeated decoder commands.
It reads pinned original files and checks complete postimages through the
same record encoder used by `compile` and a caller-pinned host C verifier.
It adds no firmware code or device access. Deployment packs still use the
ordinary compiler's complete CUE/FILE checks.

Build the existing C test verifier, then pin its resulting binary:

```sh
mkdir -p tests/build
cc -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -IFirm_MCU/Saturn tests/sector_overlay.c Firm_MCU/Saturn/sector_overlay.c \
  -o tests/build/overlay-source-verifier
tools/target/debug/sector-overlay verify-source manifest.json tests/build/source-check
```

The manifest contains `recipe`, `pack` and `decoder` objects, each with `path`
and lowercase `sha256`, plus `programs`. Each program contains a pinned
`source` object, `lba` and the expected complete `postimage_sha256`. Paths
resolve against the manifest's directory. Supply one to eight nonoverlapping
logical files of at most 1 MiB each. The decoder must be your trusted build
of the test verifier; its binary identity is pinned, rather than inferred
from its filename. Synthetic examples and negative controls run in
`tests/test_sector_overlay_later.py` as part of `make -C tests check-sector-overlay`.

Preflight checks all pins, extents, preimages, complete postimage hashes and
the recipe/reference-record correspondence before creating output. It refuses
missing edited extents and nonmatching records. Missing read-only guard
sectors are listed explicitly: `all_target_sources_verified` stays false.
Those CRCs are retained only to compare the other reference fields, not called
fresh source checks. `full_track_identity_verified` always stays false.
Generated padding after an unedited partial last sector is not original raw
padding. Target sectors require their complete original logical payloads.

The fresh output retains generated streams, a copy of the pinned verifier,
per-program logs and a structured receipt. `decoder.SOV` carries the test
product ID for the host fixture; deployment
uses the normal compiler and original CUE. Each verifier process has a
30-second deadline; an incomplete/failed decode cannot produce a successful
receipt. Existing outputs are refused. This is logical-source and host-decoder
verification, with no complete-disc, bus/cache, timing or game qualification.

### First-read guard and receipt audits

Optional `first_read_guard` supplies a complete first-file SHA-256 and reason.
`compile` derives one bounded ABI4 word edit and its completion marker from
the original IP/ISO, including attributes and interleaved logical file mapping.
See [loaded-file qualification and capability](../doc/SPECIAL-SERVICE-PROFILES.md).
`verify-source` additionally requires the original IP and complete first file;
its logical-file manifests remain contiguous extents. Include complete target
sector payloads and distinguish supplied/reconstructed padding from original
raw evidence. Full track identity remains a separate compiler check.

`sector-overlay verify-receipt RECEIPT.json NEW-REPORT.json` replaces repeated
one-off historical inventory hashing. It accepts `inputs[]` / `pins[]` or a `files{}`
map with paths, byte counts and lowercase SHA-256. Historical `source_inputs{}`
and `inputs{}` maps containing SHA-256 alone are also accepted; their absent
byte counts remain explicit. Relative paths resolve from the receipt, or an
explicit `--base DIRECTORY` for inventories authored relative to a repository.
It streams bounded regular files, writes a new report and prints
counts. Missing/changed/unreadable pins cause nonzero exit after preserving
the report; malformed receipts or I/O errors refuse. Existing output is never
overwritten. Keep reports with local paths in ignored evidence storage. A
matching inventory proves current file hashes, not historical execution,
source ownership, game correctness or hardware qualification.

Optional `rom_cart` profiles compile ABI5 packs with bounded SOR1 metadata in
verified original IP padding. They require the advertised ROM-capable initial
launch. `verify-source` requires the complete original32KiB initial transfer
as well as every edited first/later file; it validates padding/IP bounds and
reproduces the descriptor rather than trusting reference metadata. The tool
does not bundle or distribute companion ROMs. See
[ROM startup, pairing and limits](../doc/SPECIAL-SERVICE-PROFILES.md#checked-generic-rom-startup).

Optional `expire_after_fad` compiles a callback-replacing ABI6 profile with a
separate completion sector after every edit/guard. It cannot combine with the
loaded-file guard, ROM service or alternative bundles. The compiler authenticates
that sector from the same complete source FILE; the MCU preserves retries and
waits for all edited records before retiring delivery. Launch ownership survives
until unload. Older firmware refuses the format. Full Git history is required by
the previous-decoder control, which compiles public `8ee7dad` directly.
See [finite-source admission, tests, costs and limits](../doc/SD-SECTOR-OVERLAYS.md#finite-native-source-edits).

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

### Vandal Hearts native startup comparison

The [native-source profile](../doc/VANDAL-HEARTS.md) includes reproducible
`vandal-source` assembly/ELF checks and `sector-overlay vandal-inputs`
extraction/production-C checks using caller-owned media. It documents all
commands, exact hashes, finite delivery, attribution and remaining sound,
reset and physical limits. No proprietary media is distributed.

`firmware-build vandal` builds a headless interpreter and the public
[`tests/vandal_native.c`](../tests/vandal_native.c) comparison. It uses a clean,
caller-supplied local [Yabause source checkout](https://github.com/Yabause/yabause/tree/7e38821dbac265490f115e163c523a939acda759)
at that exact revision. It never downloads, installs firmware, opens a device
or changes that checkout. This command currently supports macOS with local
CMake, Ninja and Apple's C compiler/frameworks.

```sh
tools/target/debug/firmware-build vandal --repo "$PWD" \
  --out tests/build/vandal-native-check \
  --upstream /path/to/local/yabause-checkout --inputs /path/to/inputs.json
```

The input JSON contains exactly five objects named `bios`, `ip_original`,
`ip_postimage`, `packed_original` and `packed_postimage`. Each has a `path`
(relative to that JSON or absolute) and a complete lowercase `sha256`.
Inputs are bounded regular files of524,288 /6,144 /6,144 /4,608 /4,608 bytes.
Supply your own BIOS and exact DLCT v20240517 media; neither is distributed.
Required original SHA-256 identities are:

| Input | SHA-256 |
| --- | --- |
| BIOS | `96e106f740ab448cf89f0dd49dfbac7fe5391cb6bd6e14ad5e3061c13330266f` |
| Complete IP | `15c5342a7fdfdbded2d3bb6f7ec3eb688fc5a401e47310d3a25aa71bc5288697` |
| Packed bootstrap tail | `e328e1f325f073d8a5843c15bf869bbe31b76de38d994aed9b6532b5d542c0d5` |

Only IP offsets`f00h–f79h` and packed offsets`d6h–d7h` may differ in the
postimages. The native comparison executes the original AIP with the retained
tpunix/yzb48-byte control and the replacement native AIP, original CD callees,
supplied BIOS unpacker and original VDP routines. It covers ten cases: cache
off/on with five legal entry-state/stack seeds. It compares complete2MiB work
RAM after accounting only for intended input/code differences, including the
156-byte maximum observed private stack,288 VDP register bytes,512KiB VDP2
VRAM and4KiB CRAM. R0/R1/PR differ at handoff; their actual first definitions
execute before comparing the complete live register state. Four broken-code
controls must fail for their runtime effects: bad packed instruction, source
pointer, reader argument and delayed literal load.

Only two build adaptations apply to the staged dependency: omit its unused
obsolete macOS optical-drive driver and raise Musashi's old CMake minimum.
No emulator instruction/cache/device code changes. Network/frontends are
disabled. Original dependency files/notices, copied fixture/inputs, exact
commands, compiler/tool versions, logs and hash receipt stay in the fresh
ignored output. The workflow bounds source/output sizes, each subprocess to60
seconds and the complete build/check to300 seconds; an interrupted/failed run
has no successful receipt and preserves its logs.

This host fixture is GPL-2.0-or-later; see
[`tests/LICENSE-vandal-native`](../tests/LICENSE-vandal-native). Yabause retains
its own copyright and GPL notices. The Rust orchestrator keeps its existing
license and is not linked into firmware. Original workaround attribution:
[tpunix addition](https://github.com/tpunix/SAROO/commit/e6854bf651f55b1f413ea8fa700a10bff7562cae),
[yzb revision](https://github.com/tpunix/SAROO/commit/f6a35a4d1d5a32af507f7d6d02edc8b7ab169fbc);
the native rewrite and comparison are papsg's adaptation.

CD replies/FIFO prefix, BIOS clock effects and one SMPC read are explicit
models. Scheduling-call counts are execution bounds, not physical timing.
This does not qualify complete BIOS/game boot, repeated reads, reset lifetime,
PCM/audio, gameplay or a Saturn. It does not publish or activate a recipe.
The standard Cargo test command above also tests refusal of incomplete native
results, collateral edits and malformed extents without proprietary assets.

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

### Actual RTX API contracts, offline

`firmware-build rtos-check` uses the same explicit compiler/CMSIS/STM32/RTX
arguments as `mcu`, plus `--case delay|queue|timer|pool|message|event|retire`.
Choose a fresh `--out tests/build/...`
for each invocation. `--baseline` selects the original provider for that case
and intentionally fails the contract after displaying its false-success results;
`--lto` repeats with LTO and `--archive` checks owned-source selection against
a stock-kernel GNU archive. Requires local `qemu-system-arm` supporting
`mps2-an500`. Inputs, exact commands, logs and receipts remain in the output.
No download, device access or test firmware installation occurs. See
[RTOS correctness](../doc/RTOS-CORRECTNESS.md) for controls, licenses and limits.

For pool publication use `--atomic=0` or `--atomic=1` and one of
`--phase=release`, `--phase=allocate`, `--phase=masked`, `--phase=after`.
Defaults are atomic mode1 and release. Release/allocation pend a test IRQ at
the copied provider's link/count boundary; masked preserves an already-masked
caller; after uses the uninstrumented provider and pends the IRQ after return.
Only the copied test input receives hooks; both its original and instrumented
hashes are recorded. Repeated or misplaced pool options refuse. The original
mode1 release/allocation runs intentionally fail with256 wrong observations
each, while mode0, masked and after controls pass. Existing results are retained.

Message checks accept `--atomic=0|1` and
`--scenario=reuse|waiters|walk|space` (default reuse). Walk injects only a
pending IRQ into a copied inserter; other scenarios use uninstrumented code.
`--old-space` with space reverts only the available-slot expression and must
fail with816 wrong observations. `--omit-walk-invalidation` with walk must
fail a payload check. These controls never modify the production provider.
The original `--baseline --scenario=reuse` must report432 rejected replacement
puts, retaining thread/after-IRQ/reset controls. `--lto` and `--archive` remain
available. Test sources, mutations, input hashes, commands and logs are preserved.

Event checks accept `--atomic=0|1` and
`--scenario=single|waiters|nested|legacy` (default single). Single checks full
and last-slot admission with an actual waiting thread; waiters checks three
priority/FIFO waiters. Nested schedules a higher IRQ between admission and
flag publication in a copied provider. `--ignore-event-refusal` is a negative
control for nested only and must fail its refusal assertion. Legacy checks the
original void wrapper's overflow notification. `--baseline --scenario=single`
selects only the original event frontend and must report32 lost wakeups.

`--case retire --atomic=0|1` checks ring-slot retirement with real event
callbacks and a blocked waiter. `--baseline` reverses only the consumer's
retirement order, retaining the new admission helper; mode1 must report16
lost callbacks, while mode0 remains a valid passing control. Only copied
providers receive scheduling hooks. The owned source has no test callbacks.
Both cases support `--lto` and `--archive`; nested's baseline must use the
refusal mutation instead of the stock frontend. The required support headers
are pinned: `rtx_lib.h` permits only its declared postprocess prototype
addition; all other bytes and the original SDK hashes must match.

### Compile the existing conditional BOOT body

`firmware-build mcu` accepts `--boot-object` alongside optional `--no-lto`.
The normal App is still compiled, linked and verified with its original flags.
A separate complete `Main/main.c` object additionally defines `BOOT=1` and
omits LTO for inspection. It is never passed to the App linker or installed.
The receipt separates its source/object hashes, `load_app` bytes and compiler
frames from normal App resources; `boot-object.asm` and symbols are retained.
Repeated/unknown options refuse before output staging. This is a source/compiler
check, without a complete BOOT image, proprietary Keil linkage or device access.
