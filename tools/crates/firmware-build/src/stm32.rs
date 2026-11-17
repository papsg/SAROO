//! Compile-only contracts for the exact STM32H750 device headers selected by App.
use super::{capture, fail, new_build_output, sha, Result};
use serde_json::json;
use std::{ffi::OsString, fs, path::Path, process::Command};

const RAW_WRAPPER: &str = "71fa0474c33bb7a63b0f2222a65d7ecaa618140e8a15131c12c8eda158d77ed5";
const RAW_PERIPHERAL: &str = "bf0d085e5063e8da5000dea16acdc76c212aec382d1685ae9263113da5ffa8a2";

pub(super) fn run(args: &[String]) -> Result<()> {
    let (case, provider, mode) = (args[12].as_str(), args[14].as_str(), args[16].as_str());
    if ![
        "version",
        "sdram",
        "qspi",
        "ccr5",
        "dac",
        "ethernet",
        "dma2d",
        "exti",
        "hsem",
        "jpeg",
        "ltdc",
        "sai",
        "timer_modes",
        "lptim",
        "hrtim",
        "timer_burst",
    ]
    .contains(&case)
        || !["selected", "original"].contains(&provider)
        || !["contract", "control", "alias"].contains(&mode)
        || (mode == "alias" && case != "version")
    {
        return fail("Unknown STM32 case, provider or mode");
    }
    let repo = Path::new(&args[2]).canonicalize()?;
    let toolchain = Path::new(&args[6]).canonicalize()?;
    let core = Path::new(&args[8]).canonicalize()?.join("Core/Include");
    let sdk = Path::new(&args[10]).canonicalize()?;
    let fixture = repo.join(format!("tests/stm32_header_{case}.c"));
    let wrapper = sdk.join("stm32h7xx.h");
    let peripheral = sdk.join("stm32h750xx.h");
    if sha(&fs::read(&wrapper)?) != RAW_WRAPPER
        || sha(&fs::read(&peripheral)?) != RAW_PERIPHERAL
        || !fixture.is_file()
        || fixture.metadata()?.len() > 65536
    {
        return fail("Missing exact DFP 2.3.1 input headers or bounded public fixture");
    }
    let project = fs::read_to_string(repo.join("Firm_MCU/ssmaster.uvprojx"))?;
    if project.matches("Keil.STM32H7xx_DFP.2.3.1").count() != 2
        || project
            .matches("<IncludePath>.\\inc;.\\FatFS</IncludePath>")
            .count()
            != 2
        || !project.contains("<TargetName>App</TargetName>")
        || !project.contains("<TargetName>BootLoader</TargetName>")
    {
        return fail("App and BootLoader do not select the expected DFP/include profile");
    }
    let owned = repo.join("Firm_MCU/inc");
    let expected_wrapper = if provider == "selected" && owned.join("stm32h7xx.h").is_file() {
        owned.join("stm32h7xx.h")
    } else {
        wrapper.clone()
    };
    let expected_peripheral = if provider == "selected" && owned.join("stm32h750xx.h").is_file() {
        owned.join("stm32h750xx.h")
    } else {
        peripheral.clone()
    };
    let out = new_build_output(&repo, Path::new(&args[4]))?;
    let cc = toolchain.join("bin/arm-none-eabi-gcc");
    if capture(Command::new(&cc).arg("-dumpmachine"))? != "arm-none-eabi" {
        return fail("Expected Arm bare-metal GCC");
    }
    let version = capture(Command::new(&cc).arg("--version"))?;
    let mut common: Vec<OsString> = vec![
        "-std=gnu11".into(),
        "-mcpu=cortex-m7".into(),
        "-mthumb".into(),
        "-mfpu=fpv5-d16".into(),
        "-mfloat-abi=softfp".into(),
        "-DSTM32H750xx".into(),
        "-DHSE_VALUE=16000000".into(),
        format!("-DSTM32_CHECK_{}", mode.to_ascii_uppercase()).into(),
    ];
    for dir in if provider == "selected" {
        [&owned, &sdk, &core]
    } else {
        [&sdk, &core, &owned]
    } {
        common.push("-I".into());
        common.push(dir.as_os_str().to_owned());
    }
    let dependencies = out.join("dependencies.d");
    let dep = Command::new(&cc)
        .args(&common)
        .arg("-M")
        .arg("-MF")
        .arg(&dependencies)
        .arg(&fixture)
        .output()?;
    fs::write(out.join("dependency.stderr"), &dep.stderr)?;
    if !dep.status.success() {
        return fail("STM32 dependency preprocessing failed");
    }
    let dep_text = fs::read_to_string(&dependencies)?;
    if !dep_text.contains(expected_wrapper.to_string_lossy().as_ref())
        || !dep_text.contains(expected_peripheral.to_string_lossy().as_ref())
        || (expected_wrapper != wrapper && dep_text.contains(wrapper.to_string_lossy().as_ref()))
        || (expected_peripheral != peripheral
            && dep_text.contains(peripheral.to_string_lossy().as_ref()))
    {
        return fail("Compiler did not select the expected device-header pair");
    }
    let object = out.join("contract.o");
    let result = Command::new(&cc)
        .args(&common)
        .arg("-Wall")
        .arg("-Wextra")
        .arg("-Werror")
        .arg("-c")
        .arg(&fixture)
        .arg("-o")
        .arg(&object)
        .output()?;
    fs::write(out.join("compile.stdout"), &result.stdout)?;
    fs::write(out.join("compile.stderr"), &result.stderr)?;
    let receipt = json!({
        "case":case,"provider":provider,"mode":mode,"success":result.status.success(),
        "scope":"Actual selected H750 header compile and dependencies; no target execution or hardware qualification",
        "device_pack":"Keil.STM32H7xx_DFP.2.3.1",
        "raw_wrapper_sha256":RAW_WRAPPER,"raw_peripheral_sha256":RAW_PERIPHERAL,
        "selected_wrapper_sha256":sha(&fs::read(&expected_wrapper)?),
        "selected_peripheral_sha256":sha(&fs::read(&expected_peripheral)?),
        "fixture_sha256":sha(&fs::read(&fixture)?),
        "compiler_sha256":sha(&fs::read(&cc)?),"compiler_version":version,
        "dependencies_sha256":sha(dep_text.as_bytes()),
        "object_sha256":if result.status.success(){Some(sha(&fs::read(&object)?))}else{None},
        "compile_arguments":common.iter().map(|s|s.to_string_lossy().into_owned()).collect::<Vec<_>>(),
    });
    fs::write(
        out.join("receipt.json"),
        serde_json::to_vec_pretty(&receipt)?,
    )?;
    println!("{}", serde_json::to_string_pretty(&receipt)?);
    if !result.status.success() {
        return fail("STM32 header contract failed; inspect compile.stderr");
    }
    Ok(())
}
