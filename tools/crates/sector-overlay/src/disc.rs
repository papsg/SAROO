use std::fs::{self, File};
use std::io::{Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};

use crate::{fail, Result};

pub const RAW_BYTES: usize = 2352;
const MAX_CUE_BYTES: usize = 200 * RAW_BYTES - 0x1000;
const MAX_FAD: u32 = 0x00ff_ffff;

pub struct Image {
    pub path: PathBuf,
    pub bytes: u32,
}

pub struct Track {
    pub image: usize,
    pub mode: u8,
    pub width: u32,
    pub offset: u32,
    pub start: u32,
    pub end: u32,
    pregap: Option<u32>,
    postgap: Option<u32>,
    anchor: Option<u32>,
    index1: u32,
    last_index: u32,
    last_frame: u32,
}

impl Track {
    fn offset_at(&self, image: &Image, frames: u32) -> Result<u32> {
        if self.offset > image.bytes || frames > (image.bytes - self.offset) / self.width {
            return fail("CUE index exceeds the track file");
        }
        Ok(self.offset + frames * self.width)
    }

    fn finish(&mut self, frames: u32) -> Result<u32> {
        if self.last_index == 0
            || frames == 0
            || frames > MAX_FAD - self.start
            || self.postgap.unwrap_or(0) > MAX_FAD - self.start - frames
            || self.last_frame - self.index1 >= frames
        {
            return fail("Invalid CUE track extent");
        }
        self.end = self.start + frames;
        Ok(self.end + self.postgap.unwrap_or(0))
    }
}

pub struct Disc {
    pub images: Vec<Image>,
    pub tracks: Vec<Track>,
}

fn token(line: &str) -> Result<Option<(&str, &str)>> {
    let line = line.trim_start_matches([' ', '\t']);
    if line.is_empty() {
        return Ok(None);
    }
    if let Some(quoted) = line.strip_prefix('"') {
        let (field, rest) = quoted.split_once('"').ok_or("Unterminated CUE quote")?;
        if !rest.is_empty() && !rest.starts_with([' ', '\t']) {
            return fail("Invalid CUE quote delimiter");
        }
        return Ok(Some((field, rest)));
    }
    let length = line.find([' ', '\t']).unwrap_or(line.len());
    Ok(Some(line.split_at(length)))
}

fn tokens<'a>(keyword: &'a str, mut line: &'a str) -> Result<Vec<&'a str>> {
    let mut fields = Vec::new();
    fields.push(keyword);
    while fields.len() < 4 {
        let Some((field, rest)) = token(line)? else {
            break;
        };
        fields.push(field);
        line = rest;
    }
    Ok(fields)
}

fn number(text: &str, limit: u32) -> Result<u32> {
    if text.is_empty() || !text.bytes().all(|b| b.is_ascii_digit()) {
        return fail("Invalid CUE number");
    }
    let n: u32 = text.parse()?;
    if n > limit {
        return fail("CUE number exceeds the firmware limit");
    }
    Ok(n)
}

fn frames(text: &str) -> Result<u32> {
    let (minute, rest) = text.split_once(':').ok_or("Invalid CUE time")?;
    let (second, frame) = rest.split_once(':').ok_or("Invalid CUE time")?;
    Ok(number(minute, 3728)? * 4500 + number(second, 59)? * 75 + number(frame, 74)?)
}

fn mode(kind: &str) -> Result<(u8, u32)> {
    if kind == "AUDIO" {
        return Ok((3, 2352));
    }
    let (name, width) = kind.split_once('/').ok_or("Unsupported CUE track type")?;
    let width = number(width, 2352)?;
    match (name, width) {
        ("MODE1", 2048 | 2352) => Ok((1, width)),
        ("MODE2", 2048 | 2324 | 2336 | 2352) => Ok((2, width)),
        _ => fail("Unsupported CUE track type"),
    }
}

pub fn read_bounded(path: &Path, limit: usize) -> Result<Vec<u8>> {
    if !fs::metadata(path)?.is_file() {
        return fail("Input must be a regular file");
    }
    let file = File::open(path)?;
    let info = file.metadata()?;
    if !info.is_file() || info.len() > limit as u64 {
        return fail("Input is not a regular file within the size limit");
    }
    let mut data = Vec::new();
    file.take(limit as u64 + 1).read_to_end(&mut data)?;
    if data.len() > limit {
        return fail("Input exceeds the size limit");
    }
    Ok(data)
}

impl Disc {
    pub fn read(path: &Path) -> Result<Self> {
        let data = read_bounded(path, MAX_CUE_BYTES - 1)?;
        if data.contains(&0) {
            return fail("NUL in CUE");
        }
        let text = std::str::from_utf8(&data)?;
        let text = text.strip_prefix('\u{feff}').unwrap_or(text);
        let parent = path.parent().unwrap_or(Path::new("."));
        let mut images: Vec<Image> = Vec::new();
        let mut tracks: Vec<Track> = Vec::new();
        let mut first = 0;
        let mut next_fad = 150;
        let mut subindices = 0;
        for line in text.split(['\r', '\n']) {
            let Some((keyword, arguments)) = token(line)? else {
                continue;
            };
            if !matches!(keyword, "FILE" | "TRACK" | "PREGAP" | "POSTGAP" | "INDEX") {
                continue;
            }
            let fields = tokens(keyword, arguments)?;
            match fields.as_slice() {
                ["FILE", name, "BINARY"] => {
                    if !images.is_empty() {
                        if tracks.len() == first {
                            return fail("CUE FILE has no tracks");
                        }
                        let track = tracks.last_mut().unwrap();
                        let image = &images[track.image];
                        next_fad = track.finish((image.bytes - track.offset) / track.width)?;
                    }
                    let name = name.replace('\\', "/");
                    let source = Path::new(&name);
                    if name.is_empty()
                        || source.is_absolute()
                        || name.contains(':')
                        || parent.as_os_str().len() + name.len() + 1 >= 512
                    {
                        return fail("Invalid CUE track path");
                    }
                    let image = fs::canonicalize(parent.join(source))?;
                    let info = fs::metadata(&image)?;
                    if !info.is_file() || info.len() > u32::MAX.into() {
                        return fail("Track is not a regular file within the FAT32 size limit");
                    }
                    images.push(Image {
                        path: image,
                        bytes: info.len() as u32,
                    });
                    first = tracks.len();
                }
                ["TRACK", n, format] => {
                    if images.is_empty()
                        || number(n, 99)? as usize != tracks.len() + 1
                        || (tracks.len() > first && tracks.last().unwrap().last_index == 0)
                    {
                        return fail("Invalid CUE track order");
                    }
                    let (mode, width) = mode(format)?;
                    tracks.push(Track {
                        image: images.len() - 1,
                        mode,
                        width,
                        offset: 0,
                        start: 0,
                        end: 0,
                        pregap: None,
                        postgap: None,
                        anchor: None,
                        index1: 0,
                        last_index: 0,
                        last_frame: 0,
                    });
                }
                [kind @ ("PREGAP" | "POSTGAP"), time] if tracks.len() > first => {
                    let track = tracks.last_mut().unwrap();
                    let length = frames(time)?;
                    if *kind == "PREGAP" && track.pregap.is_none() && track.anchor.is_none() {
                        track.pregap = Some(length);
                    } else if *kind == "POSTGAP" && track.postgap.is_none() && track.last_index != 0
                    {
                        track.postgap = Some(length);
                    } else {
                        return fail("Invalid CUE gap order");
                    }
                }
                ["INDEX", index, time] if tracks.len() > first => {
                    let index = number(index, 99)?;
                    let frame = frames(time)?;
                    let (track, previous) = tracks.split_last_mut().unwrap();
                    let image = &images[track.image];
                    if track.postgap.is_some() {
                        return fail("CUE index after POSTGAP");
                    }
                    let anchor = match track.anchor {
                        Some(_) if index == 0 => return fail("Duplicate CUE INDEX 00"),
                        Some(anchor) => anchor,
                        None => {
                            if index > 1 {
                                return fail("Missing CUE INDEX 01");
                            }
                            track.offset = if previous.len() > first {
                                let prev = previous.last_mut().unwrap();
                                let delta =
                                    frame.checked_sub(prev.index1).ok_or("Reversed CUE index")?;
                                let offset = prev.offset_at(image, delta)?;
                                next_fad = prev.finish(delta)?;
                                offset
                            } else {
                                track.offset_at(image, frame)?
                            };
                            track.start = next_fad
                                .checked_add(track.pregap.unwrap_or(0))
                                .filter(|fad| *fad <= MAX_FAD)
                                .ok_or("CUE pregap exceeds FAD range")?;
                            track.anchor = Some(frame);
                            frame
                        }
                    };
                    let delta = frame.checked_sub(anchor).ok_or("Reversed CUE index")?;
                    let offset = track.offset_at(image, delta + 1)? - track.width;
                    match index {
                        0 => (),
                        1 if track.last_index == 0 => {
                            track.start = track
                                .start
                                .checked_add(delta)
                                .filter(|fad| *fad <= MAX_FAD)
                                .ok_or("CUE index exceeds FAD range")?;
                            if previous.is_empty() {
                                track.start = 150;
                            }
                            track.offset = offset;
                            track.anchor = Some(frame);
                            track.index1 = frame;
                            track.last_index = index;
                            track.last_frame = frame;
                        }
                        n if track.last_index != 0
                            && n == track.last_index + 1
                            && frame > track.last_frame
                            && subindices < 2048 =>
                        {
                            subindices += 1;
                            track.last_index = index;
                            track.last_frame = frame;
                        }
                        _ => return fail("Invalid CUE index order"),
                    }
                }
                _ => return fail("Invalid CUE directive"),
            }
        }
        if tracks.len() == first {
            return fail("CUE has no complete final track");
        }
        let track = tracks.last_mut().unwrap();
        let image = &images[track.image];
        track.finish((image.bytes - track.offset) / track.width)?;
        Ok(Self { images, tracks })
    }

    pub fn raw_data(&self, fad: u32) -> Result<(usize, u32)> {
        let (index, track) = self
            .tracks
            .iter()
            .enumerate()
            .find(|(_, track)| (track.start..track.end).contains(&fad))
            .ok_or("Target FAD has no stored INDEX 01 data")?;
        if track.mode != 1 || track.width != RAW_BYTES as u32 {
            return fail("Target FAD is outside a raw Mode 1 track");
        }
        let offset = track.offset_at(&self.images[track.image], fad - track.start)?;
        Ok((index, offset))
    }

    pub fn read_ip(&self) -> Result<[u8; 16]> {
        let track = &self.tracks[0];
        let prefix = match (track.mode, track.width) {
            (1, 2352) => 16,
            (2, 2352) => 24,
            (2, 2336) => 8,
            (1 | 2, _) => 0,
            _ => return fail("First track has no Saturn IP"),
        };
        let mut file = File::open(&self.images[track.image].path)?;
        file.seek(SeekFrom::Start(u64::from(track.offset) + prefix))?;
        let mut ip = [0; 48];
        file.read_exact(&mut ip)?;
        if &ip[..16] != b"SEGA SEGASATURN " {
            return fail("First track does not have a Saturn IP header");
        }
        Ok(ip[32..].try_into().unwrap())
    }
}
