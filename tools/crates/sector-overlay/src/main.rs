//! Compile reviewed SD sector recipes against read-only game images.
mod disc;
mod mode1;

use disc::{Disc, RAW_BYTES};
use mode1::{DATA_START, TAIL_START};
use serde::Deserialize;
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::OsString;
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::Path;

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

const HEADER_BYTES: usize = 24;
const RECORD_BYTES: usize = 348;
const MAX_RECORDS: usize = 12;
const MAX_RECIPE_BYTES: usize = 64 * 1024;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Recipe {
    abi: u8,
    product_id: String,
    disc_id: Option<String>,
    origin: String,
    title: String,
    summary: String,
    rationale: String,
    tracks: Vec<TrackSource>,
    writes: Vec<WriteSource>,
    #[serde(default)]
    guards: Vec<GuardSource>,
    #[serde(default)]
    replaces_callback: bool,
    #[serde(default)]
    cart_mode: u8,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct GuardSource {
    fad: u32,
    reason: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct TrackSource {
    number: usize,
    sha256: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct WriteSource {
    fad: u32,
    offset: usize,
    before: String,
    after: String,
    effect: String,
    reason: String,
}

struct Edit {
    offset: usize,
    before: Vec<u8>,
    after: Vec<u8>,
}

type Targets = BTreeMap<u32, Vec<Edit>>;

fn fail<T>(message: impl Into<Box<dyn std::error::Error>>) -> Result<T> {
    Err(message.into())
}

fn product_bytes(id: &str) -> Result<[u8; 10]> {
    if id.is_empty()
        || id.len() > 10
        || !id
            .bytes()
            .all(|byte| byte.is_ascii_uppercase() || byte.is_ascii_digit() || byte == b'-')
    {
        return fail("Invalid product ID");
    }
    let mut product = [b' '; 10];
    product[..id.len()].copy_from_slice(id.as_bytes());
    Ok(product)
}

fn disc_id_bytes<'a>(id: Option<&'a str>, product: &[u8; 10]) -> Result<Option<&'a [u8]>> {
    let Some(id) = id else { return Ok(None) };
    let bytes = id.as_bytes();
    if bytes.len() != 16
        || !bytes.starts_with(product)
        || !bytes.iter().all(|b| (b' '..=b'~').contains(b))
    {
        return fail("Invalid full disc ID: use the exact 16 printable IP bytes");
    }
    Ok(Some(bytes))
}

fn crc32(data: &[u8]) -> u32 {
    let mut crc = u32::MAX;
    for &byte in data {
        crc ^= u32::from(byte);
        for _ in 0..8 {
            crc = (crc >> 1) ^ (0xedb88320 & 0u32.wrapping_sub(crc & 1));
        }
    }
    !crc
}

fn hex(bytes: &[u8]) -> String {
    const DIGITS: &[u8; 16] = b"0123456789abcdef";
    let mut text = String::with_capacity(bytes.len() * 2);
    for &byte in bytes {
        text.push(char::from(DIGITS[(byte >> 4) as usize]));
        text.push(char::from(DIGITS[(byte & 15) as usize]));
    }
    text
}

fn decode_hex(value: &str) -> Result<Vec<u8>> {
    let bytes = value.len() / 2;
    if value.len() % 2 != 0
        || !(matches!(bytes, 1 | 2 | 4) || (5..=44).contains(&bytes))
        || !value.bytes().all(|b| b.is_ascii_hexdigit())
    {
        return fail("Edits require 1, 2, 4 or 5–44 hex-encoded bytes");
    }
    (0..value.len())
        .step_by(2)
        .map(|i| Ok(u8::from_str_radix(&value[i..i + 2], 16)?))
        .collect()
}

fn check_text(fields: &[&str]) -> Result<()> {
    if fields.iter().any(|field| field.trim().is_empty()) {
        return fail("Recipe provenance and edit explanations must be nonempty");
    }
    Ok(())
}

fn targets(recipe: &Recipe) -> Result<Targets> {
    if recipe.abi != 2
        || recipe.writes.is_empty()
        || recipe.writes.len() > MAX_RECORDS * 4
        || !matches!(recipe.cart_mode, 0 | 1 | 4)
    {
        return fail("Unsupported recipe ABI or write count");
    }
    check_text(&[
        &recipe.origin,
        &recipe.title,
        &recipe.summary,
        &recipe.rationale,
    ])?;
    let mut by_fad: Targets = BTreeMap::new();
    for write in &recipe.writes {
        check_text(&[&write.effect, &write.reason])?;
        let before = decode_hex(&write.before)?;
        let after = decode_hex(&write.after)?;
        if !(150..450000).contains(&write.fad)
            || !(DATA_START..TAIL_START).contains(&write.offset)
            || write.offset + before.len() > TAIL_START
            || before.len() != after.len()
            || before == after
        {
            return fail("Invalid FAD, payload span or replacement");
        }
        by_fad.entry(write.fad).or_default().push(Edit {
            offset: write.offset,
            before,
            after,
        });
    }
    for guard in &recipe.guards {
        check_text(&[&guard.reason])?;
        if !(150..450000).contains(&guard.fad) || by_fad.insert(guard.fad, Vec::new()).is_some() {
            return fail("Invalid, duplicated or edited guard sector");
        }
    }
    if by_fad.len() > MAX_RECORDS {
        return fail("Too many target sectors");
    }
    for edits in by_fad.values_mut() {
        edits.sort_unstable_by_key(|edit| edit.offset);
        if edits.len() > 4
            || (edits.len() != 1 && edits.iter().any(|edit| edit.before.len() > 4))
            || edits
                .windows(2)
                .any(|pair| pair[0].offset + pair[0].before.len() > pair[1].offset)
        {
            return fail("Too many or overlapping edits in a sector");
        }
    }
    Ok(by_fad)
}

/* Hash each source once, capturing each edited sector during that same read. */
fn read_targets(
    disc: &Disc,
    recipe: &Recipe,
    targets: &Targets,
) -> Result<BTreeMap<u32, [u8; RAW_BYTES]>> {
    let mut positions: BTreeMap<usize, Vec<(u32, u32)>> = BTreeMap::new();
    let mut used = BTreeSet::new();
    for &fad in targets.keys() {
        let (track, offset) = disc.raw_data(fad)?;
        used.insert(track + 1);
        positions
            .entry(disc.tracks[track].image)
            .or_default()
            .push((offset, fad));
    }
    let mut declarations = BTreeMap::new();
    for item in &recipe.tracks {
        if !used.contains(&item.number)
            || item.sha256.len() != 64
            || !item
                .sha256
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
            || declarations
                .insert(item.number, item.sha256.as_str())
                .is_some()
        {
            return fail("Invalid, duplicate or unused source track hash");
        }
    }
    if declarations.len() != used.len() {
        return fail("Source track hashes must cover every touched track");
    }
    let mut image_hashes = BTreeMap::new();
    for (track, hash) in declarations {
        let image = disc.tracks[track - 1].image;
        if image_hashes
            .insert(image, hash)
            .is_some_and(|prior| prior != hash)
        {
            return fail("Tracks in the same FILE must agree on the image hash");
        }
    }
    let mut sectors = BTreeMap::new();
    let mut chunk = vec![0; 64 * 1024];
    for (index, mut offsets) in positions {
        offsets.sort_unstable();
        let image = &disc.images[index];
        let mut input = File::open(&image.path)?;
        let mut digest = Sha256::new();
        let mut position = 0;
        for (offset, fad) in offsets {
            if offset < position {
                return fail("Overlapping source sectors");
            }
            hash_bytes(&mut input, &mut digest, &mut chunk, offset - position)?;
            let mut sector = [0; RAW_BYTES];
            input.read_exact(&mut sector)?;
            digest.update(sector);
            sectors.insert(fad, sector);
            position = offset + RAW_BYTES as u32;
        }
        hash_bytes(&mut input, &mut digest, &mut chunk, image.bytes - position)?;
        if input.read(&mut chunk[..1])? != 0 || hex(&digest.finalize()) != image_hashes[&index] {
            return fail("Source track SHA-256 differs");
        }
    }
    Ok(sectors)
}

fn hash_bytes(
    file: &mut File,
    digest: &mut Sha256,
    chunk: &mut [u8],
    mut bytes: u32,
) -> Result<()> {
    while bytes != 0 {
        let count = (bytes as usize).min(chunk.len());
        file.read_exact(&mut chunk[..count])?;
        digest.update(&chunk[..count]);
        bytes -= count as u32;
    }
    Ok(())
}

fn compile(
    recipe: &Recipe,
    cue: &Path,
    product: [u8; 10],
    disc_id: Option<&[u8]>,
) -> Result<Vec<u8>> {
    let targets = targets(recipe)?;
    let disc = Disc::read(cue)?;
    let ip = disc.read_ip()?;
    if ip[..10] != product {
        return fail("Recipe product ID disagrees with the original Saturn IP");
    }
    if disc_id.is_some_and(|id| ip != id) {
        return fail("Full disc ID disagrees with the original Saturn IP");
    }
    let mut originals = read_targets(&disc, recipe, &targets)?;
    let size = HEADER_BYTES + targets.len() * RECORD_BYTES;
    let mut pack = vec![0; size];
    pack[..4].copy_from_slice(b"SSO1");
    let long_edit = targets.values().flatten().any(|edit| edit.before.len() > 4);
    pack[4] = if recipe.replaces_callback || long_edit {
        3
    } else if recipe.guards.is_empty() {
        1
    } else {
        2
    };
    pack[5] = targets.len() as u8;
    pack[7] = recipe.cart_mode | if recipe.replaces_callback { 0x80 } else { 0 };
    pack[8..18].copy_from_slice(&product);
    pack[18..20].copy_from_slice(&(size as u16).to_be_bytes());
    for ((fad, edits), record) in targets
        .into_iter()
        .zip(pack[HEADER_BYTES..].chunks_exact_mut(RECORD_BYTES))
    {
        let mut sector = originals.remove(&fad).unwrap();
        mode1::check(&sector, fad)?;
        record[..4].copy_from_slice(&fad.to_be_bytes());
        record[4..8].copy_from_slice(&crc32(&sector).to_be_bytes());
        if edits.is_empty() {
            continue;
        }
        record[8] = edits.len() as u8;
        for edit in &edits {
            if sector[edit.offset..edit.offset + edit.before.len()] != edit.before {
                return fail(format!("Original bytes differ at FAD {fad}"));
            }
        }
        if edits[0].before.len() > 4 {
            let edit = &edits[0];
            record[8] = 0x80 | edit.before.len() as u8;
            record[12..14].copy_from_slice(&(edit.offset as u16).to_be_bytes());
            record[16..16 + edit.after.len()].copy_from_slice(&edit.after);
            sector[edit.offset..edit.offset + edit.after.len()].copy_from_slice(&edit.after);
        } else {
            for (edit, descriptor) in edits.iter().zip(record[12..60].chunks_exact_mut(12)) {
                let width = edit.before.len();
                let bytes = &mut sector[edit.offset..edit.offset + width];
                descriptor[..2].copy_from_slice(&(edit.offset as u16).to_be_bytes());
                descriptor[2] = width as u8;
                descriptor[4..4 + width].copy_from_slice(&edit.before);
                descriptor[8..8 + width].copy_from_slice(&edit.after);
                bytes.copy_from_slice(&edit.after);
            }
        }
        mode1::rebuild(&mut sector);
        record[60..].copy_from_slice(&sector[TAIL_START..]);
    }
    let checksum = crc32(&pack);
    pack[20..24].copy_from_slice(&checksum.to_be_bytes());
    Ok(pack)
}

fn read_recipe(path: &Path) -> Result<Recipe> {
    Ok(serde_json::from_slice(&disc::read_bounded(
        path,
        MAX_RECIPE_BYTES,
    )?)?)
}

fn pack_name(recipe: &Recipe) -> Result<String> {
    let product = product_bytes(&recipe.product_id)?;
    let disc_id = disc_id_bytes(recipe.disc_id.as_deref(), &product)?;
    Ok(format!("{:08X}.SOV", crc32(disc_id.unwrap_or(&product))))
}

fn bundle(args: &[OsString]) -> Result<()> {
    if args.len() < 5 || args.len() > 1 + MAX_RECORDS * 2 || args.len() % 2 != 1 {
        return fail("bundle requires an output path and two to twelve recipe/CUE pairs");
    }
    let output = Path::new(&args[0]);
    let mut policy = None;
    let mut header = Vec::new();
    let mut records = Vec::new();
    let mut abi = 1;
    for pair in args[1..].chunks_exact(2) {
        let recipe = read_recipe(Path::new(&pair[0]))?;
        let product = product_bytes(&recipe.product_id)?;
        let disc_id = disc_id_bytes(recipe.disc_id.as_deref(), &product)?;
        let name = pack_name(&recipe)?;
        if output.file_name().is_none_or(|leaf| leaf != name.as_str()) {
            return fail(format!("Output filename must be {name}"));
        }
        let single = compile(&recipe, Path::new(&pair[1]), product, disc_id)?;
        if single[5] != 1 {
            return fail("Each alternative must touch exactly one sector");
        }
        let identity = (recipe.product_id, recipe.disc_id, single[7]);
        if policy.as_ref().is_some_and(|prior| *prior != identity) {
            return fail("Alternatives must share the product, revision and launch actions");
        }
        policy = Some(identity);
        abi = abi.max(single[4]);
        header = single[..HEADER_BYTES].to_vec();
        records.push(single[HEADER_BYTES..].to_vec());
    }
    records.sort_unstable_by(|a, b| a[..8].cmp(&b[..8]));
    if records.windows(2).any(|pair| pair[0][..8] == pair[1][..8]) {
        return fail("Duplicate alternative source sector");
    }
    header[4] = abi;
    header[5] = records.len() as u8;
    header[6] = 1;
    let mut pack = header;
    for record in records {
        pack.extend_from_slice(&record);
    }
    let size = pack.len() as u16;
    pack[18..20].copy_from_slice(&size.to_be_bytes());
    pack[20..24].fill(0);
    let checksum = crc32(&pack);
    pack[20..24].copy_from_slice(&checksum.to_be_bytes());
    write_pack(output, &pack)?;
    println!("{}: {} bytes", output.display(), pack.len());
    Ok(())
}

fn write_pack(path: &Path, bytes: &[u8]) -> Result<()> {
    let mut file = OpenOptions::new().write(true).create_new(true).open(path)?;
    if let Err(error) = file.write_all(bytes).and_then(|()| file.sync_all()) {
        drop(file);
        let _ = fs::remove_file(path);
        return Err(error.into());
    }
    Ok(())
}

fn run() -> Result<()> {
    let args: Vec<_> = std::env::args_os().skip(1).collect();
    let usage =
        "sector-overlay name RECIPE.json\nsector-overlay compile RECIPE.json DISC.cue OUTPUT.SOV\nsector-overlay bundle OUTPUT.SOV RECIPE.json DISC.cue RECIPE.json DISC.cue [RECIPE.json DISC.cue ...]";
    if args.first().is_some_and(|arg| arg == "bundle") {
        return bundle(&args[1..]);
    }
    let (operation, path, extra) = match args.as_slice() {
        [op, path] if op == "name" => (op, path, None),
        [op, path, cue, out] if op == "compile" => {
            (op, path, Some((Path::new(cue), Path::new(out))))
        }
        [flag] if flag == "--help" || flag == "-h" => {
            println!("{usage}");
            return Ok(());
        }
        _ => return fail(usage),
    };
    let recipe = read_recipe(Path::new(path))?;
    let product = product_bytes(&recipe.product_id)?;
    let disc_id = disc_id_bytes(recipe.disc_id.as_deref(), &product)?;
    let name = pack_name(&recipe)?;
    if operation == "name" {
        println!("{name}");
    } else if let Some((cue, path)) = extra {
        if path.file_name().is_none_or(|leaf| leaf != name.as_str()) {
            return fail(format!("Output filename must be {name}"));
        }
        let pack = compile(&recipe, cue, product, disc_id)?;
        write_pack(path, &pack)?;
        println!(
            "{name}: {} bytes, SHA-256 {}",
            pack.len(),
            hex(&Sha256::digest(pack))
        );
    }
    Ok(())
}

fn main() -> std::process::ExitCode {
    match run() {
        Ok(()) => std::process::ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("sector-overlay: {error}");
            std::process::ExitCode::FAILURE
        }
    }
}
