# Local generic-array 0.14.7 correction (SSR-169)

This directory preserves all sixteen files of the locked crates.io package,
including its MIT license, changelog, manifests, build script and VCS metadata.
Original archive SHA-256:
`85649ca51fd72272d7821adaf274ad91c288277713d9c18820d8499a7ff69e9a`.
The package VCS identifies `1397d2bb40c406901b7827161b73b34b056cbe3a`.
Apart from this note, only two lines in `src/hex.rs` change.

Both hexadecimal formatters cap the requested precision to the available
representation before computing the byte extent or slicing. Precision can
truncate the available output; it cannot require nonexistent bytes. This avoids
panics for valid empty, small and chunked arrays, including usize::MAX precision.
Default and within-capacity output, odd digits, bounded stack/chunk buffers,
input borrowing and output-writer error propagation remain.

The workspace path patch selects this exact version for existing SHA/digest
users without changing the Cargo registry cache or upgrading other packages.
The explicit workspace exclusion keeps optional/dev package requirements out of
this workspace. Preserve provenance and the correction when updating/retiring
this patch. No MCU/menu/FPGA source or physical/game qualification is implied;
normal SAROO callers requesting excessive precision have not been established.

Run the maintained public-API regression with:

```sh
cargo test --offline --locked --manifest-path tools/Cargo.toml -p firmware-build --test generic_array_hex
```

It checks all output precisions through capacity plus three and maximum usize
values, both x/X forms, empty/small/chunk-boundary arrays, output errors, input
preservation and actual SHA256 digest output. Other formatting flags, optional
serde/zeroize, all compiler/platform combinations and whole-provider memory
safety are outside this correction's qualification. See the independent
SSR-169 review for original counterexamples and remaining limits.

The unchanged upstream build script uses `relaxed_coherence` without declaring
it to newer check-cfg. Local path builds expose four existing cfg warnings that
registry lint caps hid; this patch leaves that unrelated source unchanged.
