//! Exercise the actual public HIRQ register block; no board or patched model.
use sha2::{Digest, Sha256};
use std::{
    env, fs,
    path::Path,
    process::{Command, Stdio},
    thread,
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

fn run(cmd: &mut Command, log: &Path) -> bool {
    let output = fs::File::create(log).unwrap();
    let mut child = cmd
        .stdout(Stdio::from(output.try_clone().unwrap()))
        .stderr(Stdio::from(output))
        .spawn()
        .expect("Install Icarus Verilog (iverilog/vvp) to run FPGA checks");
    let start = Instant::now();
    loop {
        if let Some(status) = child.try_wait().unwrap() {
            return status.success();
        }
        if start.elapsed() > Duration::from_secs(30)
            || fs::metadata(log).unwrap().len() > 1024 * 1024
        {
            child.kill().unwrap();
            child.wait().unwrap();
            panic!("HDL check exceeded time/log bounds");
        }
        thread::sleep(Duration::from_millis(20));
    }
}

fn simulate(mode: u32, updates: u32) {
    let repo = Path::new(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .nth(3)
        .unwrap();
    let source = env::var_os("SAROO_FPGA_HIRQ_SOURCE")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|| repo.join("FPGA/SSMaster.v"));
    assert!(source.symlink_metadata().unwrap().is_file());
    assert!(source.metadata().unwrap().len() <= 2 * 1024 * 1024);
    let raw = fs::read_to_string(source).unwrap();
    let start = raw
        .find("\treg[15:0] ss_hirq;")
        .expect("Missing actual HIRQ register");
    let end = start
        + raw[start..]
            .find("\n///////////////////////////////////////////////////////")
            .expect("Missing HIRQ block end");
    let parent = repo.join("tests/build/fpga-hirq");
    fs::create_dir_all(&parent).unwrap();
    let out = parent.join(format!(
        "{}-{}-{}",
        std::process::id(),
        mode,
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos()
    ));
    fs::create_dir(&out).unwrap();
    let wrapper=format!("module hirq_dut(input mclk,NRESET,st_wr_start,ss_wr_start,ss_cdc_cs,\ninput [24:0] fsmc_addr,input [23:0] SS_ADDR,\ninput [15:0] ST_AD,SS_DATA,output [15:0] value);\n{}\nassign value=ss_hirq;\nendmodule\n",&raw[start..end]);
    fs::write(out.join("hirq.v"), wrapper).unwrap();
    let testbench = repo.join("tests/fpga_hirq_tb.v");
    let identity = serde_json::json!({"source_sha256":format!("{:x}",Sha256::digest(raw.as_bytes())),"testbench_sha256":format!("{:x}",Sha256::digest(fs::read(&testbench).unwrap())),"mode":mode,"hardware_tested":false});
    fs::write(
        out.join("inputs.json"),
        serde_json::to_vec_pretty(&identity).unwrap(),
    )
    .unwrap();
    assert!(
        run(
            Command::new("iverilog")
                .args(["-g2012", "-s", "tb", "-o"])
                .arg(out.join("test.vvp"))
                .arg(out.join("hirq.v"))
                .arg(testbench),
            &out.join("compile.log")
        ),
        "HDL compile failed"
    );
    let passed = run(
        Command::new("vvp")
            .arg(out.join("test.vvp"))
            .arg(format!("+mode={mode}")),
        &out.join("run.log"),
    );
    let log = fs::read_to_string(out.join("run.log")).unwrap();
    assert!(passed, "{log}");
    assert!(
        log.contains(&format!("PASS {updates} register updates; mode={mode}")),
        "{log}"
    );
}

#[test]
fn unchanged_single_writer_hold_reset_decode_and_fpga05_set_priority() {
    simulate(0, 1857);
}

#[test]
fn simultaneous_clear_same_and_different_bits_and_multibit_masks() {
    simulate(1, 5121);
}
