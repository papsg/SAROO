# SAROO contribution porting progress

The development baseline is v0.8 commit
[`ba387b08`](https://github.com/papsg/SAROO/commit/ba387b083bb9d2a001ae6a22a24d6e6785e515bd).
The fork's default `main` starts there. The previous `master` is preserved;
subsequent upstream changes are not automatically imported.

Each contribution is verified on a focused branch, then integrated directly
into `main` and pushed. New GitHub pull requests are not required. Original authors,
related requests and relevant primary documentation are credited in commits and this ledger.
USB integration, custom diagnostics and private patch-application machinery
are excluded. This solo-maintainer workflow was authorized by the owner.

Last updated: 2026-11-08.

| Contribution | Original request | Fork commit / status | Dependencies and qualification |
| --- | --- | --- | --- |
| Language hash-table bounds | [#377](https://github.com/tpunix/SAROO/pull/377) | [6fb42d0](https://github.com/papsg/SAROO/commit/6fb42d0), on main | Independent. ASan/UBSan passes 1,680 lookups over 14 languages; original and individual old-mask controls fail. SH-2 objects compile at this checkpoint; current assembled-main link qualification is recorded below. No new hardware qualification. |
| MEMS overwrite corruption | [#372](https://github.com/tpunix/SAROO/pull/372) | [5920eb9](https://github.com/papsg/SAROO/commit/5920eb9), on main | Independent. Nine overwrite scenarios fail on v0.8; read-only export passes. All five actual-CLI methods pass after correction under ASan/UBSan and normal O2. No physical Saturn qualification. |
| Bounded menu text prerequisite | Our correction for the language series | [f6bdc5e](https://github.com/papsg/SAROO/commit/f6bdc5e), on main | Existing Greek code-1 error reproduces a 40-byte stack overflow. Six error paths and two titles now use actual capacities; custom formatter termination/integer/width handling is bounded. Three ASan/UBSan methods cover 252 errors, titles, 12,880 capacity/length pairs and valid controls. Independent old-caller and old-formatter controls fail. All 16 menu units compile; final SH-2 formatter preserves null/size guards with scoped compiler flags. Hardware limits remain; the assembled-menu link limitation is cleared below. |
| Native unsigned field-read prerequisite | Our correction for [#374](https://github.com/tpunix/SAROO/pull/374) interchange | [9f41183](https://github.com/papsg/SAROO/commit/9f41183), on main | A valid high-bit field reproduces signed-shift UB in the existing native save utility. One expression now widens bytes to u32 before shifting. Two focused ASan/UBSan methods pass 4,096 byte/alignment combinations, old-expression rejection with 2,048 valid low-byte controls, and a complete valid high-bit CRC/raw-date/payload round trip with whole-archive preservation. Five prior MEMS methods pass. Explicit host prerequisite; firmware inputs unchanged. [Reproduction and source references](https://github.com/papsg/SAROO/blob/main/doc/SAVE_BYTE_READS.md). |
| Controller/multitap parsing | [#380](https://github.com/tpunix/SAROO/pull/380) | [86c7caf](https://github.com/papsg/SAROO/commit/86c7caf), [67c51f3](https://github.com/papsg/SAROO/commit/67c51f3), on main | Deduplicates #378/#379; original hardware report remains attributed. Twelve actual-code ASan/UBSan methods cover 13,422 normal-owner packets, all sockets, lengths, mouse/pad, malformed/busy/timeout/wrap and independent write-only controls. Porting also exposed and fixed the existing BIN input export losing service after IRQ handoff; a same-parser complete-page polling path preserves it. Post-admission busy test strengthened in [ecbf2a7](https://github.com/papsg/SAROO/commit/ecbf2a7). All 16 units compile; final main rebuilt, direct-pad object byte-identical. +1,052 object text/readonly and 24 BSS bytes. Exact Sega/Hitachi references in doc/PERIPHERALS.md. Clock and physical-device limits remain; current assembled main fully links. |
| Missing CJK coverage | [#203](https://github.com/tpunix/SAROO/pull/203) | [8ac9a8b](https://github.com/papsg/SAROO/commit/8ac9a8b), on main | 108 added records match the original attached firmware; all 10,628 baseline records and source notices preserved. Five original drawings already exist in v0.8. Three ASan/UBSan font methods and combined language/menu checks pass; original asset fails missing coverage and regeneration. Existing converter reproduces the complete new font after explicitly retaining the old U+000C selection. +3,289 loaded bytes; production code unchanged. Physical qualification and trailing-period filename limits remain. |
| European Portuguese | [#256](https://github.com/tpunix/SAROO/pull/256) | [a494e69](https://github.com/papsg/SAROO/commit/a494e69), on main | Original 20 labels at ID 14; IDs 0–13 unchanged. Requires language bounds and bounded menu text. ASan/UBSan: 1,800 lookups, 270 complete errors, title/formatter controls. Shipped-font glyphs and 288-pixel width checks pass. SH-2 language object compiles. Editorial and hardware limits remain. |
| Welsh | [#299](https://github.com/tpunix/SAROO/pull/299) | [73c1fbe](https://github.com/papsg/SAROO/commit/73c1fbe), on main | Original 20 labels at ID 15 after Portuguese; existing IDs unchanged. ASan/UBSan: 1,920 lookups, 288 complete errors, title/formatter controls. All new glyph/288-pixel checks and SH-2 language compilation pass. No related open fork PR; issues disabled. Editorial and hardware limits remain. |
| Dutch | [#324](https://github.com/tpunix/SAROO/pull/324) | [461d658](https://github.com/papsg/SAROO/commit/461d658), on main | ID 16. Four documented wording adaptations; sixteen labels unchanged. Original wording fails at 328 pixels; corrected glyph/288-pixel checks pass. ASan/UBSan: 2,040 lookups, 306 complete errors and title/formatter controls. SH-2 language compilation passes. Editorial and hardware limits remain. |
| Catalan | [#360](https://github.com/tpunix/SAROO/pull/360) | [360d03d](https://github.com/papsg/SAROO/commit/360d03d), on main | Original 20 labels at ID 17; IDE workspace files excluded. ASan/UBSan: 2,160 lookups, 324 complete errors and title/formatter controls. All 80 added labels/full signed-error variants pass shipped-glyph and 288-pixel checks. SH-2 language compilation passes. Editorial and hardware limits remain. |
| Arabic | [#107](https://github.com/tpunix/SAROO/pull/107) | [9429f53](https://github.com/papsg/SAROO/commit/9429f53), on main | ID 18 with all 20 fixed labels. Credits Array in a Matrix; our shaping, wording/font/caller adaptations are explicit. Requires language bounds, bounded menu text, ID sequence and CJK stage. Four Arabic methods pass 36 independent oracle cases, 648 earlier-language actual-C formatter/raster controls, 8,496 original atlas pixels, exact font rebuild and 59 production glyph checks. 2,280 lookups, 342 complete errors, existing menu/peripheral checks and four Rust failure/boundary tests pass. Both pre-Arabic main and candidate fully compile/link; only main.o/language.o change. +1,040 linked code/data, +1,216 font/image bytes; BSS/stack unchanged. Source licenses and Unicode section citations in [doc/ARABIC.md](https://github.com/papsg/SAROO/blob/main/doc/ARABIC.md). Native-speaker and Saturn appearance/timing remain unqualified. |
| English documentation/comments | [#68](https://github.com/tpunix/SAROO/pull/68), [#352](https://github.com/tpunix/SAROO/pull/352) | [73922db](https://github.com/papsg/SAROO/commit/73922db), on main | Deduplicates #343. 77 ASCII comments preserve every original source byte; all eight affected SH-2 objects are byte-identical. Source-checked English README/developer notes and local links verified. The integrated tree and all compiled inputs/hashes match the verified branch. No runtime or hardware claim. |
| Browser save editor | [#374](https://github.com/tpunix/SAROO/pull/374) | [e3e3337](https://github.com/papsg/SAROO/commit/e3e3337), on main | Credits Damián Parrino; our transactional/validation/ownership corrections are explicit. Uses public v0.8 geometry; native 9f41183 prerequisite above is integrated. Seven Python methods/421 Node tests, 24 actual ASan/UBSan native CLI exports, an independent full-capacity encoder, 18 offline Chrome workflows, two stale-selection cases and complete 4,095-slot/256 MiB edit/download preserve every unedited byte. Original destructive-growth and wrong-layout controls fail. HTML/linked exports/parser identities and Rust fmt/clippy/tests pass. [Reproduction, exact Sega/XML/source citations and notices](https://github.com/papsg/SAROO/blob/main/tools/savetool/web/README.md). Other browsers/mobile capacity, SD persistence and physical Saturn behavior remain unqualified. Firmware inputs unchanged. |
| Browser cover editor | [#374](https://github.com/tpunix/SAROO/pull/374) | [54d0c72](https://github.com/papsg/SAROO/commit/54d0c72), on main | Separate application credits Damián Parrino and identifies our preservation/validation/ownership corrections. Three Python methods/26 Node tests, independent binary encoders, original-proposal negative/positive controls and actual unchanged ASan/UBSan native CLI controls pass. Fourteen offline Chrome workflows, four delayed-file choices and two stale-render cases pass. The full 16,384-cover/400.5 MiB application test edits the last record and compares every unedited downloaded byte. Quantizer parity, deterministic HTML/linked exports, Rust and save-editor controls pass. [Public reproduction, primary source citations and compatibility limits](https://github.com/papsg/SAROO/blob/main/tools/covertool/web/README.md). v0.8 firmware lookup still covers only 2,048 records; legacy CLI limits and physical/browser qualification limits remain explicit. No firmware changes. |

Verified topic checkpoints (each is an ancestor of current main):

| Original contribution | Branch |
| --- | --- |
| #377, #372 | [codex/direct-history](https://github.com/papsg/SAROO/tree/codex/direct-history) |
| Bounded menu-text prerequisite | [codex/menu-text-capacity](https://github.com/papsg/SAROO/tree/codex/menu-text-capacity) |
| #68/#352 | [codex/english-direct](https://github.com/papsg/SAROO/tree/codex/english-direct) |
| #256 | [codex/european-portuguese](https://github.com/papsg/SAROO/tree/codex/european-portuguese) |
| #299 | [codex/welsh](https://github.com/papsg/SAROO/tree/codex/welsh) |
| #324 | [codex/dutch](https://github.com/papsg/SAROO/tree/codex/dutch) |
| #360 | [codex/catalan](https://github.com/papsg/SAROO/tree/codex/catalan) |
| #203 | [codex/cjk-coverage](https://github.com/papsg/SAROO/tree/codex/cjk-coverage) |
| #380 core | [codex/menu-peripherals](https://github.com/papsg/SAROO/tree/codex/menu-peripherals) |
| #380 busy control | [codex/peripheral-busy-control](https://github.com/papsg/SAROO/tree/codex/peripheral-busy-control) |
| #380 guest-input compatibility | [codex/guest-input-compat](https://github.com/papsg/SAROO/tree/codex/guest-input-compat) |
| #374 native field-read prerequisite | [codex/save-unsigned-fields](https://github.com/papsg/SAROO/tree/codex/save-unsigned-fields) |
| #107 | [codex/arabic-localization](https://github.com/papsg/SAROO/tree/codex/arabic-localization) |
| #374 save editor | [codex/browser-save-editor](https://github.com/papsg/SAROO/tree/codex/browser-save-editor) |
| #374 cover editor | [codex/browser-cover-editor](https://github.com/papsg/SAROO/tree/codex/browser-cover-editor) |

The former PR branches `codex/language-hash-bounds`, `codex/mems-overwrite`
and `codex/english-documentation` preserve superseded history. Current direct
integrations use the checkpoint branches above; the historical branches are
not instructions to restore their ancestry.

All twelve contributions in the original upstream series are verified and integrated directly into
[`main` at 54d0c72](https://github.com/papsg/SAROO/commit/54d0c7244db481b9218447dc902b928b5e6bb760).
Bounded menu text and native unsigned field reads are explicit prerequisites and
are not counted in that total. All verified topic checkpoints and this ledger
are pushed. No candidate remains queued or in progress in that completed series.

The Arabic checkpoint clears the earlier available-toolchain missing-libc limitation:
both preceding public `67c51f3` and Arabic `9429f53` fully compile/link all sixteen
menu units with GCC 13.2.0 / Yaul 0.3.1-3, using the existing Yaul memory functions
and GCC runtime. Exact reproduction commands and measured extents are in
[doc/ARABIC.md](https://github.com/papsg/SAROO/blob/main/doc/ARABIC.md#build-qualification).
ELF/body/font bounds and all twenty linked Arabic strings/ID registration pass.
Previous object-only receipts retain their historical scope; full linking does
not establish physical Saturn behavior.

Historical implementation evidence guides these ports; it does not qualify a
newly assembled public branch. Every contribution records fresh checks and outstanding
limits. Host tests and compilation do not establish physical Saturn behavior.

After verified integration, fully resolved issues and fully superseded PRs in this
fork can be closed with a commit link and resolution summary. Partial requests stay
open. Requests in `tpunix/SAROO` are left unchanged.

At the owner's request, the first two contributions were recreated as direct
commits on 2026-11-08. The two PR merge commits were removed from main's history.
The recreated two-fix checkpoint `5920eb9` is identical to the former main at `7e505af`; original
authorship and coauthor credit are preserved. Fresh language and MEMS sanitizer
regressions pass. The former history is also retained in a local archive tag.
GitHub retains the closed historical [PR #1](https://github.com/papsg/SAROO/pull/1)
and [PR #2](https://github.com/papsg/SAROO/pull/2) records; these do not describe
the current main's ancestry. The fork currently has issues disabled; no related
fork issue or superseded open PR needs closure. This was checked again after
the English, Latin-language, CJK, controller, Arabic, native-field and both browser-editor integrations; the
fork has no open PRs. No original upstream request has been changed.

## Completion audit — 2026-11-08

- All twelve contribution commits are ancestors of main; all have original-PR
  citations and original author/coauthor credit. Necessary prerequisites and our
  adaptations are explicit. Controller #378/#379 and English #343 overlaps have
  evidence-supported duplicate dispositions in the related contribution records.
- Hardware-facing controller claims cite exact Sega/Hitachi sections/pages in
  [PERIPHERALS.md](https://github.com/papsg/SAROO/blob/main/doc/PERIPHERALS.md).
  The save editor cites Backup Library §2.1, BupDate p. 41, BupDir p. 42 and
  function specifications 10/11 p. 48. Project-defined cover formats cite v0.8
  source functions instead of attributing their format to a Sega requirement.
  Unicode/font/dependency sources and notices remain with the relevant changes.
- The complete public host suite passes on the final tree, including language,
  bounded text, fonts, peripherals, Arabic, native field reads, both browser
  cores and Rust tools. The browser suites and complete 256 MiB save / 400.5 MiB
  cover application checks pass against their exact published HTML hashes.
  Per-contribution documents give public reproduction commands and valid controls.
- MCU/menu/FPGA build inputs have not changed since the fully linked Arabic
  checkpoint. The two browser contributions and native unsigned-field fix are
  host changes. The earlier link/build receipts retain their actual scope;
  no new physical hardware, game, editorial or other-browser acceptance is claimed.
- The v0.8 baseline remains an ancestor, main remains the default, and historical
  master remains `594a15e59de12fd75b8fa9dd4431d743005b6cf3`. The sixteen focused
  commits after the baseline contain twelve contributions, two prerequisites and
  two controller follow-ups; no merge commit or later upstream import was added.
- The final source tree is clean and published. No new PR was opened. Fork issues
  remain disabled and no open PR needs closure; the historical closed fork PRs
  remain records. Original upstream requests are unchanged.
- Builds and evidence are confined to the public workspace. The private SAROO
  workspace, its branches/stashes and the Saturn were left unchanged. No device
  operations, unrelated research or subagents were used for these ports.

Outstanding limits above describe separate qualification or existing-baseline
work, not unfinished port candidates. In particular, firmware cover lookup and
legacy native-cover utility limitations are documented rather than silently
folded into the browser contribution.

## Native save-tool safety series

This subsequent owner-authorized series starts at public main `54d0c72` and
ports seven independently investigated maintenance changes, one verified commit
at a time. Historical private implementations/evidence are read-only references;
public tests and builds establish each new integration. No PRs, device access,
private writes, format migration or console firmware changes are included.

| Finding / historical reference | Public checkpoint | Validation, source references and limits |
| --- | --- | --- |
| SAVETOOL-002 MEMS resizing; `3eada90f` | [de8715b](https://github.com/papsg/SAROO/commit/de8715b), on main; [codex/savetool-mems-resize](https://github.com/papsg/SAROO/tree/codex/savetool-mems-resize) | All 19 new actual-CLI scenarios fail at the public baseline; all five previous overwrite-control methods pass there. All eight combined MEMS methods now pass ASan/UBSan and normal O2. Thirteen successful resize transitions and six refusals cover old-block reuse, capacity, complete permitted mutation extents, neighbors, stable metadata, allocation/free counts and fresh payload/date/CRC exports. Two unsigned-reader and seven browser/native checks pass. [Attribution, exact source/format references, public reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md). Historical author paps-g and original PR #372/Damián Parrino credit retained. Full metadata admission and checked persistence remain subsequent scope. |
| SAVETOOL-003 input extents/integrity; `4bbc849a` | Next | Independently revalidate against current public main; unsigned-field fix 9f41183 is already integrated. |
| SAVETOOL-004 new MEMS allocation; `5a000472` | Queued | Plan complete allocation before mutation. |
| SAVETOOL-005 MEMS metadata; `9e486906` | Queued | Bound pointers/chains and validate cross-save ownership. |
| SAVETOOL-005 SAROO slots; `a6d7e31e` | Queued | Bounds, resizing, damaged-slot preservation and safe creation. |
| SAVETOOL-005 Saturn reader; `75ccd91b` | Queued | Plain/odd-byte metadata, fragmented chains and short/odd exports. |
| SAVETOOL-006 file replacement/paths; `c8ee25f7` | Queued | Checked persistence and save-derived identity/path handling. |

One of seven is integrated. The public source tree was clean after the first
integration; main and the verified topic branch were pushed. Fork issues remain
disabled and no open fork PR requires closure. Original upstream requests are
unchanged. The MEMS correction changes only host code/tests/docs; private sources,
branches/stashes, console firmware inputs and the Saturn remain untouched.
