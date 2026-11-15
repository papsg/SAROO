//! Read-only historical receipt audit. Missing/changed pins remain explicit.
use crate::{disc::read_bounded, fail, hex, write_pack, Result};
use serde::Deserialize;
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::Read;
use std::path::{Path, PathBuf};

#[derive(Deserialize)]
struct Pin {
    path: PathBuf,
    bytes: u64,
    sha256: String,
}

fn pins(value: &Value) -> Result<Vec<Pin>> {
    let entries = if let Some(inputs) = value.get("inputs").or_else(|| value.get("pins")) {
        serde_json::from_value::<Vec<Pin>>(inputs.clone())?
    } else if let Some(files) = value.get("files").and_then(Value::as_object) {
        let mut result = Vec::new();
        for (path, value) in files {
            let mut pin = value.clone();
            pin.as_object_mut()
                .ok_or("Invalid file pin")?
                .insert("path".into(), json!(path));
            result.push(serde_json::from_value(pin)?);
        }
        result
    } else {
        return fail("Receipt requires inputs[], pins[] or files{} pins");
    };
    if entries.is_empty()
        || entries.len() > 100_000
        || entries.iter().any(|p| {
            p.path.as_os_str().is_empty()
                || p.sha256.len() != 64
                || !p
                    .sha256
                    .bytes()
                    .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        })
    {
        return fail("Invalid receipt pins or inventory bound");
    }
    Ok(entries)
}

pub fn run(input: &Path, output: &Path) -> Result<()> {
    let input = std::fs::canonicalize(input)?;
    let source = read_bounded(&input, 32 * 1024 * 1024)?;
    let value: Value = serde_json::from_slice(&source)?;
    let entries = pins(&value)?;
    let base = input.parent().ok_or("Receipt has no parent")?;
    let mut counts = [0; 4];
    let mut rows = Vec::new();
    let mut buffer = vec![0; 1024 * 1024];
    for pin in entries {
        let path = base.join(&pin.path);
        let mut row = json!({"path":pin.path,"bytes":pin.bytes,"sha256":pin.sha256});
        let opened = std::fs::metadata(&path).and_then(|metadata| {
            if metadata.is_file() && metadata.len() <= 64 * 1024 * 1024 * 1024 {
                File::open(&path)
            } else {
                Err(std::io::Error::new(
                    std::io::ErrorKind::InvalidInput,
                    "Receipt input is not a bounded regular file",
                ))
            }
        });
        match opened {
            Ok(mut file) => {
                let mut hash = Sha256::new();
                let mut observed = 0u64;
                loop {
                    let count = file.read(&mut buffer)?;
                    if count == 0 {
                        break;
                    }
                    hash.update(&buffer[..count]);
                    observed = observed
                        .checked_add(count as u64)
                        .ok_or("Receipt file size overflow")?;
                }
                let digest = hex(&hash.finalize());
                let matches = observed == pin.bytes && digest == pin.sha256;
                counts[if matches { 0 } else { 2 }] += 1;
                row["status"] = json!(if matches { "matched" } else { "changed" });
                if !matches {
                    row["observed_bytes"] = json!(observed);
                    row["observed_sha256"] = json!(digest);
                }
            }
            Err(error) => {
                let missing = error.kind() == std::io::ErrorKind::NotFound;
                counts[if missing { 1 } else { 3 }] += 1;
                row["status"] = json!(if missing { "missing" } else { "unreadable" });
            }
        }
        rows.push(row);
    }
    let report = json!({"receipt_sha256":hex(&Sha256::digest(&source)),
        "matched":counts[0],"missing":counts[1],"changed":counts[2],"unreadable":counts[3],
        "pins":rows,"qualification":"Current file hashes only; no runtime, hardware or game qualification."});
    write_pack(output, &serde_json::to_vec_pretty(&report)?)?;
    println!(
        "{} matched, {} missing, {} changed, {} unreadable",
        counts[0], counts[1], counts[2], counts[3]
    );
    if counts[1..].iter().any(|&count| count != 0) {
        return fail("Receipt has unresolved pins; see the preserved audit output");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn receipt_shapes_retain_expected_size_and_reject_invalid_digests() {
        let digest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        let list = json!({"inputs":[{"path":"fixture.bin","bytes":3,"sha256":digest}]});
        let named = json!({"pins":[{"path":"fixture.bin","bytes":3,"sha256":digest}]});
        let map = json!({"files":{"fixture.bin":{"bytes":3,"sha256":digest}}});
        for value in [list, named, map] {
            let rows = pins(&value).unwrap();
            assert_eq!(rows.len(), 1);
            assert_eq!(rows[0].bytes, 3);
            assert_eq!(rows[0].sha256, digest);
        }
        for value in [
            json!({}),
            json!({"inputs":[]}),
            json!({"inputs":[{"path":"x","bytes":3,"sha256":"ABC"}]}),
        ] {
            assert!(pins(&value).is_err());
        }
    }

    #[test]
    fn audit_records_changed_and_missing_inputs_without_overwriting_output() {
        let root = std::env::temp_dir().join(format!(
            "overlay-pin-test-{}-{}",
            std::process::id(),
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        std::fs::create_dir(&root).unwrap();
        std::fs::write(root.join("original"), b"abc").unwrap();
        std::fs::write(root.join("changed"), b"xyz").unwrap();
        let digest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        let rows: Vec<_> = ["original", "changed", "missing"]
            .iter()
            .map(|path| json!({"path":path,"bytes":3,"sha256":digest}))
            .collect();
        let input = root.join("receipt.json");
        let output = root.join("report.json");
        std::fs::write(&input, serde_json::to_vec(&json!({"inputs":rows})).unwrap()).unwrap();
        assert!(run(&input, &output).is_err());
        let bytes = std::fs::read(&output).unwrap();
        let report: Value = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(report["matched"], 1);
        assert_eq!(report["changed"], 1);
        assert_eq!(report["missing"], 1);
        assert!(run(&input, &output).is_err());
        assert_eq!(std::fs::read(&output).unwrap(), bytes);
        assert_eq!(std::fs::read(root.join("original")).unwrap(), b"abc");
        std::fs::remove_dir_all(root).unwrap();
    }
}
