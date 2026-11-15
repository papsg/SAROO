//! Reproducible headless native startup check. No firmware/device operations.
use super::{capture, fail, new_build_output, sha, Result};
use serde_json::{json, Value};
use std::{
    collections::BTreeMap,
    fs,
    io::Read,
    os::unix::process::CommandExt,
    path::Path,
    process::{Command, Stdio},
    thread,
    time::{Duration, Instant},
};

const REVISION: &str = "7e38821dbac265490f115e163c523a939acda759";
const KEYS: [&str; 5] = [
    "bios",
    "ip_original",
    "ip_postimage",
    "packed_original",
    "packed_postimage",
];
const LIMITS: [usize; 5] = [524288, 6144, 6144, 4608, 4608];

unsafe extern "C" {
    fn kill(pid: i32, signal: i32) -> i32;
}

pub(super) fn read(path: &Path, limit: usize) -> Result<Vec<u8>> {
    if !path.symlink_metadata()?.is_file() || path.metadata()?.len() > limit as u64 {
        return fail("Native input is not a bounded regular file");
    }
    let mut bytes = Vec::new();
    fs::File::open(path)?
        .take(limit as u64 + 1)
        .read_to_end(&mut bytes)?;
    if bytes.len() > limit {
        return fail("Native input grew beyond its limit");
    }
    Ok(bytes)
}

fn inputs(config: &Path) -> Result<BTreeMap<String, Vec<u8>>> {
    let config = config.canonicalize()?;
    let value: Value = serde_json::from_slice(&read(&config, 16384)?)?;
    let object = value.as_object().ok_or("Native inputs must be an object")?;
    if object.len() != KEYS.len() || object.keys().any(|k| !KEYS.contains(&k.as_str())) {
        return fail("Supply exactly the five native inputs");
    }
    let mut result = BTreeMap::new();
    for (key, limit) in KEYS.into_iter().zip(LIMITS) {
        let pin = &value[key];
        let path = Path::new(pin["path"].as_str().ok_or("Input needs a path")?);
        let path = if path.is_absolute() {
            path.to_owned()
        } else {
            config.parent().unwrap().join(path)
        };
        let bytes = read(&path, limit)?;
        let digest = sha(&bytes);
        if bytes.len() != limit || pin["sha256"].as_str() != Some(&digest) {
            return fail("Native input size or pinned checksum differs");
        }
        result.insert(key.to_owned(), bytes);
    }
    if sha(&result["bios"]) != "96e106f740ab448cf89f0dd49dfbac7fe5391cb6bd6e14ad5e3061c13330266f"
        || sha(&result["ip_original"])
            != "15c5342a7fdfdbded2d3bb6f7ec3eb688fc5a401e47310d3a25aa71bc5288697"
        || sha(&result["packed_original"])
            != "e328e1f325f073d8a5843c15bf869bbe31b76de38d994aed9b6532b5d542c0d5"
    {
        return fail("Require the reviewed BIOS and exact DLCT originals");
    }
    regions(
        &result["ip_original"],
        &result["ip_postimage"],
        &result["packed_original"],
        &result["packed_postimage"],
    )?;
    Ok(result)
}

fn regions(ip: &[u8], post: &[u8], packed: &[u8], decoded: &[u8]) -> Result<()> {
    if ip.len() != 6144 || post.len() != 6144 || packed.len() != 4608 || decoded.len() != 4608 {
        return fail("Native source extents differ");
    }
    if ip[..0xf00] != post[..0xf00]
        || ip[0xf7a..] != post[0xf7a..]
        || ip[0xf00..0xf7a] == post[0xf00..0xf7a]
        || packed[..0xd6] != decoded[..0xd6]
        || decoded[0xd6..0xd8] != [0, 9]
        || packed[0xd8..] != decoded[0xd8..]
    {
        return fail("Native postimages must stay inside the reviewed source regions");
    }
    Ok(())
}

fn native_result(stdout: &str) -> Result<Value> {
    let value: Value = serde_json::from_str(stdout.lines().next().ok_or("Missing native result")?)?;
    let required = json!({
        "complete":true,"cases":10,"cache_modes":2,"seeds":5,
        "dead_stack_differences":0,"full_wram_bytes":2097152,
        "vdp_register_bytes":288,"vdp2_vram_bytes":524288,"vdp2_cram_bytes":4096,
        "native_fifo_bytes":194560,"native_filter_and_reader_executed":true,
        "native_decoder_executed":true,"native_vdp_routines_executed":true,
        "native_late_code_writes":0,"entry_first_definitions_checked":true,
        "device_accessed":false
    });
    if required
        .as_object()
        .unwrap()
        .iter()
        .any(|(k, v)| value[k] != *v)
        || !matches!(value["max_scheduling_calls"].as_u64(), Some(1..=2_000_000))
        || !matches!(value["private_stack_bytes"].as_u64(), Some(1..=512))
        || !stdout
            .lines()
            .skip(1)
            .any(|line| line.starts_with("LIMIT: "))
    {
        return fail("Incomplete native result");
    }
    Ok(value)
}

fn copy_tree(
    from: &Path,
    to: &Path,
    prefix: &str,
    hashes: &mut BTreeMap<String, String>,
    total: &mut usize,
    depth: usize,
) -> Result<()> {
    if depth > 16 || !from.symlink_metadata()?.is_dir() {
        return fail("Invalid vendor source tree");
    }
    fs::create_dir(to)?;
    for entry in fs::read_dir(from)? {
        let entry = entry?;
        let kind = entry.file_type()?;
        let key = format!("{prefix}/{}", entry.file_name().to_string_lossy());
        if kind.is_dir() {
            copy_tree(
                &entry.path(),
                &to.join(entry.file_name()),
                &key,
                hashes,
                total,
                depth + 1,
            )?;
        } else if kind.is_file() {
            let bytes = read(&entry.path(), 16 * 1024 * 1024)?;
            *total += bytes.len();
            if hashes.len() >= 16384 || *total > 128 * 1024 * 1024 {
                return fail("Vendor source cohort exceeds bounds");
            }
            fs::write(to.join(entry.file_name()), &bytes)?;
            hashes.insert(key, sha(&bytes));
        } else {
            return fail("Vendor source symlinks and special files are refused");
        }
    }
    Ok(())
}

fn replace(path: &Path, before: &str, after: &str) -> Result<()> {
    let text = String::from_utf8(read(path, 1024 * 1024)?)?;
    if text.matches(before).count() != 1 {
        return fail("Headless build source context differs");
    }
    fs::write(path, text.replacen(before, after, 1))?;
    Ok(())
}

pub(super) fn execute(
    command: &mut Command,
    out: &Path,
    label: &str,
    deadline: Instant,
) -> Result<(bool, String, String)> {
    execute_bounded(command, out, label, deadline, Duration::from_secs(60))
}

pub(super) fn execute_bounded(
    command: &mut Command,
    out: &Path,
    label: &str,
    deadline: Instant,
    child_limit: Duration,
) -> Result<(bool, String, String)> {
    let stdout_path = out.join(format!("{label}.stdout"));
    let stderr_path = out.join(format!("{label}.stderr"));
    fs::write(
        out.join(format!("{label}.command.json")),
        serde_json::to_vec_pretty(&json!({
            "program":command.get_program().to_string_lossy(),
            "args":command.get_args().map(|s|s.to_string_lossy()).collect::<Vec<_>>()
        }))?,
    )?;
    let explicit_env = command
        .get_envs()
        .map(|(k, v)| (k.to_owned(), v.map(|x| x.to_owned())))
        .collect::<Vec<_>>();
    command
        .current_dir(out)
        .env_clear()
        .env("PATH", "/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin")
        .env("LANG", "C")
        .stdout(Stdio::from(fs::File::create(&stdout_path)?))
        .stderr(Stdio::from(fs::File::create(&stderr_path)?))
        .process_group(0);
    for (key, value) in explicit_env {
        if let Some(value) = value {
            command.env(key, value);
        }
    }
    // Preserve the caller's actual home for local Homebrew/CMake discovery;
    // never redirect it or inherit arbitrary tool-injection variables.
    if let Some(home) = std::env::var_os("HOME") {
        command.env("HOME", home);
    }
    let mut child = command.spawn()?;
    let start = Instant::now();
    let status = loop {
        if let Some(status) = child.try_wait()? {
            break status;
        }
        if start.elapsed() > child_limit
            || Instant::now() >= deadline
            || stdout_path.metadata()?.len() > 8 * 1024 * 1024
            || stderr_path.metadata()?.len() > 8 * 1024 * 1024
        {
            unsafe {
                kill(-(child.id() as i32), 9);
            }
            let _ = child.kill();
            let _ = child.wait();
            return fail("Native check command exceeded time or log bounds");
        }
        thread::sleep(Duration::from_millis(20));
    };
    if !status.success() && status.code() != Some(1) {
        return fail("Native child exited abnormally");
    }
    Ok((
        status.success(),
        String::from_utf8(read(&stdout_path, 8 * 1024 * 1024)?)?,
        String::from_utf8(read(&stderr_path, 8 * 1024 * 1024)?)?,
    ))
}

pub fn run(repo: &Path, output: &Path, upstream: &Path, config: &Path) -> Result<()> {
    if !cfg!(target_os = "macos") {
        return fail("This native model build currently requires macOS");
    }
    let repo = repo.canonicalize()?;
    let upstream = upstream.canonicalize()?;
    let assets = inputs(config)?;
    let git = |args: &[&str]| -> Result<String> {
        capture(
            Command::new("git")
                .env("GIT_OPTIONAL_LOCKS", "0")
                .args(["-c", "core.fsmonitor=false", "-C"])
                .arg(&upstream)
                .args(args),
        )
    };
    if git(&["rev-parse", "HEAD"])? != REVISION || !git(&["status", "--porcelain=v1"])?.is_empty() {
        return fail("Require a clean local Yabause checkout at the reviewed revision");
    }
    let out = new_build_output(&repo, output)?;
    let out = out.canonicalize()?;
    let source = out.join("vendor");
    let mut hashes = BTreeMap::new();
    let mut total = 0;
    copy_tree(
        &upstream.join("yabause"),
        &source,
        "vendor",
        &mut hashes,
        &mut total,
        0,
    )?;
    // Build-only adaptations. No emulator instruction, cache or device code changes.
    replace(
        &source.join("src/CMakeLists.txt"),
        "macjoy.c permacjoy.c cd-macosx.c sndmac.c",
        "macjoy.c permacjoy.c sndmac.c",
    )?;
    replace(
        &source.join("src/musashi/CMakeLists.txt"),
        "cmake_minimum_required(VERSION 2.6)",
        "cmake_minimum_required(VERSION 3.5)",
    )?;
    let adaptations = ["src/CMakeLists.txt", "src/musashi/CMakeLists.txt"]
        .into_iter()
        .map(|name| Ok((name, sha(&read(&source.join(name), 1024 * 1024)?))))
        .collect::<Result<BTreeMap<_, _>>>()?;
    let fixture = read(&repo.join("tests/vandal_native.c"), 65536)?;
    fs::write(out.join("vandal_native.c"), &fixture)?;
    for (key, bytes) in &assets {
        fs::write(out.join(format!("{key}.bin")), bytes)?;
    }
    let build = out.join("build");
    let deadline = Instant::now() + Duration::from_secs(300);
    for tool in ["cmake", "ninja", "cc"] {
        if !execute(Command::new(tool).arg("--version"), &out, tool, deadline)?.0 {
            return fail("Native build tool identity failed");
        }
    }
    let mut cmake = Command::new("cmake");
    cmake.arg("-S").arg(&source).arg("-B").arg(&build).args([
        "-G",
        "Ninja",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
        "-DYAB_PORTS=",
        "-DYAB_NETWORK=OFF",
        "-DYAB_DEBUG=NONE",
        "-DYAB_USE_PLAY_JIT=OFF",
        "-DYAB_WANT_OPENGL=OFF",
        "-DYAB_WANT_OPENAL=OFF",
        "-DYAB_WANT_SDL=OFF",
        "-DYAB_USE_SSF=OFF",
        "-DYAB_WANT_GDBSTUB=OFF",
        "-DYAB_WANT_MUSASHI=ON",
        "-DYAB_WANT_C68K=OFF",
        "-DYAB_WANT_Q68=OFF",
        "-DYAB_WANT_ARM7=OFF",
    ]);
    if !execute(&mut cmake, &out, "configure", deadline)?.0 {
        return fail("Headless vendor configure failed; see logs");
    }
    if !execute(
        Command::new("cmake")
            .arg("--build")
            .arg(&build)
            .args(["--target", "yabause", "-j2"]),
        &out,
        "vendor-build",
        deadline,
    )?
    .0
    {
        return fail("Headless vendor build failed; see logs");
    }
    let library = build.join("src/libyabause.a");
    let mut compiler = Command::new("cc");
    compiler
        .args([
            "-std=c11",
            "-O2",
            "-g",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DHAVE_STDINT_H=1",
            "-DHAVE_MUSASHI=1",
            "-isystem",
        ])
        .arg(source.join("src"))
        .arg(out.join("vandal_native.c"))
        .arg(&library);
    for name in [
        "CoreFoundation",
        "IOKit",
        "Carbon",
        "AudioUnit",
        "AudioToolbox",
    ] {
        compiler.args(["-framework", name]);
    }
    let checker = out.join("check");
    if !execute(
        compiler.arg("-lz").arg("-o").arg(&checker),
        &out,
        "checker-build",
        deadline,
    )?
    .0
    {
        return fail("Native checker compile failed; see logs");
    }
    let command = || {
        let mut cmd = Command::new(&checker);
        for key in KEYS {
            cmd.arg(out.join(format!("{key}.bin")));
        }
        cmd
    };
    let (ok, stdout, _) = execute(&mut command(), &out, "native", deadline)?;
    if !ok {
        return fail("Native startup comparison failed; see logs");
    }
    let native = native_result(&stdout)?;
    for (mode, expected) in [
        ("break-packed", "native entry state/patch-writer count"),
        ("break-source", "unexpected FIFO address"),
        ("break-reader", "reader geometry"),
        ("break-filter", "decoder arguments"),
    ] {
        let (ok, _, stderr) = execute(command().arg(mode), &out, mode, deadline)?;
        if ok || !stderr.contains(&format!("FAIL {expected}")) {
            return fail("Broken native control did not fail for its expected reason");
        }
    }
    for (key, hash) in &hashes {
        let original = upstream
            .join("yabause")
            .join(key.strip_prefix("vendor/").unwrap());
        if sha(&read(&original, 16 * 1024 * 1024)?) != *hash {
            return fail("Original vendor source changed during build");
        }
    }
    if !git(&["status", "--porcelain=v1"])?.is_empty()
        || read(&repo.join("tests/vandal_native.c"), 65536)? != fixture
    {
        return fail("Native check sources changed during execution");
    }
    let receipt = json!({"complete":true,"upstream_revision":REVISION,"vendor_source_sha256":hashes,"vendor_source_bytes":total,"build_only_adaptations_sha256":adaptations,"builder_source_sha256":sha(&read(&repo.join("tools/crates/firmware-build/src/vandal.rs"),65536)?),"fixture_sha256":sha(&fixture),"checker_sha256":sha(&read(&checker,64*1024*1024)?),"interpreter_sha256":sha(&read(&library,64*1024*1024)?),"input_sha256":assets.iter().map(|(k,v)|(k,sha(v))).collect::<BTreeMap<_,_>>(),"native":native,"negative_controls":4,"qualification":"Original Yabause interpreter and actual native code with explicit CD/clock models. No complete BIOS/game boot, read/reset lifetime, audio/timing or physical qualification."});
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    println!("Native startup: ten state/cache cases and four broken controls pass");
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn refuses_collateral_ip_and_packed_changes_without_panicking_on_short_inputs() {
        let ip = vec![0x5a; 6144];
        let mut post = ip.clone();
        post[0xf00] ^= 1;
        let packed = vec![0xa5; 4608];
        let mut after = packed.clone();
        after[0xd6..0xd8].copy_from_slice(&[0, 9]);
        assert!(regions(&ip, &post, &packed, &after).is_ok());
        for at in [0, 0xeff, 0xf7a, 6143] {
            post[at] ^= 1;
            assert!(regions(&ip, &post, &packed, &after).is_err());
            post[at] ^= 1;
        }
        after[4607] ^= 1;
        assert!(regions(&ip, &post, &packed, &after).is_err());
        assert!(regions(&[], &[], &[], &[]).is_err());
        assert!(regions(&ip, &ip, &packed, &packed).is_err());
    }

    #[test]
    fn incomplete_or_changed_execution_claims_cannot_publish_a_receipt() {
        let stdout = include_str!("../../../fixtures/vandal-native-result.stdout");
        let valid = native_result(stdout).unwrap();
        for key in valid.as_object().unwrap().keys() {
            let mut bad = valid.clone();
            bad.as_object_mut().unwrap().remove(key);
            assert!(
                native_result(&(bad.to_string() + "\nLIMIT: test\n")).is_err(),
                "{key}"
            );
        }
        for (key, value) in [
            ("device_accessed", json!(true)),
            ("private_stack_bytes", json!(513)),
            ("max_scheduling_calls", json!(2_000_001)),
            ("native_late_code_writes", json!(1)),
            ("dead_stack_differences", json!(1)),
        ] {
            let mut bad = valid.clone();
            bad[key] = value;
            assert!(native_result(&(bad.to_string() + "\nLIMIT: test\n")).is_err());
        }
        assert!(native_result(&valid.to_string()).is_err());
    }
}
