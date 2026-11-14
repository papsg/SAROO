//! Read-only logical-file checks, followed by the caller-pinned C test decoder.
//! This does not authenticate a complete disc or qualify a game or device.
use crate::disc::{read_bounded, RAW_BYTES};
use crate::{
    crc32, disc_id_bytes, encode, fail, hex, mode1, product_bytes, targets, write_pack, Recipe,
    Result,
};
use serde::Deserialize;
use serde_json::json;
use sha2::{Digest, Sha256};
use std::collections::BTreeMap;
use std::fs::{self, File};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Pin {
    path: PathBuf,
    sha256: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Program {
    source: Pin,
    lba: u32,
    postimage_sha256: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Manifest {
    recipe: Pin,
    pack: Pin,
    decoder: Pin,
    programs: Vec<Program>,
}

fn digest(value: &str) -> bool {
    value.len() == 64
        && value
            .bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
}

fn pinned(base: &Path, pin: &Pin, limit: usize) -> Result<(PathBuf, Vec<u8>)> {
    if !digest(&pin.sha256) {
        return fail("A source pin must contain a lowercase SHA-256");
    }
    let path = fs::canonicalize(base.join(&pin.path))?;
    let data = read_bounded(&path, limit)?;
    if hex(&Sha256::digest(&data)) != pin.sha256 {
        return fail("Pinned input SHA-256 differs");
    }
    Ok((path, data))
}

fn raw(fad: u32, data: &[u8]) -> [u8; RAW_BYTES] {
    fn bcd(n: u32) -> u8 {
        ((n / 10) * 16 + n % 10) as u8
    }
    let mut sector = [0; RAW_BYTES];
    sector[..12].copy_from_slice(&mode1::SYNC);
    sector[12..16].copy_from_slice(&[bcd(fad / 4500), bcd(fad / 75 % 60), bcd(fad % 75), 1]);
    sector[16..16 + data.len()].copy_from_slice(data);
    mode1::rebuild(&mut sector);
    sector
}

struct Prepared {
    first: u32,
    hits: usize,
    original: Vec<u8>,
    post: Vec<u8>,
    report: serde_json::Value,
}

fn decode(binary: &Path, out: &Path, index: usize, program: &Prepared) -> Result<()> {
    let original = out.join(format!("program-{index}.raw"));
    let post = out.join(format!("program-{index}.post.raw"));
    write_pack(&original, &program.original)?;
    write_pack(&post, &program.post)?;
    let log = File::create_new(out.join(format!("program-{index}.log")))?;
    let mut child = Command::new(binary)
        .arg("stream")
        .arg(out.join("decoder.SOV"))
        .arg(original)
        .arg(post)
        .arg(program.first.to_string())
        .arg(program.hits.to_string())
        .env("UBSAN_OPTIONS", "halt_on_error=1")
        .stdin(Stdio::null())
        .stdout(Stdio::from(log.try_clone()?))
        .stderr(Stdio::from(log))
        .spawn()?;
    let start = Instant::now();
    loop {
        if let Some(status) = child.try_wait()? {
            return if status.success() {
                Ok(())
            } else {
                fail("C decoder rejected a complete source stream; see its retained log")
            };
        }
        if start.elapsed() >= Duration::from_secs(30) {
            let _ = child.kill();
            let _ = child.wait();
            return fail("C decoder exceeded its 30-second bound");
        }
        std::thread::sleep(Duration::from_millis(5));
    }
}

pub fn run(manifest_path: &Path, output: &Path) -> Result<()> {
    let manifest_path = fs::canonicalize(manifest_path)?;
    let base = manifest_path.parent().ok_or("Manifest has no parent")?;
    let manifest: Manifest = serde_json::from_slice(&read_bounded(&manifest_path, 64 * 1024)?)?;
    if manifest.programs.is_empty() || manifest.programs.len() > 8 {
        return fail("Supply one to eight bounded original logical files");
    }
    let (_, recipe_data) = pinned(base, &manifest.recipe, 64 * 1024)?;
    let recipe: Recipe = serde_json::from_slice(&recipe_data)?;
    let edits = targets(&recipe)?;
    let product = product_bytes(&recipe.product_id)?;
    disc_id_bytes(recipe.disc_id.as_deref(), &product)?;
    let (_, pack) = pinned(base, &manifest.pack, 24 + 12 * 348)?;
    if pack.len() != 24 + edits.len() * 348 {
        return fail("Reference pack length differs from the recipe");
    }
    let (decoder_path, decoder) = pinned(base, &manifest.decoder, 32 * 1024 * 1024)?;
    let mut intervals = Vec::new();
    let mut originals = BTreeMap::new();
    let mut programs = Vec::new();
    for program in &manifest.programs {
        let (_, data) = pinned(base, &program.source, 0x100000)?;
        let count = data.len().div_ceil(2048);
        if count == 0
            || !digest(&program.postimage_sha256)
            || program.lba >= 449850
            || count as u32 > 449850 - program.lba
        {
            return fail("Invalid bounded source extent or postimage hash");
        }
        let first = program.lba + 150;
        let end = first + count as u32;
        if intervals.iter().any(|&(a, b)| first < b && a < end) {
            return fail("Original logical-file extents overlap");
        }
        intervals.push((first, end));
        let mut original = Vec::with_capacity(count * RAW_BYTES);
        let mut post = Vec::with_capacity(count * RAW_BYTES);
        let mut post_hash = Sha256::new();
        let mut hits = 0;
        for (index, payload) in data.chunks(2048).enumerate() {
            let fad = first + index as u32;
            let sector = raw(fad, payload);
            let mut after = sector;
            if let Some(changes) = edits.get(&fad) {
                if payload.len() != 2048 {
                    return fail("A targeted sector requires its complete original payload");
                }
                originals.insert(fad, sector);
                for change in changes {
                    let offset = change.offset;
                    let width = change.before.len();
                    if offset - 16 + width > payload.len()
                        || sector[offset..offset + width] != change.before
                    {
                        return fail("Recipe preimage differs in an original logical file");
                    }
                    after[offset..offset + width].copy_from_slice(&change.after);
                }
                if !changes.is_empty() {
                    hits += 1;
                    mode1::rebuild(&mut after);
                }
            }
            post_hash.update(&after[16..16 + payload.len()]);
            original.extend_from_slice(&sector);
            post.extend_from_slice(&after);
        }
        if hex(&post_hash.finalize()) != program.postimage_sha256 {
            return fail("Complete logical postimage SHA-256 differs");
        }
        programs.push(Prepared {
            first,
            hits,
            original,
            post,
            report: json!({"lba":program.lba,"bytes":data.len(),"sectors":count,
                "source_sha256":program.source.sha256,"postimage_sha256":program.postimage_sha256,
                "edited_sectors":hits,"generated_padding_bytes":count*2048-data.len()}),
        });
    }
    let mut unavailable_guards = Vec::new();
    for (&fad, changes) in &edits {
        if let std::collections::btree_map::Entry::Vacant(entry) = originals.entry(fad) {
            if !changes.is_empty() {
                return fail("An edited sector is missing from the supplied original files");
            }
            unavailable_guards.push(fad);
            entry.insert(raw(fad, &[0; 2048]));
        }
    }
    let mut expected = encode(&recipe, product, &edits, originals)?;
    // Missing read-only guards are not fresh source checks. Retain their
    // reference CRC solely to compare the other metadata/available records.
    for (index, &fad) in edits.keys().enumerate() {
        if unavailable_guards.contains(&fad) {
            let at = 24 + index * 348;
            expected[at + 4..at + 8].copy_from_slice(&pack[at + 4..at + 8]);
        }
    }
    expected[20..24].fill(0);
    let checksum = crc32(&expected);
    expected[20..24].copy_from_slice(&checksum.to_be_bytes());
    if expected != pack {
        return fail("Reference pack differs from the recipe and available original sectors");
    }
    fs::create_dir(output)?;
    let output = fs::canonicalize(output)?;
    let binary = output.join(if cfg!(windows) {
        "decoder.exe"
    } else {
        "decoder"
    });
    write_pack(&binary, &decoder)?;
    fs::set_permissions(&binary, fs::metadata(decoder_path)?.permissions())?;
    let mut for_decoder = pack;
    for_decoder[8..18].copy_from_slice(b"T-TEST    ");
    for_decoder[20..24].fill(0);
    let checksum = crc32(&for_decoder);
    for_decoder[20..24].copy_from_slice(&checksum.to_be_bytes());
    write_pack(&output.join("decoder.SOV"), &for_decoder)?;
    const TOOL_INPUTS: [(&str, &[u8]); 6] = [
        ("main.rs", include_bytes!("main.rs")),
        ("source_check.rs", include_bytes!("source_check.rs")),
        ("mode1.rs", include_bytes!("mode1.rs")),
        ("disc.rs", include_bytes!("disc.rs")),
        ("Cargo.toml", include_bytes!("../Cargo.toml")),
        ("Cargo.lock", include_bytes!("../../../Cargo.lock")),
    ];
    let tool_sources: BTreeMap<_, _> = TOOL_INPUTS
        .into_iter()
        .map(|(name, data)| (name, hex(&Sha256::digest(data))))
        .collect();
    let mut receipt = json!({"complete":false,"recipe_sha256":manifest.recipe.sha256,
        "pack_sha256":manifest.pack.sha256,"decoder_sha256":manifest.decoder.sha256,
        "tool_source_sha256":tool_sources,
        "programs":programs.iter().map(|p| &p.report).collect::<Vec<_>>(),
        "all_target_sources_verified":unavailable_guards.is_empty(),
        "unverified_guard_fads":unavailable_guards,"full_track_identity_verified":false,
        "qualification":"Pinned logical files and recipe/available records; caller-pinned C test decoder. Generated final padding is not original raw padding. No device or game qualification."});
    for (index, program) in programs.iter().enumerate() {
        if let Err(error) = decode(&binary, &output, index, program) {
            receipt["error"] = json!(error.to_string());
            write_pack(
                &output.join("receipt.json"),
                &serde_json::to_vec_pretty(&receipt)?,
            )?;
            return Err(error);
        }
    }
    receipt["complete"] = json!(true);
    write_pack(
        &output.join("receipt.json"),
        &serde_json::to_vec_pretty(&receipt)?,
    )?;
    println!("{}", serde_json::to_string_pretty(&receipt)?);
    Ok(())
}
