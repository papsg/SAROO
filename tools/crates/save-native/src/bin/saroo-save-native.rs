//! Offline, lossless conversion to native device-0 accounting. No device I/O.
use serde_json::json;
use sha2::{Digest, Sha256};
use std::collections::HashSet;
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Seek, Write};
use std::path::{Path, PathBuf};
type Result<T> = std::result::Result<T, String>;
const SLOT: usize = 65536;
const MAX: usize = 510 * 58 - 30;
fn word(b: &[u8], at: usize) -> usize {
    u16::from_be_bytes(b[at..at + 2].try_into().unwrap()) as usize
}
fn long(b: &[u8], at: usize) -> usize {
    u32::from_be_bytes(b[at..at + 4].try_into().unwrap()) as usize
}
fn put16(b: &mut [u8], at: usize, value: usize) {
    b[at..at + 2].copy_from_slice(&(value as u16).to_be_bytes());
}
fn put32(b: &mut [u8], at: usize, value: usize) {
    b[at..at + 4].copy_from_slice(&(value as u32).to_be_bytes());
}
fn bit(b: &[u8], n: usize) -> bool {
    b[64 + n / 8] & (1 << (n % 8)) != 0
}
fn mark(b: &mut [u8], n: usize) {
    b[64 + n / 8] |= 1 << (n % 8);
}
#[derive(Debug, PartialEq, Eq)]
struct Save {
    metadata: [u8; 32],
    payload: Vec<u8>,
}
fn parse(b: &[u8]) -> Result<(bool, Vec<Save>)> {
    if b.len() != SLOT {
        return Err("Complete 64 KiB slot required".into());
    }
    let native = long(b, 8) == 32768 && word(b, 12) == 64;
    if &b[..8] != b"SaroSave"
        || long(b, 8) != if native { 32768 } else { SLOT }
        || word(b, 12) != if native { 64 } else { 128 }
        || b[64] & if native { 3 } else { 1 } != if native { 3 } else { 1 }
    {
        return Err("Invalid slot geometry/signature/reservation".into());
    }
    let reserved = if native { 2 } else { 1 };
    let mut seen = [false; 512];
    seen[..reserved].fill(true);
    let mut headers = Vec::new();
    let mut block = word(b, 62);
    let claim = |n: usize, seen: &mut [bool; 512]| -> Result<()> {
        if n < reserved || n >= 512 || seen[n] || !bit(b, n) {
            return Err("Invalid, shared or cyclic slot allocation".into());
        }
        seen[n] = true;
        Ok(())
    };
    while block != 0 {
        claim(block, &mut seen)?;
        headers.push(block);
        block = word(b, block * 128 + 62);
    }
    let mut names = HashSet::new();
    let mut saves = Vec::new();
    for block in headers {
        let header = &b[block * 128..(block + 1) * 128];
        let size = long(header, 12);
        if header[0] == 0 || size > if native { MAX } else { 510 * 128 } {
            return Err("Invalid save name or payload extent".into());
        }
        let name: Vec<_> = header[..11]
            .iter()
            .copied()
            .take_while(|b| *b != 0)
            .collect();
        if !names.insert(name) {
            return Err("Duplicate save filename".into());
        }
        let mut payload = Vec::with_capacity(size);
        if native {
            payload.extend_from_slice(&header[32..32 + size.min(28)]);
        }
        let expected = if native {
            (size + 29) / 58
        } else {
            size.div_ceil(128)
        };
        let mut count = 0;
        for n in 0..512 {
            if bit(header, n) {
                claim(n, &mut seen)?;
                count += 1;
                let bytes = (if native { 58 } else { 128 }).min(size - payload.len());
                payload.extend_from_slice(&b[n * 128..n * 128 + bytes]);
            }
        }
        if count != expected || payload.len() != size {
            return Err("Incomplete or excessive payload allocation".into());
        }
        saves.push(Save {
            metadata: header[..32].try_into().unwrap(),
            payload,
        });
    }
    // Unknown allocated bytes cannot be discarded during a lossless conversion.
    if (reserved..512).any(|i| bit(b, i) != seen[i]) {
        return Err("Orphan allocation: recover it before conversion".into());
    }
    Ok((native, saves))
}
fn encode(key: &[u8], saves: &[Save]) -> Result<Vec<u8>> {
    let required: usize = saves.iter().map(|s| 1 + (s.payload.len() + 29) / 58).sum();
    if saves.iter().any(|s| s.payload.len() > MAX) || required > 510 {
        return Err(format!(
            "Native pool requires {required} blocks; capacity is 510. Original preserved; no files dropped."
        ));
    }
    let mut out = vec![0; SLOT];
    out[..8].copy_from_slice(b"SaroSave");
    put32(&mut out, 8, 32768);
    put16(&mut out, 12, 64);
    put16(&mut out, 14, 510 - required);
    out[32..48].copy_from_slice(key);
    out[64] = 3;
    let mut next = 2;
    let mut previous = 0;
    for save in saves {
        let header = next;
        next += 1;
        mark(&mut out, header);
        put16(&mut out, previous * 128 + 62, header);
        out[header * 128..header * 128 + 32].copy_from_slice(&save.metadata);
        let inline = save.payload.len().min(28);
        out[header * 128 + 32..header * 128 + 32 + inline].copy_from_slice(&save.payload[..inline]);
        for chunk in save.payload[inline..].chunks(58) {
            mark(&mut out, next);
            mark(&mut out[header * 128..(header + 1) * 128], next);
            out[next * 128..next * 128 + chunk.len()].copy_from_slice(chunk);
            next += 1;
        }
        previous = header;
    }
    let (_, decoded) = parse(&out)?;
    if decoded != saves {
        return Err("Independent converted payload/metadata readback differs".into());
    }
    Ok(out)
}
struct Temp(PathBuf);
impl Drop for Temp {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.0);
    }
}
fn create_temp(path: PathBuf) -> Result<(Temp, File)> {
    let file = OpenOptions::new()
        .write(true)
        .read(true)
        .create_new(true)
        .open(&path)
        .map_err(|e| e.to_string())?;
    // Only exclusive creation grants cleanup ownership.
    Ok((Temp(path), file))
}
fn hash_file(file: &mut File) -> Result<String> {
    file.rewind().map_err(|e| e.to_string())?;
    let mut hash = Sha256::new();
    let mut b = [0; SLOT];
    loop {
        let n = file.read(&mut b).map_err(|e| e.to_string())?;
        if n == 0 {
            break;
        }
        hash.update(&b[..n]);
    }
    Ok(format!("{:x}", hash.finalize()))
}
fn journal_stamp(input: &Path) -> Result<Option<String>> {
    let path = input.with_file_name("SAVEJRN.BIN");
    let metadata = match path.symlink_metadata() {
        Ok(value) => value,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error.to_string()),
    };
    if !metadata.is_file() || metadata.len() > 512 * 1024 {
        return Err("Regular, bounded companion SAVEJRN.BIN required".into());
    }
    let mut bytes = Vec::new();
    File::open(path)
        .map_err(|e| e.to_string())?
        .take(512 * 1024 + 1)
        .read_to_end(&mut bytes)
        .map_err(|e| e.to_string())?;
    // Match the established journal's inactive-header rule. Retained body
    // bytes after a cleared 512-byte header cannot authorize a replay.
    if bytes.len() > 512 * 1024
        || (!bytes.is_empty() && (bytes.len() < 512 || bytes[..512].iter().any(|byte| *byte != 0)))
    {
        return Err("Pending or malformed SAVEJRN.BIN: recover it with the old matched firmware before conversion".into());
    }
    Ok(Some(format!("{:x}", Sha256::digest(bytes))))
}
fn run(input: &Path, output: Option<&Path>) -> Result<serde_json::Value> {
    let journal = journal_stamp(input)?;
    if !input
        .symlink_metadata()
        .map_err(|e| e.to_string())?
        .is_file()
    {
        return Err("Regular source archive required".into());
    }
    let mut source = File::open(input).map_err(|e| e.to_string())?;
    let size = source.metadata().map_err(|e| e.to_string())?.len();
    if size < SLOT as u64 || size > SLOT as u64 * 4096 || size % SLOT as u64 != 0 {
        return Err("Archive must contain complete slots, at most 4095 games".into());
    }
    let mut directory = vec![0; SLOT];
    source
        .read_exact(&mut directory)
        .map_err(|e| e.to_string())?;
    if &directory[..16] != b"Saroo Save File\0" {
        return Err("Not a SAROO per-game archive".into());
    }
    let slots = size as usize / SLOT;
    let mut keys = HashSet::new();
    for i in 1..4096 {
        let key = &directory[i * 16..i * 16 + 16];
        if key[0] != 0 && (i >= slots || !keys.insert(key.to_vec())) {
            return Err("Out-of-range or duplicate game key".into());
        }
    }
    let temp_path = output.map(|p| {
        let parent = p
            .parent()
            .filter(|p| !p.as_os_str().is_empty())
            .unwrap_or(Path::new("."));
        parent.join(format!(
            ".saroo-native-{}-{}.tmp",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ))
    });
    let (temp, mut destination) = match temp_path {
        Some(path) => {
            let (temp, file) = create_temp(path)?;
            (Some(temp), Some(file))
        }
        None => (None, None),
    };
    let mut source_hash = Sha256::new();
    source_hash.update(&directory);
    let mut out_hash = Sha256::new();
    out_hash.update(&directory);
    if let Some(f) = &mut destination {
        f.write_all(&directory).map_err(|e| e.to_string())?;
    }
    let mut reports = Vec::new();
    let mut all_fit = true;
    for i in 1..slots {
        let mut bytes = vec![0; SLOT];
        source.read_exact(&mut bytes).map_err(|e| e.to_string())?;
        source_hash.update(&bytes);
        let key = &directory[i * 16..i * 16 + 16];
        if key[0] != 0 {
            let (_, saves) = if bytes.iter().all(|b| *b == 0) || bytes.iter().all(|b| *b == 255) {
                (false, Vec::new())
            } else {
                parse(&bytes).map_err(|e| format!("Slot {i}: {e}"))?
            };
            let blocks: usize = saves.iter().map(|s| 1 + (s.payload.len() + 29) / 58).sum();
            let fits = blocks <= 510 && saves.iter().all(|s| s.payload.len() <= MAX);
            all_fit &= fits;
            reports.push(json!({"slot":i,"files":saves.len(),"native_blocks":blocks,"fits":fits}));
            if destination.is_some() {
                bytes = encode(key, &saves).map_err(|e| format!("Slot {i}: {e}"))?;
            }
        }
        out_hash.update(&bytes);
        if let Some(f) = &mut destination {
            f.write_all(&bytes).map_err(|e| e.to_string())?;
        }
    }
    let source_sha = format!("{:x}", source_hash.finalize());
    if source_sha != hash_file(&mut source)? {
        return Err("Source changed during conversion".into());
    }
    if journal_stamp(input)? != journal {
        return Err("Companion journal changed during conversion".into());
    }
    let output_sha = format!("{:x}", out_hash.finalize());
    if let (Some(f), Some(t), Some(path)) = (&mut destination, &temp, output) {
        f.sync_all().map_err(|e| e.to_string())?;
        if output_sha != hash_file(f)? {
            return Err("Complete output readback differs".into());
        }
        // Publish without replacing an existing path. A failed conversion only
        // removes its private temporary file; originals remain byte-identical.
        fs::hard_link(&t.0, path).map_err(|e| e.to_string())?;
        File::open(
            path.parent()
                .filter(|p| !p.as_os_str().is_empty())
                .unwrap_or(Path::new(".")),
        )
        .and_then(|f| f.sync_all())
        .map_err(|e| format!("Output published; directory sync failed: {e}"))?;
    }
    Ok(
        json!({"complete":true,"source_sha256":source_sha,"output_sha256":output.map(|_|output_sha),
        "bytes":size,"all_fit":all_fit,"slots":reports,"companion_journal_sha256":journal,
        "source_modified":false,"device_access":false}),
    )
}
fn main() {
    let a: Vec<_> = std::env::args_os().skip(1).collect();
    if a.len() == 1 && (a[0] == "--help" || a[0] == "-h") {
        println!(
            "saroo-save-native inspect INPUT\nsaroo-save-native convert INPUT NEW_OUTPUT\nReads a per-game archive. Conversion preserves originals, refuses capacity loss and never overwrites output."
        );
        return;
    }
    let result = match a.as_slice() {
        [mode, input] if mode == "inspect" => run(Path::new(input), None),
        [mode, input, output] if mode == "convert" => {
            run(Path::new(input), Some(Path::new(output)))
        }
        _ => Err("Use --help for inspect/convert syntax".into()),
    };
    match result {
        Ok(v) => println!("{v}"),
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    fn saves(sizes: &[usize]) -> Vec<Save> {
        sizes
            .iter()
            .enumerate()
            .map(|(i, n)| {
                let mut metadata = [0; 32];
                metadata[..11].copy_from_slice(format!("FILE{i:07}").as_bytes());
                put32(&mut metadata, 12, *n);
                metadata[16..26].copy_from_slice(b"Keep meta!");
                metadata[27] = 5;
                put32(&mut metadata, 28, 12345);
                Save {
                    metadata,
                    payload: (0..*n).map(|p| (p % 251) as u8).collect(),
                }
            })
            .collect()
    }
    #[test]
    fn native_thresholds_and_many_tiny_files() {
        for n in [0, 1, 28, 29, 86, 87, 992, 1276, MAX] {
            let s = saves(&[n]);
            let b = encode(b"GAME000000000001", &s).unwrap();
            assert_eq!(parse(&b).unwrap().1, s);
        }
        let s = saves(&[1; 510]);
        let b = encode(b"GAME000000000001", &s).unwrap();
        assert_eq!(word(&b, 14), 0);
        assert_eq!(parse(&b).unwrap().1, s);
        assert!(encode(b"GAME000000000001", &saves(&[1; 511])).is_err());
        assert!(encode(b"GAME000000000001", &saves(&[MAX + 1])).is_err());
    }
    #[test]
    fn corrupt_ownership_and_orphans_refused() {
        let b = encode(b"GAME000000000001", &saves(&[992, 1276])).unwrap();
        for (at, val) in [(62, 600), (2 * 128 + 62, 2), (12, 128)] {
            let mut bad = b.clone();
            put16(&mut bad, at, val);
            assert!(parse(&bad).is_err());
        }
        let mut bad = b;
        mark(&mut bad, 500);
        assert!(parse(&bad).is_err());
    }
    fn legacy(s: &[Save]) -> Vec<u8> {
        let mut b = vec![0; SLOT];
        b[..8].copy_from_slice(b"SaroSave");
        put32(&mut b, 8, SLOT);
        put16(&mut b, 12, 128);
        mark(&mut b, 0);
        let mut next = 1;
        let mut previous = 0;
        for save in s {
            let h = next;
            next += 1;
            mark(&mut b, h);
            put16(&mut b, previous * 128 + 62, h);
            b[h * 128..h * 128 + 32].copy_from_slice(&save.metadata);
            for chunk in save.payload.chunks(128) {
                mark(&mut b, next);
                mark(&mut b[h * 128..(h + 1) * 128], next);
                b[next * 128..next * 128 + chunk.len()].copy_from_slice(chunk);
                next += 1;
            }
            previous = h;
        }
        b
    }
    #[test]
    fn legacy_payload_and_metadata_preserved() {
        let s = saves(&[0, 992, 1276]);
        let b = legacy(&s);
        let (native, old) = parse(&b).unwrap();
        assert!(!native);
        assert_eq!(old, s);
        let converted = encode(b"GAME000000000001", &old).unwrap();
        assert_eq!(parse(&converted).unwrap().1, s);
    }
    #[test]
    fn archive_conversion_is_non_overwriting_and_refuses_capacity_loss() {
        let dir =
            std::env::temp_dir().join(format!("saroo-save-native-test-{}", std::process::id()));
        fs::create_dir(&dir).unwrap();
        let input = dir.join("SS_SAVE.BIN");
        let output = dir.join("new.bin");
        let mut archive = vec![0; 2 * SLOT];
        archive[..16].copy_from_slice(b"Saroo Save File\0");
        archive[16..32].copy_from_slice(b"GAME000000000001");
        archive[SLOT..]
            .copy_from_slice(&encode(b"GAME000000000001", &saves(&[992, 1276])).unwrap());
        fs::write(&input, &archive).unwrap();
        assert!(journal_stamp(&input).unwrap().is_none());
        let journal = dir.join("SAVEJRN.BIN");
        for length in [1, 511] {
            fs::write(&journal, vec![0; length]).unwrap();
            assert!(run(&input, Some(&output)).is_err());
            assert!(!output.exists());
        }
        fs::write(&journal, []).unwrap();
        assert!(journal_stamp(&input).unwrap().is_some());
        fs::write(&journal, vec![0x53; 512]).unwrap();
        assert!(run(&input, Some(&output)).is_err());
        assert!(!output.exists());
        let mut inactive = vec![0; 512];
        inactive.extend_from_slice(b"Retained inactive body");
        fs::write(&journal, inactive).unwrap();
        assert!(create_temp(input.clone()).is_err());
        assert_eq!(fs::read(&input).unwrap(), archive);
        assert!(run(&input, Some(&output)).unwrap()["all_fit"]
            .as_bool()
            .unwrap());
        assert_eq!(fs::read(&input).unwrap(), archive);
        assert!(run(&input, Some(&output)).is_err());
        assert!(run(&input, Some(&input)).is_err());
        let mut corrupt = archive.clone();
        put16(&mut corrupt, SLOT + 62, 600);
        fs::write(&input, corrupt).unwrap();
        let refused = dir.join("refused.bin");
        assert!(run(&input, Some(&refused)).is_err());
        assert!(!refused.exists());
        let large = legacy(&saves(&[16384, 16384]));
        archive[SLOT..].copy_from_slice(&large);
        fs::write(&input, &archive).unwrap();
        assert_eq!(run(&input, None).unwrap()["all_fit"], false);
        assert!(run(&input, Some(&refused)).is_err());
        assert!(!refused.exists());
        assert_eq!(fs::read(&input).unwrap(), archive);
        fs::remove_dir_all(dir).unwrap();
    }
}
