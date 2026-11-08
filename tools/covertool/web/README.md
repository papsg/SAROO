# Offline SAROO cover editor

Open [index.html](index.html) as a local file in a recent desktop browser.
The checked-in HTML contains the application, worker and GPLv3 license. No server,
runtime package installation or firmware update is required. Files stay local;
the application makes no automatic network requests.

Open/drop a `cover.bin`, or choose **Add images** to start a library. Search by
serial/hash, select a cover, edit its identity or artwork, then apply the edit.
**Export library** prepares the visible **Download cover.bin** link. Retain that
download before replacing edited work. A download request does not prove an SD
write; copy the resulting file to the SD card's `SAROO` directory yourself.

Indexed BMP import preserves the complete palette, pixels and original size.
Other browser-decodable images are converted to 256 colors. Fit keeps the whole
image with white borders, Fill crops and Stretch changes its aspect ratio.
Transparency becomes white. Portrait/square presets and custom dimensions are
available. BMP and PNG exports are supported.

Batch filenames `cover_SERIAL~1234ABCD.bmp` or `.png` carry an identity. Missing
identities, duplicates and failed inputs appear in a review list. Cancellation
retains the completed prefix and lists the remaining files. Identify from disc
image reads only the first 256 bytes of a BIN/ISO data track, using Saturn
headers at offset 0 or 16. It does not parse CUE sheets or locate other tracks.

## File format and compatibility limits

`cover.bin` is a SAROO format, rather than a Sega-defined archive. The primary
references are the v0.8
[`cover_hdr_t`/native utility](https://github.com/papsg/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/tools/covertool/main.c#L34)
and firmware
[`find_cover`, `cover_init` and `load_cover`](https://github.com/papsg/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_utils.c#L649).
Those sources also define the existing disc serial extraction and Adler-32 hash
of the 64-byte IP title field at offset `0x30`; this hash is a lookup key.

The file holds a 512 KiB index of 16,384 little-endian 32-byte records, followed
by image blobs. Each active record contains twelve serial bytes, a 32-bit IP
hash, a 32-bit absolute image offset, 16-bit width/height and eight reserved bytes.
Each blob is 1,024 palette bytes followed by top-down 8-bit indices. The editor
supports all 1–128 × 1–192 dimensions and all twelve serial bytes. `%HH` notation
provides reversible serial/filename byte access. Equal checksum candidates share
an offset only after complete palette/pixel/length comparison.

An untouched export preserves the whole input, including padding, inactive
records, gaps and trailers. An actual edit or explicit **Compact unused space**
repacks active records, retaining their untouched metadata and artwork while
removing unused space. A deliberate serial edit clears that field's old padding.
Failed admissions do not change the current library.

The file's capacity and the baseline tools' behavior are distinct:

- **v0.8 firmware searches only the first 2,048 records** and uses string-based
  serial lookup. This browser contribution does not extend that lookup or qualify
  a twelve-byte non-NUL serial on the device. Use ordinary game serials for new
  entries intended for that firmware; lossless storage remains available.
- The unchanged native CLI has serial truncation, BMP row handling and
  palette-agnostic checksum-only sharing limits. The fresh native interchange test covers three
  ordinary-ID, aligned-row, 40-byte-DIB images. It does not establish that the old
  CLI preserves every valid browser export or that these unrelated CLI defects
  are fixed.
- Other-browser decoding, image metadata/color profiles and physical cover
  display remain unqualified. Automated image checks cover indexed BMP/PNG and
  synthetic RGBA. Generic formats follow the browser's decoder.

## Ownership and resources

One worker owns the complete input and current image source. The UI draws at most
24 covers per page, and worker results carry workspace/revision ownership.
Dialog tokens and file-choice owners discard late completions. Replacing a
library validates the candidate before publication. Imported serials are rendered
as literal text. Duplicate batch identities are checked before decoding images.

Closing/replacing an image releases its bitmap. One prepared library download
remains available until replaced or invalidated by a mutation. If a worker fails,
edits stop and a prepared download stays usable; unpublished edits in a dead
process cannot be recovered by the UI.

The canonical maximum library is **419,954,688 bytes (400.5 MiB)**; padding can
make inputs larger. Original views retain their backing allocation while they
remain live. Replacement loads may retain old and new inputs temporarily, and
downloads/Blob storage/garbage collection add browser costs. Repacking uses
bounded sorting/index/batch arrays and avoids a second full contiguous output
array. A small visible page does not make the complete application's memory small.
Generic input images retain a decoded bitmap at source resolution. Cooperative
checkpoints do not guarantee a response/cancellation deadline or mobile capacity.

## Origin and licenses

**Damián Parrino / Bucanero** proposed the browser tools in
[original PR #374](https://github.com/tpunix/SAROO/pull/374), reviewed at
[`15a8e388`](https://github.com/tpunix/SAROO/commit/15a8e388a79ec2598fb285abf8d05e8fef2bee97).
This fork supplies the corrected implementation: complete raw palette/serial
preservation, checked extents and identities, exact deduplication, transactional
edits, bounded rendering and worker/dialog/file-choice ownership. The proposal's
weighted median-cut behavior is preserved while avoiding repeated box scans.

The application/modules and test-only original codec excerpt retain GPLv3
attribution. The complete [GPL-3.0 license](LICENSE) is embedded in the HTML.
The original excerpt is excluded from the application. No third-party runtime
dependency is bundled. The shared Rust asset linker is separately MIT-licensed;
Playwright is a development-only dependency with its own installed notices.

## Rebuild and verify

From the repository root, with Rust 1.85+, Python 3, Node.js 22+ and a C compiler:

```sh
cargo run --locked --manifest-path tools/Cargo.toml -p web-assets -- cover tools/covertool/web tools/covertool/web/index.html > tools/covertool/web/index.build.json
make -C tests check-web-cover check-host-tools
```

The [receipt](index.build.json) binds exact runtime inputs and output. The host
linker uses the same bounded input/output, closed module convention and atomic
file replacement as the save editor. Rebuilding the save editor still produces
its unchanged checked-in HTML and receipt. No new Rust dependencies are required.

Using an installed Google Chrome, install the pinned development harness:

```sh
npm ci --prefix tests/browser --ignore-scripts --no-audit --no-fund
make -C tests check-web-cover-browser
make -C tests check-web-cover-capacity
```

The capacity target generates/downloads 400.5 MiB synthetic files. Ordinary host
checks do not start a browser. Tests do not use personal artwork or access a device.

Fresh macOS checks with Node 25.9.0 and Chrome 156.0.8078.80 pass:

- Three Python methods run 26 Node tests and the unchanged native CLI under
  ASan/UBSan. Independent binary fixtures cover all five DIB sizes, row
  orientations/padding, complete palettes, serial bytes, allocation extents,
  collisions, rejected changes and 16,384-record admission.
- The original proposal reproduces palette-byte loss, checksum aliasing,
  twelve-byte serial truncation and short-BMP admission. Its ordinary-image
  positive control passes. Six RGBA workloads retain identical complete palettes
  and indices with the adapted quantizer.
- Fourteen offline browser workflows, four delayed file-choice cases and two
  stale-render cases pass. No accidental page errors or HTTP requests occur.
- The complete capacity workflow cancels a load, opens 16,384 distinct maximum
  images, edits the last record, downloads all 419,954,688 bytes and independently
  stream-compares every byte except the deliberately edited serial field.
- Deterministic HTML, linked/ESM export parity, failure preservation, Rust
  formatting/clippy/tests and existing save-editor controls pass.

Leak reporting is disabled only for the old native CLI's lifetime buffers;
bounds/undefined-behavior checks remain enabled. Other browsers, mobile memory,
SD persistence and physical Saturn behavior remain unqualified. No MCU, menu or
FPGA source/build input changes, and nothing is installed on a device.
