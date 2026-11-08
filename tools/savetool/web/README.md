# Offline SAROO save editor

Open [index.html](index.html) as a local file in a recent desktop browser.
The checked-in HTML contains the application, background worker, XML parser and
licenses. It needs no server, package installation or firmware change to run.
Files are processed locally; the application makes no automatic network requests.

Open or drop an archive, select a game/save, edit or import a replacement, then
apply the complete edit. **Export archive and download it to retain changes.**
An individual save can also be downloaded as SRO, BUP, XML or raw payload.
Browser downloads do not write directly to an SD card or prove Saturn acceptance.

| Archive | Supported behavior and capacity |
| --- | --- |
| `SS_SAVE.BIN` | Public v0.8 SAROO layout: up to 4,095 game slots, 511 empty saves per slot, 65,280 payload bytes per save |
| `SS_MEMS.BIN` | 8 MiB memory card: 448 directory entries and up to 479 × 1,024 = 490,496 payload bytes per save |
| Saturn backup | Complete 32 KiB logical or odd-byte 64 KiB physical image, read-only; up to 510 headers and 29,550 payload bytes per save |

The SAROO writer uses the baseline's 128-byte blocks and 64 KiB game slots.
Existing game IDs retain all sixteen bytes. A damaged or unsupported named slot
is preserved while other healthy slots remain accessible; selecting it never
automatically reformats it. Unmodified exports retain the complete original file.

The interface shows 24 games/saves per page and 256 hex bytes at a time. Search,
paging and the offset field reach all supported records and payload bytes.
Saturn-backup write actions are disabled and also rejected by the archive core.
Failed capacity, format or field checks preserve the existing archive. Metadata
and payload edits are committed together. Replacement imports keep the selected
identity by default; taking the imported identity is an explicit option.

Shift-JIS, UTF-8 and reversible `%HH` byte views preserve unchanged encoded
fields, including bytes after NUL. Unsupported or overlong new text is rejected.
Deliberate text changes replace old padding. The Shift-JIS inverse chooses the
first valid byte spelling; it is not a claim of WHATWG canonical encoder behavior.
All stored language bytes and unsigned date words are preserved. Blank date means
unset in this UI; date editing uses Gregorian minute arithmetic from 1980.

## Interchange formats

SRO and BUP need complete 64-byte headers and exact declared payload extents.
A nonzero SRO CRC covers metadata and payload from offset `0x10` through EOF;
zero keeps the existing “not recorded” convention. Incorrect nonzero CRCs are
rejected. Related original work is [PR #269](https://github.com/tpunix/SAROO/pull/269)
for BUP, and [issue #270](https://github.com/tpunix/SAROO/issues/270) /
[PR #271](https://github.com/tpunix/SAROO/pull/271) for the revised SRO extension
and CRC layout. Those features already exist in the baseline and are not imported
again here.

XML follows the original
[SS Backup RAM Parser writer](https://github.com/hitomi2500/ss-save-parser/blob/06922280e79cf19956654413d40e06448a738f23/extract.cpp).
Binary name/comment fields are authoritative. Import supports XML 1.0 UTF-8
and UTF-16 LE/BE, checks encoding/declaration agreement, numeric fields, dates
and complete base64 payloads, and rejects DTDs/custom entities. Supplemental
notes/counters are reported as omitted by archive storage; retain their source
XML. Nonzero seconds are rejected because the archive stores whole minutes.
Headerless raw payload is export-only. SRO/BUP containers with a `.bin` extension
are accepted by signature.

Raw/XML transport permits at most 1 MiB payload, followed by the target archive's
smaller per-save admission limit. XML reads 32 KiB chunks with bounded nesting,
attributes, tokens and field text; extra whitespace does not impose a new
whole-document size cap. The schema and strictness checks refer to
[XML 1.0 Fifth Edition §§2.3, 2.6, 2.7, 2.8, 4.1 and 4.3.3](https://www.w3.org/TR/2008/REC-xml-20081126/).

Sega's [Saturn System Library User's Guide, ST-162-R1-092994](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf),
Backup Library §2.1, `BupDate` (printed p. 41) and `BupDir` (p. 42), documents
the date fields and byte-sized name/comment/language interface. Its function
specifications No. 10, `BUP_GetDate`, and No. 11, `BUP_SetDate` (p. 48), describe
the packed 32-bit date interface. These are PDF pages 46, 47 and 53 respectively.
The project-specific archive layouts and minute epoch are grounded in the baseline
[`game_save.c`](https://github.com/papsg/SAROO/blob/9f41183/Firm_Saturn/game_save.c)
and [native save utilities](https://github.com/papsg/SAROO/tree/9f41183/tools/savetool).
This editor's calendar tests do not qualify a physical BIOS's calendar quirks.

## Origin, corrections and licenses

The browser tools were proposed by **Damián Parrino / Bucanero** in
[original PR #374](https://github.com/tpunix/SAROO/pull/374), reviewed at
[`15a8e388`](https://github.com/tpunix/SAROO/commit/15a8e388a79ec2598fb285abf8d05e8fef2bee97).
This fork supplies a corrected implementation of that feature. Our adaptations
include complete allocation/chain validation, planned transactional edits,
lossless raw fields, strict interchange validation, paged rendering, worker and
dialog ownership, and stale-selection guards. The original delete-before-growth
failure is reproduced on fully owned full archives; the corrected core retains
every byte on those same rejected edits.

The application/modules are GPL-3.0-only; the complete [license](LICENSE) is also
embedded in the single-file HTML. The test-only original codec snapshot retains
the original author's credit and is excluded from the application.

XML uses unmodified **sax-js 1.6.1**, revision
[`961ceeef`](https://github.com/isaacs/sax-js/tree/961ceeefcc5186005466f2085aeb36f31b161e37).
Its [Blue Oak Model License 1.0.0](vendor/sax/LICENSE.md), original README,
package metadata and [source hashes](vendor/sax/sources.json) are retained.
The complete dependency license is embedded too. The host Rust asset linker
has its own MIT license. Playwright is a development-only test dependency;
its Apache-2.0 notices remain in the installed package and it is not bundled.

## Rebuild and verify

From the repository root, with Rust 1.85+, Python 3, Node.js 22+ and a C compiler:

```sh
cargo run --locked --manifest-path tools/Cargo.toml -p web-assets -- save tools/savetool/web tools/savetool/web/index.html > tools/savetool/web/index.build.json
make -C tests check-save-endian check-web-save check-host-tools
```

The small host linker accepts the authored named-import/declaration-export
convention, rejects unsupported syntax and altered parser/license identities,
escapes worker JSON, and writes HTML by atomic rename. It does not install or
download dependencies. [The build receipt](index.build.json) binds the exact
runtime sources and output. The checked-in source and locked host dependencies
are sufficient to rebuild it.

Install the pinned development harness, using an installed Google Chrome:

```sh
npm ci --prefix tests/browser --ignore-scripts --no-audit --no-fund
make -C tests check-web-save-browser
make -C tests check-web-save-capacity
```

The capacity target creates and downloads a synthetic 256 MiB archive. Ordinary
host checks do not launch a browser. No test opens personal saves or a device.

Fresh qualification uses Node 25.9.0 and Chrome 156.0.8078.80 on macOS:

- Seven Python methods pass 421 Node tests/subtests: 21 archive, 9 worker,
  5 text/calendar and 386 XML checks. All 2,057 independent Gregorian dates,
  supported encoding cases and malformed-input refusals pass.
- Twenty-four actual ASan/UBSan native CLI SRO/BUP exports match byte for byte.
  Complete new maximum-SAROO output also matches an independent encoder.
  The native reader's [unsigned-field prerequisite](../../../doc/SAVE_BYTE_READS.md)
  is explicit. The original CLI's process-lifetime buffers have leak reporting
  disabled; bounds and undefined-behavior checks remain enabled.
- Eighteen offline browser workflows and two rendered-selection ownership cases
  pass, including full-size SAROO/MEMS saves, no-op raw preservation, import
  cancellation, read-only backups, late file selection and a prepared download
  surviving a worker error. No accidental page errors or HTTP requests occur.
- The complete 4,095-slot application test selects/edits the last save, refuses
  another slot, downloads all 268,435,456 bytes and compares every unedited byte.
  The full-size SAROO regression checks the public v0.8 layout.
- HTML regeneration, linked module exports, parser identity rejection, Rust
  formatting/clippy and boundary/failure tests pass.

The archive core owns one full input buffer and bounded edit plans. Opening a
replacement may retain both old and new inputs until validation completes.
File/Blob copies, downloads and garbage collection add browser memory costs.
Cooperative worker checkpoints are not response-time guarantees. Other browsers,
mobile capacity, SD persistence and physical Saturn/game acceptance remain
unqualified. No MCU, menu or FPGA source/build input is changed by this feature.
