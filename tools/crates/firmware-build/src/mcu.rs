//! Offline GNU compile/link accounting for the original MDK App, never installation.
use super::{capture, fail, new_build_output, parse_stack_usage, sha, Result};
use serde_json::{json, Value};
use std::{
    collections::BTreeMap,
    fs,
    io::Read,
    os::unix::process::CommandExt,
    path::{Component, Path, PathBuf},
    process::{Command, Stdio},
    thread,
    time::{Duration, Instant},
};

#[derive(Default)]
struct Inputs {
    hashes: BTreeMap<String, String>,
    bytes: u64,
}

unsafe extern "C" {
    fn kill(pid: i32, signal: i32) -> i32;
}

fn copy_file(from: &Path, to: &Path, key: String, hashes: &mut Inputs) -> Result<()> {
    if !from.symlink_metadata()?.is_file()
        || from.metadata()?.len() > 8 * 1024 * 1024
        || hashes.hashes.len() >= 4096
    {
        return fail("MCU input is not a bounded regular file");
    }
    let mut bytes = Vec::new();
    fs::File::open(from)?
        .take(8 * 1024 * 1024 + 1)
        .read_to_end(&mut bytes)?;
    if bytes.len() > 8 * 1024 * 1024 || hashes.bytes + bytes.len() as u64 > 128 * 1024 * 1024 {
        return fail("MCU input cohort exceeds 128MiB");
    }
    hashes.bytes += bytes.len() as u64;
    fs::create_dir_all(to.parent().ok_or("Input has no parent")?)?;
    fs::write(to, &bytes)?;
    hashes.hashes.insert(key, sha(&bytes));
    Ok(())
}

fn copy_tree(from: &Path, to: &Path, prefix: &str, hashes: &mut Inputs) -> Result<()> {
    if prefix.split('/').count() > 16 {
        return fail("MCU SDK input nesting exceeds 16 levels");
    }
    for entry in fs::read_dir(from)? {
        let entry = entry?;
        if entry.file_type()?.is_symlink() {
            return fail("MCU SDK symlinks are not staged");
        }
        let key = format!("{prefix}/{}", entry.file_name().to_string_lossy());
        if entry.file_type()?.is_dir() {
            copy_tree(&entry.path(), &to.join(entry.file_name()), &key, hashes)?;
        } else if entry
            .path()
            .extension()
            .is_some_and(|s| ["h", "c", "S", "s", "txt"].iter().any(|x| s == *x))
            || entry.file_name().to_string_lossy().starts_with("LICENSE")
        {
            copy_file(&entry.path(), &to.join(entry.file_name()), key, hashes)?;
        }
    }
    Ok(())
}

fn app_sources(project: &str) -> Result<Vec<PathBuf>> {
    let app = project
        .split_once("<Target>")
        .ok_or("Missing App target")?
        .1
        .split_once("</Target>")
        .ok_or("Unterminated App target")?
        .0;
    if !app.contains("<TargetName>App</TargetName>") || !app.contains("STM32H750VBTx") {
        return fail("Expected the original H750 App target");
    }
    let mut paths = Vec::new();
    for rest in app.split("<FilePath>").skip(1) {
        let value = rest
            .split_once("</FilePath>")
            .ok_or("Unterminated App path")?
            .0;
        if !value.ends_with(".c") {
            continue;
        }
        let value = value
            .strip_prefix(".\\")
            .ok_or("App input must be relative")?
            .replace('\\', "/");
        let path = PathBuf::from(value);
        if path
            .components()
            .any(|p| !matches!(p, Component::Normal(_)))
            || path.to_string_lossy().contains('&')
            || paths.contains(&path)
        {
            return fail("Ambiguous App source path");
        }
        paths.push(path);
    }
    if paths.is_empty() || paths.len() > 32 {
        return fail("Unexpected App source count");
    }
    Ok(paths)
}

fn vector_names(original: &str) -> Result<Vec<String>> {
    let begin = original
        .find("__Vectors       DCD")
        .ok_or("Missing original vector table")?;
    let end = original[begin..]
        .find("__Vectors_End")
        .ok_or("Missing vector table end")?
        + begin;
    let mut names = Vec::new();
    for line in original[begin..end].lines() {
        if let Some((_, rest)) = line.split_once("DCD") {
            let name = rest
                .split_whitespace()
                .next()
                .ok_or("Missing vector value")?;
            if !name.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_') {
                return fail("Unexpected original vector expression");
            }
            names.push(name.to_owned());
        }
    }
    if names.len() != 166 || names[0] != "__initial_sp" || names[1] != "Reset_Handler" {
        return fail("Original vector layout changed");
    }
    Ok(names)
}

fn verify_vectors(elf: &[u8], names: &[String], symbols: &BTreeMap<String, u32>) -> Result<()> {
    let word = |offset: usize| -> Result<u32> {
        Ok(u32::from_le_bytes(
            elf.get(offset..offset + 4)
                .ok_or("Truncated MCU ELF word")?
                .try_into()?,
        ))
    };
    let short = |offset: usize| -> Result<u16> {
        Ok(u16::from_le_bytes(
            elf.get(offset..offset + 2)
                .ok_or("Truncated MCU ELF halfword")?
                .try_into()?,
        ))
    };
    let headers = word(28)? as usize;
    let width = short(42)? as usize;
    let count = short(44)? as usize;
    if width != 32 || count > 32 {
        return fail("Unexpected MCU ELF program headers");
    }
    let mut vector_offset = None;
    for index in 0..count {
        let at = headers
            .checked_add(index * width)
            .ok_or("MCU program-header overflow")?;
        if word(at)? == 1 && word(at + 8)? == 0 && word(at + 16)? >= 664 {
            vector_offset = Some(word(at + 4)? as usize);
        }
    }
    let offset = vector_offset.ok_or("Missing loaded MCU vector table")?;
    for (index, name) in names.iter().enumerate() {
        let expected = if name == "0" {
            0
        } else {
            let address = *symbols
                .get(name)
                .ok_or_else(|| format!("Missing vector symbol {name}"))?;
            if index == 0 {
                address
            } else {
                address | 1
            }
        };
        if word(offset + index * 4)? != expected {
            return fail(&format!("Wrong linked vector {index} ({name})"));
        }
    }
    for name in [
        "SVC_Handler",
        "PendSV_Handler",
        "SysTick_Handler",
        "SDMMC1_IRQHandler",
        "EXTI1_IRQHandler",
        "TIM2_IRQHandler",
    ] {
        if symbols.get(name).is_none() || symbols.get(name) == symbols.get("Default_Handler") {
            return fail(&format!(
                "Required handler {name} did not override its weak default"
            ));
        }
    }
    Ok(())
}

fn execute(
    command: &mut Command,
    out: &Path,
    rows: &mut Vec<Value>,
    deadline: Instant,
) -> Result<()> {
    let arguments = format!("{command:?}");
    let started = Instant::now();
    let log = fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(out.join("build.log"))?;
    command
        .stdout(Stdio::from(log.try_clone()?))
        .stderr(Stdio::from(log));
    // Own a separate process group, including GCC's compiler/assembler children.
    command.process_group(0);
    let mut child = command.spawn()?;
    let status = loop {
        if let Some(status) = child.try_wait()? {
            break status;
        }
        if started.elapsed() > Duration::from_secs(60) || Instant::now() >= deadline {
            // Only the group created above is addressed; never a caller's group.
            unsafe {
                kill(-(child.id() as i32), 9);
            }
            let _ = child.kill();
            let _ = child.wait();
            rows.push(json!({"arguments":arguments,"timed_out":true}));
            return fail("MCU compiler/link command timed out");
        }
        thread::sleep(Duration::from_millis(20));
    };
    rows.push(json!({"arguments":arguments,"success":status.success(),"elapsed_ms":started.elapsed().as_millis()}));
    if !status.success() {
        return fail("MCU compile/link failed; preserved build.log has details");
    }
    Ok(())
}

pub fn build(
    repo: &Path,
    output: &Path,
    toolchain: &Path,
    cmsis: &Path,
    stm32: &Path,
    rtx: &Path,
) -> Result<()> {
    let repo = repo.canonicalize()?;
    let out = new_build_output(&repo, output)?;
    let app = out.join("Firm_MCU");
    fs::create_dir(&app)?;
    let mut hashes = Inputs::default();
    let listed = Command::new("git")
        .arg("-C")
        .arg(&repo)
        .args([
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "Firm_MCU",
        ])
        .output()?;
    if !listed.status.success() {
        return fail("Cannot enumerate public MCU inputs");
    }
    for name in listed.stdout.split(|x| *x == 0).filter(|x| !x.is_empty()) {
        let path = Path::new(std::str::from_utf8(name)?);
        let rel = path.strip_prefix("Firm_MCU")?;
        if rel.components().any(|p| !matches!(p, Component::Normal(_))) {
            return fail("Invalid public MCU input path");
        }
        if !repo.join(path).canonicalize()?.starts_with(&repo) {
            return fail("Public MCU input escapes repository");
        }
        copy_file(
            &repo.join(path),
            &app.join(rel),
            path.to_string_lossy().into_owned(),
            &mut hashes,
        )?;
    }
    let project = fs::read_to_string(app.join("ssmaster.uvprojx"))?;
    let mut sources: Vec<_> = app_sources(&project)?
        .into_iter()
        .map(|x| app.join(x))
        .collect();
    // MDK selects this configured component through RTE rather than FilePath.
    let config = app.join("RTE/CMSIS/RTX_Config.c");
    if !sources.contains(&config) {
        sources.push(config);
    }
    let sdk = out.join("sdk");
    copy_tree(
        &cmsis.join("Core/Include"),
        &sdk.join("core"),
        "CMSIS/Core/Include",
        &mut hashes,
    )?;
    copy_tree(stm32, &sdk.join("stm32"), "STM32/Include", &mut hashes)?;
    copy_tree(rtx, &sdk.join("rtos"), "CMSIS/RTOS2", &mut hashes)?;
    let mut rtx_sources: Vec<_> = fs::read_dir(sdk.join("rtos/RTX/Source"))?
        .map(|e| e.map(|e| e.path()))
        .collect::<std::io::Result<_>>()?;
    rtx_sources.sort();
    sources.extend(
        rtx_sources
            .into_iter()
            .filter(|x| x.extension().is_some_and(|s| s == "c")),
    );
    sources.push(sdk.join("rtos/RTX/Source/GCC/irq_cm4f.S"));
    sources.push(sdk.join("rtos/Source/os_systick.c"));
    if sources.len() > 80 {
        return fail("Unexpected MCU/RTX translation-unit count");
    }
    let names = vector_names(&fs::read_to_string(
        app.join("Startup/startup_stm32h750xx.s"),
    )?)?;
    let mut startup = String::from(".syntax unified\n.thumb\n.section .isr_vector,\"a\",%progbits\n.balign 1024\n.global __Vectors\n__Vectors:\n");
    for name in &names {
        startup.push_str(&format!(".word {name}\n"));
    }
    startup.push_str(".global __Vectors_End\n__Vectors_End:\n");
    for name in &names[2..] {
        if name != "0" && name != "HardFault_Handler" {
            startup.push_str(&format!(
                ".weak {name}\n.thumb_set {name},Default_Handler\n"
            ));
        }
    }
    startup.push_str(include_str!("mcu-startup.S"));
    fs::write(out.join("startup.S"), startup)?;
    fs::write(out.join("link.ld"), include_str!("mcu-link.ld"))?;
    fs::write(
        out.join("LICENSE-ST-startup"),
        include_str!("../LICENSE-ST-startup"),
    )?;
    // MDK's disable primitive returns the previous mask; GCC's intrinsic is void.
    fs::write(out.join("mdk-irq.h"),"#include \"stm32h7xx.h\"\n#define __disable_irq() ({ uint32_t saved = __get_PRIMASK(); __asm volatile(\"cpsid i\" ::: \"memory\"); saved; })\n")?;
    sources.push(out.join("startup.S"));
    let bin = toolchain.canonicalize()?.join("bin");
    let cc = bin.join("arm-none-eabi-gcc");
    if capture(Command::new(&cc).arg("-dumpmachine"))?.trim() != "arm-none-eabi" {
        return fail("Expected Arm bare-metal GCC");
    }
    let version = capture(Command::new(&cc).arg("--version"))?;
    let compiler_sha = sha(&fs::read(&cc)?);
    let flags = [
        "-mcpu=cortex-m7",
        "-mthumb",
        "-mfpu=fpv5-d16",
        "-mfloat-abi=softfp",
        "-Os",
        "-g",
        "-fstack-usage",
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-strict-aliasing",
        "-fno-delete-null-pointer-checks",
        "-DSTM32H750xx",
        "-DHSE_VALUE=16000000",
    ];
    let includes = [
        app.join("inc"),
        app.join("FatFS"),
        app.join("RTE/CMSIS"),
        app.join("RTE/_App"),
        sdk.join("core"),
        sdk.join("stm32"),
        sdk.join("rtos/Include"),
        sdk.join("rtos/RTX/Include"),
        sdk.join("rtos/RTX/Source"),
    ];
    let mut rows = Vec::new();
    let mut objects = Vec::new();
    let deadline = Instant::now() + Duration::from_secs(300);
    let result = (|| -> Result<Value> {
        for (index, source) in sources.iter().enumerate() {
            let obj = out.join(format!(
                "{index}-{}.o",
                source.file_stem().unwrap().to_string_lossy()
            ));
            let mut command = Command::new(&cc);
            command.args(flags).env("SOURCE_DATE_EPOCH", "1765029659");
            command
                .arg(format!("-ffile-prefix-map={}=.", out.display()))
                .arg(format!("-fdebug-prefix-map={}=.", out.display()));
            if source.extension().is_some_and(|s| s == "c") {
                command
                    .args(["-std=gnu11", "-include"])
                    .arg(out.join("mdk-irq.h"));
            }
            for include in &includes {
                command.arg("-I").arg(include);
            }
            command.arg("-c").arg(source).arg("-o").arg(&obj);
            execute(&mut command, &out, &mut rows, deadline)?;
            objects.push(obj);
        }
        let elf = out.join("qualification.elf");
        let mut command = Command::new(&cc);
        command
            .args(flags)
            .args([
                "-nostartfiles",
                "--specs=nano.specs",
                "--specs=nosys.specs",
                "-Wl,--gc-sections,--build-id=none",
            ])
            .arg("-T")
            .arg(out.join("link.ld"))
            .arg(format!(
                "-Wl,-Map={}",
                out.join("qualification.map").display()
            ))
            .args(&objects)
            .arg("-o")
            .arg(&elf);
        execute(&mut command, &out, &mut rows, deadline)?;
        let raw = fs::read(&elf)?;
        if raw.len() < 52
            || &raw[..7] != b"\x7fELF\x01\x01\x01"
            || u16::from_le_bytes([raw[18], raw[19]]) != 40
        {
            return fail("MCU output is not ELF32 little-endian ARM");
        }
        let table = capture(
            Command::new(bin.join("arm-none-eabi-nm"))
                .arg("--defined-only")
                .arg(&elf),
        )?;
        let mut symbols = BTreeMap::new();
        for line in table.lines() {
            let parts: Vec<_> = line.split_whitespace().collect();
            if parts.len() == 3 {
                if let Ok(n) = u32::from_str_radix(parts[0], 16) {
                    symbols.insert(parts[2].to_owned(), n);
                }
            }
        }
        let get = |n: &str| {
            symbols
                .get(n)
                .copied()
                .ok_or_else(|| format!("Missing MCU symbol {n}"))
        };
        verify_vectors(&raw, &names, &symbols)?;
        if u32::from_le_bytes(raw[24..28].try_into()?) != (get("Reset_Handler")? | 1) {
            return fail("MCU ELF entry does not match the Thumb reset vector");
        }
        let image = get("_image_end")?;
        if get("_sdata")? != 0x20000000
            || get("_edata")? < get("_sdata")?
            || get("_sbss")? < get("_edata")?
            || get("_ebss")? < get("_sbss")?
            || get("_stack_top")? != 0x20020000
            || get("_ebss")? > get("_stack_top")?
        {
            return fail("Unexpected MCU data/BSS/stack placement");
        }
        let bss = get("_ebss")? - get("_sbss")?;
        let gap = get("_stack_top")? - get("_ebss")?;
        if image > 65536 || gap < 1536 || get("__Vectors")? != 0 || get("__Vectors_End")? != 664 {
            return fail("MCU App/vector/resource limits failed");
        }
        let mut frames = BTreeMap::new();
        for entry in fs::read_dir(&out)? {
            let p = entry?.path();
            if p.extension().is_some_and(|x| x == "su") {
                if p.metadata()?.len() > 131072 || frames.len() >= 80 {
                    return fail("MCU compiler stack report exceeds bounds");
                }
                frames.insert(
                    p.file_name().unwrap().to_string_lossy().into_owned(),
                    parse_stack_usage(&fs::read_to_string(p)?)?,
                );
            }
        }
        let saves: BTreeMap<_, _> = symbols
            .iter()
            .filter(|(n, _)| {
                [
                    "open_savefile",
                    "load_savefile",
                    "flush_savefile",
                    "load_smems",
                    "flush_smems",
                ]
                .contains(&n.as_str())
            })
            .collect();
        Ok(
            json!({"image_load_bytes":image,"itcm_free_bytes":65536-image,"dtcm_data_bss_span":get("_ebss")?-0x20000000,"bss_bytes":bss,"data_bytes":get("_edata")?-get("_sdata")?,"main_stack_heap_address_gap":gap,"vector_entries":166,"elf_sha256":sha(&raw),"save_symbols":saves,"compiler_direct_stack":frames}),
        )
    })();
    let mut glue = BTreeMap::new();
    for name in ["startup.S", "link.ld", "mdk-irq.h"] {
        glue.insert(name, sha(&fs::read(out.join(name))?));
    }
    let head = capture(
        Command::new("git")
            .arg("-C")
            .arg(&repo)
            .args(["rev-parse", "HEAD"]),
    )?;
    let builder_sha = sha(&fs::read(std::env::current_exe()?)?);
    let mut receipt = json!({"qualification":"GNU App compile/link accounting; not installed or MDK/hardware qualification","head":head.trim(),"success":false,"builder_sha256":builder_sha,"compiler_version":version,"compiler_sha256":compiler_sha,"verification_glue_sha256":glue,"source_sha256":hashes.hashes,"source_total_bytes":hashes.bytes,"commands":rows,"c_units_from_original_project":app_sources(&project)?.len(),"configured_rte_units":1});
    match result {
        Ok(value) => {
            receipt["success"] = json!(true);
            receipt["resources"] = value;
        }
        Err(e) => receipt["error"] = json!(e.to_string()),
    }
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    println!("{}", serde_json::to_string_pretty(&receipt)?);
    if receipt["success"] != true {
        return fail("MCU link check did not qualify");
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn validates_the_original_app_paths_and_vector_identity() {
        let p="<Target><TargetName>App</TargetName><Device>STM32H750VBTx</Device><FilePath>.\\Saturn\\saturn_utils.c</FilePath></Target>";
        assert_eq!(
            app_sources(p).unwrap(),
            vec![PathBuf::from("Saturn/saturn_utils.c")]
        );
        assert!(app_sources(&p.replace("Saturn\\saturn_utils.c", "..\\private.c")).is_err());
        assert!(app_sources(&p.replace("STM32H750VBTx", "wrong")).is_err());
        assert!(vector_names(
            "__Vectors       DCD __initial_sp\n DCD Reset_Handler\n__Vectors_End"
        )
        .is_err());
    }

    #[test]
    fn rejects_wrong_linked_vectors_and_missing_rtx_overrides() {
        let mut elf = vec![0u8; 1024];
        let mut put = |offset: usize, value: u32| {
            elf[offset..offset + 4].copy_from_slice(&value.to_le_bytes())
        };
        put(28, 52);
        put(52, 1);
        put(56, 128);
        put(68, 664);
        elf[42..44].copy_from_slice(&32u16.to_le_bytes());
        elf[44..46].copy_from_slice(&1u16.to_le_bytes());
        let mut names = vec!["0".to_owned(); 166];
        names[0] = "__initial_sp".into();
        names[1] = "Reset_Handler".into();
        let mut symbols = BTreeMap::from([
            ("__initial_sp".into(), 0x20020000),
            ("Reset_Handler".into(), 0x300),
            ("Default_Handler".into(), 0x400),
        ]);
        for (index, name) in [
            (11, "SVC_Handler"),
            (14, "PendSV_Handler"),
            (15, "SysTick_Handler"),
            (23, "EXTI1_IRQHandler"),
            (44, "TIM2_IRQHandler"),
            (65, "SDMMC1_IRQHandler"),
        ] {
            names[index] = name.into();
            symbols.insert(name.into(), 0x500 + index as u32 * 4);
        }
        for (index, name) in names.iter().enumerate() {
            let word = if name == "0" {
                0
            } else if index == 0 {
                symbols[name]
            } else {
                symbols[name] | 1
            };
            elf[128 + index * 4..132 + index * 4].copy_from_slice(&word.to_le_bytes());
        }
        assert!(verify_vectors(&elf, &names, &symbols).is_ok());
        elf[132] ^= 2;
        assert!(verify_vectors(&elf, &names, &symbols).is_err());
        elf[132] ^= 2;
        for (index, name) in [(11, "SVC_Handler"), (44, "TIM2_IRQHandler")] {
            let vector = 128 + index * 4..132 + index * 4;
            let saved = symbols.insert(name.into(), 0x400).unwrap();
            elf[vector.clone()].copy_from_slice(&0x401u32.to_le_bytes());
            assert!(verify_vectors(&elf, &names, &symbols).is_err());
            symbols.insert(name.into(), saved);
            elf[vector].copy_from_slice(&(saved | 1).to_le_bytes());
        }
    }
}
