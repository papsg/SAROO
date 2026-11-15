//! Exact ISO first-file qualification for one bounded source word edit.
use crate::{crc32, fail, hex, mode1, ChecksumGuard, Disc, Recipe, Result, Targets, RAW_BYTES};
use sha2::{Digest, Sha256};
use std::collections::BTreeMap;
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};

pub struct Plan {
    pub descriptor: [u8; 24],
    pub completion: u32,
    pub witnesses: BTreeMap<u32, [u8; RAW_BYTES]>,
}

struct Reader<'a> {
    disc: &'a Disc,
    files: BTreeMap<usize, File>,
    witnesses: BTreeMap<u32, [u8; RAW_BYTES]>,
}

impl Reader<'_> {
    fn sector(&mut self, lba: u32) -> Result<[u8; 2048]> {
        let fad = lba.checked_add(150).ok_or("ISO address overflow")?;
        if let std::collections::btree_map::Entry::Vacant(slot) = self.witnesses.entry(fad) {
            let (track, offset) = self.disc.raw_data(fad)?;
            let image = self.disc.tracks[track].image;
            if let std::collections::btree_map::Entry::Vacant(slot) = self.files.entry(image) {
                slot.insert(File::open(&self.disc.images[image].path)?);
            }
            let file = self.files.get_mut(&image).unwrap();
            file.seek(SeekFrom::Start(u64::from(offset)))?;
            let mut raw = [0; RAW_BYTES];
            file.read_exact(&mut raw)?;
            mode1::check(&raw, fad)?;
            slot.insert(raw);
        }
        Ok(self.witnesses[&fad][16..2064].try_into().unwrap())
    }

    fn file(&mut self, blocks: &[u32], length: usize) -> Result<Vec<u8>> {
        let mut data = Vec::with_capacity(blocks.len() * 2048);
        for &block in blocks {
            data.extend_from_slice(&self.sector(block)?);
        }
        data.truncate(length);
        Ok(data)
    }
}

fn dual32(data: &[u8], offset: usize) -> Result<u32> {
    let field = data.get(offset..offset + 8).ok_or("Truncated ISO number")?;
    let value = u32::from_le_bytes(field[..4].try_into().unwrap());
    if value != u32::from_be_bytes(field[4..].try_into().unwrap()) {
        return fail("ISO 32-bit endian fields differ");
    }
    Ok(value)
}

fn dual16(data: &[u8], offset: usize) -> Result<u16> {
    let field = data.get(offset..offset + 4).ok_or("Truncated ISO number")?;
    let value = u16::from_le_bytes(field[..2].try_into().unwrap());
    if value != u16::from_be_bytes(field[2..].try_into().unwrap()) {
        return fail("ISO 16-bit endian fields differ");
    }
    Ok(value)
}

fn record(data: &[u8]) -> Result<(u32, u32)> {
    if data.len() < 34
        || usize::from(data[0]) != data.len()
        || data[32] == 0
        || 33 + usize::from(data[32]) > data.len()
    {
        return fail("Invalid ISO directory record");
    }
    Ok((dual32(data, 2)?, dual32(data, 10)?))
}

fn blocks(data: &[u8], volume: u32) -> Result<Vec<u32>> {
    let (lba, length) = record(data)?;
    let (attributes, unit, gap) = (u32::from(data[1]), u32::from(data[26]), u32::from(data[27]));
    if data[25] & 0x80 != 0
        || dual16(data, 28)? != 1
        || (unit == 0 && gap != 0)
        || (data[25] & 2 != 0 && unit != 0)
        || (unit != 0 && attributes != 0 && attributes != unit)
        || length == 0
        || length > 0x100000
    {
        return fail("Unsupported ISO file section or volume");
    }
    let start = u64::from(lba)
        + u64::from(attributes)
        + if unit != 0 && attributes != 0 {
            u64::from(gap)
        } else {
            0
        };
    let mut result = Vec::new();
    for index in 0..length.div_ceil(2048) {
        let at = start
            + u64::from(index)
            + if unit == 0 {
                0
            } else {
                u64::from(index / unit) * u64::from(gap)
            };
        if lba >= volume || at >= u64::from(volume) {
            return fail("ISO file data exceeds its volume");
        }
        result.push(at as u32);
    }
    Ok(result)
}

pub fn prepare(disc: &Disc, recipe: &Recipe, targets: &Targets) -> Result<Plan> {
    let ChecksumGuard { sha256, reason } = recipe
        .first_read_guard
        .as_ref()
        .ok_or("Missing first-read guard")?;
    crate::check_text(&[reason])?;
    let first_track = &disc.tracks[0];
    if first_track.mode != 1
        || first_track.width != RAW_BYTES as u32
        || first_track.start != 150
        || first_track.offset != 0
        || targets.len() != 1
        || recipe.writes.len() != 1
        || !recipe.replaces_callback
        || !recipe.guards.is_empty()
    {
        return fail("First-read guard requires one word edit and an original raw first track");
    }
    let mut reader = Reader {
        disc,
        files: BTreeMap::new(),
        witnesses: BTreeMap::new(),
    };
    let ip = reader.sector(0)?;
    let ip_size = u32::from_be_bytes(ip[0xe0..0xe4].try_into().unwrap());
    let load = u32::from_be_bytes(ip[0xf0..0xf4].try_into().unwrap());
    if &ip[..16] != b"SEGA SEGASATURN "
        || !(0x1000..=0x8000).contains(&ip_size)
        || ip_size & 3 != 0
        || load & 3 != 0
        || !(0x06002000 + ip_size..0x06100000).contains(&load)
    {
        return fail("Unsupported declared IP or first-load address");
    }
    let pvd = reader.sector(16)?;
    let volume = dual32(&pvd, 80)?;
    if &pvd[..7] != b"\x01CD001\x01"
        || volume == 0
        || volume > 449850
        || dual16(&pvd, 120)? != 1
        || dual16(&pvd, 124)? != 1
        || dual16(&pvd, 128)? != 2048
    {
        return fail("Unsupported ISO primary volume");
    }
    let root = pvd
        .get(156..156 + usize::from(pvd[156]))
        .ok_or("Invalid root record")?;
    let (_, root_size) = record(root)?;
    if root[25] & 2 == 0 || !(68..=65536).contains(&root_size) {
        return fail("Unsupported ISO root directory");
    }
    let root_blocks = blocks(root, volume)?;
    let directory = reader.file(&root_blocks, root_size as usize)?;
    let mut entries = Vec::new();
    let mut at = 0;
    while at < directory.len() {
        let length = usize::from(directory[at]);
        if length == 0 {
            at = (at / 2048 + 1) * 2048;
            continue;
        }
        if at + length > directory.len() || at / 2048 != (at + length - 1) / 2048 {
            return fail("ISO directory record crosses a block or extent");
        }
        let item = &directory[at..at + length];
        record(item)?;
        entries.push(item);
        at += length;
    }
    if entries.len() < 3 || entries[0][33] != 0 || entries[1][33] != 1 || entries[2][25] & 2 != 0 {
        return fail("ISO file ID 2 is not the first file after dot entries");
    }
    let (_, length) = record(entries[2])?;
    if length < 2 || length > 0x06100000 - load {
        return fail("First-read file exceeds Work RAM");
    }
    let data_blocks = blocks(entries[2], volume)?;
    let mut data = reader.file(&data_blocks, length as usize)?;
    if hex(&Sha256::digest(&data)) != *sha256 {
        return fail("Complete first-read source SHA-256 differs");
    }
    let (&fad, edits) = targets.first_key_value().unwrap();
    if edits.len() != 1 || edits[0].before.len() != 2 {
        return fail("First-read guard requires one two-byte edit");
    }
    let index = data_blocks
        .iter()
        .position(|&lba| lba + 150 == fad)
        .ok_or("Edit is outside first-file data")?;
    let edit = &edits[0];
    let offset = index * 2048 + edit.offset - 16;
    if offset & 1 != 0 || offset + 2 > data.len() || data[offset..offset + 2] != edit.before {
        return fail("First-read instruction span or preimage differs");
    }
    data[offset..offset + 2].copy_from_slice(&edit.after);
    let mut descriptor = [0; 24];
    descriptor[..4].copy_from_slice(&load.to_be_bytes());
    descriptor[4..8].copy_from_slice(&length.to_be_bytes());
    descriptor[8..12].copy_from_slice(&crc32(&data).to_be_bytes());
    descriptor[12..16].copy_from_slice(&(offset as u32).to_be_bytes());
    descriptor[16..18].copy_from_slice(&edit.before);
    descriptor[18..20].copy_from_slice(&edit.after);
    let checksum = crc32(&descriptor[..20]);
    descriptor[20..24].copy_from_slice(&checksum.to_be_bytes());
    let completion = data_blocks.last().unwrap() + 150;
    if completion <= fad {
        return fail("First-read edit needs a later completion sector");
    }
    Ok(Plan {
        descriptor,
        completion,
        witnesses: reader.witnesses,
    })
}

pub fn attach(pack: &mut [u8], descriptor: &[u8; 24]) -> Result<()> {
    if pack.len() != 720
        || pack[5] != 2
        || pack[24 + 8] != 1
        || pack[24 + 348 + 8] != 0
        || crc32(&descriptor[..20]) != u32::from_be_bytes(descriptor[20..].try_into().unwrap())
    {
        return fail("Invalid loaded-guard pack shape");
    }
    pack[4] = 4;
    pack[48..72].copy_from_slice(descriptor);
    pack[24 + 348 + 9] = 1;
    pack[20..24].fill(0);
    let checksum = crc32(pack);
    pack[20..24].copy_from_slice(&checksum.to_be_bytes());
    Ok(())
}

pub fn metadata(pack: &[u8]) -> Result<([u8; 24], u32)> {
    if pack.len() != 720
        || pack[4] != 4
        || pack[5] != 2
        || pack[6] != 0
        || pack[7] & 0x80 == 0
        || pack[32] != 1
        || pack[38] != 2
        || pack[380] != 0
        || pack[381] != 1
    {
        return fail("Invalid first-read reference pack");
    }
    let descriptor: [u8; 24] = pack[48..72].try_into().unwrap();
    if crc32(&descriptor[..20]) != u32::from_be_bytes(descriptor[20..].try_into().unwrap()) {
        return fail("First-read descriptor checksum differs");
    }
    Ok((
        descriptor,
        u32::from_be_bytes(pack[372..376].try_into().unwrap()),
    ))
}
