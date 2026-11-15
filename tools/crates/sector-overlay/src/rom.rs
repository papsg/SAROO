//! Bounded SOR1 startup metadata, derived without copying a companion ROM.
use crate::{crc32, fail, mode1, Disc, Edit, Recipe, Result, Targets, RAW_BYTES};
use serde::Deserialize;
use std::collections::BTreeMap;
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct RomCart {
    path: String,
    sha256: String,
    crc32: String,
    phase: String,
    resume: u32,
}

fn lower_hex(value: &str, bytes: usize) -> bool {
    value.len() == bytes * 2
        && value
            .bytes()
            .all(|c| c.is_ascii_digit() || (b'a'..=b'f').contains(&c))
}

pub fn descriptor(recipe: &Recipe) -> Result<Option<[u8; 64]>> {
    let Some(rom) = &recipe.rom_cart else {
        return Ok(None);
    };
    let path = rom.path.as_bytes();
    if !recipe.replaces_callback
        || recipe.cart_mode != 0
        || recipe.first_read_guard.is_some()
        || !lower_hex(&rom.sha256, 32)
        || !lower_hex(&rom.crc32, 4)
        || !(16..32).contains(&path.len())
        || !path.starts_with(b"/SAROO/ISO/")
        || !path.ends_with(b".BIN")
        || !path[11..path.len() - 4]
            .iter()
            .all(|c| c.is_ascii_uppercase() || c.is_ascii_digit() || *c == b'_' || *c == b'-')
        || !(0x06002000..0x06003800).contains(&rom.resume)
        || rom.resume & 1 != 0
    {
        return fail("Invalid bounded ROM-cart descriptor");
    }
    let phase = match rom.phase.as_str() {
        "game_entry" => 1,
        "first_read" => 2,
        _ => return fail("Unsupported ROM startup phase"),
    };
    let mut d = [0; 64];
    d[..4].copy_from_slice(b"SOR1");
    d[4] = phase;
    d[8..12].copy_from_slice(&rom.resume.to_be_bytes());
    d[12..16].copy_from_slice(&0x200000u32.to_be_bytes());
    d[16..20].copy_from_slice(&u32::from_str_radix(&rom.crc32, 16)?.to_be_bytes());
    d[24..24 + path.len()].copy_from_slice(path);
    let checksum = crc32(&d[..60]);
    d[60..].copy_from_slice(&checksum.to_be_bytes());
    Ok(Some(d))
}

pub fn add_target(recipe: &Recipe, targets: &mut Targets) -> Result<()> {
    if let Some(d) = descriptor(recipe)? {
        if targets
            .insert(
                165,
                vec![Edit {
                    offset: 2000,
                    before: vec![0; 64],
                    after: d.to_vec(),
                }],
            )
            .is_some()
        {
            return fail("ROM metadata overlaps an explicit source edit");
        }
    }
    Ok(())
}

pub fn check_ip(ip: &[u8]) -> Result<()> {
    if ip.len() < 0x8000 || &ip[..16] != b"SEGA SEGASATURN " {
        return fail("ROM metadata requires the complete original initial 32 KiB transfer");
    }
    let size = u32::from_be_bytes(ip[0xe0..0xe4].try_into().unwrap());
    if !(0x1000..=0x7fc0).contains(&size) || size & 3 != 0 || ip[0x7fc0..0x8000] != [0; 64] {
        return fail("ROM metadata overlaps declared IP or occupied source padding");
    }
    Ok(())
}

pub fn witnesses(disc: &Disc) -> Result<BTreeMap<u32, [u8; RAW_BYTES]>> {
    let first = &disc.tracks[0];
    if first.mode != 1 || first.width != RAW_BYTES as u32 || first.start != 150 || first.offset != 0
    {
        return fail("ROM startup requires an original raw first track");
    }
    let mut result = BTreeMap::new();
    for fad in [150, 165] {
        let (track, offset) = disc.raw_data(fad)?;
        let mut file = File::open(&disc.images[disc.tracks[track].image].path)?;
        file.seek(SeekFrom::Start(u64::from(offset)))?;
        let mut raw = [0; RAW_BYTES];
        file.read_exact(&mut raw)?;
        mode1::check(&raw, fad)?;
        result.insert(fad, raw);
    }
    // Only the header and reserved tail are inspected; both are subsequently
    // bound to the same complete FILE hash as the source edits.
    let mut ip = [0; 0x8000];
    ip[..2048].copy_from_slice(&result[&150][16..2064]);
    ip[0x7800..].copy_from_slice(&result[&165][16..2064]);
    check_ip(&ip)?;
    Ok(result)
}
