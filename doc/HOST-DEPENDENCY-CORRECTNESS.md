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
