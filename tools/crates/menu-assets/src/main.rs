//! Host-only compiler for fixed menu labels and the existing indexed font format.
use serde::Deserialize;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt::Write as _;
use std::fs::{self, File, OpenOptions};
use std::io::{Read, Write};
use std::path::Path;

const FONT_LIMIT: usize = 0x60000; // 512 KiB window less the 128 KiB menu body.
const JSON_LIMIT: usize = 65536;
const HELP: &str = "menu-assets: compile fixed Arabic labels and merge indexed fonts\n\
Usage:\n\
  menu-assets arabic-table SPEC.json OUTPUT.inc\n\
  menu-assets arabic-glyphs SPEC.json OUTPUT.txt\n\
  menu-assets merge-font BASE.bin EXTRA.bin OUTPUT.bin\n\
Paths are explicit. Outputs use a sibling temporary file and atomic rename.\n\
Inputs cannot be outputs. JSON is limited to 64 KiB; fonts to 384 KiB.\n\
This is fixed-label shaping, not a general bidi or filename layout engine.\n";

type Result<T> = std::result::Result<T, String>;
type Forms = BTreeMap<String, BTreeMap<String, char>>;
type Font = BTreeMap<u16, Vec<u8>>;

#[derive(Deserialize)]
#[serde(rename_all = "lowercase")]
enum Direction {
    Rtl,
    Ltr,
}

#[derive(Deserialize)]
struct Spec {
    language_id: u8,
    labels: Vec<Vec<(Direction, String)>>,
}

fn read_bounded(path: &Path, limit: usize) -> Result<Vec<u8>> {
    let mut bytes = Vec::new();
    File::open(path)
        .map_err(|e| format!("{}: {e}", path.display()))?
        .take((limit + 1) as u64)
        .read_to_end(&mut bytes)
        .map_err(|e| format!("{}: {e}", path.display()))?;
    if bytes.len() > limit {
        return Err(format!("{} exceeds {limit} bytes", path.display()));
    }
    Ok(bytes)
}

fn shape_rtl(text: &str, forms: &Forms) -> Result<String> {
    let chars: Vec<char> = text.chars().collect();
    let mut tokens = Vec::new();
    let mut i = 0;
    while i < chars.len() {
        let mut token = chars[i].to_string();
        if i + 1 < chars.len() {
            let pair: String = chars[i..i + 2].iter().collect();
            if forms.contains_key(&pair) {
                token = pair;
                i += 1;
            }
        }
        if !forms.contains_key(&token) && !matches!(token.as_str(), " " | "." | "!") {
            return Err(format!("unsupported character in Arabic run: {token:?}"));
        }
        tokens.push(token);
        i += 1;
    }
    let mut shaped = Vec::new();
    for (i, token) in tokens.iter().enumerate() {
        let Some(current) = forms.get(token) else {
            shaped.push(token.chars().next().unwrap());
            continue;
        };
        let before = i > 0
            && forms
                .get(&tokens[i - 1])
                .is_some_and(|f| f.contains_key("initial"))
            && current.contains_key("final");
        let after = current.contains_key("initial")
            && tokens
                .get(i + 1)
                .and_then(|t| forms.get(t))
                .is_some_and(|f| f.contains_key("final"));
        let form = match (before, after) {
            (true, true) => "medial",
            (true, false) => "final",
            (false, true) => "initial",
            (false, false) => "isolated",
        };
        shaped.push(*current.get(form).ok_or("missing contextual form")?);
    }
    Ok(shaped.into_iter().rev().collect())
}

fn formats(text: &str) -> Result<usize> {
    let mut chars = text.chars();
    let mut count = 0;
    while let Some(c) = chars.next() {
        if c == '%' {
            if chars.next() != Some('d') {
                return Err("only the call site's %d placeholders are supported".into());
            }
            count += 1;
        }
    }
    Ok(count)
}

fn compile_labels(bytes: &[u8]) -> Result<Vec<String>> {
    let spec: Spec = serde_json::from_slice(bytes).map_err(|e| e.to_string())?;
    if spec.language_id != 18 || spec.labels.len() != 20 {
        return Err("Arabic must be ID 18 with all 20 labels".into());
    }
    let forms: Forms =
        serde_json::from_str(include_str!("../../../menu-assets/joining-forms.json"))
            .map_err(|e| e.to_string())?;
    spec.labels
        .into_iter()
        .enumerate()
        .map(|(slot, runs)| {
            if runs.is_empty() {
                return Err(format!("empty label {slot}"));
            }
            let mut logical = String::new();
            let mut visual = Vec::new();
            for (direction, text) in runs {
                if text.is_empty() || text.contains(['\0', '\n', '\r']) {
                    return Err(format!("empty or multiline run at label {slot}"));
                }
                logical.push_str(&text);
                visual.push(match direction {
                    Direction::Rtl => shape_rtl(&text, &forms)?,
                    Direction::Ltr
                        if text
                            .chars()
                            .all(|c| c.is_ascii_alphanumeric() || " ()/%d.!+-".contains(c)) =>
                    {
                        text
                    }
                    Direction::Ltr => return Err(format!("unsupported LTR run at label {slot}")),
                });
            }
            let visual: String = visual.into_iter().rev().collect();
            let expected = if slot < 2 {
                2
            } else if matches!(slot, 3 | 5 | 18) {
                1
            } else {
                0
            };
            if formats(&logical)? != expected || formats(&visual)? != expected {
                return Err(format!(
                    "format arguments differ from call site at label {slot}"
                ));
            }
            Ok(visual)
        })
        .collect()
}

fn decode_font(bytes: &[u8], strict_geometry: bool) -> Result<Font> {
    if bytes.len() < 2 || bytes.len() > FONT_LIMIT {
        return Err("invalid font size".into());
    }
    let count = u16::from_be_bytes([bytes[0], bytes[1]]) as usize;
    let mut start = 2 + 5 * count;
    if start > bytes.len() || (count == 0 && start != bytes.len()) {
        return Err("truncated or empty font index".into());
    }
    let mut records = BTreeMap::new();
    let mut previous = None;
    for i in 0..count {
        let entry = 2 + 5 * i;
        let cp = u16::from_be_bytes([bytes[entry], bytes[entry + 1]]);
        let offset = |at: usize| {
            ((bytes[at] as usize) << 16) | ((bytes[at + 1] as usize) << 8) | bytes[at + 2] as usize
        };
        let begin = offset(entry + 2);
        let end = if i + 1 < count {
            offset(entry + 7)
        } else {
            bytes.len()
        };
        if previous.is_some_and(|p| cp <= p)
            || begin != start
            || end < begin + 3
            || end > bytes.len()
        {
            return Err("invalid font order or record span".into());
        }
        let record = &bytes[begin..end];
        if strict_geometry {
            let width = (record[1] >> 4) as usize;
            let height = (record[1] & 15) as usize;
            let x = (record[2] >> 4) as usize;
            let y = (record[2] & 15) as usize;
            if !(1..=16).contains(&record[0])
                || (width == 0) != (height == 0)
                || x + width > 16
                || y + height > 16
                || record.len() != 3 + height * if width <= 8 { 1 } else { 2 }
            {
                return Err(format!(
                    "extra glyph U+{cp:04X} does not fit the 16-pixel cell"
                ));
            }
        }
        records.insert(cp, record.to_vec());
        previous = Some(cp);
        start = end;
    }
    Ok(records)
}

fn encode_font(records: &Font) -> Result<Vec<u8>> {
    let count = u16::try_from(records.len()).map_err(|_| "too many glyphs")?;
    let mut size = 2 + 5 * records.len();
    for record in records.values() {
        size = size.checked_add(record.len()).ok_or("font size overflow")?;
    }
    if size > FONT_LIMIT {
        return Err("font exceeds the 384 KiB menu allocation".into());
    }
    let mut bytes = Vec::with_capacity(size);
    bytes.extend_from_slice(&count.to_be_bytes());
    let mut offset = 2 + 5 * records.len();
    for (cp, record) in records {
        bytes.extend_from_slice(&cp.to_be_bytes());
        bytes.extend_from_slice(&(offset as u32).to_be_bytes()[1..]);
        offset += record.len();
    }
    for record in records.values() {
        bytes.extend_from_slice(record);
    }
    Ok(bytes)
}

fn merge_font(base: &[u8], extra: &[u8]) -> Result<Vec<u8>> {
    // Baseline legacy records are opaque; never silently repair their pixels.
    let mut records = decode_font(base, false)?;
    for (cp, record) in decode_font(extra, true)? {
        if records.insert(cp, record).is_some() {
            return Err(format!("extra font overlaps baseline U+{cp:04X}"));
        }
    }
    encode_font(&records)
}

fn write_output(output: &Path, inputs: &[&Path], bytes: &[u8]) -> Result<()> {
    if let Ok(canonical) = output.canonicalize() {
        for input in inputs {
            if input.canonicalize().map_err(|e| e.to_string())? == canonical {
                return Err("an input cannot also be the output".into());
            }
        }
    }
    let name = output.file_name().ok_or("output must be a file")?;
    let mut temporary_name = name.to_os_string();
    temporary_name.push(format!(".{}.tmp", std::process::id()));
    let temporary = output.with_file_name(temporary_name);
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&temporary)
        .map_err(|e| format!("{}: {e}", temporary.display()))?;
    let result = (|| {
        file.write_all(bytes).map_err(|e| e.to_string())?;
        file.sync_all().map_err(|e| e.to_string())?;
        drop(file);
        fs::rename(&temporary, output).map_err(|e| e.to_string())
    })();
    if result.is_err() {
        // Only a temporary successfully created by this call is ours to remove.
        let _ = fs::remove_file(&temporary);
    }
    result
}

fn run(args: Vec<String>) -> Result<()> {
    if args.len() == 1 || (args.len() == 2 && matches!(args[1].as_str(), "--help" | "-h")) {
        print!("{HELP}");
        return Ok(());
    }
    match args.get(1).map(String::as_str) {
        Some(command @ ("arabic-table" | "arabic-glyphs")) if args.len() == 4 => {
            let input = Path::new(&args[2]);
            let labels = compile_labels(&read_bounded(input, JSON_LIMIT)?)?;
            let text = if command == "arabic-table" {
                let mut table = String::from("static char *lang_ar[LANG_STR_NR] = {\n");
                for label in labels {
                    table.push_str(&format!(
                        "    {},\n",
                        serde_json::to_string(&label).unwrap()
                    ));
                }
                table.push_str("};\n");
                table
            } else {
                let glyphs: BTreeSet<char> = labels
                    .iter()
                    .flat_map(|s| s.chars())
                    .filter(|c| !c.is_ascii())
                    .collect();
                let mut selected = String::new();
                for glyph in glyphs {
                    writeln!(selected, "{:04X}", glyph as u32).unwrap();
                }
                selected
            };
            write_output(Path::new(&args[3]), &[input], text.as_bytes())
        }
        Some("merge-font") if args.len() == 5 => {
            let base = Path::new(&args[2]);
            let extra = Path::new(&args[3]);
            let merged = merge_font(
                &read_bounded(base, FONT_LIMIT)?,
                &read_bounded(extra, FONT_LIMIT)?,
            )?;
            write_output(Path::new(&args[4]), &[base, extra], &merged)
        }
        _ => Err(HELP.into()),
    }
}

fn main() {
    if let Err(error) = run(std::env::args().collect()) {
        eprintln!("menu-assets: {error}");
        std::process::exit(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn contextual_joining_and_ligature_controls() {
        let forms =
            serde_json::from_str(include_str!("../../../menu-assets/joining-forms.json")).unwrap();
        assert_eq!(shape_rtl("لا", &forms).unwrap(), "ﻻ");
        assert_eq!(shape_rtl("بلا", &forms).unwrap(), "ﻼﺑ");
        assert_eq!(shape_rtl("اب", &forms).unwrap(), "ﺏﺍ");
        assert_eq!(shape_rtl("ب!ا", &forms).unwrap(), "ﺍ!ﺏ");
        for invalid in ["بَ", "ب\u{200d}ا", "A", "\0"] {
            assert!(shape_rtl(invalid, &forms).is_err(), "{invalid:?}");
        }
    }

    #[test]
    fn all_labels_and_rejected_authored_inputs() {
        let source = include_bytes!("../../../menu-assets/arabic.json");
        assert_eq!(compile_labels(source).unwrap().len(), 20);
        for (before, after) in [
            ("\"language_id\": 18", "\"language_id\": 9"),
            ("%d/%d", "%s/%d"),
            ("\"SAROO\"", "\"SAROO\\n\""),
            ("اختر", "اخترَ"),
        ] {
            let altered = String::from_utf8(source.to_vec())
                .unwrap()
                .replacen(before, after, 1);
            assert_ne!(altered.as_bytes(), source);
            assert!(compile_labels(altered.as_bytes()).is_err(), "{before}");
        }
        assert!(compile_labels(b"{}").is_err());
    }

    #[test]
    fn merge_preserves_legacy_records_and_rejects_overlap_or_damage() {
        let original = BTreeMap::from([(1, vec![0, 0x10, 0, 0x81])]);
        let added = BTreeMap::from([(0xfe8d, vec![6, 0x61, 3, 0x80])]);
        let base = encode_font(&original).unwrap();
        let extra = encode_font(&added).unwrap();
        let merged = decode_font(&merge_font(&base, &extra).unwrap(), false).unwrap();
        assert_eq!(merged.get(&1), original.get(&1));
        assert_eq!(merged.get(&0xfe8d), added.get(&0xfe8d));
        assert!(merge_font(&extra, &extra).is_err());
        assert!(merge_font(&base, &base).is_err()); // Legacy geometry is not accepted as new.
        for length in 0..extra.len() {
            assert!(decode_font(&extra[..length], true).is_err());
        }
        let mut damaged = extra.clone();
        damaged[6] += 1;
        assert!(decode_font(&damaged, true).is_err());
        assert_eq!(merge_font(&base, &[0, 0]).unwrap(), base);
        let huge = BTreeMap::from([(1, vec![0; FONT_LIMIT])]);
        assert!(encode_font(&huge).is_err());
    }

    #[test]
    fn output_preserves_inputs_and_unowned_temporary_files() {
        let directory =
            std::env::temp_dir().join(format!("saroo-menu-assets-{}", std::process::id()));
        fs::create_dir(&directory).unwrap();
        let input = directory.join("input");
        fs::write(&input, b"input").unwrap();
        assert!(write_output(&input, &[&input], b"bad").is_err());
        let output = directory.join("output");
        let temporary = directory.join(format!("output.{}.tmp", std::process::id()));
        fs::write(&temporary, b"unowned").unwrap();
        assert!(write_output(&output, &[&input], b"bad").is_err());
        assert_eq!(fs::read(&temporary).unwrap(), b"unowned");
        fs::remove_file(&temporary).unwrap();
        write_output(&output, &[&input], b"complete").unwrap();
        assert_eq!(fs::read(&input).unwrap(), b"input");
        assert_eq!(fs::read(&output).unwrap(), b"complete");
        assert!(read_bounded(&input, 4).is_err());
        assert_eq!(read_bounded(&input, 5).unwrap(), b"input");
        fs::remove_dir_all(directory).unwrap();
    }
}
