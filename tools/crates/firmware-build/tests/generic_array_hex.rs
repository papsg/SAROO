use generic_array::{
    typenum::{Sum, U0, U1, U1023, U1024, U16, U2, U2048, U255, U256, U32, U4096},
    ArrayLength, GenericArray,
};
use sha2::{Digest, Sha256};
use std::{fmt, fmt::Write, ops::Add};

type U1025 = Sum<U1024, U1>;
type U4097 = Sum<U4096, U1>;

fn reference(data: &[u8], upper: bool) -> String {
    // Standard primitive-byte formatting supplies an independent hex oracle.
    let value: String = data.iter().map(|byte| format!("{byte:02x}")).collect();
    if upper {
        value.to_ascii_uppercase()
    } else {
        value
    }
}

fn matrix<N>() -> usize
where
    N: ArrayLength<u8> + Add<N>,
    Sum<N, N>: ArrayLength<u8>,
{
    let data: Vec<u8> = (0..N::USIZE)
        .map(|i| ((i * 73) ^ (i >> 3) ^ 0xa5) as u8)
        .collect();
    let before = data.clone();
    let array = GenericArray::<u8, N>::from_slice(&data);
    let mut calls = 0;
    for upper in [false, true] {
        let full = reference(&data, upper);
        let actual = if upper {
            format!("{array:X}")
        } else {
            format!("{array:x}")
        };
        assert_eq!(actual, full);
        calls += 1;
        for precision in (0..=full.len() + 3).chain([usize::MAX - 1, usize::MAX]) {
            let expected: String = full.chars().take(precision).collect();
            let actual = if upper {
                format!("{array:.precision$X}")
            } else {
                format!("{array:.precision$x}")
            };
            assert_eq!(
                actual,
                expected,
                "bytes={} upper={upper} precision={precision}",
                data.len()
            );
            calls += 1;
        }
    }
    assert_eq!(data, before);
    calls
}

#[test]
fn empty_and_short_arrays_obey_precision_limits() {
    let calls = matrix::<U0>()
        + matrix::<U1>()
        + matrix::<U2>()
        + matrix::<U16>()
        + matrix::<U32>()
        + matrix::<U255>()
        + matrix::<U256>();
    assert_eq!(calls, 2346);
    println!("small/default/precision format calls={calls}");
}

#[test]
fn chunk_boundaries_obey_precision_limits() {
    let calls = matrix::<U1023>()
        + matrix::<U1024>()
        + matrix::<U1025>()
        + matrix::<U2048>()
        + matrix::<U4097>();
    assert_eq!(calls, 36938);
    println!("boundary/default/precision format calls={calls}");
}

struct FailWriter {
    fail_at: usize,
    calls: usize,
    failed: bool,
    output: String,
}

impl fmt::Write for FailWriter {
    fn write_str(&mut self, text: &str) -> fmt::Result {
        assert!(!self.failed, "write after an output error");
        let call = self.calls;
        self.calls += 1;
        if call == self.fail_at {
            self.failed = true;
            return Err(fmt::Error);
        }
        self.output.push_str(text);
        Ok(())
    }
}

fn writer_errors<N>() -> usize
where
    N: ArrayLength<u8> + Add<N>,
    Sum<N, N>: ArrayLength<u8>,
{
    let data = vec![0xabu8; N::USIZE];
    let before = data.clone();
    let array = GenericArray::<u8, N>::from_slice(&data);
    let mut calls = 0;
    for upper in [false, true] {
        let full = reference(&data, upper);
        for precision in [1, full.len(), usize::MAX] {
            let expected: String = full.chars().take(precision).collect();
            let writes = if N::USIZE < 1024 {
                1
            } else {
                expected.as_bytes().chunks(2048).len()
            };
            for fail_at in [0, 1] {
                let mut writer = FailWriter {
                    fail_at,
                    calls: 0,
                    failed: false,
                    output: String::new(),
                };
                let result = if upper {
                    write!(&mut writer, "{array:.precision$X}")
                } else {
                    write!(&mut writer, "{array:.precision$x}")
                };
                assert_eq!(result.is_err(), fail_at < writes);
                assert_eq!(writer.calls, writes.min(fail_at + 1));
                let emitted = if fail_at >= writes {
                    expected.len()
                } else if N::USIZE < 1024 {
                    0
                } else {
                    fail_at * 2048
                };
                assert_eq!(writer.output, &expected[..emitted]);
                calls += 1;
            }
        }
    }
    assert_eq!(data, before);
    calls
}

#[test]
fn writer_errors_propagate_and_stop_output() {
    let calls = writer_errors::<U2>() + writer_errors::<U1025>();
    assert_eq!(calls, 24);
    println!("writer format calls={calls}");
}

#[test]
fn real_sha256_output_keeps_default_and_bounded_hex() {
    let mut calls = 0;
    for (input, expected) in [
        (
            b"".as_slice(),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        ),
        (
            b"abc".as_slice(),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        ),
    ] {
        let digest = Sha256::digest(input);
        for upper in [false, true] {
            let full = if upper {
                expected.to_ascii_uppercase()
            } else {
                expected.to_owned()
            };
            let actual = if upper {
                format!("{digest:X}")
            } else {
                format!("{digest:x}")
            };
            assert_eq!(actual, full);
            calls += 1;
            for precision in (0..=68).chain([usize::MAX]) {
                let expected: String = full.chars().take(precision).collect();
                let actual = if upper {
                    format!("{digest:.precision$X}")
                } else {
                    format!("{digest:.precision$x}")
                };
                assert_eq!(actual, expected);
                calls += 1;
            }
        }
    }
    assert_eq!(calls, 284);
    println!("SHA256 digest format calls={calls}");
}
