// SSR-171: include the complete selected predicate source. On AArch64 only,
// its architecture import is inactive, so `arch` is an explicit query facade.
// This models predicates; it neither executes x86 nor substitutes public APIs.
#[cfg(target_arch = "aarch64")]
#[path = "../../../vendor/cpufeatures/src/x86.rs"]
#[macro_use]
mod x86_predicates;

#[cfg(target_arch = "aarch64")]
use std::{cell::Cell, thread_local};

#[cfg(target_arch = "aarch64")]
thread_local! {
    static XCR0: Cell<u64> = const { Cell::new(0) };
    static READS: Cell<usize> = const { Cell::new(0) };
}

#[cfg(target_arch = "aarch64")]
mod arch {
    pub const _XCR_XFEATURE_ENABLED_MASK: u32 = 0;
    pub unsafe fn _xgetbv(index: u32) -> u64 {
        assert_eq!(index, 0);
        crate::READS.with(|n| n.set(n.get() + 1));
        crate::XCR0.with(|v| v.get())
    }
}

#[cfg(target_arch = "aarch64")]
#[derive(Clone, Copy, Default)]
#[allow(dead_code)]
struct Registers {
    eax: u32,
    ebx: u32,
    ecx: u32,
    edx: u32,
}

#[cfg(target_arch = "aarch64")]
fn registers(flags: u32, avx2: bool) -> [Registers; 2] {
    let mut cr = [Registers::default(); 2];
    cr[0].ecx = ((flags & 1) << 26) | (((flags >> 1) & 1) << 27) | (((flags >> 2) & 1) << 28);
    cr[1].ebx = u32::from(avx2) << 5;
    cr
}

#[cfg(target_arch = "aarch64")]
fn state(value: u64) {
    XCR0.with(|v| v.set(value));
    READS.with(|v| v.set(0));
}

#[cfg(target_arch = "aarch64")]
#[test]
fn avx_requires_legal_ymm_os_state() {
    let cr = registers(7, true);
    for (xcr0, expected) in [(3, false), (7, true), (1, false)] {
        state(xcr0);
        assert_eq!(check!(cr, "avx"), expected, "AVX at XCR0={xcr0}");
        assert_eq!(check!(cr, "avx2"), expected, "AVX2 at XCR0={xcr0}");
        READS.with(|v| assert_eq!(v.get(), 2));
    }
}

#[cfg(target_arch = "aarch64")]
#[test]
fn complete_predicates_match_independent_admission_and_guard_model() {
    let mut cases = 0;
    for flags in 0u32..8 {
        for avx2 in [false, true] {
            let cr = registers(flags, avx2);
            // Arbitrary masks are predicate inputs, not a claim that every
            // combination is architecturally legal to install with XSETBV.
            for xcr0 in (0u64..256).chain([
                (1u64 << 32) | 3,
                (1u64 << 32) | 7,
                (1u64 << 63) | 3,
                u64::MAX,
            ]) {
                state(xcr0);
                let expected = flags == 7 && (xcr0 & 2 != 0) && (xcr0 & 4 != 0);
                assert_eq!(check!(cr, "avx"), expected, "flags={flags} XCR0={xcr0}");
                assert_eq!(check!(cr, "avx2"), expected && avx2);
                READS.with(|v| assert_eq!(v.get(), if flags & 3 == 3 { 2 } else { 0 }));
                cases += 1;
            }
        }
    }
    assert_eq!(cases, 4160);
}

#[cfg(target_arch = "aarch64")]
#[test]
fn legacy_features_do_not_require_xgetbv() {
    for enabled in [false, true] {
        let mut cr = [Registers::default(); 2];
        cr[0].ecx = if enabled { (1 << 25) | (1 << 9) } else { 0 };
        cr[0].edx = if enabled { 1 << 26 } else { 0 };
        cr[1].ebx = if enabled { 1 << 29 } else { 0 };
        state(0);
        assert_eq!(check!(cr, "aes"), enabled);
        assert_eq!(check!(cr, "sha"), enabled);
        assert_eq!(check!(cr, "sse2"), enabled);
        assert_eq!(check!(cr, "ssse3"), enabled);
        READS.with(|v| assert_eq!(v.get(), 0));
    }
}

#[cfg(target_arch = "aarch64")]
cpufeatures::new!(native, "aes", "sha2");
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
cpufeatures::new!(native, "avx");

#[test]
fn native_public_cache_matches_host_feature_detection() {
    #[cfg(target_arch = "aarch64")]
    let expected = std::arch::is_aarch64_feature_detected!("aes")
        && std::arch::is_aarch64_feature_detected!("sha2");
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    let expected = std::arch::is_x86_feature_detected!("avx");
    #[cfg(any(target_arch = "aarch64", target_arch = "x86", target_arch = "x86_64"))]
    {
        let (token, value) = native::init_get();
        assert_eq!(value, expected);
        assert_eq!(token.get(), value);
        assert_eq!(native::get(), value);
        assert_eq!(native::init().get(), value);
    }
}

// Compile and execute this contract on AArch64 Linux/Android. Other hosts
// retain the x86 predicate and native API tests above.
#[cfg(all(
    target_arch = "aarch64",
    any(target_os = "linux", target_os = "android")
))]
#[path = "support/cpufeatures_arm_contract.rs"]
mod arm_contract;

#[cfg(all(
    target_arch = "aarch64",
    any(target_os = "linux", target_os = "android")
))]
#[test]
fn linux_existing_hwcap_masks_require_all_members_and_allow_supersets() {
    let result = arm_contract::run();
    assert_eq!(result.states, 512);
    assert_eq!(result.comparisons, 2560);
    assert!(result.passed(), "{result:?}");
}
