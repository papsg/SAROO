# Local cpufeatures 0.2.17 correction

SSR-171: AVX runtime detection requires both XMM and YMM OS state in XCR0.
The AVX table entry uses the existing `ymm` capability check, retaining the
XSAVE/OSXSAVE and CPU feature checks. Other selectors and cache behavior are
unchanged. Intel SDM Vol. 1, order 253665-086US (December 2024), section 13.3,
printed page 13-5, gives the requirement.

This is a conditional dependency API defect. Current SAROO uses SHA-256;
no normal caller of the faulty explicit AVX selector has been established.
It is not a Saturn firmware or game fix. SSR-172 is covered separately below.

All 16 original package members, including licenses and upstream provenance,
are retained. SSR-171 changes only `src/x86.rs`.
Archive SHA-256:
`59ed5838eebb26a2bb2e58f6d5b5316989ae9d08bab10e0e6d103e656d1b0280`.
This workspace patch does not alter the global registry cache. The public
`firmware-build` contract test checks the complete x86 predicate over explicit
CPUID/XCR0 inputs and the native public cache against host feature detection.
It does not execute guest XCR0 state changes or qualify other platforms.

SSR-172: Linux/Android AArch64 predicates require every bit in each existing
HWCAP group. AES/PMULL, SHA3/SHA512 and SM3/SM4 no longer pass when only one
component is present. Existing single-bit checks and unrelated capability
supersets retain their behavior. This changes only one predicate line in
`src/aarch64.rs`; the Apple branch and SSR-171 x86 row remain unchanged.
The public Linux AArch64 contract test covers all group combinations and
supersets. The QEMU acceptance used a temporary bare guest with explicit
`getauxval` inputs and is described in the public correctness note; it is not
a claim about a physical partial-feature CPU or ordinary SAROO behavior.
