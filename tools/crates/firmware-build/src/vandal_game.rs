//! Accepted-delivery lifetime observation; a pinned external model, not hardware.
use super::vandal::{execute, execute_bounded, read};
use super::{fail, new_build_output, sha, Result};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    fs,
    io::Read,
    path::{Path, PathBuf},
    process::Command,
    time::{Duration, Instant},
};

const CORE: &str = "70e075ec857c65b7bf802e302f2a8dfdaf0468cb8d6ba28d3193340c43dcb96b";
// Isolated host research variant, never a standard-model qualification.
const SOUND_STATE_CORE: &str = "21092963b328519e47a8d9bec5244a798567eeea6349b3b8046fa621609d063f";
const TRACK: &str = "0e9361f5b1d130af6b513a8a80ad9f4d872b14669655159e77ff7ee339a21178";
const CUE: &str = "6b2ea2acc141bdb9bb08e6989886a0a10a4a0154e4d9de5d9d76d1546d5e1177";

fn hash(path: &Path, limit: u64) -> Result<String> {
    let metadata = path.symlink_metadata()?;
    if !metadata.is_file() || metadata.len() > limit {
        return fail("Lifetime input is not a bounded regular file");
    }
    let mut sum = Sha256::new();
    let mut file = fs::File::open(path)?;
    let mut buf = [0u8; 65536];
    let mut total = 0u64;
    loop {
        let n = file.read(&mut buf)?;
        if n == 0 {
            break;
        }
        total += n as u64;
        if total > limit {
            return fail("Lifetime input grew");
        }
        sum.update(&buf[..n]);
    }
    Ok(format!("{:x}", sum.finalize()))
}
fn pin(config: &Path, value: &Value, limit: u64) -> Result<(PathBuf, String)> {
    let path = Path::new(value["path"].as_str().ok_or("Missing lifetime path")?);
    let path = if path.is_absolute() {
        path.to_owned()
    } else {
        config.parent().unwrap().join(path)
    };
    let digest = hash(&path, limit)?;
    if value["sha256"].as_str() != Some(&digest) {
        return fail("Lifetime input checksum differs");
    }
    Ok((path, digest))
}
fn bounds(value: &Value) -> Result<(u64, u64, &str)> {
    let frames = value["frames"]
        .as_u64()
        .ok_or("Missing lifetime frame bound")?;
    let reset = value["reset_frame"].as_u64().ok_or("Missing reset frame")?;
    let mode = value["control"].as_str().ok_or("Missing control")?;
    let opening = value
        .get("opening_progression")
        .map(|v| v.as_bool().ok_or("opening_progression must be Boolean"))
        .transpose()?
        .unwrap_or(false);
    let timing = if opening {
        frames == 13000 && reset == 0
    } else {
        (1900..=2400).contains(&frames) && (900..=1200).contains(&reset) && reset + 900 <= frames
    };
    if !timing || !["native", "inherited"].contains(&mode) {
        return fail("Require a bounded cold/reset observation in native or inherited mode");
    }
    Ok((frames, reset, mode))
}

fn observations(stderr: &str, reset: u64, mode: &str) -> Result<Vec<Value>> {
    let action = if mode == "native" { 128 } else { 0 };
    let reset_line = format!("LIFETIME_RESET,frame={reset},action={action}");
    let resets = if reset == 0 {
        Vec::new()
    } else {
        vec![reset_line.as_str()]
    };
    if stderr
        .lines()
        .filter(|s| s.starts_with("LIFETIME_RESET,"))
        .collect::<Vec<_>>()
        != resets
    {
        return fail("Missing/duplicate reset or lost launch ownership");
    }
    let lines = stderr
        .lines()
        .filter_map(|s| s.strip_prefix("LIFETIME_DELIVERY,"))
        .collect::<Vec<_>>();
    let epochs = if reset == 0 { 1 } else { 2 };
    if lines.len() != 3 * epochs {
        return fail("Both complete native-source delivery epochs are required");
    }
    let mut records = Vec::new();
    let mut previous = 0;
    for (i, line) in lines.into_iter().enumerate() {
        let mut fields = BTreeMap::new();
        for part in line.split(',') {
            let (key, value) = part.split_once('=').ok_or("Malformed delivery field")?;
            if fields.insert(key, value).is_some() {
                return fail("Duplicate delivery field");
            }
        }
        if fields.len() != 6 {
            return fail("Incomplete delivery record");
        }
        let number =
            |key| -> Result<u64> { Ok(fields.get(key).ok_or("Missing delivery field")?.parse()?) };
        let frame = number("frame")?;
        let (fad, before, post) = match i % 3 {
            0 => (151, "fbdc79c0", "89da30c9"),
            1 => (292306, "bbb147c7", "ed631514"),
            _ => (292308, "daee2a88", "daee2a88"),
        };
        let edited = mode == "native" && i < 2;
        if number("fad")? != fad
            || number("result")? != u64::from(edited)
            || fields.get("before") != Some(&before)
            || fields.get("after") != Some(&if edited { post } else { before })
            || number("action")? != action
            || frame < previous
            || (reset != 0 && ((i < 3 && frame >= reset) || (i >= 3 && frame <= reset)))
        {
            return fail("Source delivery/retirement differs from the one-time contract");
        }
        previous = frame;
        records.push(json!({"frame":frame,"fad":fad,"edited":edited,"before_crc32":before,"after_crc32":if edited{post}else{before},"action":action}));
    }
    let inherited = usize::from(mode == "inherited");
    if stderr
        .lines()
        .filter(|s| s.starts_with("VANDAL_AIP_ENTER,"))
        .count()
        != inherited
        || stderr
            .lines()
            .filter(|s| s.starts_with("VANDAL_AIP_RETURN,"))
            .count()
            != inherited
        || stderr
            .lines()
            .filter(|s| s.starts_with("VANDAL_AIP_TRACE,pc=06002d04,"))
            .count()
            != inherited
    {
        return fail("Inherited control did not run exactly once or leaked into native execution");
    }
    let opcodes = stderr
        .lines()
        .filter(|s| s.starts_with("VANDAL_AIP_TRACE,pc=060400c8,"))
        .collect::<Vec<_>>();
    if opcodes.len() != epochs
        || !opcodes[0].contains(",op=0009,")
        || (epochs == 2 && !opcodes[1].contains(",op=2202,"))
    {
        return fail("Require actual NOP execution followed by the original reset bootstrap");
    }
    Ok(records)
}
pub fn run(repo: &Path, output: &Path, config: &Path) -> Result<()> {
    if !cfg!(target_os = "macos") {
        return fail("The retained lifetime core requires macOS");
    }
    let repo = repo.canonicalize()?;
    let config = config.canonicalize()?;
    let config_bytes = read(&config, 16384)?;
    let value: Value = serde_json::from_slice(&config_bytes)?;
    let (frames, reset, mode) = bounds(&value)?;
    let sound_state = value
        .get("sound_state_research")
        .map(|v| v.as_bool().ok_or("sound_state_research must be Boolean"))
        .transpose()?
        .unwrap_or(false);
    let opening = reset == 0;
    let warm = value
        .get("warm_reset_model")
        .map(|v| v.as_bool().ok_or("warm_reset_model must be Boolean"))
        .transpose()?
        .unwrap_or(false);
    if opening && (warm || sound_state) {
        return fail("Opening comparison uses the unchanged baseline core without a reset");
    }
    let mut inputs = BTreeMap::new();
    for (key, limit, expected) in [
        (
            "core",
            32 * 1024 * 1024,
            Some(if sound_state { SOUND_STATE_CORE } else { CORE }),
        ),
        ("cue", 65536, Some(CUE)),
        ("track", 1024 * 1024 * 1024, Some(TRACK)),
        ("libretro_header", 1024 * 1024, None),
        ("pack", 4200, None),
    ] {
        let (p, h) = pin(&config, &value[key], limit)?;
        if expected.is_some_and(|e| e != h) {
            return fail("Require the exact reviewed model and DLCT source");
        }
        inputs.insert(key, (p, h, limit));
    }
    let seeds = value["seeds"]
        .as_array()
        .filter(|s| s.len() == 3)
        .ok_or("Require the three original BIOS/RTC seeds")?;
    let mut seeded = BTreeMap::new();
    for seed in seeds {
        let (p, h) = pin(&config, seed, 1024 * 1024)?;
        let name = p
            .file_name()
            .and_then(|x| x.to_str())
            .ok_or("Invalid seed name")?
            .to_owned();
        let expected = match name.as_str() {
            "mpr-17933.bin" => "96e106f740ab448cf89f0dd49dfbac7fe5391cb6bd6e14ad5e3061c13330266f",
            "sega_101.bin" => "dcfef4b99605f872b6c3b6d05c045385cdea3d1b702906a0ed930df7bcb7deac",
            "VANDAL_HEARTS_CN2_BY_DCLT.smpc" => {
                "82c4405db95a5b2fbc185ba704129dfd10a70e6438a2c83c89eb2456bef3d3fa"
            }
            _ => return fail("Unknown BIOS/RTC seed"),
        };
        if h != expected || seeded.insert(name, (p, h)).is_some() {
            return fail("Changed/duplicate BIOS/RTC seed");
        }
    }
    let out = new_build_output(&repo, output)?.canonicalize()?;
    let system = out.join("system");
    fs::create_dir(&system)?;
    for (name, (path, _)) in &seeded {
        fs::copy(path, system.join(name))?;
    }
    let fixture = read(&repo.join("tests/vandal_lifetime.c"), 65536)?;
    let header = read(&repo.join("Firm_MCU/Saturn/sector_overlay.h"), 16384)?;
    let production = read(&repo.join("Firm_MCU/Saturn/sector_overlay.c"), 65536)?;
    fs::write(out.join("production.c"), &production)?;
    fs::write(out.join("sector_overlay.h"), &header)?;
    fs::write(out.join("vandal_lifetime.c"), &fixture)?;
    fs::copy(&inputs["libretro_header"].0, out.join("libretro.h"))?;
    fs::copy(&inputs["core"].0, out.join("core.dylib"))?;
    fs::copy(&inputs["pack"].0, out.join("overlay.SOV"))?;
    for (key, name) in [
        ("core", "core.dylib"),
        ("pack", "overlay.SOV"),
        ("libretro_header", "libretro.h"),
    ] {
        if hash(&out.join(name), inputs[key].2)? != inputs[key].1 {
            return fail("Copied lifetime input differs");
        }
    }
    let deadline = Instant::now() + Duration::from_secs(if opening { 1500 } else { 420 });
    let runner = out.join("check");
    if !execute(
        Command::new("cc")
            .args(["-std=c11", "-O2", "-g", "-Wall", "-Wextra", "-Werror"])
            .arg("-I")
            .arg(&out)
            .arg(out.join("vandal_lifetime.c"))
            .arg(out.join("production.c"))
            .arg("-lz")
            .arg("-o")
            .arg(&runner),
        &out,
        "compile",
        deadline,
    )?
    .0
    {
        return fail("Lifetime frontend build failed; see logs");
    }
    let mut command = Command::new(&runner);
    command
        .arg(out.join("core.dylib"))
        .arg(&system)
        .arg(&inputs["cue"].0)
        .arg(&inputs["track"].0)
        .arg(frames.to_string())
        .arg(if mode == "native" {
            out.join("overlay.SOV")
        } else {
            PathBuf::from("-")
        })
        .arg(reset.to_string())
        .env("SH2JIT_OFF", "1")
        .env("SATURN_REGION", "jp")
        .env("SATURN_CART", "extram4")
        .env("SAROO_VANDAL_AIP_TRACE", "1");
    // Only the reviewed input/config selects controls and output paths.
    for key in [
        "SAROO_VANDAL_AIP_CONTROL",
        "AUDIO_DUMP_PATH",
        "WARM_RESET_MODEL",
        "PAD_SCRIPT",
        "VIDEO_SNAPSHOT_PATH",
        "VIDEO_SNAPSHOT_FRAME",
    ] {
        command.env_remove(key);
    }
    if mode == "inherited" {
        command.env("SAROO_VANDAL_AIP_CONTROL", "1");
    }
    if sound_state {
        command.env("AUDIO_DUMP_PATH", out.join("audio.pcm"));
    }
    if warm {
        command.env("WARM_RESET_MODEL", "1");
    }
    if opening {
        command.env("PAD_SCRIPT","start@7600:7620,start@9000:9020,c@9500:9520,c@10500:10520,c@11500:11520,c@12500:12520")
            .env("AUDIO_DUMP_PATH",out.join("audio.pcm"))
            .env("VIDEO_SNAPSHOT_PATH",out.join("video.ppm"))
            .env("VIDEO_SNAPSHOT_FRAME",frames.to_string());
    }
    let (ok, stdout, stderr) = execute_bounded(
        &mut command,
        &out,
        "game",
        deadline,
        Duration::from_secs(if opening { 1440 } else { 360 }),
    )?;
    if !ok {
        return fail("Lifetime model failed; see logs");
    }
    let rows = stdout
        .lines()
        .map(serde_json::from_str::<Value>)
        .collect::<std::result::Result<Vec<_>, _>>()?;
    let actual = rows
        .iter()
        .filter(|r| r["event"] == "frame")
        .filter_map(|r| r["frame"].as_u64())
        .collect::<Vec<_>>();
    let mut expected = (1..=frames).step_by(60).collect::<Vec<_>>();
    if expected.last() != Some(&frames) {
        expected.push(frames);
    }
    let last = rows
        .iter()
        .rev()
        .find(|r| r["event"] == "frame")
        .ok_or("Missing final frame")?;
    let hits = last["overlay_read_hits"]
        .as_u64()
        .ok_or("Missing delivery counter")?;
    let hit_count = if mode == "native" {
        hits >= if opening { 3 } else { 6 }
    } else {
        hits == 0
    };
    if actual != expected
        || !hit_count
        || rows.last() != Some(&json!({"event":"unloaded","action":0}))
    {
        return fail("Incomplete/duplicate lifetime observations");
    }
    let deliveries = observations(&stderr, reset, mode)?;
    for (p, h, limit) in inputs.values() {
        if hash(p, *limit)? != *h {
            return fail("Original lifetime inputs changed during execution");
        }
    }
    for (p, h) in seeded.values() {
        if hash(p, 1024 * 1024)? != *h {
            return fail("Original seed changed during execution");
        }
    }
    if read(&config, 16384)? != config_bytes
        || read(&repo.join("tests/vandal_lifetime.c"), 65536)? != fixture
        || read(&repo.join("Firm_MCU/Saturn/sector_overlay.c"), 65536)? != production
    {
        return fail("Lifetime config/source changed during execution");
    }
    let pcm = if sound_state || opening {
        Some(hash(&out.join("audio.pcm"), 64 * 1024 * 1024)?)
    } else {
        None
    };
    let video = if opening {
        Some(hash(&out.join("video.ppm"), 2 * 1024 * 1024)?)
    } else {
        None
    };
    let receipt = json!({"complete":true,"frames":frames,"reset_frame":reset,"warm_reset_model":warm,"opening_progression":opening,"control":mode,"inputs":inputs,"seeds":seeded,"fixture_sha256":sha(&fixture),"production_editor_sha256":sha(&production),"production_header_sha256":sha(&header),"runner_sha256":hash(&runner,16*1024*1024)?,"observations":deliveries,"frame_rows":rows,"sound_state_research":sound_state,"pcm_sha256":pcm,"video_sha256":video,"qualification":"Pinned external emulator core and production editor. Reset variants use either the core's power-on path or its SS_Reset(false) entry; neither qualifies a physical reset button/BIOS return. Optional sound-state research needs independent output-neutrality checks. Opening progression uses the reviewed fixed Start/C schedule. No live SAROO menu/mount, tactical gameplay/audio or physical qualification."});
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    println!("Completed bounded accepted-delivery/reset observations; inspect receipt for lifetime disposition");
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn keeps_both_reset_epochs_within_the_observed_run() {
        let mut config = json!({"frames":2100,"reset_frame":1100,"control":"native"});
        assert!(bounds(&config).is_ok());
        for (key, value) in [
            ("frames", json!(2401)),
            ("reset_frame", json!(1500)),
            ("reset_frame", json!(true)),
            ("control", json!("other")),
        ] {
            let mut bad = config.clone();
            bad[key] = value;
            assert!(bounds(&bad).is_err());
        }
        config["control"] = json!("inherited");
        assert!(bounds(&config).is_ok());
    }

    #[test]
    fn opening_requires_the_complete_reviewed_schedule_without_a_reset() {
        let valid =
            json!({"frames":13000,"reset_frame":0,"control":"native","opening_progression":true});
        assert!(bounds(&valid).is_ok());
        for (key, value) in [
            ("frames", json!(12999)),
            ("reset_frame", json!(1100)),
            ("opening_progression", json!(false)),
            ("opening_progression", json!("true")),
        ] {
            let mut bad = valid.clone();
            bad[key] = value;
            assert!(bounds(&bad).is_err());
        }
        let first = include_str!("../../../fixtures/vandal-lifetime-native.log")
            .split("LIFETIME_RESET,")
            .next()
            .unwrap();
        assert_eq!(observations(first, 0, "native").unwrap().len(), 3);
        assert!(observations(first, 1100, "native").is_err());
        assert!(observations(&first.replace("op=0009", "op=2202"), 0, "native").is_err());
    }

    #[test]
    fn missing_delivery_wrong_retirement_and_missing_original_execution_refuse() {
        let valid = include_str!("../../../fixtures/vandal-lifetime-native.log");
        assert_eq!(observations(valid, 1100, "native").unwrap().len(), 6);
        for (before, after) in [
            ("result=1", "result=0"),
            ("before=fbdc79c0", "before=00000000"),
            ("action=128", "action=0"),
            ("frame=1530", "frame=1000"),
            ("op=2202", "op=0009"),
        ] {
            assert!(observations(&valid.replacen(before, after, 1), 1100, "native").is_err());
        }
        let truncated = valid
            .lines()
            .filter(|s| !s.contains("fad=292306"))
            .collect::<Vec<_>>()
            .join("\n");
        assert!(observations(&truncated, 1100, "native").is_err());
        assert!(observations(
            &(valid.to_owned() + "\nLIFETIME_RESET,frame=1100,action=128\n"),
            1100,
            "native"
        )
        .is_err());
        let inherited = include_str!("../../../fixtures/vandal-lifetime-inherited.log");
        assert_eq!(observations(inherited, 1100, "inherited").unwrap().len(), 6);
        assert!(observations(inherited, 1100, "native").is_err());
    }
}
