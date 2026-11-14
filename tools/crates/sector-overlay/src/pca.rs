//! Bounded offline PCA/PRS correspondence check, adapted from paps-g's
//! native-source review. This is a format reader, not a Saturn emulator.
use crate::disc::read_bounded;
use crate::{fail, hex, write_pack, Result};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::path::Path;

const LIMIT: usize = 1024 * 1024;

struct Reader<'a> {
    bytes: &'a [u8],
    at: usize,
    bits: u8,
    remaining: u8,
}

impl Reader<'_> {
    fn byte(&mut self) -> Result<u8> {
        let value = *self.bytes.get(self.at).ok_or("Truncated PRS token")?;
        self.at += 1;
        Ok(value)
    }

    fn bit(&mut self) -> Result<u8> {
        if self.remaining == 0 {
            self.bits = self.byte()?;
            self.remaining = 8;
        }
        let value = self.bits & 1;
        self.bits >>= 1;
        self.remaining -= 1;
        Ok(value)
    }
}

struct Decoded {
    bytes: Vec<u8>,
    segments: Vec<(u32, u32)>,
}

fn decode(input: &[u8]) -> Result<Decoded> {
    if input.len() > LIMIT {
        return fail("PCA input exceeds one MiB");
    }
    let word = |offset| -> Result<u32> {
        Ok(u32::from_be_bytes(
            input
                .get(offset..offset + 4)
                .ok_or("Truncated PCA header")?
                .try_into()?,
        ))
    };
    let count = word(0)? as usize;
    if count > 16 {
        return fail("PCA segment count exceeds sixteen");
    }
    let mut segments = Vec::new();
    let mut expected = 0usize;
    for i in 0..count {
        let address = word(4 + i * 8)?;
        let length = word(8 + i * 8)?
            .checked_mul(4)
            .ok_or("PCA longword count overflow")?;
        let end = address.checked_add(length).ok_or("PCA address overflow")?;
        if length == 0
            || address & 3 != 0
            || address < 0x06000000
            || end > 0x06100000
            || segments.iter().any(|&(a, n)| address < a + n && a < end)
        {
            return fail("Invalid or overlapping PCA destinations");
        }
        expected = expected
            .checked_add(length as usize)
            .ok_or("PCA output overflow")?;
        if expected > LIMIT {
            return fail("PCA output exceeds one MiB");
        }
        segments.push((address, length));
    }
    let mut reader = Reader {
        bytes: input,
        at: 4 + count * 8,
        bits: 0,
        remaining: 0,
    };
    let mut output = Vec::with_capacity(expected);
    loop {
        if reader.bit()? != 0 {
            if output.len() == LIMIT {
                return fail("PRS literal exceeds one MiB output");
            }
            output.push(reader.byte()?);
            continue;
        }
        let (length, distance) = if reader.bit()? == 0 {
            let length = usize::from((reader.bit()? << 1) | reader.bit()?) + 2;
            (length, 256 - usize::from(reader.byte()?))
        } else {
            let word = u16::from_be_bytes([reader.byte()?, reader.byte()?]);
            let length = if word & 7 == 0 {
                let extra = reader.byte()?;
                if extra == 0 {
                    break;
                }
                usize::from(extra) + 1
            } else {
                usize::from(word & 7) + 2
            };
            (length, 8192 - usize::from(word >> 3))
        };
        if distance > output.len() || length > LIMIT - output.len() {
            return fail("PRS back-reference or output exceeds its bound");
        }
        for _ in 0..length {
            output.push(output[output.len() - distance]);
        }
    }
    if reader.at != input.len() || (count != 0 && output.len() != expected) {
        return fail("PCA trailing bytes or incomplete declared output");
    }
    Ok(Decoded {
        bytes: output,
        segments,
    })
}

pub fn run(source: &Path, output: &Path) -> Result<()> {
    let input = read_bounded(source, LIMIT)?;
    let decoded = decode(&input)?;
    write_pack(output, &decoded.bytes)?;
    println!(
        "{}",
        serde_json::to_string_pretty(&json!({
            "complete": true,
            "source_bytes": input.len(), "source_sha256": hex(&Sha256::digest(&input)),
            "decoded_bytes": decoded.bytes.len(), "decoded_sha256": hex(&Sha256::digest(&decoded.bytes)),
            "segments": decoded.segments.iter().map(|&(address, bytes)|
                json!({"address": address, "bytes": bytes})).collect::<Vec<_>>(),
            "qualification": "Complete bounded format decode only; no native CPU, timing or device qualification."
        }))?
    );
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    // Independent packet writer for synthetic data only. There is no game
    // executable, BIOS or proprietary compressed stream in these fixtures.
    #[derive(Default)]
    struct Packet {
        data: Vec<u8>,
        phase: u8,
        flag: usize,
    }
    impl Packet {
        fn bits(&mut self, bits: &[u8]) {
            for bit in bits {
                if self.phase == 0 {
                    self.flag = self.data.len();
                    self.data.push(0);
                }
                self.data[self.flag] |= bit << self.phase;
                self.phase = (self.phase + 1) % 8;
            }
        }
        fn literal(&mut self, byte: u8) {
            self.bits(&[1]);
            self.data.push(byte);
        }
        fn short(&mut self, distance: usize, length: usize) {
            let count = length - 2;
            self.bits(&[0, 0, (count >> 1) as u8, (count & 1) as u8]);
            self.data.push((256 - distance) as u8);
        }
        fn long(&mut self, distance: usize, length: usize, extended: bool) {
            self.bits(&[0, 1]);
            let word =
                (((8192 - distance) as u16) << 3) | if extended { 0 } else { (length - 2) as u16 };
            self.data.extend(word.to_be_bytes());
            if extended {
                self.data.push((length - 1) as u8);
            }
        }
        fn finish(mut self) -> Vec<u8> {
            self.bits(&[0, 1]);
            self.data.extend([0, 0, 0]);
            let mut result = vec![0; 4];
            result.extend(self.data);
            result
        }
    }

    #[test]
    fn fixed_literal_short_inline_and_extended_packets() {
        // A, short distance 1 / length 2, then native end marker.
        assert_eq!(
            decode(&[0, 0, 0, 0, 0x41, b'A', 0xff, 0, 0, 0])
                .unwrap()
                .bytes,
            b"AAA"
        );
        for prefix in 1..=8 {
            for (kind, lengths) in [
                (0, vec![2, 3, 4, 5]),
                (1, (3..=9).collect()),
                (2, vec![2, 10, 256]),
            ] {
                for length in lengths {
                    let mut p = Packet::default();
                    for _ in 0..prefix {
                        p.literal(b'X');
                    }
                    match kind {
                        0 => p.short(1, length),
                        1 => p.long(1, length, false),
                        _ => p.long(1, length, true),
                    }
                    assert_eq!(
                        decode(&p.finish()).unwrap().bytes,
                        vec![b'X'; prefix + length]
                    );
                }
            }
        }
    }

    #[test]
    fn maximum_distances_and_overlapping_copies() {
        let mut p = Packet::default();
        let mut expected: Vec<_> = (0..8192).map(|i| (i % 251) as u8).collect();
        for &byte in &expected {
            p.literal(byte);
        }
        p.short(256, 5);
        for _ in 0..5 {
            expected.push(expected[expected.len() - 256]);
        }
        p.long(8192, 9, false);
        for _ in 0..9 {
            expected.push(expected[expected.len() - 8192]);
        }
        assert_eq!(decode(&p.finish()).unwrap().bytes, expected);
    }

    #[test]
    fn every_truncation_trailing_data_and_uninitialized_reference_refuse() {
        let mut p = Packet::default();
        p.literal(7);
        p.short(1, 5);
        p.long(1, 256, true);
        let valid = p.finish();
        assert!(decode(&valid).is_ok());
        for end in 0..valid.len() {
            assert!(decode(&valid[..end]).is_err());
        }
        let mut extra = valid;
        extra.push(0);
        assert!(decode(&extra).is_err());
        let mut p = Packet::default();
        p.short(1, 2);
        assert!(decode(&p.finish()).is_err());
        let mut p = Packet::default();
        p.long(8192, 9, false);
        assert!(decode(&p.finish()).is_err());
    }

    #[test]
    fn longword_destinations_bind_complete_disjoint_output() {
        let mut p = Packet::default();
        for byte in b"abcdefgh" {
            p.literal(*byte);
        }
        let raw = p.finish();
        let header = |rows: &[(u32, u32)]| {
            let mut data = (rows.len() as u32).to_be_bytes().to_vec();
            for (a, n) in rows {
                data.extend(a.to_be_bytes());
                data.extend(n.to_be_bytes());
            }
            data.extend(&raw[4..]);
            data
        };
        let good = decode(&header(&[(0x06014000, 1), (0x0601f000, 1)])).unwrap();
        assert_eq!(good.bytes, b"abcdefgh");
        assert_eq!(good.segments, [(0x06014000, 4), (0x0601f000, 4)]);
        for rows in [
            vec![(0x06000000, 1)],
            vec![(0x06000000, 0)],
            vec![(0x06000001, 2)],
            vec![(0x05fffffc, 2)],
            vec![(0x060ffffc, 2)],
            vec![(0x06000000, u32::MAX)],
            vec![(0x06000000, 2), (0x06000004, 1)],
            vec![(0x06000000, 1); 17],
        ] {
            assert!(decode(&header(&rows)).is_err());
        }
    }

    #[test]
    fn input_and_expanding_output_bounds_refuse() {
        assert!(decode(&vec![0; LIMIT + 1]).is_err());
        let mut p = Packet::default();
        p.literal(1);
        for _ in 0..LIMIT / 256 + 1 {
            p.long(1, 256, true);
        }
        assert!(decode(&p.finish()).is_err());
    }
}
