//! SSR-172: the complete provider checks every component of its existing mask.
//! Synthetic capability values; no auxv query, crypto instruction or kernel.

#[derive(Debug)]
pub struct Counts {
    pub states: u32,
    pub comparisons: u32,
    pub group_bad: [u32; 3],
    pub single_bad: u32,
}

impl Counts {
    pub fn passed(&self) -> bool {
        self.group_bad == [0; 3] && self.single_bad == 0
    }
}

pub fn run() -> Counts {
    let mut result = Counts {
        states: 0,
        comparisons: 0,
        group_bad: [0; 3],
        single_bad: 0,
    };
    let components = [
        libc::HWCAP_AES,
        libc::HWCAP_PMULL,
        libc::HWCAP_SHA3,
        libc::HWCAP_SHA512,
        libc::HWCAP_SM3,
        libc::HWCAP_SM4,
        libc::HWCAP_SHA2,
        libc::HWCAP_DIT,
    ];
    for flags in 0u16..256 {
        for unrelated in [0, (1u64 << 63) | 1] {
            let mut caps = unrelated;
            for (bit, component) in components.iter().enumerate() {
                if flags & (1 << bit) != 0 {
                    caps |= component;
                }
            }
            let actual = [
                cpufeatures::check!(caps, "aes"),
                cpufeatures::check!(caps, "sha3"),
                cpufeatures::check!(caps, "sm4"),
            ];
            // Independent per-component decisions; extra capabilities may be
            // present. Do not mistake membership for equality of whole auxv.
            for (group, value) in actual.iter().enumerate() {
                let left = flags & (1 << (group * 2)) != 0;
                let right = flags & (1 << (group * 2 + 1)) != 0;
                result.group_bad[group] += u32::from(*value != (left && right));
                result.comparisons += 1;
            }
            result.single_bad +=
                u32::from(cpufeatures::check!(caps, "sha2") != (flags & (1 << 6) != 0));
            result.single_bad +=
                u32::from(cpufeatures::check!(caps, "dit") != (flags & (1 << 7) != 0));
            result.comparisons += 2;
            result.states += 1;
        }
    }
    result
}
