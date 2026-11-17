# Locked host dependency corrections

These corrections apply to Rust tooling under `tools/`, not to the Saturn MCU,
menu or FPGA images. The vendored packages retain their original licenses and
metadata. The repository's changes are identified in each vendor directory's
`SAROO-PATCH.md`.

## SSR-169: bound generic-array hexadecimal precision

The locked `generic-array` 0.14.7 `LowerHex` and `UpperHex` implementations use
the requested precision as a slice extent. A precision greater than the two
digits available per byte panics, even for a valid empty array. Rust describes
precision for nonnumeric output as a maximum width, and `Formatter::precision`
returns that requested bound: [Rust formatting reference, Precision](https://doc.rust-lang.org/std/fmt/#precision),
[Formatter::precision](https://doc.rust-lang.org/core/fmt/struct.Formatter.html#method.precision).
The original package and its hex implementation are available in the
[generic-array 0.14.7 source](https://docs.rs/crate/generic-array/0.14.7/source/).

This adapts the independent paps-g correction `7a787e2d` (SSR-169). It caps
precision to the available digit count before calculating byte extents. Both
upstream formatter branches are otherwise unchanged. The vendored MIT package,
original metadata and exact two-line change are recorded in
[`tools/vendor/generic-array/SAROO-PATCH.md`](../tools/vendor/generic-array/SAROO-PATCH.md).
The local Cargo path patch selects the existing locked version; it does not
upgrade `sha2` or alter any firmware code. No original tpunix issue or PR was
identified for this independent correction.

Reproduce and verify from the repository root:

```sh
cargo test --offline --locked --manifest-path tools/Cargo.toml -p firmware-build --test generic_array_hex
cargo build --offline --locked --release --manifest-path tools/Cargo.toml --workspace
```

The first command's four methods fail on the original locked registry package
with bounds panics for empty, short, chunked and SHA-256 arrays. On this patch
they pass 39,592 lower/upper formatting calls, checking default, zero, odd,
within-capacity, over-capacity and `usize::MAX` precision, unchanged input and
writer-error propagation. The release workspace build and formatting check
pass. The upstream package's undeclared `relaxed_coherence` cfg produces four
warnings when built as a local path; it is unchanged here. No ordinary SAROO
caller requesting excessive precision, Saturn effect or physical qualification
is claimed.

## SSR-171: require YMM OS state for AVX

The locked `cpufeatures` 0.2.17 AVX selector checks the CPU's AVX capability
and OSXSAVE, but its table row requests only XMM state. With XCR0=3 (x87 and
SSE state, no YMM state), it reports AVX available. Intel requires enabling
YMM state through XCR0 bit 2 before AVX instructions can execute:
[Intel 64 and IA-32 SDM, Vol. 1 §13.3](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html).
The [original cpufeatures 0.2.17 package](https://docs.rs/crate/cpufeatures/0.2.17/source/)
contains the selector and its CPU/OS guards.

This adapts paps-g `2221698e` (SSR-171) and changes only the AVX table row
from its existing `xmm` requirement to `ymm`. The CPU AVX, XSAVE and OSXSAVE
guards, AVX2 and other selectors, compile-time promises and public cache API
remain unchanged. The original Apache-2.0/MIT package members and provenance
are retained under [`tools/vendor/cpufeatures`](../tools/vendor/cpufeatures),
with the one-line difference recorded in its `SAROO-PATCH.md`. Two trailing
spaces in the original package manifests are preserved byte-for-byte. No
original tpunix issue or PR was identified for this independent correction.

```sh
cargo test --offline --locked --manifest-path tools/Cargo.toml -p firmware-build --test cpufeatures_contract
cargo check --offline --locked --manifest-path tools/Cargo.toml -p firmware-build --tests --target x86_64-apple-darwin
cargo build --offline --locked --release --manifest-path tools/Cargo.toml --workspace
```

The first command includes the complete x86 predicate source with controlled
CPUID/XCR0 inputs on the host. The original row fails the XCR0=3 legal-state
case and the 4,160-vector admission oracle; legacy and native API controls pass.
The corrected row passes all four methods, and the actual x86 target source
type-checks. This public regression uses a query facade; it does not execute
XGETBV on an x86 guest or test every OS. Normal SAROO tooling has no identified
caller of the explicit AVX selector, so no runtime fault, performance change,
Saturn effect or game qualification is claimed.

## SSR-172: require complete AArch64 capability groups

The Linux/Android AArch64 selector in locked `cpufeatures` 0.2.17 combines
several individual OS capability bits into one requested Rust feature. Its old
predicate accepted any overlap with that mask, so a machine advertising only
one component could be treated as supporting both. The existing groups are
AES with PMULL, SHA3 with SHA512, and SM3 with SM4. Linux publishes these as
separate HWCAP flags, while the Rust target-feature definitions name the paired
features: [Linux arm64 ELF HWCAP documentation](https://www.kernel.org/doc/html/v5.16/arm64/elf_hwcaps.html),
[Rust Reference, AArch64 target features](https://doc.rust-lang.org/reference/attributes/codegen.html#target_feature).

This adapts paps-g `1d27efe8` (SSR-172) after SSR-171. The predicate now
requires every bit of each existing mask; extra unrelated bits remain valid.
Only one line in `src/aarch64.rs` changes. The Apple branch, mask definitions,
single-bit checks, public cache and x86 correction remain unchanged. Original
package licenses and source attribution are retained in the vendor directory.
No original tpunix issue or PR was identified for this independent correction.

```sh
cargo check --offline --locked --manifest-path tools/Cargo.toml -p firmware-build --tests --target aarch64-unknown-linux-gnu
cargo test --offline --locked --manifest-path tools/Cargo.toml -p firmware-build --test cpufeatures_contract
cargo build --offline --locked --release --manifest-path tools/Cargo.toml --workspace
```

On an AArch64 Linux/Android test host, run the contract with
`cargo test --offline --locked --manifest-path tools/Cargo.toml -p firmware-build --test cpufeatures_contract`;
it exercises 512 capability
inputs and 2,560 complete-provider comparisons. The current Mac builds that
test for Linux but cannot execute it as a native Cargo test. For acceptance,
a temporary bare AArch64 QEMU guest linked the current public vendor package
and locked `libc` 0.2.191, supplying `getauxval` values explicitly. The original
predicate produced seven false positives in 18 public API cases and 768 group
errors in the 512-state matrix; 11 public controls and all single-bit checks
passed. The corrected predicate produced zero errors and retained positive and
extra-bit controls. The temporary guest was kept outside this repository; no
device operation or product diagnostic was added.

The guest supplies OS capability data rather than reading a Linux kernel's
actual auxiliary vector. Neither a physical partial-feature CPU, Android
execution, cryptographic instruction use, normal SAROO caller nor Saturn/game
effect is established. Separate implied-feature mapping questions remain open.
