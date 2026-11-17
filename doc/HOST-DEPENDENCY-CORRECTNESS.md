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
