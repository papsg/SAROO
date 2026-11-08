//! Isolated builds of the public firmware. No downloads or device operations.
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    env, fs,
    path::{Component, Path, PathBuf},
    process::{Command, Stdio},
    thread,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
const IMAGE: &str =
    "ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6";
const PREFIX: &str = "/opt/tool-chains/sh2eb-elf/bin/sh2eb-elf-";

fn fail<T>(message: &str) -> Result<T> {
    Err(message.into())
}

fn capture(cmd: &mut Command) -> Result<String> {
    let output = cmd.output()?;
    if !output.status.success() {
        return fail(&String::from_utf8_lossy(&output.stderr));
    }
    Ok(String::from_utf8(output.stdout)?.trim().into())
}

fn sha(bytes: &[u8]) -> String {
    format!("{:x}", Sha256::digest(bytes))
}

fn word(bytes: &[u8], offset: usize, width: usize) -> Result<u32> {
    let end = offset.checked_add(width).ok_or("ELF offset overflow")?;
    let field = bytes.get(offset..end).ok_or("Truncated ELF field")?;
    Ok(field
        .iter()
        .fold(0, |value, byte| (value << 8) | u32::from(*byte)))
}

fn resources(elf: &[u8], image: &[u8], font: &[u8]) -> Result<Value> {
    if elf.get(..6) != Some(b"\x7fELF\x01\x02") || word(elf, 18, 2)? != 42 {
        return fail("Expected a big-endian ELF32 SuperH executable");
    }
    if image.len() != 131072 + font.len()
        || image.len() > 524288
        || image.get(131072..) != Some(font)
    {
        return fail("Menu body/font size or appended font bytes are invalid");
    }
    let offset = word(elf, 32, 4)? as usize;
    let stride = word(elf, 46, 2)? as usize;
    let count = word(elf, 48, 2)? as usize;
    if stride != 40 || count == 0 || count > 1024 {
        return fail("Invalid ELF section table dimensions");
    }
    let end = offset
        .checked_add(stride * count)
        .ok_or("ELF table overflow")?;
    if end > elf.len() {
        return fail("Truncated ELF section table");
    }
    let section = |index: usize| -> Result<&[u8]> {
        if index >= count {
            return fail("Invalid ELF section index");
        }
        Ok(&elf[offset + index * stride..offset + (index + 1) * stride])
    };
    let mut symbols = BTreeMap::new();
    let required = [
        "__ip_start",
        "__ip_end",
        "__bss_start",
        "__stack_start",
        "__stack_end",
        "__bss_end",
    ];
    for index in 0..count {
        let sec = section(index)?;
        if word(sec, 4, 4)? != 2 {
            continue;
        }
        let strings = section(word(sec, 24, 4)? as usize)?;
        let string_offset = word(strings, 16, 4)? as usize;
        let string_size = word(strings, 20, 4)? as usize;
        let strings = elf
            .get(
                string_offset
                    ..string_offset
                        .checked_add(string_size)
                        .ok_or("String table overflow")?,
            )
            .ok_or("Truncated ELF strings")?;
        let start = word(sec, 16, 4)? as usize;
        let size = word(sec, 20, 4)? as usize;
        if word(sec, 36, 4)? != 16 || size % 16 != 0 {
            return fail("Invalid ELF symbol table dimensions");
        }
        let entries = elf
            .get(start..start.checked_add(size).ok_or("Symbol table overflow")?)
            .ok_or("Truncated ELF symbols")?;
        for entry in entries.chunks_exact(16) {
            let name = strings
                .get(word(entry, 0, 4)? as usize..)
                .ok_or("Invalid symbol name")?;
            let name = &name[..name
                .iter()
                .position(|b| *b == 0)
                .ok_or("Unterminated symbol name")?];
            if let Ok(name) = std::str::from_utf8(name) {
                if required.contains(&name)
                    && symbols
                        .insert(name.to_owned(), word(entry, 4, 4)?)
                        .is_some()
                {
                    return fail("Ambiguous menu resource symbol");
                }
            }
        }
    }
    let get = |name: &str| {
        symbols
            .get(name)
            .copied()
            .ok_or_else(|| format!("Missing menu symbol {name}"))
    };
    let base = get("__ip_start")?;
    let loaded_end = get("__ip_end")?;
    let bss_start = get("__bss_start")?;
    let stack_start = get("__stack_start")?;
    let stack_end = get("__stack_end")?;
    if base != 0x02000000
        || loaded_end < base
        || bss_start < loaded_end
        || stack_start < bss_start
        || stack_end.checked_sub(stack_start) != Some(16384)
        || stack_end != get("__bss_end")?
        || stack_end > base + 131072
    {
        return fail("Menu code/BSS/stack exceeds the original body layout");
    }
    let entry = word(elf, 24, 4)?;
    if entry < base || entry >= loaded_end {
        return fail("Menu entry is outside loaded code/data");
    }
    Ok(
        json!({"code_data_bytes": loaded_end-base, "bss_bytes": stack_start-bss_start,
        "reserved_stack_bytes": stack_end-stack_start, "body_margin_bytes": base+131072-stack_end,
        "entry": entry, "symbols": symbols, "image_bytes": image.len(), "font_bytes": font.len(),
        "elf_sha256": sha(elf), "image_sha256": sha(image), "font_sha256": sha(font)}),
    )
}

fn build(repo: &Path, out: &Path) -> Result<()> {
    let repo = repo.canonicalize()?;
    if !repo.join("Firm_Saturn/Makefile").is_file() {
        return fail("Repository has no original Firm_Saturn Makefile");
    }
    let out = if out.is_absolute() {
        out.to_owned()
    } else {
        repo.join(out)
    };
    if out
        .components()
        .any(|part| matches!(part, Component::ParentDir))
        || out.to_string_lossy().contains(',')
    {
        return fail("Output path must not contain parent traversal or commas");
    }
    let build_root = repo.join("tests/build");
    fs::create_dir_all(&build_root)?;
    if !build_root.canonicalize()?.starts_with(&repo) || !out.starts_with(&build_root) {
        return fail("Output must be inside this repository's tests/build directory");
    }
    fs::create_dir_all(out.parent().ok_or("Output has no parent")?)?;
    if !out
        .parent()
        .unwrap()
        .canonicalize()?
        .starts_with(build_root.canonicalize()?)
    {
        return fail("Output parent escapes tests/build");
    }
    // Refuse reuse: a prior source cohort, failure or build remains reviewable.
    fs::create_dir(&out)?;
    let staged = out.join("source");
    fs::create_dir(&staged)?;
    let paths = Command::new("git")
        .arg("-C")
        .arg(&repo)
        .args([
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "Firm_Saturn",
        ])
        .output()?;
    if !paths.status.success() {
        return fail("Cannot enumerate public menu inputs");
    }
    let mut sources = BTreeMap::new();
    let mut total = 0usize;
    for path in paths
        .stdout
        .split(|byte| *byte == 0)
        .filter(|path| !path.is_empty())
    {
        let path = PathBuf::from(std::str::from_utf8(path)?);
        let relative = path.strip_prefix("Firm_Saturn")?;
        if relative
            .components()
            .any(|part| !matches!(part, Component::Normal(_)))
            || relative.as_os_str().is_empty()
        {
            return fail("Invalid menu input path");
        }
        let source = repo.join(&path);
        if !source.symlink_metadata()?.file_type().is_file()
            || source.metadata()?.len() > 2 * 1024 * 1024
        {
            return fail("Menu input is not a bounded regular file");
        }
        let bytes = fs::read(source)?;
        total += bytes.len();
        if sources.len() >= 1024 || total > 64 * 1024 * 1024 {
            return fail("Menu input cohort exceeds build limits");
        }
        let destination = staged.join(relative);
        fs::create_dir_all(destination.parent().unwrap())?;
        fs::write(destination, &bytes)?;
        sources.insert(path, sha(&bytes));
    }
    let head = capture(
        Command::new("git")
            .arg("-C")
            .arg(&repo)
            .args(["rev-parse", "HEAD"]),
    )?;
    let endpoint = if let Ok(value) = env::var("DOCKER_HOST") {
        value
    } else {
        capture(Command::new("docker").args([
            "context",
            "inspect",
            "--format",
            "{{.Endpoints.docker.Host}}",
        ]))?
    };
    if !endpoint.starts_with("unix://") {
        return fail("Only a local Unix-socket Docker endpoint is admitted");
    }
    let identity = format!(
        "saroo-build-{}-{}",
        std::process::id(),
        SystemTime::now().duration_since(UNIX_EPOCH)?.as_nanos()
    );
    let mount = format!(
        "type=bind,source={},target=/build",
        staged.canonicalize()?.display()
    );
    let flags = vec!["run".into(), "--rm".into(), "--pull=never".into(), "--network=none".into(),
        "--platform=linux/amd64".into(), "--name".into(), identity.clone(), "--mount".into(), mount,
        "--workdir=/build".into(), "--env=SOURCE_DATE_EPOCH=1765029659".into(), "--entrypoint=make".into(), IMAGE.into(),
        format!("CC={PREFIX}gcc -mb"), format!("AS={PREFIX}as"), format!("OBJDUMP={PREFIX}objdump"), format!("OBJCOPY={PREFIX}objcopy"),
        "LDFLAGS=-m2 -mb -nostartfiles -nostdlib -T ldscript -Wl,--oformat=elf32-sh,--build-id=none,-Map,ssfirm.map".into(),
        "LIBS=-lyaul -lgcc".into()];
    let mut receipt = json!({"head": head, "source_sha256": sources, "docker_endpoint": endpoint,
        "docker_image": IMAGE, "command_arguments": flags, "success": false,
        "qualification": "Target compile/link and body/font/resource bounds only; no device execution"});
    fs::write(
        out.join("request.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    let mut child = Command::new("docker")
        .env_remove("DOCKER_CONTEXT")
        .env_remove("DOCKER_HOST")
        .arg("--host")
        .arg(&endpoint)
        .args(&flags)
        .stdout(Stdio::from(fs::File::create(out.join("stdout.log"))?))
        .stderr(Stdio::from(fs::File::create(out.join("stderr.log"))?))
        .spawn()?;
    let started = Instant::now();
    let result = loop {
        if let Some(status) = child.try_wait()? {
            break status.success();
        }
        if started.elapsed() > Duration::from_secs(300) {
            child.kill()?;
            child.wait()?;
            let mut cleanup = Command::new("docker")
                .env_remove("DOCKER_CONTEXT")
                .env_remove("DOCKER_HOST")
                .arg("--host")
                .arg(&endpoint)
                .args(["rm", "-f", &identity])
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .spawn()?;
            let deadline = Instant::now();
            while cleanup.try_wait()?.is_none() {
                if deadline.elapsed() > Duration::from_secs(20) {
                    cleanup.kill()?;
                    cleanup.wait()?;
                    break;
                }
                thread::sleep(Duration::from_millis(50));
            }
            receipt["timeout"] = json!(true);
            break false;
        }
        thread::sleep(Duration::from_millis(50));
    };
    receipt["elapsed_ms"] = json!(started.elapsed().as_millis());
    let verification = if result {
        resources(
            &fs::read(staged.join("ssfirm.elf"))?,
            &fs::read(staged.join("ssfirm.bin"))?,
            &fs::read(staged.join("font_cjk.bin"))?,
        )
    } else {
        fail("Menu make failed or timed out; inspect preserved build logs")
    };
    match verification {
        Ok(value) => {
            receipt["resources"] = value;
            receipt["success"] = json!(true);
        }
        Err(error) => receipt["error"] = json!(error.to_string()),
    }
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    println!("{}", serde_json::to_string_pretty(&receipt)?);
    if receipt["success"] != true {
        return fail("Firmware build did not qualify");
    }
    Ok(())
}

fn run() -> Result<()> {
    let args: Vec<_> = env::args().skip(1).collect();
    if args.is_empty() || args == ["--help"] {
        println!("firmware-build menu --repo REPOSITORY --out tests/build/UNUSED-DIRECTORY\nUses the cached pinned Yaul image and original Makefile; no network or devices.");
        return Ok(());
    }
    if args.len() != 5 || args[0] != "menu" || args[1] != "--repo" || args[3] != "--out" {
        return fail("Expected: menu --repo REPOSITORY --out UNUSED-DIRECTORY");
    }
    build(Path::new(&args[2]), Path::new(&args[4]))
}

fn main() {
    if let Err(error) = run() {
        eprintln!("firmware-build: {error}");
        std::process::exit(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn rejects_wrong_machine_encoding_and_truncated_elf() {
        let mut elf = vec![0; 52];
        let image = vec![0; 131072];
        assert!(resources(&elf, &image, &[]).is_err());
        elf[..6].copy_from_slice(b"\x7fELF\x01\x02");
        elf[19] = 42;
        assert!(resources(&elf, &image, &[]).is_err());
        elf[47] = 40;
        elf[49] = 1;
        elf[35] = 52;
        assert!(resources(&elf, &image, &[]).is_err());
        assert!(word(&elf, usize::MAX, 4).is_err());
    }
    #[test]
    fn pins_cached_image_and_checks_font_extent() {
        assert_eq!(IMAGE.split("@sha256:").nth(1).unwrap().len(), 64);
        let mut elf = vec![0; 52];
        elf[..6].copy_from_slice(b"\x7fELF\x01\x02");
        elf[19] = 42;
        assert!(resources(&elf, &[0; 64], &[]).is_err());
        assert!(resources(&elf, &vec![0; 131073], &[1]).is_err());
    }
}
