//! Offline, actual Cortex-M7 RTX API/exception contracts. No board operations.
use super::{capture, fail, mcu, new_build_output, sha, Result};
use serde_json::{json, Value};
use std::{
    fs,
    path::Path,
    process::Command,
    time::{Duration, Instant},
};

pub(super) fn validate(app: &Path, rtx: &Path) -> Result<()> {
    let owned = app.join("RTE/CMSIS/RTX");
    if !owned.is_dir() {
        return Ok(());
    }
    validate_owned(&owned, rtx)
}

fn validate_owned(owned: &Path, rtx: &Path) -> Result<()> {
    let pins: Value = serde_json::from_slice(&fs::read(owned.join("provenance.json"))?)?;
    for section in ["headers", "providers"] {
        for (name, expected) in pins[section].as_object().ok_or("Missing RTX pins")? {
            if !name.starts_with("rtx_") || name.contains(['/', '\\']) {
                return fail("Invalid RTX pin path");
            }
            let source = rtx.join("RTX/Source").join(name);
            if Some(sha(&fs::read(source)?).as_str()) != expected.as_str() {
                return fail("RTX SDK differs from the selected 5.5.1 provider");
            }
            if section == "headers"
                && fs::read(owned.join(name))? != fs::read(rtx.join("RTX/Source").join(name))?
            {
                return fail("RTX support header is not the pinned original");
            }
        }
    }
    Ok(())
}

pub(super) fn run(args: &[String]) -> Result<()> {
    let case = args[14].as_str();
    let provider = match case {
        "delay" => "rtx_delay.c",
        "queue" => "rtx_msgqueue.c",
        "timer" => "rtx_timer.c",
        _ => return fail("Unknown RTOS contract case"),
    };
    for flag in &args[15..] {
        if !["--baseline", "--lto", "--archive"].contains(&flag.as_str()) {
            return fail("Unknown RTOS contract option");
        }
    }
    let baseline = args[15..].iter().any(|v| v == "--baseline");
    let archive = args[15..].iter().any(|v| v == "--archive");
    let lto = args[15..].iter().any(|v| v == "--lto");
    let repo = Path::new(&args[2]).canonicalize()?;
    let out = new_build_output(&repo, Path::new(&args[4]))?;
    let bin = Path::new(&args[6]).canonicalize()?.join("bin");
    let cc = bin.join("arm-none-eabi-gcc");
    if capture(Command::new(&cc).arg("-dumpmachine"))?.trim() != "arm-none-eabi" {
        return fail("Expected Arm bare-metal GCC");
    }
    let mut pins = mcu::Inputs::default();
    for (from, name, key) in [
        (
            Path::new(&args[8]).join("Core/Include"),
            "core",
            "CMSIS/Core/Include",
        ),
        (Path::new(&args[10]).to_owned(), "stm32", "STM32/Include"),
        (Path::new(&args[12]).to_owned(), "rtos", "CMSIS/RTOS2"),
        (
            repo.join("Firm_MCU/RTE/CMSIS"),
            "config",
            "Firm_MCU/RTE/CMSIS",
        ),
        (repo.join("Firm_MCU/RTE/_App"), "rte", "Firm_MCU/RTE/_App"),
    ] {
        mcu::copy_tree(&from, &out.join(name), key, &mut pins)?;
    }
    mcu::copy_file(
        &repo.join("Firm_MCU/RTE/CMSIS/RTX/provenance.json"),
        &out.join("config/RTX/provenance.json"),
        "Firm_MCU/RTE/CMSIS/RTX/provenance.json".into(),
        &mut pins,
    )?;
    // Validate the staged owner, not a later read of the working tree.
    validate_owned(&out.join("config/RTX"), &out.join("rtos"))?;
    let compiler_version = capture(Command::new(&cc).arg("--version"))?;
    let compiler_sha256 = sha(&fs::read(&cc)?);
    let builder_sha256 = sha(&fs::read(std::env::current_exe()?)?);
    for name in [
        "rtos_contract.c",
        "rtos_contract.ld",
        "rtos_timer_contract.inc",
    ] {
        mcu::copy_file(
            &repo.join("tests").join(name),
            &out.join(name),
            format!("tests/{name}"),
            &mut pins,
        )?;
    }
    let deadline = Instant::now() + Duration::from_secs(180);
    let mut commands = Vec::new();
    let result = (|| -> Result<()> {
        let kernel = out.join("rtos/RTX/Source");
        let owned = out.join("config/RTX");
        let mut sources = fs::read_dir(&kernel)?
            .map(|e| e.map(|e| e.path()))
            .collect::<std::io::Result<Vec<_>>>()?;
        sources.retain(|s| s.extension().is_some_and(|v| v == "c"));
        sources.sort();
        let mut direct = vec![
            out.join("rtos_contract.c"),
            out.join("config/RTX_Config.c"),
            kernel.join("GCC/irq_cm4f.S"),
            out.join("rtos/Source/os_systick.c"),
        ];
        let mut kernel_objects = Vec::new();
        for source in &mut sources {
            let name = source.file_name().ok_or("Missing kernel source name")?;
            if owned.join(name).is_file() && !(baseline && name == provider) {
                direct.push(owned.join(name));
                if !archive {
                    *source = owned.join(name);
                }
            }
        }
        if !archive {
            direct.retain(|s| s.parent() != Some(owned.as_path()));
            direct.extend(sources.iter().cloned());
            sources.clear();
        }
        let mut objects = Vec::new();
        for (index, source) in sources.iter().chain(&direct).enumerate() {
            let object = out.join(format!(
                "{index}-{}.o",
                source.file_stem().unwrap().to_string_lossy()
            ));
            let mut command = Command::new(&cc);
            command.args([
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
                "-DSTM32H750xx",
                "-DHSE_VALUE=16000000",
            ]);
            if case == "timer" {
                command.args([
                    "-DTEST_RTX_TIMERS=1",
                    "-DOS_TIMER_THREAD_STACK_SIZE=512",
                    "-DOS_TIMER_CB_QUEUE=8",
                ]);
            }
            if case == "queue" {
                command.arg("-DTEST_RTX_QUEUE=1");
            }
            if lto {
                command.args(["-flto=4", "-flto-partition=one"]);
            }
            for include in [
                out.join("config"),
                owned.clone(),
                out.join("rte"),
                out.join("core"),
                out.join("stm32"),
                out.join("rtos/Include"),
                out.join("rtos/RTX/Include"),
                kernel.clone(),
            ] {
                command.arg("-I").arg(include);
            }
            command.args(["-c"]).arg(source).arg("-o").arg(&object);
            mcu::execute(&mut command, &out, &mut commands, deadline)?;
            if index < sources.len() {
                kernel_objects.push(object);
            } else {
                objects.push(object);
            }
        }
        if archive {
            let library = out.join("rtx-original.a");
            mcu::execute(
                Command::new(bin.join("arm-none-eabi-ar"))
                    .arg("rcs")
                    .arg(&library)
                    .args(kernel_objects),
                &out,
                &mut commands,
                deadline,
            )?;
            objects.push(library);
        }
        let elf = out.join("contract.elf");
        let mut command = Command::new(&cc);
        command.args([
            "-mcpu=cortex-m7",
            "-mthumb",
            "-mfpu=fpv5-d16",
            "-mfloat-abi=softfp",
        ]);
        if lto {
            command.args(["-flto=4", "-flto-partition=one"]);
        }
        command
            .arg("-nostartfiles")
            .arg("-Wl,--gc-sections")
            .arg(format!("-Wl,-Map={}", out.join("contract.map").display()))
            .arg("-T")
            .arg(out.join("rtos_contract.ld"));
        command
            .arg("-Wl,--start-group")
            .args(objects)
            .args(["-lc", "-lgcc", "-Wl,--end-group", "-o"])
            .arg(&elf);
        mcu::execute(&mut command, &out, &mut commands, deadline)?;
        let version = capture(Command::new("qemu-system-arm").arg("--version"))?;
        fs::write(out.join("qemu-version.txt"), version)?;
        mcu::execute(
            Command::new("qemu-system-arm")
                .args([
                    "-M",
                    "mps2-an500",
                    "-nographic",
                    "-monitor",
                    "none",
                    "-serial",
                    "none",
                    "-semihosting-config",
                    "enable=on,target=native",
                    "-kernel",
                ])
                .arg(&elf),
            &out,
            &mut commands,
            deadline,
        )?;
        Ok(())
    })();
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(
            &json!({"case":case,"baseline":baseline,"archive":archive,"lto":lto,"compiler_version":compiler_version,"compiler_sha256":compiler_sha256,"builder_sha256":builder_sha256,"success":result.is_ok(),"error":result.as_ref().err().map(ToString::to_string),"inputs_sha256":pins.hashes,"commands":commands,"scope":"Actual RTX API/SVC/kernel in QEMU; no SAROO or game qualification"}),
        )?,
    )?;
    result?;
    println!("RTOS {case} contract passes; {}", out.display());
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn provider_admission_refuses_changed_headers_and_sdk_versions() {
        let root = std::env::temp_dir().join(format!("saroo-rtx-pins-{}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let app = root.join("app");
        let owned = app.join("RTE/CMSIS/RTX");
        let sdk = root.join("sdk/RTX/Source");
        fs::create_dir_all(&owned).unwrap();
        fs::create_dir_all(&sdk).unwrap();
        fs::write(owned.join("rtx_lib.h"), b"header").unwrap();
        fs::write(sdk.join("rtx_lib.h"), b"header").unwrap();
        fs::write(sdk.join("rtx_delay.c"), b"provider").unwrap();
        fs::write(
            owned.join("provenance.json"),
            serde_json::to_vec(&json!({
                "headers":{"rtx_lib.h":sha(b"header")},
                "providers":{"rtx_delay.c":sha(b"provider")}
            }))
            .unwrap(),
        )
        .unwrap();
        assert!(validate(&app, &root.join("sdk")).is_ok());
        fs::write(owned.join("rtx_lib.h"), b"modified").unwrap();
        assert!(validate(&app, &root.join("sdk")).is_err());
        fs::write(owned.join("rtx_lib.h"), b"header").unwrap();
        fs::write(sdk.join("rtx_delay.c"), b"different version").unwrap();
        assert!(validate(&app, &root.join("sdk")).is_err());
        assert_eq!(
            fs::read(sdk.join("rtx_delay.c")).unwrap(),
            b"different version"
        );
        fs::remove_dir_all(root).unwrap();
    }
}
