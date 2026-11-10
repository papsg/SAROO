//! Offline source-only Quartus comparison; no device access or image download.
//! Adapts the paps-g staged Quartus/QEMU build procedure, in Rust.
use super::{fail, new_build_output, sha, Result};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    env, fs,
    io::Read,
    os::unix::fs::PermissionsExt,
    path::Path,
    process::{Command, Stdio},
    thread,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

// Resolve the cached amd64 manifest by immutable ID: some Docker image stores
// list its tag but cannot inspect/run the tag without contacting a registry.
const IMAGE: &str = "sha256:d60907a0e1139b4c866760fbc76007971f7330ba1f5d47e99494acf1f8119c29";
const REQUIRED: &[&str] = &[
    "SSMaster.sof",
    "SSMaster.rbf",
    "ssmaster.jic",
    "SSMaster.fit.rpt",
    "SSMaster.sta.rpt",
    "SSMaster.sta.summary",
];
const QSF_ADDITIONS: &str = "\nset_global_assignment -name NUM_PARALLEL_PROCESSORS 2\nset_global_assignment -name SDC_FILE qualification-clock.sdc\nset_global_assignment -name FITTER_EFFORT \"STANDARD FIT\"\n";
const SDC: &str = "derive_clock_uncertainty\n";

fn run(cmd: &mut Command, log: &Path, seconds: u64) -> Result<bool> {
    let file = fs::File::create(log)?;
    let mut child = cmd
        .stdout(Stdio::from(file.try_clone()?))
        .stderr(Stdio::from(file))
        .spawn()?;
    let started = Instant::now();
    loop {
        if let Some(status) = child.try_wait()? {
            return Ok(status.success());
        }
        if started.elapsed() > Duration::from_secs(seconds)
            || fs::metadata(log)?.len() > 128 * 1024 * 1024
        {
            child.kill()?;
            child.wait()?;
            return fail("Command timed out or exceeded log bounds; inspect preserved identity/log before retrying");
        }
        thread::sleep(Duration::from_millis(100));
    }
}

fn read_command(cmd: &mut Command, log: &Path) -> Result<String> {
    if !run(cmd, log, 30)? {
        return fail("Local build setup command failed; inspect log");
    }
    Ok(fs::read_to_string(log)?.trim().into())
}

#[derive(Default, PartialEq)]
struct Compiler {
    files: BTreeMap<String, String>,
    bytes: u64,
}

fn compiler_tree(root: &Path, directory: &Path, depth: u32, result: &mut Compiler) -> Result<()> {
    if depth > 32 {
        return fail("Compiler nesting exceeds 32 levels");
    }
    for entry in fs::read_dir(directory)? {
        let entry = entry?;
        let path = entry.path();
        let relative = path
            .strip_prefix(root)?
            .to_str()
            .ok_or("Non-UTF8 compiler path")?
            .to_owned();
        let metadata = entry.metadata()?;
        if result.files.len() >= 100_000 {
            return fail("Compiler exceeds file-count bound");
        }
        if entry.file_type()?.is_symlink() {
            if !path.canonicalize()?.starts_with(root) {
                return fail("Compiler symlink escapes its root");
            }
            let target = fs::read_link(&path)?;
            result
                .files
                .insert(relative, format!("symlink:{}", target.to_string_lossy()));
        } else if metadata.is_dir() {
            compiler_tree(root, &path, depth + 1, result)?;
        } else if metadata.is_file() {
            if metadata.len() > 2 * 1024 * 1024 * 1024u64 {
                return fail("Compiler file exceeds 2GiB");
            }
            let mut source = fs::File::open(&path)?;
            let mut hasher = Sha256::new();
            let mut buffer = [0; 65536];
            let mut size = 0;
            loop {
                let count = source.read(&mut buffer)?;
                if count == 0 {
                    break;
                }
                size += count as u64;
                result.bytes += count as u64;
                if size > metadata.len() || result.bytes > 16 * 1024 * 1024 * 1024u64 {
                    return fail("Compiler input changed or exceeds 16GiB");
                }
                hasher.update(&buffer[..count]);
            }
            if size != metadata.len() {
                return fail("Compiler input changed while hashing");
            }
            result
                .files
                .insert(relative, format!("{:x}", hasher.finalize()));
        } else {
            return fail("Compiler contains a non-file input");
        }
    }
    Ok(())
}

fn timing(text: &str) -> Result<Vec<Value>> {
    let mut kind = None;
    let mut rows = Vec::new();
    for line in text.lines() {
        let Some((key, value)) = line.split_once(':') else {
            continue;
        };
        if key.trim() == "Type" {
            kind = Some(value.trim());
        }
        if key.trim() == "Slack" {
            let slack: f64 = value.trim().parse()?;
            if !slack.is_finite() {
                return fail("Non-finite timing slack");
            }
            rows.push(
                json!({"type": kind.take().ok_or("Slack without timing type")?, "slack_ns": slack}),
            );
        }
    }
    if rows.is_empty() {
        return fail("Missing constrained-path timing results");
    }
    Ok(rows)
}

pub fn build(repo: &Path, output: &Path, quartus: &Path) -> Result<()> {
    let repo = repo.canonicalize()?;
    let quartus = quartus.canonicalize()?;
    if !quartus.join("quartus/bin/quartus_sh").is_file()
        || !repo.join("FPGA/SSMaster.qpf").is_file()
    {
        return fail("Missing original FPGA project or installed Quartus compiler");
    }
    if quartus.starts_with(&repo) {
        return fail("Compiler must be outside public build outputs");
    }
    if quartus.to_string_lossy().contains(',') {
        return fail("Compiler mount path must not contain commas");
    }
    let out = new_build_output(&repo, output)?;
    let mut endpoint = env::var("DOCKER_HOST").unwrap_or_default();
    if endpoint.is_empty() {
        endpoint = read_command(
            Command::new("docker").args([
                "context",
                "inspect",
                "--format",
                "{{.Endpoints.docker.Host}}",
            ]),
            &out.join("endpoint.log"),
        )?;
    }
    if !endpoint.starts_with("unix://") {
        return fail("Only a local Unix-socket Docker endpoint is admitted");
    }
    let docker = || {
        let mut cmd = Command::new("docker");
        cmd.env_remove("DOCKER_CONTEXT")
            .env_remove("DOCKER_HOST")
            .arg("--host")
            .arg(&endpoint);
        cmd
    };
    let image = read_command(
        docker().args([
            "image",
            "inspect",
            "--platform",
            "linux/amd64",
            IMAGE,
            "--format",
            "{{.Id}} {{.Os}}/{{.Architecture}}",
        ]),
        &out.join("runtime.log"),
    )?;
    if image != format!("{IMAGE} linux/amd64") {
        return fail("Unexpected cached Quartus runtime identity");
    }
    let arch = read_command(
        docker().args(["info", "--format", "{{.Architecture}}"]),
        &out.join("architecture.log"),
    )?;
    let qemu = match arch.as_str() {
        "aarch64" | "arm64" => "1",
        "amd64" | "x86_64" => "0",
        _ => return fail("Unsupported Docker architecture"),
    };
    let staged = out.join("source");
    fs::create_dir(&staged)?;
    let paths = read_command(
        Command::new("git").arg("-C").arg(&repo).args([
            "ls-files",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "FPGA",
        ]),
        &out.join("source-list.log"),
    )?;
    let mut hashes = BTreeMap::new();
    for name in paths.lines() {
        let path = Path::new(name);
        if path.components().count() != 2 || !path.starts_with("FPGA") || hashes.len() >= 64 {
            return fail("Unexpected FPGA input path/count");
        }
        let source = repo.join(path);
        if !source.symlink_metadata()?.is_file() || source.metadata()?.len() > 2 * 1024 * 1024 {
            return fail("FPGA input is not a bounded regular file");
        }
        let bytes = fs::read(source)?;
        fs::write(
            staged.join(path.file_name().ok_or("Missing input name")?),
            &bytes,
        )?;
        hashes.insert(name.to_owned(), sha(&bytes));
    }
    for name in [
        "SSMaster.qpf",
        "SSMaster.qsf",
        "SSMaster.sdc",
        "SSMaster.v",
        "ssmaster.cof",
    ] {
        if !hashes.contains_key(&format!("FPGA/{name}")) {
            return fail("Missing original project input");
        }
    }
    let original_qsf = fs::read_to_string(staged.join("SSMaster.qsf"))?;
    if !original_qsf.contains("-name DEVICE EP4CE6F17C8") {
        return fail("Unexpected target device");
    }
    fs::write(
        staged.join("SSMaster.qsf"),
        format!("{original_qsf}{QSF_ADDITIONS}"),
    )?;
    fs::write(staged.join("qualification-clock.sdc"), SDC)?;
    let wrapper = include_bytes!("../fpga/java-wrapper.sh");
    fs::write(out.join("java-wrapper.sh"), wrapper)?;
    fs::set_permissions(
        out.join("java-wrapper.sh"),
        fs::Permissions::from_mode(0o755),
    )?;
    let mut compiler = Compiler::default();
    compiler_tree(&quartus, &quartus, 0, &mut compiler)?;
    fs::write(
        out.join("compiler-inputs.json"),
        serde_json::to_vec_pretty(&json!({"files":compiler.files,"bytes":compiler.bytes}))?,
    )?;
    let mut receipt = json!({"source_sha256":hashes,"compiler_input_manifest_sha256":sha(&fs::read(out.join("compiler-inputs.json"))?),"runtime":IMAGE,
        "builder_sha256":sha(include_bytes!("fpga.rs")),"java_wrapper_sha256":sha(wrapper),"qsf_additions":QSF_ADDITIONS,"extra_sdc":SDC,
        "hardware_tested":false,"external_timing_qualified":false,"build_complete":false,"phases":[]});
    fs::write(
        out.join("request.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    let common = [
        "run",
        "--rm",
        "--pull=never",
        "--platform=linux/amd64",
        "--network=none",
        "--cpus=2",
        "--memory=4g",
        "--ulimit=core=0",
    ];
    let mounts = [
        format!(
            "type=bind,source={},target=/opt/quartus,readonly",
            quartus.display()
        ),
        format!(
            "type=bind,source={}/quartus/linux64/jre64,target=/saroo/original-jre,readonly",
            quartus.display()
        ),
        format!(
            "type=bind,source={},target=/opt/quartus/quartus/linux64/jre64/bin/java,readonly",
            out.join("java-wrapper.sh").display()
        ),
        format!("type=bind,source={},target=/project", staged.display()),
    ];
    let started = Instant::now();
    for tool in [
        "quartus_map",
        "quartus_fit",
        "quartus_asm",
        "quartus_sta",
        "quartus_cpf",
    ] {
        let identity = format!(
            "saroo-fpga-{}-{}",
            std::process::id(),
            SystemTime::now().duration_since(UNIX_EPOCH)?.as_nanos()
        );
        let mut args: Vec<String> = common.iter().map(|s| s.to_string()).collect();
        args.extend([
            "--name".into(),
            identity.clone(),
            "--env".into(),
            format!("SAROO_QEMU={qemu}"),
            "--env=QEMU_GUEST_BASE=0x800000000000".into(),
        ]);
        for mount in &mounts {
            args.extend(["--mount".into(), mount.clone()]);
        }
        args.extend([
            "--workdir=/project".into(),
            IMAGE.into(),
            "/bin/bash".into(),
            "/saroo/run-quartus.sh".into(),
            tool.into(),
        ]);
        if tool == "quartus_cpf" {
            args.extend(["-c".into(), "ssmaster.cof".into()]);
        } else {
            if ["quartus_map", "quartus_fit", "quartus_asm"].contains(&tool) {
                args.extend([
                    "--read_settings_files=on".into(),
                    "--write_settings_files=off".into(),
                ]);
            }
            args.extend(["SSMaster".into(), "-c".into(), "SSMaster".into()]);
        }
        let phase = json!({"tool":tool,"container":identity,"arguments":args,"complete":false});
        receipt["phases"].as_array_mut().unwrap().push(phase);
        fs::write(
            out.join("receipt.json"),
            serde_json::to_vec_pretty(&receipt)?,
        )?;
        let result = run(docker().args(&args), &out.join(format!("{tool}.log")), 3600);
        if result.is_err() {
            // Client timeout is not container completion. Remove only this
            // recorded container, with a separate bounded cleanup operation.
            let cleanup = run(
                docker().args(["rm", "-f", &identity]),
                &out.join(format!("{tool}-cleanup.log")),
                20,
            );
            receipt["cleanup"] = json!(cleanup.is_ok_and(|ok| ok));
        }
        let success = result.as_ref().is_ok_and(|ok| *ok);
        let row = receipt["phases"]
            .as_array_mut()
            .unwrap()
            .last_mut()
            .unwrap();
        row["complete"] = json!(success);
        if !success {
            receipt["error"] = json!(result
                .err()
                .map(|e| e.to_string())
                .unwrap_or_else(|| "Compiler stage failed".into()));
            fs::write(
                out.join("receipt.json"),
                serde_json::to_vec_pretty(&receipt)?,
            )?;
            return fail("Quartus build failed; preserve and inspect its logs/identity");
        }
    }
    let mut artifacts = BTreeMap::new();
    for name in REQUIRED {
        let path = staged.join("output_files").join(name);
        if !path.symlink_metadata()?.is_file()
            || path.metadata()?.len() == 0
            || path.metadata()?.len() > 64 * 1024 * 1024
        {
            return fail("Missing/invalid required FPGA artifact");
        }
        let bytes = fs::read(&path)?;
        artifacts.insert(*name, json!({"bytes":bytes.len(),"sha256":sha(&bytes)}));
    }
    for entry in fs::read_dir(&staged)? {
        if entry?
            .file_name()
            .to_string_lossy()
            .starts_with("hs_err_pid")
        {
            return fail("Java helper crashed; inspect preserved log");
        }
    }
    for tool in [
        "quartus_map",
        "quartus_fit",
        "quartus_asm",
        "quartus_sta",
        "quartus_cpf",
    ] {
        let log = fs::read_to_string(out.join(format!("{tool}.log")))?;
        if [
            "A fatal error has been detected by the Java Runtime Environment",
            "Internal Error:",
            "Segmentation fault",
            "qemu: uncaught target signal",
        ]
        .iter()
        .any(|marker| log.contains(marker))
        {
            return fail("Compiler/helper fatal marker despite successful stage exit");
        }
    }
    let slacks = timing(&fs::read_to_string(
        staged.join("output_files/SSMaster.sta.summary"),
    )?)?;
    let met = slacks
        .iter()
        .all(|row| row["slack_ns"].as_f64().unwrap() >= 0.0);
    let mut after = Compiler::default();
    compiler_tree(&quartus, &quartus, 0, &mut after)?;
    if after != compiler {
        return fail("Read-only compiler inputs changed during build");
    }
    receipt["artifacts"] = json!(artifacts);
    receipt["timing_slacks"] = json!(slacks);
    receipt["constrained_timing_met"] = json!(met);
    receipt["compiler_inputs_unchanged"] = json!(true);
    receipt["build_complete"] = json!(true);
    receipt["elapsed_seconds"] = json!(started.elapsed().as_secs_f64());
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    println!("{}", serde_json::to_string_pretty(&receipt)?);
    // Producing bitstreams is separate from timing qualification. External
    // buses remain incompletely constrained even when reported slack passes.
    if !met {
        return fail("Build artifacts complete but constrained timing failed; see receipt");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn timing_results_preserve_negative_slack_and_reject_missing_or_nonfinite() {
        let rows = timing("Type  : Setup\nSlack : -0.125\n  Type : Hold\n Slack : 0.5\n").unwrap();
        assert_eq!(rows[0]["slack_ns"], -0.125);
        assert_eq!(rows[1]["type"], "Hold");
        for bad in ["", "Slack : 1\n", "Type : Setup\nSlack : NaN\n"] {
            assert!(timing(bad).is_err());
        }
    }
    #[test]
    fn compiler_manifest_detects_changes_and_rejects_escaping_symlinks() {
        let project = Path::new(env!("CARGO_MANIFEST_DIR"))
            .ancestors()
            .nth(3)
            .unwrap();
        let root = project.join("tests/build").join(format!(
            "compiler-fixture-{}-{}",
            std::process::id(),
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_nanos()
        ));
        fs::create_dir_all(&root).unwrap();
        fs::write(root.join("compiler"), b"original").unwrap();
        let mut first = Compiler::default();
        compiler_tree(&root, &root, 0, &mut first).unwrap();
        fs::write(root.join("compiler"), b"modified").unwrap();
        let mut second = Compiler::default();
        compiler_tree(&root, &root, 0, &mut second).unwrap();
        assert!(first != second);
        std::os::unix::fs::symlink("/", root.join("outside")).unwrap();
        assert!(compiler_tree(&root, &root, 0, &mut Compiler::default()).is_err());
        fs::remove_dir_all(&root).unwrap();
    }
}
