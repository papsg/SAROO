use crate::disc::RAW_BYTES;
use crate::{fail, Result};

pub const DATA_START: usize = 16;
pub const TAIL_START: usize = 2064;
pub const SYNC: [u8; 12] = [0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0];

const fn edc_table() -> [u32; 256] {
    let mut table = [0; 256];
    let mut i = 0;
    while i < table.len() {
        let mut value = i as u32;
        let mut bit = 0;
        while bit < 8 {
            value = (value >> 1) ^ (0xd8018001 & 0u32.wrapping_sub(value & 1));
            bit += 1;
        }
        table[i] = value;
        i += 1;
    }
    table
}

const fn parity_tables() -> ([u8; 256], [u8; 256]) {
    let mut double = [0; 256];
    let mut inverse = [0; 256];
    let mut i = 0;
    while i < double.len() {
        let value = i as u8;
        let doubled = (value << 1) ^ if value & 128 == 0 { 0 } else { 0x1d };
        double[i] = doubled;
        inverse[(value ^ doubled) as usize] = value;
        i += 1;
    }
    (double, inverse)
}

const EDC: [u32; 256] = edc_table();
const PARITY: ([u8; 256], [u8; 256]) = parity_tables();

fn ecc(source: &[u8], parity: &mut [u8], minors: usize, step: usize, stride: usize) {
    let majors = parity.len() / 2;
    debug_assert_eq!(source.len(), majors * minors);
    for major in 0..majors {
        let mut index = (major / 2) * step + major % 2;
        let (mut a, mut b) = (0, 0);
        for _ in 0..minors {
            let value = source[index];
            index = (index + stride) % source.len();
            a = PARITY.0[(a ^ value) as usize];
            b ^= value;
        }
        a = PARITY.1[(PARITY.0[a as usize] ^ b) as usize];
        parity[major] = a;
        parity[major + majors] = a ^ b;
    }
}

pub fn rebuild(sector: &mut [u8; RAW_BYTES]) {
    let mut edc = 0u32;
    for &byte in &sector[..TAIL_START] {
        edc = (edc >> 8) ^ EDC[((edc as u8) ^ byte) as usize];
    }
    sector[TAIL_START..2068].copy_from_slice(&edc.to_le_bytes());
    sector[2068..2076].fill(0);
    let (source, parity) = sector.split_at_mut(2076);
    ecc(&source[12..], &mut parity[..172], 24, 2, 86);
    let (source, parity) = sector.split_at_mut(2248);
    ecc(&source[12..], parity, 43, 86, 88);
}

fn bcd(value: u32) -> u8 {
    ((value / 10) * 16 + value % 10) as u8
}

pub fn check(sector: &[u8; RAW_BYTES], fad: u32) -> Result<()> {
    if sector[..12] != SYNC
        || sector[15] != 1
        || sector[12..15] != [bcd(fad / 4500), bcd(fad / 75 % 60), bcd(fad % 75)]
    {
        return fail(format!("FAD {fad} disagrees with the raw Mode 1 header"));
    }
    let mut canonical = *sector;
    rebuild(&mut canonical);
    if canonical[TAIL_START..] != sector[TAIL_START..] {
        return fail(format!("FAD {fad} has noncanonical Mode 1 EDC/ECC"));
    }
    Ok(())
}
