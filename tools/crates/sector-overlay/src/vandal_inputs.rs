//! Reproduce exact native-check inputs from caller-owned, read-only media.
//! Reuses the complete-track compiler and the actual-C logical-source verifier.
use crate::{
    compile,
    disc::{read_bounded, Disc, RAW_BYTES},
    disc_id_bytes, fail, hex, mode1, pack_name, product_bytes, read_recipe, source_check,
    write_pack, Recipe, Result,
};
use serde_json::{json, Value};
use sha2::{Digest, Sha256};
use std::{
    fs::{self, File},
    io::{Read, Seek, SeekFrom},
    path::Path,
};

fn sha(bytes: &[u8]) -> String {
    hex(&Sha256::digest(bytes))
}

fn logical(disc: &Disc, recipe: &Recipe, first: u32, count: u32) -> Result<(Vec<u8>, Vec<u8>)> {
    let mut original = Vec::with_capacity(count as usize * 2048);
    let mut post = Vec::with_capacity(count as usize * 2048);
    for fad in first..first + count {
        let (track, offset) = disc.raw_data(fad)?;
        let mut file = File::open(&disc.images[disc.tracks[track].image].path)?;
        file.seek(SeekFrom::Start(offset.into()))?;
        let mut sector = [0; RAW_BYTES];
        file.read_exact(&mut sector)?;
        mode1::check(&sector, fad)?;
        original.extend_from_slice(&sector[16..2064]);
        for edit in recipe.writes.iter().filter(|edit| edit.fad == fad) {
            let before = crate::decode_hex(&edit.before)?;
            let after = crate::decode_hex(&edit.after)?;
            // The compiler validates widths/bounds first; retain explicit checks here.
            let end = edit
                .offset
                .checked_add(before.len())
                .ok_or("Source edit overflow")?;
            if before.len() != after.len()
                || edit.offset < 16
                || end > 2064
                || sector[edit.offset..end] != before
            {
                return fail("Native extraction preimage differs");
            }
            sector[edit.offset..end].copy_from_slice(&after);
        }
        post.extend_from_slice(&sector[16..2064]);
    }
    Ok((original, post))
}

/// Emit only into a new directory; no media, BIOS or existing output is changed.
pub fn run(
    recipe_path: &Path,
    cue: &Path,
    bios: &Path,
    decoder: &Path,
    output: &Path,
) -> Result<()> {
    let recipe_path = recipe_path.canonicalize()?;
    let recipe_bytes = read_bounded(&recipe_path, 65536)?;
    let recipe = read_recipe(&recipe_path)?;
    if recipe.product_id != "T-1219G"
        || recipe.disc_id.as_deref() != Some("T-1219G   V1.002")
        || recipe.expire_after_fad != Some(292308)
    {
        return fail("Require the reviewed DLCT native recipe");
    }
    let product = product_bytes(&recipe.product_id)?;
    let pack = compile(
        &recipe,
        cue,
        product,
        disc_id_bytes(recipe.disc_id.as_deref(), &product)?,
    )?;
    let disc = Disc::read(cue)?;
    let (ip, ip_post) = logical(&disc, &recipe, 150, 3)?;
    let (read, read_post) = logical(&disc, &recipe, 292214, 95)?;
    let packed = &read[0x2e600..];
    let packed_post = &read_post[0x2e600..];
    for (bytes, expected) in [
        (
            ip.as_slice(),
            "15c5342a7fdfdbded2d3bb6f7ec3eb688fc5a401e47310d3a25aa71bc5288697",
        ),
        (
            ip_post.as_slice(),
            "3ac6bebb942ba413da25f0c9a0d83b76be872d1b72b5c926b25984693d3df981",
        ),
        (
            read.as_slice(),
            "c9d624acb790fc257f1a6442dc3d0291fbc7e8bd442583b300d95fa2036f8992",
        ),
        (
            read_post.as_slice(),
            "5b1da17ebe5f93282997b3997e004542967492b6dcbd28dd18be146ae40c21c4",
        ),
    ] {
        if sha(bytes) != expected {
            return fail("Complete reviewed original/postimage differs");
        }
    }
    let bios = read_bounded(bios, 524288)?;
    if bios.len() != 524288
        || sha(&bios) != "96e106f740ab448cf89f0dd49dfbac7fe5391cb6bd6e14ad5e3061c13330266f"
    {
        return fail("Require the reviewed caller-owned BIOS");
    }
    let decoder = decoder.canonicalize()?;
    let decoder_bytes = read_bounded(&decoder, 32 * 1024 * 1024)?;
    fs::create_dir(output)?;
    let output = output.canonicalize()?;
    if read_bounded(&recipe_path, 65536)? != recipe_bytes {
        return fail("Native recipe changed during extraction");
    }
    write_pack(&output.join("recipe.json"), &recipe_bytes)?;
    write_pack(&output.join(pack_name(&recipe)?), &pack)?;
    let mut inputs = serde_json::Map::new();
    for (name, bytes) in [
        ("bios", bios.as_slice()),
        ("ip_original", ip.as_slice()),
        ("ip_postimage", ip_post.as_slice()),
        ("packed_original", packed),
        ("packed_postimage", packed_post),
    ] {
        let path = format!("{name}.bin");
        write_pack(&output.join(&path), bytes)?;
        inputs.insert(name.into(), json!({"path":path,"sha256":sha(bytes)}));
    }
    write_pack(&output.join("native-read.bin"), &read)?;
    write_pack(
        &output.join("inputs.json"),
        &serde_json::to_vec_pretty(&inputs)?,
    )?;
    let manifest = json!({"recipe":{"path":"recipe.json","sha256":sha(&recipe_bytes)},
        "pack":{"path":pack_name(&recipe)?,"sha256":sha(&pack)},
        "decoder":{"path":decoder,"sha256":sha(&decoder_bytes)},
        "programs":[{"source":{"path":"ip_original.bin","sha256":sha(&ip)},"lba":0,"postimage_sha256":sha(&ip_post)},
        {"source":{"path":"native-read.bin","sha256":sha(&read)},"lba":292064,"postimage_sha256":sha(&read_post)}]});
    let manifest_path = output.join("source.json");
    write_pack(&manifest_path, &serde_json::to_vec_pretty(&manifest)?)?;
    source_check::run(&manifest_path, &output.join("production-c"))?;
    let receipt: Value = serde_json::from_slice(&read_bounded(
        &output.join("production-c/receipt.json"),
        65536,
    )?)?;
    if receipt["complete"] != true || receipt["all_target_sources_verified"] != true {
        return fail("Incomplete native source verification");
    }
    write_pack(
        &output.join("receipt.json"),
        &serde_json::to_vec_pretty(&json!({
        "complete":true,"pack_sha256":sha(&pack),"recipe_sha256":sha(&recipe_bytes),
        "extractor_source_sha256":sha(include_bytes!("vandal_inputs.rs")),
        "complete_cue_source_verified_by_compiler":true,"logical_production_c_verified":true,
        "inputs":inputs,"qualification":"Exact reviewed logical source/postimages and caller-pinned C decoder; separate native execution, finite lifetime and device qualification are required. Proprietary inputs remain local."}))?,
    )?;
    println!("Verified whole-source pack and complete native input regions; inputs.json is ready for firmware-build vandal");
    Ok(())
}
