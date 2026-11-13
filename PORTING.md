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

Last updated: 2026-11-12.

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
| SAVETOOL-003 input extents/integrity; `4bbc849a` | [bb64566](https://github.com/papsg/SAROO/commit/bb64566), on main; [codex/savetool-input-validation](https://github.com/papsg/SAROO/tree/codex/savetool-input-validation) | Five new methods pass with actual CLI sanitizers/normal O2 and included-source I/O controls. Covers 22 malformed files, 12 valid format/size combinations, 19 malformed archives, full 256 MiB SAROO admission/extra-unit refusal, 14 direct loader controls and opaque MEMS preservation. Baseline corpus and final API/full-unit controls fail. All 8 prior MEMS, 2 unsigned-reader and 7 web-save methods/24 native-browser exports pass. An overstrict MEMS candidate and collector decoding errors are preserved, corrected before publication. [Exact original format/C11 section-page citations, authorship, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md#savetool-003-input-extents-integrity-and-complete-reads). The 9f41183 field fix is deduplicated. Block ownership and checked output persistence remain subsequent scope. |
| SAVETOOL-004 new MEMS allocation; `5a000472` | [be30f6b](https://github.com/papsg/SAROO/commit/be30f6b), on main; [codex/savetool-mems-allocation](https://github.com/papsg/SAROO/tree/codex/savetool-mems-allocation) | Five new actual-CLI methods cover 13 successful creations and 11 refusals; the preceding public source fails ten scenarios. All 13 combined MEMS methods pass ASan/UBSan and normal O2. Tests check complete mutation extents, bitmap/free counts, old saves, fresh exports, maximal SRO/BUP, holes/final directory entry, stale counts and fragmented allocation ending at block 8063. Refusal preserves every archive byte, including when only reserved blocks are free. Five input, two unsigned-reader and seven web-save methods/24 native-browser exports pass. [Attribution, exact baseline allocation references, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md#savetool-004-plan-new-mems-allocation-before-mutation). Full ownership validation and checked persistence remain subsequent scope. |
| SAVETOOL-005 MEMS metadata; `9e486906` + `0dce8f0c` (SSR-102) | [f922ed8](https://github.com/papsg/SAROO/commit/f922ed8), on main; [codex/savetool-mems-metadata](https://github.com/papsg/SAROO/tree/codex/savetool-mems-metadata) | Five new actual-CLI methods cover 105 malformed-metadata refusals, ten unchanged read-only stale-count controls, ten deletion/recount controls, fixed-width padding and 78 raw-name operations. Prior public source fails 145 subcases; restoring only bounded string comparison fails all 36 malformed-suffix cases while valid names pass. All 18 MEMS and 5 input methods pass ASan/UBSan and normal O2; two unsigned-reader and seven web-save methods/24 native-browser exports pass. [Primary project-format references, original authorship, necessary refinement, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md#savetool-005-mems-metadata-and-ownership). The raw identity policy matches the existing public browser contract; Sega does not define private MEMS padding. Checked persistence remains following scope. |
| SAVETOOL-005 SAROO slots; `a6d7e31e` | [b1f3ac4](https://github.com/papsg/SAROO/commit/b1f3ac4), on main; [codex/savetool-slot-validation](https://github.com/papsg/SAROO/tree/codex/savetool-slot-validation) | Eight actual-CLI methods pass ASan/UBSan and normal O2. Prior source fails 117 subcases with five bounded timeouts in the seven historical methods; the added fragmented-allocation control also fails there. Covers stored indices, 100 malformed selected-slot operations, resize/capacity/refusal, stale counts/orphans, ID padding/holes/duplicates/append, full 256 MiB directory, six new SRO/BUP boundary saves and blocks 100/510/511. All 31 native, two unsigned-reader and seven web-save methods/24 exports pass. [Project-format citations, original authorship, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md#savetool-005-saroo-slots-resizing-and-creation). Retains public v0.8 layout; excludes later native-backup migration. Saturn reader and checked persistence remain following scope. |
| SAVETOOL-005 Saturn reader; `75ccd91b` | [b6df5de](https://github.com/papsg/SAROO/commit/b6df5de), on main; [codex/savetool-saturn-reader](https://github.com/papsg/SAROO/tree/codex/savetool-saturn-reader) | Six public methods pass ASan/UBSan and normal O2, including an actual included-reader exact-size sanitizer harness. Prior source fails 113 subcases in the 185-scenario historical corpus; four extent controls already pass due to SAVETOOL-003. Covers 100 SRO/BUP exports over 25 lengths/both encodings, fragmented/multiple saves, 78 malformed operations and six unchanged-input read-only controls. All 37 native, two unsigned-reader and seven web-save methods/24 exports pass. [Original authorship, exact primary raw-format references, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md#savetool-005-read-only-saturn-backup-tables-and-payloads). Independent libslinga references and their MIT notice are preserved as evidence; no implementation imported. Larger cartridge formats remain unsupported; host checks do not establish game/hardware qualification. Checked replacement/paths remain final scope. |
| SAVETOOL-006 file replacement/paths; `c8ee25f7` + `3151c6f5` (SSR-042) | [7d1d461](https://github.com/papsg/SAROO/commit/7d1d461), on main; [codex/savetool-atomic-io](https://github.com/papsg/SAROO/tree/codex/savetool-atomic-io) | Ten I/O methods cover checked pre/post-replacement errors, retries, names/aliases/special files, explicit/inferred paths, archive links and real metadata. Prior source has 218 failed assertions, 24 missing escaped-output errors and six FIFO timeouts. Omitting only the protection guard leaves 18 protected stages; 12 ordinary controls pass. Both corrected profiles pass all 30 metadata cases plus four ACL/archive-link controls. All 47 native methods pass sanitizers/normal O2, with two unsigned-reader and seven web-save methods/24 exports. Release binary has no test-fault hook. Excluded incomplete integration/control-setup runs are preserved. [Exact host-manual sections, original authorship, necessary refinement, public commands and remaining limits](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md#savetool-006-checked-file-replacement-and-derived-paths). Checked replacement changes inode identity; post-rename failure leaves complete new data visible. No arbitrary power-loss/concurrent-writer or new hardware qualification is claimed. |

All seven are integrated. The public source tree was clean after each
integration; main and all seven verified topic branches were pushed. Fork issues remain
disabled and no open fork PR requires closure. Original upstream requests are
unchanged. The integrated changes affect only host code/tests/docs; private sources,
branches/stashes, console firmware inputs and the Saturn remain untouched.

### Native-series completion audit

- Each independently confirmed contribution is a separate ordinary source commit
  on main, with its verified topic checkpoint published. No new PR or merge
  commit was created, and no subsequent upstream change was imported.
- Ports were evaluated against their immediately preceding public checkpoint.
  Existing unsigned field reads `9f41183` are the explicit SAVETOOL-003 prerequisite,
  not another contribution. The MEMS gate follows corrected resize/input/new-save
  paths. Slot validation and the read-only Saturn reader consume checked extents.
  The final writer/error propagation binds all three validated backends. Complete
  MEMS identity (`0dce8f0c`, SSR-102) and protected macOS destination refusal
  (`3151c6f5`, SSR-042) are independently reproduced necessary refinements.
- All seven candidate defects are applicable and corrected. Already-correct
  unsigned reads and the Saturn reader's four extent/signature controls are
  explicitly deduplicated. Later native-backup migration, console storage,
  cover publication and unrelated research remain outside this series.
- Public tests execute the actual native CLI and separately included C shims.
  All 47 native methods pass ASan/UBSan and normal O2; loader, exact reader and
  syscall-fault fixtures remain sanitized in both profiles. Two unsigned-reader
  and seven web-save methods pass, including 24 native/browser exports. Public
  commands, authorship and exact project-format/host-manual citations are in
  [SAVETOOL-SAFETY.md](https://github.com/papsg/SAROO/blob/main/doc/SAVETOOL-SAFETY.md).
- Qualification is new Apple Silicon macOS host behavior. Thirty native metadata
  cases pass per profile, including eighteen user-protected refusals and twelve
  ordinary controls. System protection masks are compiled but not applied.
  Historical Intel results are not promoted to new port qualification. Other
  platforms, physical storage, game/hardware timing, arbitrary drive power loss,
  concurrent metadata/writers and termination remain outside the recorded checks.
- Firmware build inputs are unchanged from the start of this series. Baseline
  `ba387b083bb9d2a001ae6a22a24d6e6785e515bd`, the twelve preceding contributions,
  default main and historical master are preserved. No private framework,
  .patch files, runtime diagnostics/loaders or USB integration were introduced.
- All new build/evidence/porting records are confined to the public workspace;
  private sources, branches/stashes and the Saturn were left unchanged. Initial
  rejected/incomplete trials are preserved and distinguished from acceptance.
  Fork issues remain disabled and no open fork PR requires closure; original
  upstream requests remain unchanged. No native safety candidate remains queued.

## Firmware save-safety series

This owner-authorized series starts at public `7d1d461`, retaining the v0.8
baseline, completed ports, default main and historical master. Ports are direct
source commits with new public evidence; private sources/history are read-only.
No PRs, USB integration, added runtime diagnostics, private patch framework,
experimental journal/native-format migration or device operation is included.

| Candidate / original reference | Public checkpoint | Dependencies, validation and limits |
| --- | --- | --- |
| Build qualification prerequisite | [0371ff8](https://github.com/papsg/SAROO/commit/0371ff8); [codex/firmware-build](https://github.com/papsg/SAROO/tree/codex/firmware-build) | MIT host-only Rust command stages public inputs and invokes the original menu Makefile with cached pinned GCC/Yaul, no image pull/container networking and a local Unix-socket endpoint. Preserves new output cohorts, bounds build time and verifies ELF/body/font/code/BSS/stack. Workspace fmt/clippy/tests, real baseline/candidate builds and output-preservation/path-refusal controls pass. Existing firmware build remains available; no runtime feature or hardware acceptance. [Usage and source-contract citations](https://github.com/papsg/SAROO/blob/main/tools/README.md#isolated-firmware-qualification). |
| Compiler stack-report prerequisite | [f03ed8d](https://github.com/papsg/SAROO/commit/f03ed8d); [codex/firmware-stack-report](https://github.com/papsg/SAROO/tree/codex/firmware-stack-report) | Host-only follow-up records bounded GCC direct frame reports and terminal verification errors. Rust formatting, strict clippy, three unit tests and full target compilation pass. Direct frames exclude transitive callees/IRQs and do not prove game stack headroom. [GCC option citation and usage](https://github.com/papsg/SAROO/blob/main/tools/README.md#isolated-firmware-qualification). No runtime diagnostic is added. |
| SATURN-SAVE-002 dirty-window masks; `d8fe6875` | [1239a7d](https://github.com/papsg/SAROO/commit/1239a7d); [codex/firmware-save-dirty-mask](https://github.com/papsg/SAROO/tree/codex/firmware-save-dirty-mask) | Explicit sanitizer prerequisite for resizing, separated from the original combined commit. Three actual-C methods / 194 invocations per profile exercise all 64 positions at three starts, repeated/combined marks and slot-device preservation. Seven UBSan subcases fail on baseline; corrected ASan/UBSan and O2 pass. Full SH-2 object/ELF/image are byte-identical, with unchanged 75,056 code/data, 7,180 BSS, 16,384 stack and 32,452 body margin. [Exact C11 section/pages, original paps-g authorship, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#saturn-save-002-unsigned-dirty-window-shifts). No observed game crash or issue #363 resolution is claimed. |
| SATURN-SAVE-001 resizing/allocation persistence; `d8fe6875` | [6e9c217](https://github.com/papsg/SAROO/commit/6e9c217); [codex/firmware-save-resize](https://github.com/papsg/SAROO/tree/codex/firmware-save-resize) | Depends on the separate dirty-mask correction. Two actual-C methods cover 32 cases (21 successful round trips, eleven unchanged refusals); baseline fails 28, corrected sanitizers/O2 pass. Omitting only changed-allocation flush fails eight reload checks. All five coupled firmware methods pass. Complete SH-2 build adds 960 code/data and 2,028 BSS bytes; body margin 29,464. GCC direct write frame grows 52 to 104 bytes; menu stack reserve does not establish game stack headroom. [Exact Sega BupDir/BUP_Write sections/pages, original paps-g authorship, source references, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#saturn-save-001-resize-saves-and-flush-changed-allocation). Synchronous host-endian modeled reload is not a physical reset/SD write or power-loss transaction. Full metadata/I/O/retry work follows. |
| SATURN-SAVE-003 archive validation; `c97d2d70` | [b084a3c](https://github.com/papsg/SAROO/commit/b084a3c); [codex/firmware-save-validation](https://github.com/papsg/SAROO/tree/codex/firmware-save-validation) | After resizing and stack-report prerequisite. Six actual-C methods / 357 scenarios pass sanitizers and O2; eleven coupled methods pass. Baseline matrix has 322 failures and twelve bounded case deadlines; eight additional identity cases fail with longer observation. A strncmp-only mutant fails seven mismatches while equal raw fields pass. Complete SH-2 build: +1,200 code/data, +8 BSS, 28,256 body margin, unchanged image/font sizes. Cold full-directory status/read still need 448/449 header requests; batching follows separately. MEMS blank policy inspects only 8KiB and needs paired MCU existing-file admission. [Exact Sega sections/pages, authorship, reproducible checks, costs and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#saturn-save-003-validate-metadata-and-block-ownership). No physical/game/SD qualification. |
| SATURN-SAVE-005 dirty clear; `a9ddfcc6` | [0c76466](https://github.com/papsg/SAROO/commit/0c76466); [codex/firmware-save-dirty-clear](https://github.com/papsg/SAROO/tree/codex/firmware-save-dirty-clear) | Separate from batching. Two actual-C methods / 33 flag/status/retry and read-after-write cases; baseline fails 17, sanitizers/O2 pass. Wrong-success and wrong-command mutants fail twelve/four cases. All thirteen coupled methods pass. Full SH-2 build adds 32 code/data, no BSS/direct-frame growth, 28,224 body margin. Existing MCU still ignores actual I/O errors; this is reported-status bookkeeping, with full eviction/error/retry work following. [Protocol/FatFs citations, attribution and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#saturn-save-005-release-dirty-bits-after-a-successful-buffer-flush). |
| SATURN-SAVE-005 header batching; `a9ddfcc6` | [a62eb2c](https://github.com/papsg/SAROO/commit/a62eb2c); [codex/firmware-save-batching](https://github.com/papsg/SAROO/tree/codex/firmware-save-batching) | After dirty bookkeeping; independently qualified. Seven actual-C methods / 57 scenarios and twenty coupled methods pass ASan/UBSan and O2. Thirteen paired profiles retain exact header bytes: dense full status 448 to seven commands, selected zero-size cold read 449 to eight; sparse/warm status can still need 448. Full warm-cache control preserves every payload-window byte and requires no reload. Load/cache/validation mutants fail 3/1/12 cases. +624 code/data, unchanged BSS/image, 27,600 body margin; archive direct frame 28 to 48, excluding callees/IRQs. [Attribution, FatFs/source citations, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#saturn-save-005-batch-full-runs-of-validation-headers). Model counts are not hardware timing; checked MCU I/O and Saturn retry work follow. |
| GNU MCU allocator prototype prerequisite | [0203758](https://github.com/papsg/SAROO/commit/0203758); [codex/firmware-malloc-prototype](https://github.com/papsg/SAROO/tree/codex/firmware-malloc-prototype) | Reproduced Arm GNU/newlib type conflict in the unchanged cdimg.c translation unit. Use size_t in the existing allocator declaration/definition; body/pool/ownership are unchanged. Credits the same size-type adaptation in paps-g a2f8f885, excluding its USB/diagnostics and unrelated changes. Full GNU App/RTX compile/link passes; complete 20-byte allocator instruction section and zero direct frame are unchanged. [C11 section/page, source citations and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#gnu-mcu-build-prerequisite-malloc-size-type). No MDK defect or physical/reentrancy/game qualification is claimed. |
| Offline MCU build qualification prerequisite | [54282ae](https://github.com/papsg/SAROO/commit/54282ae); [codex/firmware-mcu-build](https://github.com/papsg/SAROO/tree/codex/firmware-mcu-build) | Host Rust command compiles original App/RTE/RTX with explicit offline SDKs. Actual baseline/candidate ARM compile/link and all 166 vector checks pass; six builder/four font/four web unit tests, workspace formatting/strict clippy and 219-file output-preservation control pass. Shared output guard rejects linked parent directories before creating descendants. Menu fully rebuilds byte-identically. Original MDK remains standard; generated GNU verification CRT is not an installable/MDK-qualified image. ST copyright/BSD notice is preserved with MIT host code. [Original App/vector/source citations, commands and limits](https://github.com/papsg/SAROO/blob/main/tools/README.md#original-mcu-app-compilelink-check). No network/device/USB/added runtime diagnostic or private preparation. |
| MCU-SAVE-001 startup; `aa74eb3e` | [0db5d2e](https://github.com/papsg/SAROO/commit/0db5d2e); [codex/firmware-save-startup](https://github.com/papsg/SAROO/tree/codex/firmware-save-startup) | Five actual startup/caller/FatFs methods / 95 scenarios; baseline fails 83, ASan/UBSan/O2 pass. Six focused controls fail 2/4/2/4/4/1 cases. Existing files retain full-content digests/lengths; existing-file failures issue no disk writes. Includes necessary 169cb468 partial-tail outer admission, preserves MEMS trailers and the complete slot signature, and excludes startup diagnostics. Full GNU App/RTX/166-vector check: load 59,644 to 59,616, unchanged BSS/data 60,656/504, startup direct frame 24 to 56. [FatFs/source citations, attribution, adaptations, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#mcu-save-001-preserve-existing-archives-during-startup). Runtime published-entry/I/O ownership and Saturn retries follow; GNU/model results are not MDK, peak-stack, physical SD/game qualification. |
| MCU-SAVE-001 runtime; `d05062a1` | [3c2ab6f](https://github.com/papsg/SAROO/commit/3c2ab6f); [codex/firmware-save-runtime](https://github.com/papsg/SAROO/tree/codex/firmware-save-runtime) | Twelve actual runtime/caller/FatFs methods / 137 scenarios; baseline fails 96. All seventeen coupled methods (plus 95 startup cases) pass ASan/UBSan/O2. Fourteen isolated controls expose ownership/count/sync/identity/hole/bounds/coherence/extent/publication defects. Includes necessary published-extent refinement, full signature and MEMS trailer compatibility. Full GNU App/RTX/166-vector check: +116 load bytes to 59,732, 5,804 ITCM free, unchanged BSS/data. Existing-slot reads retain 128KiB/two requests; new-slot writes retain 65,552 bytes/two requests with an additional ordering sync. [Citations, attribution, public reproduction, resources and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#mcu-save-001-checked-runtime-transfers-and-cache-ownership). Real read errors remain latched until explicit fixture reopen; production recovery/reset/session and Saturn error/retry handling remain separate. No MDK/physical/game qualification. |
| MCU-SAVE-002 required extent refinement; `169cb468` | Guards included in [0db5d2e](https://github.com/papsg/SAROO/commit/0db5d2e)/[3c2ab6f](https://github.com/papsg/SAROO/commit/3c2ab6f); verification [bb49548](https://github.com/papsg/SAROO/commit/bb49548), [codex/firmware-save-extents](https://github.com/papsg/SAROO/tree/codex/firmware-save-extents) | Real volume exhaustion leaves a 512/4096-byte short-write prefix on synthetic FAT32/exFAT. Eight cases and twenty-one coupled MCU methods / 240 scenarios pass ASan/UBSan/O2; strict-alignment, missing published-bound and truncation controls fail 8/2/6. Old payload/directory/tail/MEMS bytes are compared; admission/read causes no disk write. Test-only formatter switch; firmware inputs/resources unchanged. [Attribution, applicable trigger, FatFs/source citations and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#mcu-save-002-qualify-incomplete-tails-on-full-fat32exfat-volumes). Explicit reopen is not production automatic or hard-error/physical recovery. |
| MCU-SAVE-001 Saturn API; `1f90bc28` | [6135642](https://github.com/papsg/SAROO/commit/6135642); [codex/firmware-save-api](https://github.com/papsg/SAROO/tree/codex/firmware-save-api) | Six coupled actual-C/FatFs methods / 198 scenarios; baseline fails 167, corrected ASan/UBSan/O2 pass. Two caller gates / ten results pass, with eight baseline failures. Ten isolated controls are detected. All 22 menu and 27 MCU methods (438 MCU scenarios) pass both profiles. Full public check passes: 131 Python methods, fourteen Rust unit tests, formatting/strict clippy and authored C checks. Complete SH-2 build: +624 code/data to 78,496, +16 BSS to 9,232, 26,960 body margin; image stays 488,751 bytes. Large directory staging can add a 64KiB subsequent read; individual stack frames are recorded without peak-stack claims. MCU production inputs remain unchanged. [Exact Sega/FatFs/protocol citations, original paps-g authorship, public commands, adaptations and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-SAVE-SAFETY.md#mcu-save-001-propagate-saturn-api-errors-and-retain-pending-retries). Session-local pending data is not a journal, reset/media coordination, automatic file recovery or physical/game acceptance. |

All scoped dirty-mask, resizing, archive-validation, dirty-bookkeeping,
separately evaluated header-batching, checked MCU startup/runtime, required
incomplete-tail refinement and Saturn API/error/retry members are integrated.
All verified topic branches/main are pushed; no scoped candidate remains queued.
Fork issues remain disabled and no open fork PR needs closure. No upstream
request was modified. Private sources/branches/stashes/caches, installed firmware
and Saturn remain untouched. New checks establish host/source and target-link
behavior only, with no bus/cache/IRQ, SD durability or game qualification.

Final firmware-series checkpoint: public main `6135642`. The complete offline
public check passes with the existing Python runtime's Pillow dependency; the
initial system-Python dependency failure is excluded from qualification. Initial
API trace-capacity and caller-fragment setup failures are also preserved locally
and excluded from behavioral baseline counts. Test sources and commands are
public; detailed receipts, before/candidate inputs, failed controls and builds
remain in the public checkout's ignored artifact directories.

The final menu's cumulative change from the initial qualified public menu is
3,440 code/data and 2,052 BSS bytes. Its 16KiB stack reservation is unchanged.
The last complete GNU MCU App/RTX/166-vector qualification remains 59,732 load
bytes, 5,804 ITCM bytes free and unchanged 60,656 BSS/504 initialized data; the
Saturn API checkpoint does not change MCU production inputs. These compile/link
figures do not substitute for the original MDK build or hardware/game testing.
Remaining automatic file recovery, reset/card-generation coordination, partial
in-place writes and power-loss atomicity are explicit out-of-series limits.


## Disc and CUE safety series

Starts at public `6135642`; direct verified source commits on main, with no PRs,
USB integration, added runtime diagnostics, private framework or device access.
Private sources/history/evidence are read-only references. All seven candidates
are revalidated against public checkpoints; historical qualification is not reused.

| Candidate / original reference | Public checkpoint | Dependencies, evidence and limits |
| --- | --- | --- |
| MCU-DISC-001; `82125993` | [f23a7d4](https://github.com/papsg/SAROO/commit/f23a7d4); [codex/disc-save-session](https://github.com/papsg/SAROO/tree/codex/disc-save-session) | Builds on completed save-safety service. Five actual-C/FatFs methods / 31 scenarios; baseline fails 25, sanitizers/O2 pass. Six controls produce 6/2/4/2/9/2 failures. All 32 coupled MCU methods / 469 cases pass both profiles. Full GNU App/RTX/166-vector link: +84 load bytes to 59,816; 5,720 ITCM free; static RAM and direct frames unchanged. Automatic swap save traffic is zero, versus prior 128KiB same-ID reads or 64KiB read/65,552-byte new-slot writes. Omits the original diagnostic logger; updates the existing shell's signature without adding a feature. [Citations, paps-g attribution, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#mcu-disc-001-keep-the-active-save-session-during-automatic-changes). No hardware/SD replacement/concurrency/game qualification or reset/file-recovery/atomicity claim. |
| SATURN-MENU-002; `be30cf11` | [9c3342e](https://github.com/papsg/SAROO/commit/9c3342e); [codex/menu-binary-bounds](https://github.com/papsg/SAROO/tree/codex/menu-binary-bounds) | Consumer prerequisite is integrated before producer clear removal. Nine actual-function cases pass sanitizers/O2; baseline fails six. Three controls fail 4/2/1 cases, preserving the last valid filename byte. Three existing menu-text methods pass. Full SH-2 build adds 48 code/data to 78,544; BSS/stack/direct frame remain 9,232/16,384/144, body margin 26,912. MCU unchanged. [Source/C11 citations, paps-g attribution, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#saturn-menu-002-bound-bin-selection-before-loading). Payload/execution/address authorization and corrupt/concurrent catalogs are not qualified. |
| MCU-DISC-002 correctness; `be30cf11` | [ebb232f](https://github.com/papsg/SAROO/commit/ebb232f); [codex/disc-discovery-checks](https://github.com/papsg/SAROO/tree/codex/disc-discovery-checks) | Six actual-source/FatFs methods / 43 cases, baseline fails22; ASan/UBSan/O2 pass. Eight omission controls expose17 failures. All38 coupled MCU methods /512 cases pass both profiles. Complete GNU App/RTX/166-vector link saves212 load bytes to59,604; static RAM unchanged; shared scanner direct frame176 versus old BIN120/directory168, path helpers32/24. Retains the old143,356-byte clear and catalog listing strings; optimization remains separate. [Source/FatFs citations, paps-g attribution, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#mcu-disc-002-checked-discovery-paths-and-catalog-publication). Sequential publication/API-close stubs are not concurrent generation, physical recovery or game qualification. |
| MCU-DISC-002 clear-removal evaluation; `be30cf11` | [33b7b01](https://github.com/papsg/SAROO/commit/33b7b01); [codex/disc-catalog-reuse](https://github.com/papsg/SAROO/tree/codex/disc-catalog-reuse) | Separately verified after checked producer and bounded BIN consumer. Retained-clear and optimized controls pass44 functional cases with identical directory API counts; clearing falls143,356 bytes tozero. Actual producer/menu consumer verifies valid, stale, failed and empty catalogs; three controls are detected. All39 MCU methods /513 cases pass sanitizers/O2. Complete GNU App/RTX/166-vector link saves12 more load bytes to59,592; static RAM/direct frames unchanged, menu exact. [Source contracts, attribution and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#separate-evaluation-omit-unused-catalog-clearing). Requires the paired count-based menu; not a concurrent-generation protocol or measured Saturn speedup. |
| MCU-CUE-001; `60d5df01` | [7fcd2c6](https://github.com/papsg/SAROO/commit/7fcd2c6); [codex/cue-parser-safety](https://github.com/papsg/SAROO/tree/codex/cue-parser-safety) | Ten actual parser/raw/TOC/loader/FatFs methods /85 scenarios; baseline fails78, sanitizers/O2 pass. Eleven controls detected. All49 MCU methods /598 cases pass after a preserved/excluded discovery-extraction setup correction. Complete GNU App/RTX/166-vector link adds848 load bytes to60,440, static RAM unchanged; direct CUE frame136→160, raw32, cleanup40. Preserves99 tracks/2,048 extra indices and 64-bit FatFs while guarding 32-bit reader extents; no private32-track regression or later geometry implementation imported. [Sega section/source/FatFs citations, paps-g attribution, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#mcu-cue-001-bounded-cueraw-loading-and-reader-cleanup). Virtual wide metadata/close-after-real-close and aliased scratch are not physical recovery/concurrency/game qualification. |
| MCU-FS-001 geometry; `60d5df01` | [2eb30d0](https://github.com/papsg/SAROO/commit/2eb30d0); [codex/exfat-geometry-guards](https://github.com/papsg/SAROO/tree/codex/exfat-geometry-guards) | Four actual-FatFs methods/19 cases pass ASan/UBSan and O2; baseline has ten UBSan failures. Two omitted-bound controls fail four cases each. All49 coupled MCU methods/598 cases pass both profiles. Wide5GiB+129 and fragmented FAT32/exFAT workloads preserve markers/neighbors and I/O counts. Complete GNU App/RTX/166-vector link saves16 load bytes to60,424; static RAM and direct mount/seek frames unchanged. [Exact C11 section/pages, source contracts, paps-g attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#mcu-fs-001-reject-unsafe-exfat-geometry-shifts). Not full malformed-volume, original MDK or physical qualification. |
| MCU-FS-001 seek optimization; `60d5df01` | [ace316a](https://github.com/papsg/SAROO/commit/ace316a); [codex/fatfs-seek-range](https://github.com/papsg/SAROO/tree/codex/fatfs-seek-range) | Depends on separately guarded geometry. All19 actual-FatFs cases pass before/after in both profiles, with327,200 independent 64-bit comparisons and identical wide/fragmented file contents, neighbors and I/O counts. Three formula controls detected; all49 MCU methods/598 cases pass both profiles. GNU App/RTX/166-vector link removes two wide-division calls and saves848 load bytes to59,576, static RAM/direct seek frame unchanged. [Proof, source citations, paps-g attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#separate-evaluation-equivalent-wide-seek-comparison). No filesystem bug in old division or physical speedup claimed. |
| MCU-DISC-003; `cb3abf06` | [3b49134](https://github.com/papsg/SAROO/commit/3b49134); [codex/disc-header-reads](https://github.com/papsg/SAROO/tree/codex/disc-header-reads) | Six actual-source/FatFs methods/67 cases; baseline46 failures, sanitizers/O2 pass;12 controls detected. Includes paps-g8a98f5f6 final-sector verification with public64-bit FSIZE_t, excluding later media classification. All55 MCU methods/665 cases pass both profiles after correcting an old positive-preview-allocation expectation; initial failed runs retained/excluded. GNU App/RTX/166-vector link adds96 load bytes to59,672, static RAM unchanged; CUE direct frame160→152. [Exact ECMA sections/pages, FatFs/source citations, attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#mcu-disc-003-read-the-declared-tracks-complete-user-header). No mixed-width/gap/physical/concurrency/game qualification. |
| MCU-DISC-004; `cb3abf06` | [9ff69d6](https://github.com/papsg/SAROO/commit/9ff69d6); [codex/disc-reader-handoff](https://github.com/papsg/SAROO/tree/codex/disc-reader-handoff) | Nine coupled actual-C/FatFs methods/47 cases; baseline15 failures includes six modeled command-field publication interleavings. Sanitizers/O2 pass;11 controls detected. Narrowly includes paps-g c6dc9847 atomic idle/request retirement, with8f49d46a mask checks, excluding later retained-PLAY/directory protocol. All59 MCU methods/681 cases pass both profiles. Complete public check:168 Python methods,14 Rust tests, fmt/strict clippy and C checks. GNU App/RTX/166-vector link adds112 load bytes to59,784; static RAM/comparable direct frames unchanged, new pause/ack16/0. Complete menu rebuild exact. [Project/CMSIS citations, attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md#mcu-disc-004-acknowledge-reader-pause-before-explicit-replacement). No physical RTOS/IRQ/audio/SD, original MDK, broad concurrency or game qualification. |

All seven scoped finding IDs are complete in nine independently verified
source checkpoints. Final public main is
[9ff69d6](https://github.com/papsg/SAROO/commit/9ff69d6); all nine topic branches
and main are pushed. No scoped candidate is queued. The complete public check
passes168 Python methods,14 Rust tests, formatting, strict clippy and authored
C checks. Actual MCU coverage is59 methods/681 scenarios under ASan/UBSan and
O2. Final complete MCU/menu target builds pass with bound source hashes.

Relative to starting main6135642, menu code/data grows48 bytes and MCU load
grows52 bytes; static RAM remains unchanged. MCU load is59,784 with5,752 ITCM
free. Menu code/data/BSS/reserved stack are78,544/9,232/16,384, body margin26,912
and image488,751 bytes. These are GNU/menu link and source/host results;
original MDK, transitive/IRQ peaks, physical timing/durability and games remain
unqualified. The documented mixed-width/gap, shared-workspace, media/reset/file
recovery and broader command-ownership limits remain outside this series.

Fork default remains main, original v0.8 baseline ancestry and preserved master
are retained. Issues remain disabled and no open fork PR needs closure; no
upstream request was modified. Public tests/citations/attribution are maintained
in [doc/DISC-CUE-SAFETY.md](https://github.com/papsg/SAROO/blob/main/doc/DISC-CUE-SAFETY.md).
Detailed baseline/failed/setup/mutation/build evidence remains in the public
checkout's ignored artifact cohorts. Private sources/branches/stashes/caches,
installed firmware, Saturn and other paused work remain untouched.

## Save and storage follow-up

Starts at public `9ff69d6`. Direct verified source commits, with no PRs, USB,
added runtime diagnostics, private framework or device operations. Private
sources/history/evidence are read-only. Original findings guide a fresh
investigation; historical qualification is not reused.

| Candidate / original reference | Public checkpoint | Dependencies, evidence and limits |
| --- | --- | --- |
| SAVETOOL-007 clearing; `99127a6a` | [6450e93](https://github.com/papsg/SAROO/commit/6450e93); [codex/save-export-clearing](https://github.com/papsg/SAROO/tree/codex/save-export-clearing) | Uses completed validated-reader/output work. Three methods:100 poisoned exports plus8 BUP round trips match complete independent files. Baseline passes bytes but fails50 reduced-work assertions; ASan/UBSan/O2 pass. Three controls detected. All50 native methods pass with sanitizer and O2 CLIs (direct fault helpers retain sanitizers). Clear work falls32KiB/1MiB→64 bytes; native ARM64 O2 text+20 to15,612, zero-fill1,082,580 and direct exporter frames224/208/224 unchanged. [Format/request/source citations, paps-g attribution, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-STORAGE-FOLLOWUP.md#savetool-007-clear-only-the-exported-header). Firmware unchanged; no timing/hardware claim. |
| SAVETOOL-007 discarded CRC; `99127a6a` | [b5ae0bd](https://github.com/papsg/SAROO/commit/b5ae0bd); [codex/save-export-crc](https://github.com/papsg/SAROO/tree/codex/save-export-crc) | Separately verified after clearing. Same100 poisoned exports/8 BUP round trips pass ASan/UBSan/O2; baseline bytes pass but25 BUP-work assertions fail. SRO retains one CRC overN+48 bytes; BUP drops one discarded scan tozero. Restored BUP work and missing SRO CRC controls fail25 each. All50 native methods pass both CLI profiles. ARM64 O2 text+20 to15,632; static storage/direct frames unchanged. [Citations, paps-g attribution, public commands and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-STORAGE-FOLLOWUP.md#separate-evaluation-skip-only-the-discarded-bup-crc). Firmware unchanged; no timing/hardware claim. |
| MCU-SD-001; `2ba4e96b` | [f2b44e7](https://github.com/papsg/SAROO/commit/f2b44e7); [codex/disk-semaphore-safety](https://github.com/papsg/SAROO/tree/codex/disk-semaphore-safety) | Four actual-diskio methods/18 API scenarios; baseline9 failures, ASan/UBSan/O2 pass; seven controls detected. Includes explicit uninitialized-I/O refusal and all six interface error values (not all asserted reachable from RTX). 1000 initialization calls request1000 objects before/one after. Full GNU App/RTX/166-vector link adds32 load bytes to59,816, static RAM and direct frames0/8/32/32 unchanged. Unmodified CMSIS V2.1.3 header/license vendored for tests only. [CMSIS/FatFs/source citations, paps-g attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-STORAGE-FOLLOWUP.md#mcu-sd-001-require-ownership-before-sd-transfers). Not card presence, concurrent remount, physical RTOS/DMA/SD, original MDK or game qualification. |
| SATURN-SAVE-004; `f5ed0ae1` | [19de4f3](https://github.com/papsg/SAROO/commit/19de4f3); [codex/backup-date-correctness](https://github.com/papsg/SAROO/tree/codex/backup-date-correctness) | Nine actual-module methods/311,712 independent datetime records plus256 weekday inputs, seven old-valid field controls and separate system delegation. Baseline fails7 methods while valid controls pass; ASan/UBSan/O2 pass; eight controls detected. Gregorian1980–2235 and invalid/null/range semantics are explicit policy, not exact BIOS behavior; no date migration. All31 firmware save and59 MCU methods pass both profiles; complete public check184 Python methods/14 Rust tests, fmt/strict clippy/C checks. Full menu link adds276 code/data to78,820; BSS 9,232/stack 16,384 unchanged, margin26,636, direct decode/encode24→40/20→36. MCU inputs unchanged. [Exact Sega sections/pages, pinned quirk references, paps-g attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-STORAGE-FOLLOWUP.md#saturn-save-004-bounded-calendar-conversion). No BIOS/game, physical timing/SD or original MDK qualification. |

All three scoped finding IDs are complete in four verified source commits.
Final main is [19de4f3](https://github.com/papsg/SAROO/commit/19de4f3); all four
focused branches/main are pushed. No scoped candidate remains queued.

The complete offline public check passes184 Python methods,14 Rust unit tests,
formatting, strict clippy and authored C checks. Native save50 methods pass with
sanitizer and O2 CLIs; firmware save31 and coupled MCU59 methods pass both
profiles, as do the separate18 actual-disk-I/O cases. Current source hashes
match complete menu and GNU App/RTX/166-vector receipts. Detailed failed/setup
runs, inputs, negative controls and acceptance records are preserved in public
ignored cohorts; public tests/commands and the linked document are reproducible.

From starting9ff69d6, native text grows40 bytes to15,632 with1,082,580 zero-fill
bytes/direct frames unchanged. MCU load grows32 bytes to59,816, leaving5,720
ITCM free with unchanged static RAM. Menu code/data grows276 bytes to78,820;
BSS/stack stay9,232/16,384 and body margin is26,636. These are measured work,
source and link results, not hardware timing, RTOS heap, peak stack, SD durability,
BIOS/game or original MDK qualification. Calendar/invalid-input compatibility
policies and serialized initialization/media/recovery limits remain explicit.

Default main, baseline ancestry and preserved master remain intact. Fork issues
remain disabled and no open PR needs closure; no upstream request was modified.
No PR, USB integration, runtime diagnostic, private framework or unrelated work
was introduced. Private tracked state, all refs and stashes match the initial
snapshot; no private preparer/test or device operation ran. Private files/caches,
Saturn, installed firmware and other paused work remain untouched.

## Cover and artwork safety

Starts at public `19de4f3`. Revalidate original findings and dependencies;
menu consumer precedes the MCU producer. Direct verified source commits;
no PRs, USB, runtime diagnostics, private framework or device operations.

| Candidate / original reference | Public checkpoint | Dependencies, evidence and limits |
| --- | --- | --- |
| SATURN-MENU-003; `681387bc` | [0812ccd](https://github.com/papsg/SAROO/commit/0812ccd); [codex/cover-display-ownership](https://github.com/papsg/SAROO/tree/codex/cover-display-ownership) | Consumer prerequisite. Four actual-display methods/11 scenarios; baseline 10 failures, non-VBlank control passes, sanitizers/O2 pass. Four controls fail 10/2/2/2. Public refinement uses selected stride in upward clipping; GIF stride retained. Complete SH-2 link adds 16 code/data to 78,836; BSS/stack/direct frame unchanged, margin 26,620. [Exact Sega DMA sections/pages, paps-g attribution, source contracts, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/COVER-ARTWORK-SAFETY.md#saturn-menu-003-preserve-frame-ownership-through-final-dma). Synthetic asynchronous DMA is not bus/cache/VDP timing, physical-hang or game qualification. |
| MCU-COVER-001; `681387bc` | [87ddb00](https://github.com/papsg/SAROO/commit/87ddb00); [codex/cover-cache-safety](https://github.com/papsg/SAROO/tree/codex/cover-cache-safety) | Requires 0812ccd. Seven actual-C/FatFs/catalog/display methods/55 scenarios; baseline 47 failures/eight valid controls, sanitizer/O2 pass. Four mutations fail 7/6/3/1. All 66 MCU regressions pass; complete 166-vector GNU qualification link passes, load 59,812/static DTCM 61,112. Cache 5,118 bytes; explicit scan invalidation and bounded ownership wait. [Citations, paps-g attribution, costs and limits](https://github.com/papsg/SAROO/blob/main/doc/COVER-ARTWORK-SAFETY.md#mcu-cover-001-bound-catalog-ownership-and-publish-complete-artwork). No physical/game/MDK qualification; later MCU capacity expansion remains separate. |
| COVERTOOL-001; `2231f4b5`, required protected-file refinement `3151c6f5` | [f7d7299](https://github.com/papsg/SAROO/commit/f7d7299); [codex/native-cover-safety](https://github.com/papsg/SAROO/tree/codex/native-cover-safety) | Seventeen methods, 102 functional/metadata invocations and 39 fault cases pass sanitizer/O2; selected baseline has 53 failed assertions/two filename errors. Hash-only/protected-copy controls fail 1/9 cases. Seven browser/native round trips and original native Makefile build pass; browser product source unchanged. Native text 3,044→8,364 B, direct main stack 524,688→8,624 B; full tiny-image heap increases to 393,216 B and is documented. [Bucanero/paps-g attribution, C/BMP/source/system-call citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/NATIVE-COVER-SAFETY.md). Per-file publication and manifest-last behavior do not establish whole-directory rollback or power-loss qualification. |

Series complete at **f7d7299**. All three candidate IDs have verified direct
source commits on main; topic/main checkpoints are pushed. Final integrated
suite passes **212 Python methods**, authored C checks, **14 Rust tests**, fmt
and clippy. Complete MCU/menu build receipts match all 56/36 current tracked
source inputs respectively. Firmware load/body/image and native allocation costs
are documented at the linked checkpoints; models are not hardware qualification.

The fork's issue tracker is disabled and there are no open fork PRs, so no fork
counterpart needs closing. Original tpunix/SAROO requests remain unchanged.
Historical #215/#321/#332 tool features were already in v0.8, not imported again;
this series does not establish an individual commenter's root cause.

Private HEAD/status/all refs/four stashes match the starting snapshot. No private
preparer/build/test, Saturn, USB, power/capture, runtime probe, new PR or subagent
was used. All new evidence is preserved under the public repository's ignored
`.git/porting/evidence/cover-artwork-safety/`, and tests retain their own copied
inputs/logs under `tests/build/`. Baseline/master/default branch are preserved.
Later MCU capacity/configuration/palette/GIF work remains outside this series.

## Configuration safety

Starts at public `f7d7299`. Independently revalidate the two candidates, oldest
first, as verified direct source commits. No PRs, USB, added runtime diagnostics,
private writes or Saturn operations. Review complete proposed public files and
commit metadata/messages for secrets/private PII before publication; use public
handles, noreply identity, synthetic fixtures and portable paths. Keep raw
reference/build evidence local and ignored; publish sanitized summaries.

| Candidate / original reference | Public checkpoint | Dependencies, evidence and limits |
| --- | --- | --- |
| MCU-CONFIG-001; paps-g `3e072573` | [c7a4f3d](https://github.com/papsg/SAROO/commit/c7a4f3d); [codex/config-id-safety](https://github.com/papsg/SAROO/tree/codex/config-id-safety) | Five actual-C methods, 33 processes / 5,362 pairs / 10,724 calls. Baseline 14 failures; sanitizers/O2 pass. Three omission controls fail 4/3/5 scenarios. Complete GNU MCU/RTX/166-vector link passes, load 59,844 (+32), static DTCM 61,112/direct frames unchanged. [Exact C/POSIX citations, attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/CONFIGURATION-SAFETY.md#mcu-config-001-bounded-id-matching-with-preserved-version-rules). Public source/metadata/message pass manual review, secret scan and private-PII checks. No boot/game/physical/MDK qualification. |
| MCU-CONFIG-002; paps-g `0dc69582` | [19bbd76](https://github.com/papsg/SAROO/commit/19bbd76); [codex/config-loader-safety](https://github.com/papsg/SAROO/tree/codex/config-loader-safety) | Requires c7a4f3d. Twelve actual-parser/startup methods / 177 invocations and eleven actual-parser/disc/save/FatFs methods / 55 scenarios pass sanitizers/O2. Selected baselines fail 90 assertions / eight coupled scenarios; seven controls fail 2/3/4/1/6/1/3. Shipped example passes global plus all 30 game selections. Actual callers stop/release readers before save admission; pending retry data and accepted-default reset are checked. Complete GNU MCU/RTX/166-vector link uses 59,684 load / 60,792 static DTCM bytes; full menu bytes unchanged. Public adaptations remove premature multi-disc clear and handle a closing bracket inside trailing comments. [C/FatFs/project citations, attribution, commands, policies, costs and limits](https://github.com/papsg/SAROO/blob/main/doc/CONFIGURATION-SAFETY.md#mcu-config-002-validate-owned-snapshots-before-applying-settings). Historical USB/error-position/SD tracing omitted. Complete proposed files and metadata/messages pass manual and automated secret/private-PII review. No hardware/game/MDK qualification. |

Both scoped IDs are complete in two verified source checkpoints. Final public
main is **19bbd76**; both topics and main are pushed. The integrated suite passes
**231 Python methods**, authored C checks, **14 Rust tests**, fmt and clippy.
Actual MCU fixtures cover **68 methods / 744 scenarios**. Relevant parser and
coupled disc/save checks also pass O2. Full MCU/menu receipts match all 56/36
current source inputs respectively.

From starting f7d7299, MCU load falls 128 bytes to 59,684 and static DTCM falls
320 bytes to 60,792; menu code/data/BSS/stack/image are unchanged. Parsing adds
validation work: the measured ordinary and full-command resident host workloads
are slower, while the full-comment snapshot improves. No physical performance,
boot/game, original MDK, media recovery or general concurrency claim is made.
Bounds/compatibility policies, ownership assumptions and close-error-model limits
remain explicit in the linked document. Later shrink and unrelated work remain
outside this series.

Every proposed publication is manually reviewed and scanned for secrets/private
PII, including complete files, messages and public noreply identities. Raw
reference/build logs remain local and ignored; public inputs are synthetic or
already shipped by the public repository. No private contacts, personal paths,
private network details or raw private configuration is introduced.

The fork issue tracker is disabled and no open fork PR needs closing; original
upstream requests are unchanged. Default main, baseline ancestry and preserved
master remain intact. Private HEAD/status/all refs/four stashes exactly match
the starting snapshot; no private preparer/build/test/cache writes, Saturn,
USB/device/power/capture, subagent, new PR or other-goal work occurred.
Detailed source, failures, models, mutations, build/cost receipts and privacy
reviews are preserved under public ignored evidence/test cohorts.

## Menu palette and timing safety

Starts at public `19bbd76`. Independently revalidate SATURN-MENU-001 and
SATURN-MENU-004, oldest first, as direct verified source commits. No PRs,
USB, added diagnostics, private writes or device operations. Every proposed
publication receives complete-file/message/metadata secret and private-PII
checks plus manual review; raw evidence stays local and ignored.

| Candidate / original reference | Public checkpoint | Dependencies, evidence and limits |
| --- | --- | --- |
| SATURN-MENU-001; paps-g `9d0cf199` | [7c97088](https://github.com/papsg/SAROO/commit/7c97088); [codex/menu-palette-safety](https://github.com/papsg/SAROO/tree/codex/menu-palette-safety) | Four actual-C methods pass sanitizer/O2; baseline three failures at palette entry 16, logo-only control passes. Tail omission and cover-bank overclear each fail three cases. Complete menu link adds 16 code/data to 78,852; BSS 9,232/stack 16,384/direct initialization frames 4/4 unchanged, margin 26,604. Image/font sizes unchanged; MCU source untouched. [Exact Sega/C/source citations, public attribution, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/MENU-PALETTE-TIMING-SAFETY.md#saturn-menu-001-initialize-the-defined-palette-without-overreading). Source bounds, bytes and register intent are qualified; no VDP raster, logo-duration, physical/game/freeze claim. |
| SATURN-MENU-004; paps-g `90db7fe2` | [24c5c73](https://github.com/papsg/SAROO/commit/24c5c73); [codex/menu-timing-safety](https://github.com/papsg/SAROO/tree/codex/menu-timing-safety) | Seven actual-C timer methods / 21 cases and one controller-to-repeat method / ten cases pass sanitizer/O2. Baselines fail13/10; existing carry, no-overflow reset and four scroll controls pass. Eight targeted mutations rejected; register reference order/retry and target byte-access order checked. Deduplicates public carry/helper from86c7caf; current controller code unchanged. Complete menu adds32 code/data to78,884; BSS9,232/stack16,384/direct frames0/4/12/8 unchanged, margin26,572. Image/font sizes unchanged. [Hitachi/C/source citations, public attribution, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/MENU-PALETTE-TIMING-SAFETY.md#saturn-menu-004-preserve-repeat-intervals-and-short-deadlines-across-wrap). Inherited clock polling/epoch/half-range limits remain; no physical timing, game or freeze qualification. |

Both candidates are complete as separate direct source commits, pushed with
their topic branches to main in oldest-first order. Complete public regression
validation passes243 Python methods in21 groups,14 Rust checks and native C
checks; all palette/timer/integration controls also pass O2. The complete menu
link adds48 code/data bytes across the series with unchanged BSS/stack/image/font
sizes; timing removes one existing warning (26→25), adding none. All36 menu
build inputs match source; all56 MCU project inputs match the unchanged prior
MCU build receipt. Full source/cost/limit records are linked above.

Complete proposed files/messages/metadata and actual committed contents were
reviewed; secret scans and private-PII checks have zero findings. Only approved
public contributor handles/noreply identities are published. Ignored evidence
retains baselines, controls, mutations, target disassembly/build receipts,
source identities, full-suite results and privacy reviews.

Public main is clean, default main and historical master are preserved, and
the v0.8 baseline remains an ancestor. Private head/status/refs/four stashes
match the starting snapshot; Saturn and other work remain untouched. No USB,
added runtime diagnostics, subagents, PRs or unrelated imports were used.
Fork issues remain disabled and there are no open fork PRs or corresponding
requests to close. No upstream request is edited; original notices remain.

## GIF safety and scheduling

Starts at public `24c5c73`. Port two September6 corrections, oldest first,
as separate verified source commits. No PRs, USB, added runtime diagnostics,
private writes or device operations. Full proposed publication and actual
commits receive manual/secret/private-PII review; raw evidence stays ignored.

| Candidate / original | Public checkpoint | Dependencies, verification and limits |
| --- | --- | --- |
| MCU-GIF-001; paps-g `3e62998c` | [dbafcd8](https://github.com/papsg/SAROO/commit/dbafcd8); [codex/gif-decoder-safety](https://github.com/papsg/SAROO/tree/codex/gif-decoder-safety) | Ten actual-C methods/130 cases pass sanitizers/O2; baseline126 failures and three preservation controls. Eight targeted mutations rejected; ImageIO matches39 independent first-frame RGB fixtures. Eleven coupled disc/save methods and integrated253 Python methods/14 Rust checks/C controls pass. Complete MCU/RTX/166-vector link: load59,796 (+112), ITCM margin5,740, static DTCM60,752 (-40), fixed scratch/dictionary20,480 (-8KiB), direct init/timer/exit112/24/8 unchanged, decode72→64. Complete menu bytes unchanged. Private integration/error logging omitted. [GIF/C/GCC/project citations, attribution, commands and limits](https://github.com/papsg/SAROO/blob/main/doc/GIF-SAFETY-SCHEDULING.md#mcu-gif-001-bound-decoding-and-release-the-disc-workspace). Source bounds/ownership checks do not qualify hardware, pacing/audio, full GIF89a/compositing, exact issue345 or games. |
| MCU-GIF-002; paps-g `f285ede8` | [d38e06a](https://github.com/papsg/SAROO/commit/d38e06a); [codex/gif-background-scheduling](https://github.com/papsg/SAROO/tree/codex/gif-background-scheduling) | Requires dbafcd8. Six methods/six native invocations plus a caller check pass sanitizers/O2;80 pinned RTX dispatcher cases. Baseline fails three methods; UART editing/history, BOOT/no-GIF/timing and priority controls pass. Five mutations rejected. Preserve the original UART shell via a nonblocking zero-timeout idle probe; one low-priority owner, no new allocation, same effective priorities. BOOT editor tokens and startup/UART source are unchanged; linked shell has GIF/probe/yield, disk has no GIF call. Original remaining-pixel/sentinel simplifications retain decoder bounds. Complete MCU link load59,780 (-16), margin5,756; static DTCM60,752, fixed scratch20,480/direct frames unchanged. Pinned Arm test extraction retains Apache2.0/copyright and verified public provenance. [Requirements, attribution, costs and limits](https://github.com/papsg/SAROO/blob/main/doc/GIF-SAFETY-SCHEDULING.md#mcu-gif-002-service-animation-from-the-existing-low-priority-task). Up to45 extra idle wakeups/second; long UART commands can postpone GIF service. No private USB reload/acknowledgment or diagnostic code imported. |

Both scoped IDs are complete as two verified direct source commits, pushed
with their topic branches to main in oldest-first order. The final integrated
suite passes259 Python methods,14 Rust checks and native C controls; all16 GIF
methods also pass uninstrumented O2. Complete MCU/RTX/166-vector and menu build
receipts match56/36 current project inputs. The menu remains byte-identical.
Across this series MCU load adds96 bytes, static DTCM saves40 and the reserved
scratch/dictionary footprint saves8 KiB. All resource and timing limits remain
explicit; no hardware/pacing/audio/game/original-MDK qualification is claimed.

Full proposed files/messages/metadata and actual committed contents pass manual,
secret and private-PII review with zero findings. Only approved public handles,
noreply identities, synthetic inputs and sanitized summaries are published;
original notices and pinned Arm dependency licensing are retained. Ignored
local evidence preserves baseline/control/mutation results, source identities,
ImageIO limits, exact vendor extraction, builds/disassembly, BOOT-token checks,
full-suite results and privacy reviews.

Main is clean; default main, baseline ancestry and historical master are
preserved. Private head/status/all refs/four stashes match the starting snapshot.
Saturn, other work and installed firmware remain untouched. No USB, device/SSH/
power/capture, new diagnostics, subagents or unrelated imports occurred. The
fork tracker remains disabled with no open PRs or matching requests to close;
original upstream requests remain unchanged. Issue345's exact assets and
physical behavior are still unqualified, so no blanket closure is claimed.

## MCU shrink series

Starts at public `d38e06a`. Independently port three selected MCU-SIZE-001
checkpoints, oldest first; measure current code instead of reusing private
savings. No PRs, USB, diagnostic-name packing/log changes, constant packing,
private writes or device operations. Each publication receives complete-file,
message/metadata secret/private-PII review; raw evidence remains ignored.

| Selected contribution / original | Public checkpoint | Validation, measured cost and limits |
| --- | --- | --- |
| Unused timer callback service; paps-g `1b49c143` | [2eb80d0](https://github.com/papsg/SAROO/commit/2eb80d0); [codex/unused-timer-service](https://github.com/papsg/SAROO/tree/codex/unused-timer-service) | Five actual-source methods pass sanitizer/O2; baseline policy check fails with required timing and enabled-worker controls passing. Three mutations rejected;16 GIF and disk-I/O regressions pass. Complete App/RTX/166-vector link saves1,884 load bytes to57,896, margin7,640; static DTCM saves456 to60,296, with dedicated objects256+68+52+80 removed.291 shared application direct frames and tick/delay function sizes120/224 unchanged. Standard shared RTE configuration selects supported zeros; SDK/provider selection unchanged, private exclusion unnecessary. Future timer users must restore profile; no API-link rejection or hardware/MDK/stack/pacing/game claim. [Exact Arm/project citations, attribution, tests and limits](https://github.com/papsg/SAROO/blob/main/doc/MCU-SHRINK.md#unused-software-timer-callback-service). |
| Shared CD setup; paps-g `96f9329a` | [fed7c6f](https://github.com/papsg/SAROO/commit/fed7c6f); [codex/cd-setup-sharing](https://github.com/papsg/SAROO/tree/codex/cd-setup-sharing) | Requires2eb80d0. Three actual-C methods:41,474 transfer+55,296 directory differential cases and seven independent contracts pass sanitizer/O2; preserved current-source baseline controls pass. Five mutations fail two methods each, including filter publication before wakeup. Full App/RTX/166-vector link saves288 load bytes to57,608, margin7,928; static DTCM60,296 unchanged. Shared helper frames48/16 and wrappers/callers make selected paths add8–16 stack bytes; reserved stacks unchanged. Existing logs/masks/order preserved through SSLOG; no diagnostics added. [Exact Sega sections/functions, paps-g/original attribution, supported domains and limits](https://github.com/papsg/SAROO/blob/main/doc/MCU-SHRINK.md#shared-cd-setup). No new CD validation/ownership/completion fix or physical timing/stack/game qualification. |
| ASCII .cue/.iso matching; paps-g `96f9329a` | [2664d51](https://github.com/papsg/SAROO/commit/2664d51); [codex/ascii-image-suffix](https://github.com/papsg/SAROO/tree/codex/ascii-image-suffix) | Requires fed7c6f.41,130 independent spelling/boundary checks pass sanitizers/O2; four mutations rejected. Seven real-FatFs baseline/candidate discovery methods and seven candidate cover methods pass. Complete App/RTX/166-vector link saves260 load bytes to57,348, margin8,188; static DTCM60,296 unchanged. General comparison and257-byte ctype table removed.293 common application direct frames unchanged; new helper frame8. [Public scanner/C11 citations, attribution, costs, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/MCU-SHRINK.md#exact-ascii-image-suffixes). Full UTF-8 filenames, final-dot/CUE preference, errors and bounded paths retained; no physical speed/stack/game qualification. |

All three checkpoints are complete as separately verified direct source commits,
pushed with their topics to main in oldest-first order. The integrated suite
passes268 Python methods in25 groups,14 Rust checks and native C controls.
All nine new shrink methods also pass uninstrumented O2; current-source hashes
match their passing sanitizer/O2 fixture cohorts. Baseline/valid controls and
12 targeted mutations are retained with build/source/privacy evidence locally.

MCU load falls59,780→57,348 (**2,432 bytes saved**), leaving8,188 ITCM bytes;
static DTCM falls60,752→60,296 (**456 bytes saved**). Required task stacks,
allocator pool, disc workspace and filesystem capabilities are preserved.
Selected helper stack/call costs remain explicit above. Complete menu image/ELF
and allocations are identical;56 MCU/36 menu build inputs match their receipts.
No physical speed/stack/timing/audio, game or original-MDK qualification is
inferred from source, models or links.

Complete files/messages/metadata and actual committed content pass manual,
secret and private-PII review with zero findings. Public contributor handles
and noreply identities preserve attribution; original source/dependency/font
notices remain. No private logs, paths, account/network details or credentials
are published. Primary/source citations and reproducible checks are linked above.

Main is clean; default main, historical master and baseline ancestry remain.
Private head/status/all refs/four stashes match the initial snapshot. Private
repo, Saturn, installed firmware, research branches and other paused work remain
untouched. No USB, new runtime diagnostics, PRs, subagents or unrelated imports
were used. The fork's tracker is disabled with no open PRs or matching requests
to close; original upstream requests remain unchanged.

## File-transfer safety

Starts at public `2664d51`. Port four selected checkpoints from September6,
oldest first, with each contribution independently revalidated. No PRs, USB,
new diagnostics, private writes or device operations. Existing logs and public
APIs remain; raw evidence stays ignored. Full publications/messages/metadata
receive manual, secret and private-PII review before each commit/push.

| Contribution / original | Public checkpoint | Dependencies, validation and limits |
| --- | --- | --- |
| MCU-FILE-001; paps-g `52ba5498` | [a3959f4](https://github.com/papsg/SAROO/commit/a3959f4); [codex/file-command-completion](https://github.com/papsg/SAROO/tree/codex/file-command-completion) | Nine actual-C/FatFs methods/33 scenarios pass sanitizers/O2; baseline fails13,20 controls pass. Six mutants rejected;11 coupled disc/save and six GIF scheduling methods pass. Complete App/RTX/166-vector link saves16 load bytes to57,332, margin8,204; static DTCM60,296 unchanged. IRQ frame24; dispatcher112→8/helper120, active file caller frames136→152 before callees. Public tiny/nonreentrant FatFs retained; private mutex-timeout claim excluded. Original log text/masks unchanged. [Exact public FatFs/source citations, attribution, tests/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/FILE-TRANSFER-SAFETY.md#mcu-file-001-report-positioning-and-completion-errors). Reports failure without rollback, retained-handle recovery, durability or physical/game qualification. |
| MCU-FILE-002 MCU bounds; paps-g `66c197b3` | [e1e6fdb](https://github.com/papsg/SAROO/commit/e1e6fdb); [codex/file-command-bounds](https://github.com/papsg/SAROO/tree/codex/file-command-bounds) | Requires a3959f4.12 methods/61 actual-C/FatFs scenarios pass sanitizers/O2; preceding baseline fails16 invalid cases and passes45 controls. Six mutants rejected. Full App/RTX/166-vector link adds64 load bytes to57,396, margin8,140; static DTCM60,296 and IRQ/dispatcher/helper direct frames24/8/120 unchanged. Actual addresses and64-bit types checked. Two injected wide-size cases are not real multi-gigabyte media. [Exact FPGA/FatFs/project citations, attribution, costs and limits](https://github.com/papsg/SAROO/blob/main/doc/FILE-TRANSFER-SAFETY.md#mcu-file-002-mcu-request-bounds). Legacy producer/allocation, RAM folding/cache, reset/concurrency and atomicity remain separate. |
| SATURN-WORD-001; paps-g `4df2e67a` | [ae59ff3](https://github.com/papsg/SAROO/commit/ae59ff3); [codex/menu-word-decoding](https://github.com/papsg/SAROO/tree/codex/menu-word-decoding) | Two actual-helper methods pass sanitizers/O2:8,256 full-width and4,128 baseline-valid checks across byte orders/four alignments. Both old readers fail UBSan, low-byte controls pass; two revert mutations detected. Nine existing binary-selection cases pass. Full menu image/ELF hashes, code/data78,884, BSS9,232, stack16,384, assets and frames unchanged;36 menu/56 MCU inputs match their receipts. [Exact C11/public-source citations, attribution and limits](https://github.com/papsg/SAROO/blob/main/doc/FILE-TRANSFER-SAFETY.md#saturn-word-001-unsigned-word-decoding). Source semantics/portability, with no physical/game/speed claim. |
| MCU-FILE-002 menu continuation; paps-g `4df2e67a` | [de549dc](https://github.com/papsg/SAROO/commit/de549dc); [codex/menu-file-capacity](https://github.com/papsg/SAROO/tree/codex/menu-file-capacity) | Requires ae59ff3; current MCU bounds retained, with original-peer compatibility checked.30 actual menu/MCU/FatFs transfers and40 loader cases pass sanitizers/O2; preceding menu fails15/21 with15/19 controls passing. All30 also pass against original MCU; seven discovery/seven cover methods pass. Nine meaningful mutations rejected, one incomplete name-loop variant preserved unqualified. Existing catalog checks/controller suspend→jump→resume retained. Complete menu adds688 code/data to79,572; BSS9,232, stack16,384, assets/image sizes unchanged. BIN caller sum164→220 before utilities/IRQs; MCU unchanged. [Exact Sega §1.2 Figures1.3/1.5 printed4/6, project/FPGA citations, attribution and limits](https://github.com/papsg/SAROO/blob/main/doc/FILE-TRANSFER-SAFETY.md#mcu-file-002-capacity-aware-menu-and-binary-loading). Legacy four-argument slots preserved, tagged bounded reader added; no arbitrary-allocation/coherence/reset/atomicity/hardware qualification. |

All four scoped checkpoints are complete as individually verified linear source
commits, pushed with their topics to main in oldest-first order. The integrated
suite passes291 Python methods in28 groups,14 Rust checks and native C controls.
All24 focused methods, including the expanded40-case BIN-loader fixture, also
pass uninstrumented O2. Passing sanitizer/O2 artifacts match current sources;
the older binary fixture lacks compiler-argv capture, so its explicit release
run and linked sanitizer dependency are recorded separately. Baselines, valid
controls,23 detected mutations and the unqualified initial name mutant are
preserved alongside source/build/qualification/privacy evidence locally.

The complete GNU MCU App/RTX/166-vector image grows48 load bytes across this
series,57,348→57,396, leaving8,140 ITCM bytes; static DTCM remains60,296. Complete
menu code/data grows688,78,884→79,572, with BSS9,232, reserved stack16,384,
font357,679 and padded image488,751 bytes unchanged.56 MCU/36 menu input hashes
match their receipts. Helper/caller stack and occasional EOF-probe costs remain
explicit above. Source/model/build checks are not original-MDK, physical stack,
coherence/timing/audio, game or device qualification.

MCU-FILE-001 now reports its scoped failures; MCU-FILE-002's transferred checks
are complete while legacy allocation/ownership limitations remain. This does
not resolve later cache/RAM folding, reset/wait/reentrancy, atomicity or pending-
file recovery findings. No named game/audio/save-loss issue is closed by analogy.
The private mutex-timeout reproduction is excluded from standard-public claims.

Full proposed files/messages/metadata and actual commits pass manual, secret
and private-PII review with zero findings. Attribution uses approved public
handles/noreply identities; exact primary/source citations and reproducible
checks are linked above. Original source/FatFs/font notices remain; no private
paths/accounts/network details, credentials, raw logs or build artifacts are
published. Ignored evidence retains every applicable control and stated limit.

Main is clean; default main, historical master and baseline ancestry remain.
Private head/status/all refs/four stashes match the initial snapshot. Private
repo, Saturn/installed firmware, research branches, other work and paused goals
remain untouched. No USB, added runtime diagnostics, PRs, subagents or unrelated
imports were used. Fork issues are disabled, with no open PRs or matching
requests to close; original upstream requests remain unchanged.

## Initial CD-command and buffer safety

Starts at public `de549dc`. Port MCU-CDC-001–004 as separately verified source
commits, oldest first; include accepted-software-reset error clearing with002.
Keep005–007/later work separate. No PRs, USB, added diagnostics, logging changes,
private writes or device operations. Raw evidence remains ignored; complete
files/messages/metadata and actual commits receive privacy review before push.

| Contribution / original | Public checkpoint | Verification, costs and limits |
| --- | --- | --- |
| MCU-CDC-001; paps-g `f0da2b0b` | [1d9a126](https://github.com/papsg/SAROO/commit/1d9a126); [codex/cd-filter-reply](https://github.com/papsg/SAROO/tree/codex/cd-filter-reply) | Three actual-code methods pass sanitizers/O2:71,680 reply cases,6,144 valid zero-status controls,6,144 actual set/get round trips. Baseline full sweep has71,400 byte failures and round-trip fails; zero-status control passes. Four mutations rejected; three prior CD setup methods pass. Complete App/RTX/166-vector link adds16 load bytes to57,412, margin8,124; static DTCM60,296 and IRQ/dispatch/getter frames24/16/8 unchanged. No allocation/copy/I/O/call/mask work added. [Exact Sega §8.2.5 functions5.3/5.4, public source contracts, attribution, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-COMMAND-BUFFER-SAFETY.md#mcu-cdc-001-separate-filter-reply-status-and-channel). Getter IRQ return0 preserved; encoding sweep is not256 physical states, invalid selectors/hardware timing/game acceptance remain separate. |
| MCU-CDC-002; paps-g `93391efb` plus reset continuation in `8432f7d8` | [757e7fb](https://github.com/papsg/SAROO/commit/757e7fb); [codex/cd-copy-error](https://github.com/papsg/SAROO/tree/codex/cd-copy-error) | Four actual-code methods/eight cases pass sanitizers/O2: real allocation exhaustion/partial-copy integrity, repeated/unrelated-query retention, successful replacement and accepted-reset clearing with ignored/nonreset controls. Six native baseline cases fail; power-on/success controls pass. Four mutations rejected. Full MCU adds32 load bytes to57,444, margin8,092; static DTCM60,296 and direct IRQ/dispatch/init/getter frames24/16/24/0 unchanged. No added payload copy/allocation/I/O/call/mask work. [Sega §8.2.7 function7.8, §5.5 Table5.2, §8.2.1 function1.7 and enum evidence; attribution/reproduction/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-COMMAND-BUFFER-SAFETY.md#mcu-cdc-002-retained-copymove-error-and-software-reset). Admission/accounting remain003; host scheduling is not hardware/game qualification. |
| MCU-CDC-003; selected paps-g `8432f7d8` | [c7da19b](https://github.com/papsg/SAROO/commit/c7da19b); [codex/cd-buffer-ownership](https://github.com/papsg/SAROO/tree/codex/cd-buffer-ownership) | Scoped port complete; broader finding remains partial. Eleven actual-code methods/nineteen cases pass sanitizers/O2: selector/range/sentinel/transfer/capacity admission, partial exhaustion/outcome/reset,72 zero-copy move shapes including full/same pools, eight copy shapes and six removal indices with independent payload/metadata/identity/byte/free/list oracles. Sixteen baseline cases fail; three controls pass. Seven targeted mutations rejected; existing log formats retained. Full MCU adds32 load bytes to57,476, margin8,060; static DTCM60,296 unchanged. Shared direct frame56 versus48, wrappers0, append8 versus0, removal8 versus16; no workspace growth. [Sega §5.3.4(4), §8.2.7 functions7.6–7.8; attribution, admission policies, costs and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-COMMAND-BUFFER-SAFETY.md#mcu-cdc-003-buffer-admission-accounting-and-existing-block-moves). Destination aperture routing005–007, cancellation/erase/output ownership and physical concurrency remain separate; no blanket command/game qualification. |
| MCU-CDC-004; paps-g `661fd2d4` | [b686232](https://github.com/papsg/SAROO/commit/b686232); [codex/cd-filter-bounds](https://github.com/papsg/SAROO/tree/codex/cd-filter-bounds) | Twelve actual-source methods pass sanitizers/O2: invalid/disconnected selectors,300 cycles,576 full-length routes, full200-block pool/retry, payload/metadata/byte/free/list ownership and original-log controls. Baseline fails eight groups and times out in one; three valid groups pass. Seven mutations rejected. Preserve original predicates, format conversions and false-disconnect log; publish last destination only with append, discard0xff, retry retains completed state. Full MCU adds64 load bytes to57,540, margin7,996; static DTCM60,296 and disk-task/router direct frames80/32 unchanged. No workspace/allocation/copy/I/O added. [Sega §5.2/5.3.1–5.3.3 and §8.2.4 function4.3, attribution/policies/reproduction/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-COMMAND-BUFFER-SAFETY.md#mcu-cdc-004-bounded-incoming-filter-traversal). Invalid/cyclic fallback and retry policy are not hardware timing/concurrency qualification; later metadata/connector/routing work remains separate. |

The first verified source checkpoint and topic are pushed directly to main.
All four scoped source checkpoints are complete and published, ending at
[b686232](https://github.com/papsg/SAROO/commit/b686232). Final regression coverage
passes317 Python methods and14 Rust tests: the full `make -C tests check` run
passes305 methods; the final26-method CD target reruns its14 existing methods
and adds the12 filter methods. That complete CD target also passes O2. The filter
recipe was added after the full make had parsed its recipes; these separate logs
are counted without double-counting. Current-source hashes bind all three final
host cohorts in both profiles and56 MCU/36 menu inputs to their receipts.

Final GNU App/RTX MCU is57,540 load bytes,7,996 ITCM free and60,296 static DTCM:
144 load bytes added across the four ports with no static-span growth. The menu
image/hash/resources are exactly unchanged:79,572 code/data,9,232 BSS,16,384
reserved stack,357,679 font and488,751 total image bytes. Required MCU stacks,
workspace and pool capacities remain. Direct-frame and masking tradeoffs are
recorded per checkpoint; host models are not physical stack/concurrency/timing
or game qualification.

Full proposed files/messages/public identities and actual commit blobs were
reviewed/scanned before publication; no secret/private-PII finding remains.
Public main is clean, all four topic refs and main are published, v0.8 remains
an ancestor, default main and preserved historical master are verified. Exact
private HEAD/full status/refs/stashes match the start snapshot. No Saturn access,
private write, PR, USB integration, new runtime diagnostic or upstream change
was performed. No issue closure is applicable: fork issues are disabled and
there are no open PRs. Broader MCU-CDC-003 remains partially resolved; later
routing/metadata/host-PUT005–007, cancellation/erase/general ownership and
physical/game qualification remain outside this scoped goal.
Private repo, research refs/stashes, Saturn and other work remain untouched.

## CD routing and write safety

Starts at public `b686232`; selected paps-g `50100444`/`3f7bba1a` contributions
from September6. Four separate source ports, prerequisites explicit; public
actual-source evidence, exact citations, attribution and resource review are
required before each direct main/topic push. No PRs, USB, added runtime
diagnostics/log changes, private writes or device operations.

| Contribution / original | Public checkpoint | Validation, dependencies and limits |
| --- | --- | --- |
| MCU-CDC-005 metadata; selected paps-g `50100444` | [fc7a222](https://github.com/papsg/SAROO/commit/fc7a222); [codex/cd-producer-metadata](https://github.com/papsg/SAROO/tree/codex/cd-producer-metadata) | Five actual-producer methods pass ASan/UBSan/O2; baseline fails three metadata groups and passes Mode2/transmitted-payload controls. Five disc formats; four host sizes/two poisons/all three allocation branches; header-mode/BCD matrix; independent full payload/prefix/trailing-byte, cached-field and logical-FAD checks. Seven mutants rejected; all31 CD methods pass. Full MCU adds208 load bytes to57,748, margin7,788; static DTCM60,296 unchanged. FIFO frame40→48, disc/task/dispatcher frames unchanged; no new workspace/allocation/I/O/payload copy/mask work. [Sega §5.4(2)-(3), §8.2.7 function7.1; attribution/policy/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-ROUTING-WRITE-SAFETY.md#mcu-cdc-005-coherent-producer-metadata). Complete scripted input is not physical FIFO timing/concurrency/game qualification; later lifecycle and shared routing remain explicit. |
| MCU-CDC-006 connection/reset/defaults; selected paps-g `50100444` plus `3f7bba1a` continuation | [ad690b6](https://github.com/papsg/SAROO/commit/ad690b6); [codex/cd-connections-reset](https://github.com/papsg/SAROO/tree/codex/cd-connections-reset) | Nine actual-source groups pass sanitizers/O2; baseline fails eight with ordinary configuration control passing.576 owner pairs, atomic invalid/ignored-field cases, all24 condition resets,510 nonzero-flag/ignored-argument combinations, pause/no-track/audio controls, relocated startup, software/nonreset/no-change and real unpublished allocation. Thirteen compiled mutations rejected; all40 CD methods pass. Full MCU adds160 load bytes to57,908, margin7,628; static DTCM60,296 unchanged. CD setter frame8→16, filter setter40 unchanged, reset48→32, helper8; startup/init24/24 and IRQ/dispatch24/16 unchanged. No workspace/allocation/payload copy/I/O added; original logs/masks preserved. [Exact Sega §5.2/5.3.3 Table5.1, §5.5 Table5.2, §8.2.5 functions5.5/5.7/5.9, §8.2.7 function7.1 and §8.2.1 function1.7; attribution/policies/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-ROUTING-WRITE-SAFETY.md#mcu-cdc-006-connection-ownership-and-resetdefault-distinctions). Unpublished ownership is a007 prerequisite; no captured hardware interleaving or active partition-output qualification. Host-input theft/lifecycle and later init/GET/output work remain separate. |
| MCU-CDC-003 routing continuation; selected paps-g `50100444` | [a66041a](https://github.com/papsg/SAROO/commit/a66041a); [codex/cd-shared-routing](https://github.com/papsg/SAROO/tree/codex/cd-shared-routing) | Requires005/006. Six new routing groups pass sanitizers/O2:144 selection/split/return shapes, zero-work discard/admission, five real producer formats,4,096 baseline-valid Mode2 predicate combinations, normalized non-Mode2 and full-width range controls. Baseline fails five groups with predicate control passing. Eleven mutants rejected; all47 CD methods pass both profiles, including a new baseline-valid null-context disconnected-input guard. Actual allocator body retained behind host-only call counter; discarded copies and moves make zero allocation calls. Full MCU adds80 load bytes to57,988, margin7,548; static DTCM60,296 unchanged. Route frame32, copy56→64; incoming32, disconnect8, disk80 and IRQ/dispatch24/16 remain. Joined copy/incoming sums96→136/112→144 exclude other callees/exceptions. Original logs/masks/incoming-only print preserved; no new workspace/state/I/O. [Sega §5.3.1–5.3.3, §5.4(2), §8.2.7 functions7.6–7.8, §8.2.5 function5.5; attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-ROUTING-WRITE-SAFETY.md#mcu-cdc-003-continuation-shared-destination-filter-routing). Full-width arithmetic and inversion-only ambiguity are distinguished from hardware qualification. Broader asynchronous/output/cancellation ownership and007 remain separate. |
| MCU-CDC-007 write admission/lifecycle; selected paps-g `50100444` plus `3f7bba1a` | [f9ed8fe](https://github.com/papsg/SAROO/commit/f9ed8fe); [codex/cd-write-lifecycle](https://github.com/papsg/SAROO/tree/codex/cd-write-lifecycle) | Requires005/006/shared003. Thirteen actual-source groups pass sanitizers/O2: all200 reservation counts,8,196 short-input combinations, one-snapshot/FIFO/request bounds, metadata routing, six allocation-loss boundaries, sparse private chains, reset/theft/recovery and valid full-payload controls. Baseline fails twelve groups with its payload control passing;21 compiled behavioral mutants rejected. Updated005 producer fixture retains every payload/prefix/trailing-byte/metadata oracle and its five groups also pass the preceding source. All60 CD methods pass both final profiles. Full MCU adds240 load bytes to58,228, margin7,308; static DTCM60,296 unchanged. Service frame48→24, preparation16→48, DataEnd32→40; IRQ/dispatch24/16 unchanged. No new retained state, buffer, allocation during input/end, payload copy, thread or heap object; prefixes at most24 bytes/sector. One service consumes at most4,096 bytes versus the old unconditional2,048; whole handler retains the GET fill_fifo call. [Exact Sega §8.2.7 function7.5, §8.2.1 functions1.9/1.10, §5.3.4(1), DEND constant, pinned FPGA interface and menu decoder; attribution/costs/reproduction/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-ROUTING-WRITE-SAFETY.md#mcu-cdc-007-reserved-host-writes-and-bounded-completion). Undefined unfilled payload stays untouched; no empty-read fabrication, added logs or physical FIFO/IRQ/concurrency/timing/game qualification. |

All four scoped source ports are separately verified linear commits, published
with their topics to main, ending at [f9ed8fe](https://github.com/papsg/SAROO/commit/f9ed8fe).
Each row records its checkpoint; the integrated current-source suite passes351
Python methods in35 groups and14 Rust tests. All60 CD methods also pass final
ASan/UBSan and O2 checks after the final I/O-budget assertions. Seven cohorts
bind source/header/fixture/dependency hashes in both profiles; all68 native
cases/modes per profile return success. Failing baselines, valid controls,
compiled mutants, adaptations and resource evidence remain preserved locally.

Complete GNU App/RTX/166-vector MCU load grows688 bytes across the series,
57,540→58,228, leaving7,308 ITCM bytes. Static DTCM stays60,296; required stacks,
the200-block pool and retained workspace are unchanged.56 MCU/36 menu input
hashes match their receipts and corresponding source commits. Menu image,
resources and ELF exactly match the preceding public checkpoint:79,572 code/data,
9,232 BSS,16,384 reserved stack,357,679 font and488,751 image bytes. Per-port
frame sums exclude other callees/exceptions; reservation holds blocks before
readiness and the FIFO service budget grows. These are source/model/build checks,
not original-MDK, physical stack/bus/IRQ/concurrency/timing or game qualification.

Full proposed files/messages/public identities and actual commit blobs pass
manual, secret and private-PII review. Attribution uses approved public handles
and noreply identities, with exact primary/source citations and public regression
fixtures. Original notices remain. No private paths/accounts/network details,
credentials, raw logs or build artifacts are published.

Public main is clean, all four topic refs and main are published, default main,
v0.8 ancestry and historical master are verified. Existing public refs/stashes
are preserved apart from the authorized main/progress advances. Exact private
HEAD/full status/all refs/four stashes match the start snapshot. Private source,
research branches, Saturn/installed firmware and other paused work remain
untouched. No PR, USB integration, new runtime diagnostic, subagent or upstream
change was used. Fork issues are disabled, with no open PRs or applicable request
closure; original upstream requests remain unchanged.

The selected005/006/synchronous003/007 ports are complete at source/build level.
Broader003 asynchronous/cancellation/erase/output contracts and later008
GET/delete/size/ownership/initialization remain separate. Unspecified absent-
header fill and inversion-only ambiguity are not claimed as physical behavior;
this series is not blanket CD-command or Saturn/game qualification.

## CD read safety and ownership

Starts at public `f9ed8fe`. Four separately verified source contributions from
paps-g `89e644f6`/`3d2c405e`, including `ac0aeac1`'s corrected producer-count
semantics. Direct source commits to main, no PRs, USB, added runtime diagnostics,
private writes or Saturn operations. Raw evidence remains ignored; full files,
messages/identities and actual commits receive secret/private-PII review.

| Contribution / original | Public checkpoint | Verification, dependencies and limits |
| --- | --- | --- |
| MCU-CDC-008; selected paps-g `89e644f6` | [6b184ee](https://github.com/papsg/SAROO/commit/6b184ee); [codex/cd-command-bounds](https://github.com/papsg/SAROO/tree/codex/cd-command-bounds) | Eight actual-source methods pass ASan/UBSan/O2:3,480 invalid selectors across15 commands/14 guards,65,536 position words, last/empty/short-list queries,8 transfer-type busy precedence,408 valid read/delete tails and24 valid filter/file/directory/size controls. Baseline fails three groups and passes five;18 behavioral mutants rejected, one redundant extent-guard removal survives consistent-list controls and is retained uncounted. Actual status-helper dependency added to earlier subheader fixture, preserving all oracles; its three methods also pass preceding source. All68 CD methods pass both profiles. Full MCU adds240 load bytes to58,468, margin7,068; static DTCM60,296 unchanged, cached SDK inputs identical. Get-range/connection frames0→8; IRQ/dispatch24/16, sector-info8, common GET48, size24, file32 and FIFO24 unchanged. Original logs/masks retained; no buffer/copy/allocation/I/O/wait added. [Sega ST-162 §8.2.6 functions6.1/6.2 printed92 and6.5 printed93, filter/filesystem contracts; attribution/reproduction/costs/policy/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-READ-SAFETY.md#mcu-cdc-008-command-selectors-and-sector-information-positions). Exact malformed-command replies/hardware timing/game/original-MDK behavior remain unqualified. |
| MCU-CDC-009; selected paps-g `89e644f6` | [b6181e6](https://github.com/papsg/SAROO/commit/b6181e6); [codex/cd-read-size](https://github.com/papsg/SAROO/tree/codex/cd-read-size) | Requires008. Three new actual-source groups pass sanitizers/O2:405 range/sentinel cases with retained refusals,800 actual FIFO/payload/count comparisons across all lengths and rotating first/later Mode1/Form1/Form2, incomplete variable-list error and software/nonreset/no-change lifetime. Baseline fails all three with eight existing controls passing;14 compiled behavioral mutants rejected, initial compile-only mutant failures excluded. All71 CD and three prior setup methods pass both final profiles. Full MCU saves16 load bytes to58,452, margin7,084; static DTCM60,296 unchanged. Calculation frame24→32, range12/length0; IRQ/dispatch24/16, first FIFO16, service24, common GET48, copy/move64 and delete24 unchanged. Original calculation trace and all log masks retained; no state/buffer/copy/allocation/I/O added, fixed calculations avoid list traversal. [Exact Sega ST-162 §8.2.6 functions6.3/6.4 printed93, §8.2.7 function7.1 printed95, §5.3.4(4), reset table/function and Sega size-error constant; attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-READ-SAFETY.md#mcu-cdc-009-selected-read-sizes-and-calculation-lifetime). Output disconnection remains010, pending counts012; later raw Mode1/audio-layout corrections remain separate. No physical/game/original-MDK qualification. |
| MCU-CDC-010; selected paps-g `3d2c405e` | [f7137f6](https://github.com/papsg/SAROO/commit/f7137f6); [codex/cd-read-ownership](https://github.com/papsg/SAROO/tree/codex/cd-read-ownership) | Requires008/009. Ten actual-source groups pass sanitizers/O2:720 detach shapes plus scrambled200-block pool,10,368 admissions,cancellation/preservation,real incoming append,release reuse,eight clear/shrink logging boundaries,reset,incomplete selection and exact baseline-valid payload/log controls. Baseline fails nine with one control;18 compiled ownership/scheduling mutants detected. All81 CD methods pass both profiles; final ten ownership methods with strengthened log cases rerun,2 directory and6 scheduling methods pass both. Retire fake transfer-state differential, preserving history and replacing it with actual list/FIFO/log checks. Full MCU adds224 load bytes to58,676, margin6,860; static DTCM60,296 unchanged. Common GET48→64,delete24→32,remove8→16,new helper frames8/20/8; other reported frames unchanged. Metadata detach masks at most200 nodes, payload/release outside; no state/buffer/copy/allocation added. Original logs/masks and valid text/order retained, log precedes pointer acquisition and range is re-admitted. Disk priority remains its effective Normal24 below command31. [Exact Sega §8.2.7 introduction/functions7.2–7.4 printed96, §8.2.6 function6.3 printed93, §5.3.4(1), pinned RTX/UART contracts; attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-READ-SAFETY.md#mcu-cdc-010-stable-read-ownership-and-partition-output-cancellation). Early completion arithmetic remains012; no superseded consumed-read fix imported and no physical timing/concurrency/game/original-MDK qualification. |
| MCU-CDC-012; selected paps-g `3d2c405e` plus corrected `ac0aeac1` | [5d221d0](https://github.com/papsg/SAROO/commit/5d221d0); [codex/cd-read-counts](https://github.com/papsg/SAROO/tree/codex/cd-read-counts) | Requires010; excludes superseded host-consumed read policy. Four new actual-source groups pass sanitizers/O2:10,452 first-window prefixes,12 serviced/mixed/completed controls,five fixed replies,valid zero,idle/repeated completion and occupancy masks. Independent FIFO-write counts vary host consumption and forbid new read output at DataEnd. Baseline fails all four with ten ownership groups passing;11 compiled mutants detected. All85 CD methods pass both profiles, including13 actual PUT groups. Full MCU adds32 load bytes to58,708, margin6,828; static DTCM60,296 and DataEnd frame40 unchanged. No retained state/buffer/allocation/copy/FIFO output/mask added. [Exact Sega ST-162 §8.2.1 function1.10 printed81, same24-bit menu decoder; attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-READ-SAFETY.md#mcu-cdc-012-cd-produced-read-counts-and-pending-windows). Preserved mask has synthetic upper-bit coverage, not asserted FPGA05 physical states; physical service/DataEnd timing and games remain unqualified. |

All four scoped ports are separately verified linear source commits, pushed
with their topics to main, ending at [5d221d0](https://github.com/papsg/SAROO/commit/5d221d0).
The integrated project suite passes375 Python methods in37 groups and14 Rust
tests. All85 CD methods also pass ASan/UBSan and O2; nine cohorts bind current
source/header/fixture/dependency/generated-fragment hashes, with all93 native
cases/modes per profile succeeding. The four baselines and valid controls,
61 detected compiled mutants, one explicitly uncounted redundant survivor and
original/later-correction evidence remain preserved locally.

Each MCU receipt binds56 repository inputs to its own source checkpoint. Final
MCU load is58,708,6,828 ITCM bytes free:480 bytes added across the series, with
static DTCM60,296, required stacks,200-block pool and persistent workspace
unchanged. Final menu binds36 current inputs and exactly matches preceding
image/resources/ELF:79,572 code/data,9,232 BSS,16,384 reserved stack,357,679 font
and488,751 image bytes. Frame/masking/traversal costs above are explicit;
these are source/model/GNU build checks, not original-MDK, physical stack/bus/
IRQ/concurrency/timing or game qualification.

Complete proposed files/messages/identities and actual commit blobs pass manual,
secret and private-PII review. Attribution uses approved public handles/noreply
identities and exact primary/source citations. Original notices/log formats/
masks remain; no credentials, private paths/accounts/network details, raw logs
or build artifacts are published. Final source history scanning also passes.

Public main is clean and all four source/topic refs are published. Default main,
v0.8 ancestry and historical master are verified. Prior public refs/stashes are
preserved apart from authorized main/progress advances. Exact private HEAD/full
status/all refs/four stashes match the starting snapshot; private source,
research branches, Saturn/installed firmware and other paused work remain
untouched. No PRs, USB integration, new runtime diagnostics, subagents or upstream
changes were used. Fork issues are disabled, with no open PRs or applicable
closures; original upstream requests remain unchanged.

The selected008/009/010/012 source contracts are complete. Directory/ISO/XA
filesystem011/013, broader asynchronous copy/move/MPEG ownership, later raw-
Mode1/audio representations and initialization remain separate. Unknown exact
malformed-command replies and physical/game qualification are not claimed fixed.

## CD filesystem, directory streaming and XA safety

Starts at public `5d221d0`. Selected paps-g `8e465233`/`05852d90`, adapted as
separate direct source commits. No PRs, USB integration, new runtime diagnostics,
private writes or Saturn operations. Full files/messages/identities and actual
commits receive secret/private-PII review; original notices/log formats remain.

| Contribution / original | Public checkpoint | Verification, dependencies and limits |
| --- | --- | --- |
| MCU-CDIO-001 producer core; selected paps-g `8e465233` | [525dc55](https://github.com/papsg/SAROO/commit/525dc554a71196577103e063ecfc3b75ea5787e8); [codex/cd-sector-input](https://github.com/papsg/SAROO/tree/codex/cd-sector-input) | Six actual-source methods pass ASan/UBSan/O2: all nonzero seek/read results,8,192 incomplete-input edges,6,144 valid fills,cache/boundary/retry payloads,track/metadata controls. Baseline fails two groups with four controls; six compiled mutants rejected. All91 CD methods pass both profiles. Complete MCU remains58,708 load/static DTCM60,296 bytes,6,828 ITCM free;56 exact source hashes and identical SDK checked. Producer/disk frames48/80 unchanged; no state/buffer/copy/mask/wait or valid-input I/O added. [FatFs result/count contracts, attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-FILESYSTEM-SAFETY.md#mcu-cdio-001-complete-sector-input-admission). Directory-error ownership deliberately follows with011 worker; subcode-sidecar I/O/wider offset arithmetic and physical/game/original-MDK qualification remain outside this checkpoint. |
| MCU-CDC-011 plus CDIO-001 directory-error continuation; selected paps-g `8e465233` | [251f5d7](https://github.com/papsg/SAROO/commit/251f5d7ef6ac634a098d3100177e3aec9565f21e); [codex/cd-directory-streaming](https://github.com/papsg/SAROO/tree/codex/cd-directory-streaming) | Requires input/routing/read-ownership ports. Eleven actual-source directory methods pass ASan/UBSan/O2:1,512 pages/selectors/counts,65,540 records,one-free-block progress,record/name/descriptor bounds,full IDs/FIFO replies,spans/snapshots,abort/pause/redirect/private reuse,atomic error completion,pending phase pause and unchanged log/payload controls. Six isolated old-source probes fail with one valid control;20 compiled mutants detected. All103 CD methods and11 disc/save handoff methods pass; obsolete setup differential retired with Git history preserved. GNU MCU59,288 load/60,312 static DTCM,6,248 ITCM free;56 exact source inputs/same SDK. Adds580 load/16 static bytes, no pool/payload/workspace allocation. Producer/disk/command48/80/24 frames remain; optional readonly-name logger288,parsing/printing outside masks. All old log formats/masks retained. [Sega6.2.1/6.2.2 printed52,6.2.3 printed53,later8.2.8,ECMA-1196.8.1/9.1; attribution/reproduction/costs/version/policy limits](https://github.com/papsg/SAROO/blob/main/doc/CD-FILESYSTEM-SAFETY.md#mcu-cdc-011-bounded-directory-streaming-and-file-commands). Retains public atomic pause acknowledgement and pending requests across internal directory phases. Host boundaries are models, not hardware/game/original-MDK qualification. |
| MCU-CDC-013 selected XA metadata/interleave; paps-g `05852d90` | [64fc106](https://github.com/papsg/SAROO/commit/64fc10619dcb9d268f2f976aeea49bf318eca82e); [codex/cd-xa-files](https://github.com/papsg/SAROO/tree/codex/cd-xa-files) | Requires011 and its input/ownership/routing prerequisites. Nine actual-source methods pass ASan/UBSan/O2: suffix/padding/extended attributes,65,536 ISO/XA attribute pairs and exact replies,precedence,255 units×256 gaps×5 offsets against iterative lists,1,680 extent cases with64-bit expectations,256 file numbers,accepted/discarded payloads,full-pool retries,full snapshots,ordinary playback and independent stale-repeat controls. All nine fail compiled preceding functions/fragments;11 directory methods pass as controls.24 compiled mutants detected. All112 CD methods/120 native cases per profile pass;12 current-source cohorts bind inputs/fragments. GNU MCU59,640 load/60,312 static DTCM,5,896 ITCM free,56 exact source inputs/same SDK. Adds352 load bytes; padding/alignment absorb cursor state, with no file-entry/payload/workspace allocation. File-read40→56/parser8→16/disk80→72 frames; other stated frames remain. [Sega6.2.3 Table6.1 printed53,7.2.6 dataNo6.7,ECMA-1196.4.3 printed5–6/9.1 printed27–29; attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-FILESYSTEM-SAFETY.md#mcu-cdc-013-selected-xa-metadata-and-interleaved-file-reads). Cached Mednafen ReadRecord corroborates layout only, with an observed SHA-256; original XA book unavailable and saved GNU homepage excluded as layout evidence. No emulator algorithm imported or geometry inferred from its TODO. Broader013 remains separate; no physical/game/original-MDK qualification. |

All three scoped source contributions are separately verified linear commits,
pushed with their topics to main, ending at [64fc106](https://github.com/papsg/SAROO/commit/64fc10619dcb9d268f2f976aeea49bf318eca82e).
The full integrated suite passes400 Python methods in39 groups and14 Rust tests.
Final ASan/UBSan and O2 runs pass all112 CD methods, with120 native cases per
profile across12 current-source cohorts. Failing baselines, valid controls,50
detected compiled mutants, original sources/corrections, primary references and
raw results remain preserved locally; portable fixtures are public.

Each MCU checkpoint binds56 repository inputs to its own committed blobs, with
identical cached SDK hashes. Final MCU load59,640 leaves5,896 ITCM bytes; the
series adds932 load/16 aligned static bytes, with static DTCM60,312. Required
stacks,200-block pool and retained payload workspace are unchanged. Final menu
binds36 committed inputs and exactly matches the preceding image/resources/ELF:
79,572 code/data,9,232 BSS,16,384 reserved stack,357,679 font and488,751 image
bytes. Per-port frame/log/masking/traversal costs remain explicit; host models
and GNU builds are not original-MDK or physical stack/bus/IRQ/concurrency/timing/
game qualification. No physical operation or installed-firmware change occurred.

Full proposed files/messages/public identities and actual source/ledger commit
blobs pass manual, secret and private-PII review; final source history scanning
also passes. Approved public handles/noreply identities retain attribution, and
exact primary/source citations distinguish requirements, defensive policies,
translation/version differences and implementation corroboration. Original
notices/log formats/masks remain; no credentials, private paths/accounts/network
details, raw logs or build artifacts are published. No PRs, USB integration,
new runtime diagnostics, subagents, private publication or upstream edits.

Public main is clean; source topics/main are remotely verified. Default main,
v0.8 ancestry and historical master are preserved, as are other existing public
refs/stashes apart from authorized main/progress advances. Exact private HEAD,
full status, all refs and four stashes match the start snapshot; private source,
research branches and other paused work remain unchanged. The final public
progress checkpoint is published with these source/audit results. Fork issues
are disabled, no open PRs or applicable closures; upstream requests unchanged.

Selected CDIO-001/CDC-011/XA-013 source contracts are complete. Broader013
session/default drive-range/EOF/PEND/tray/drive, later raw-Mode1/audio layout and
initialization, and asynchronous copy/move/MPEG ownership remain separate. The
fixed FAD166 descriptor start does not implement last-session selection.
Unmeasured malformed-command responses and physical/game compatibility are not
claimed resolved.

## CD drive playback, seek and repeat safety

Starts at public `64fc106`. Selected paps-g47ec46a4/6a00df29, necessary
659fe4ee/cd83b371 corrections, and a separate selectedad7c8307 FPGA clear fix.
Direct verified source commits; no PRs, USB integration, runtime diagnostics,
private writes or Saturn operations. Full files/messages/identities and actual
blobs receive secret/private-PII review; raw evidence stays ignored.

| Contribution / original | Public checkpoint | Verification, dependencies and limits |
| --- | --- | --- |
| MCU-CDC-014 plus selected016/022; paps-g47ec46a4/659fe4ee/cd83b371 | [3a68c3d](https://github.com/papsg/SAROO/commit/3a68c3d306f1d2b1aa7121b31399413b1a0a2431); [codex/cd-drive-positions](https://github.com/papsg/SAROO/tree/codex/cd-drive-positions) | Twelve actual-source methods pass ASan/UBSan/O2: all65,536 track/index words,120 FAD/count shapes,high addresses,boundary/refusal/missing-media controls,500-wait failure/retry,all16 repeat limits/exact final-sector admission,background completion,both actual cache acknowledgement branches,99 tracks,and exact baseline-valid position/report/log control. Baseline fails eleven groups with that control passing;21 compiled behavioral mutants detected, one initial compile-only attempt excluded. All124 CD methods and11 disc/save controls pass. GNU MCU59,816 load/60,312 static DTCM,5,720 ITCM free;56 exact source inputs and identical SDK checked. Adds176 load bytes,no retained allocation/payload copy/I/O; drive pause16/replacement8 nested frames,play/seek32/32,position helpers16/16,producer48/disk72/command24. Retains public atomic pause/directory retirement and all original log formats/masks; avoids private32-track regression and corrects the old mocked-cache claim. [Exact Sega CdcPos printed65–66,drive4.2 printed37–38,8.2.2 printed82–83,version/pickup ambiguity,attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-DRIVE-SAFETY.md#mcu-cdc-014-position-admission-final-sectors-and-bounded-handoff). Models/builds are not physical timing/concurrency/game/original-MDK qualification. |
| MCU-CDC-015; paps-g6a00df29 | [2091c5c](https://github.com/papsg/SAROO/commit/2091c5c739b8f872b9d9814951c80210ada986c9); [codex/cd-repeat-history](https://github.com/papsg/SAROO/tree/codex/cd-repeat-history) | Requires014. Four new actual-source groups pass sanitizers/O2:2,880 repeat vectors,finite-budget exhaustion,modeled progress before actual acknowledgement,changed-setting/failure/retry controls,all15 counts through seek/stop with exact invalid words,home pause/direct resume and absent media. Baseline fails three groups with changed-setting/failure control passing;11 compiled mutants detected. All128 CD methods pass both profiles. GNU MCU59,928 load/60,312 static DTCM,5,608 ITCM free;56 exact source hashes/same SDK. Adds112 load bytes, no state/buffer/I/O/wait; direct play/seek32/32,report8,disk72,command24 unchanged. [Exact Sega4.2 printed37–38,later7.2.5(5),attribution/reproduction/costs/policies/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-DRIVE-SAFETY.md#mcu-cdc-015-repeat-history-and-home-position-recovery). Existing explicit-seek range exceptions remain separate; no physical/game/original-MDK qualification. |
| MCU-CDC-021; paps-g cd83b371 | [d78c74f](https://github.com/papsg/SAROO/commit/d78c74f81a71073969b71c341a62592dae4e0670); [codex/cd-retained-positions](https://github.com/papsg/SAROO/tree/codex/cd-retained-positions) | Requires014/015. Three new actual-source groups pass sanitizers/O2:112 old/new endpoint pairs, failed-handoff/NOCHG/default controls, actual file/background range replacement. Preceding source fails two defect groups; third failure establishes new metadata lifetime, not another old hardware defect. Eight compiled behavioral mutants detected; all131 CD methods pass both profiles. GNU MCU59,976 load (+48),60,312 static DTCM,5,560 ITCM free;56 exact committed input hashes/same SDK. Reuses unused byte, no object/buffer/allocation/I/O/wait growth; play32,file-start32,disk72,pause16/replacement8 direct frames unchanged. [Sega later CdcPly note2,printed67–68,attribution/reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-DRIVE-SAFETY.md#mcu-cdc-021-retain-endpoint-forms-through-nochg). Existing seek policy remains; no physical concurrency/timing/game/original-MDK qualification. |
| FPGA build prerequisite; paps-g procedure adapted in Rust | [cf42977](https://github.com/papsg/SAROO/commit/cf429775a1514ee286ebec318a54eb6eed50e000); [codex/fpga-build-check](https://github.com/papsg/SAROO/tree/codex/fpga-build-check) | Extend the existing firmware-build host tool for source-only original-project comparisons. Eight host checks, formatting/lint/release build pass. Both fresh complete baseline/candidate projects pass five stages and21 reported timing checks. The first run completed stages but its initial report reader rejected Quartus spacing; that exact source/log/receipt remains preserved and excluded. The corrected reader and full builds pass. Pinned cached runtime, explicit read-only compiler/JRE, full streamed compiler manifests, bounded stages/logs/resources, network disabled, unique identities, no device access. [Original project/settings,usage,attribution,verification-only Standard Fit/uncertainty,dependencies and external-timing limits](https://github.com/papsg/SAROO/blob/main/tools/README.md#original-fpga-project-comparison). No runtime code, hardware or game qualification. |
| FPGA-HIRQ-001 selected clear/clear; paps-g ad7c8307 | [bd4dc09](https://github.com/papsg/SAROO/commit/bd4dc0914d1de4d9fdf0b6b95189f1f1b63d4fa6); [codex/fpga-hirq-clears](https://github.com/papsg/SAROO/tree/codex/fpga-hirq-clears) | Narrow adaptation to original FPGA05, without FPGA06/profile/binary import. Actual RTL baseline fails distinct clears (0001 vs0000), passes1,857 controls; corrected source passes5,121 collision updates plus1,857 controls/reset. Ten compiled HDL behavioral mutants detected. Both complete five-stage Quartus builds, SOF/RBF/JIC and21 constrained summaries pass; worst reported slack+0.042/+0.167ns. Equal18 inputs except SSMaster.v, runtime/glue/compiler manifest;96,211 compiler file/link entries and9,405,944,612 regular bytes unchanged after each build. Fit1467→1464 logic elements,1332→1333 combinational functions,816 registers/32,768 memory bits/150 pins/one PLL unchanged; RBF121,706→121,905 bytes, SOF/JIC sizes unchanged. MCU/menu unchanged. [Exact Sega3.1 printed28,8.2.9 function9.3 PDF90,original source,credit,reproduction,costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CD-DRIVE-SAFETY.md#fpga-hirq-001-combine-simultaneous-clears-on-the-original-fpga). Deliberately retains separate FPGA05 SET/CLEAR loss; no external timing, crossing, hardware or game qualification/installation. |

All scoped position/final-sector/handoff,99-track/cache,repeat/home,retained
endpoint and separate FPGA clear/clear contracts are published as verified
linear contributions. The host FPGA build prerequisite is explicit and separate.
No PRs were opened or merged.

Final full suite passes419 Python methods across40 groups and18 Rust checks,
including the actual-block HDL tests; formatting/lint/workspace checks pass.
All131 CD methods pass ASan/UBSan and O2. Fifty compiled behavioral mutants across
this series are detected; one initial compile-only attempt is excluded and
preserved. The initial position-control fixture errors and FPGA report-reader
spacing failure remain documented, with exact source/log/receipt evidence; they
are not production findings or accepted qualification.

Final complete GNU MCU load is59,976 bytes versus59,640 at series start (+336),
with5,560 ITCM bytes free. Static DTCM remains60,312; pool/workspace/stack
allocations remain unchanged. All56 committed MCU inputs and identical cached
SDK bind to the per-checkpoint/final receipts. Final MCU image/ELF is byte-exact
to021. Direct frames and nested pause-wrapper costs are recorded in the source
document; these exclude callees/exceptions and physical peaks/cycles. Final menu
retains36 baseline inputs and its exact preceding image:79,572 code/data,9,232
BSS,16,384 reserved stack,25,884 body margin,357,679 font and488,751 image bytes.
Both complete FPGA source cohorts and generated settings/dependencies bind to
their receipts; generated products remain evidence, unshipped/uninstalled.
No physical Saturn, concurrency, external-bus, game or original-MDK acceptance
is inferred from host models or target builds.

Full proposed files/messages/public identities and actual source/ledger blobs
pass manual, secret and private-PII review. Approved paps-g attribution and
papsg/noreply identities remain, with exact Sega/source citations and explicit
version, translation, compatibility-policy and qualification limits. No private
paths/accounts/network details, credentials, raw receipts or build artifacts are
published. Original notices/APIs/log formats/masks are preserved. No USB, new
runtime diagnostic/probe/telemetry/loader, subagent or upstream/private publication.

Public main and all five source topics are clean and remotely verified. Default
main, v0.8 ancestry, historical master and unrelated refs/stashes are preserved,
apart from authorized main/progress advances and new topics. Exact private HEAD,
full status, all refs and four stashes match the initial snapshot; source,
research branches, other paused work and Saturn remain untouched. The final
progress checkpoint is published. Fork issues are disabled with no open PRs or
applicable closures; original upstream requests remain unchanged.

CUE geometry/raw formats, broader initialization/reset/audio/session/tray/scan,
existing explicit-seek compatibility policy and FPGA05 SET/CLEAR arbitration
remain separate. Unmeasured hardware/game consequences are not claimed resolved.

## CUE layout and sector-reader safety

Starts at public `bd4dc09`. Revalidate the remaining paps-g659fe4ee reader,
CUE mapping, MODE2 normalization and subcode contracts as separate source ports.
MCU-CDC-01699-track behavior is already present and deduplicated. Reader support
and compact metadata precede the CUE mapper to avoid an intermediate firmware
interpreting generated gaps as file data. No PRs, USB integration, runtime
instrumentation, private writes or device operations.

| Contribution / original | Public checkpoint | Verification, dependencies and limits |
| --- | --- | --- |
| MCU-CDIO-002 and narrow metadata/gap prerequisite; paps-g659fe4ee | [0cc4bb7](https://github.com/papsg/SAROO/commit/0cc4bb700d3f00630d33e6eb66bee38cee801e78); [codex/cd-image-reader](https://github.com/papsg/SAROO/tree/codex/cd-image-reader) | Five actual-source groups pass ASan/UBSan/O2: independent mixed-width metadata and every payload byte forward/reverse/permuted; index00..99 position-only reports; high offsets/clipped seeks; generated/stored gaps; unchanged single-track warm-cache control. Preceding source fails three groups/eight cases, control passes; new gap invariant explicitly skipped on old layout. Thirteen compiled behavioral mutants detected; one equivalent signed-local attempt preserved/excluded. All131 prior CD methods pass both profiles;68 MCU controls pass sanitizers, including parser/index capacity; final18 affected methods pass both profiles after fixture metadata corrections. GNU MCU60,248 load (+272),60,304 static DTCM (-8),5,288 ITCM free;56 exact committed source inputs and unchanged SDK. ARM records remain32 bytes/100 records, all2,048 indices and AXI allocation preserved. Reader direct frame64,index16,track0,TOC32,parser152,position16,disk72; checked I/O inlined. No new buffer/task/write/wait/mask/log format. [Exact Sega access keys/layout printed24/26,report data4.0 printed60,FatFs seek,original source,credit,reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CUE-SECTOR-SAFETY.md#mcu-cdio-002-extent-bounded-reads-and-coherent-reports). Actual I/O/preemption remain models; no Saturn/game/original-MDK qualification. |
| MCU-CUE-002; paps-g659fe4ee | [ee4586e](https://github.com/papsg/SAROO/commit/ee4586e37b41e2c81e48e8dc711cf1a1beba4ce8); [codex/cue-layout](https://github.com/papsg/SAROO/tree/codex/cue-layout) | Requires the separately published reader/metadata support. Six actual parser/TOC/reader groups pass sanitizers/O2: mixed widths/files, complete bytes forward/reverse/permuted, stored/generated gaps, first prefixes0/2/150/300,13 malformed/overlapping/overflow cases,full2,048 indices/overflow/preview and99-track read control. Preceding parser fails24 cases across five groups;99-track control passes. Thirteen compiled behavioral mutants detected; one ambiguous preparation rejected before compilation and excluded. Ten existing parser,six preview/header,five reader controls pass. GNU MCU60,648 load (+400),60,304 static DTCM,4,888 ITCM free;56 bound inputs/same SDK. No record/index/AXI/buffer/task/I/O allocation growth; direct parser152→160,offset/end16,finish8,reader64/disk72 frames. [Sega CdcPos printed65–66,layout/access keys,CUE conventions,attribution,reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CUE-SECTOR-SAFETY.md#mcu-cue-002-map-stored-frames-and-generated-gaps-separately). Capacity uses virtual file size/position-only reads; ordinary payloads use actual FatFs files. No physical/game/original-MDK qualification. |
| MCU-CDIO-003; paps-g659fe4ee | [ce58d51](https://github.com/papsg/SAROO/commit/ce58d51b672500b8dee6876a0595e5f7c175c51f); [codex/cd-mode2-layouts](https://github.com/papsg/SAROO/tree/codex/cd-mode2-layouts) | Two new actual-source groups each pass96 producer/filter/routed-copy vectors across2336 Form1/Form2 and2324 payload input,four read sizes,eight patterns,all2352 container bytes/metadata/ownership. Baseline fails both with six existing controls passing. Poisoned free buffers detect all eight compiled mutants; initial zero-filled-block fixture missed two variants, preserved/excluded and corrected. All144 current CD methods pass both profiles; final poisoned routing groups pass both. GNU MCU60,776 load (+128),60,304 static DTCM,4,760 ITCM free;56 exact bound inputs/same SDK. Reader64/filter32/disk72 direct frames unchanged,no new state/buffer/I/O/wait/log/mask. [Sega5.4/Figure5.8 printed48,Form bit printed71,source attribution,reproduction,costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CUE-SECTOR-SAFETY.md#mcu-cdio-003-normalize-accepted-mode2-image-payloads). Headerless image adaptation is policy,not canonical sync/FAD/ECC/EDC reconstruction; missing channel/file information remains unknown/zero. No physical/game/original-MDK qualification. |
| MCU-CDIO-004; paps-g659fe4ee | [0010a39](https://github.com/papsg/SAROO/commit/0010a39a7dc0699b6a7a37d70448b1dfdc4a1cfa); [codex/cd-subcode-io](https://github.com/papsg/SAROO/tree/codex/cd-subcode-io) | Five actual-source groups/16 scenarios pass sanitizers/O2: planar/raw EOF conversion and14/16 windows,cache-mode admission,six mid-I/O flag changes,five sidecar failures,sector-failure-after-subcodes/ring/output/retry controls. Baseline fails13 cases across four groups;raw EOF and payload-failure controls pass. Ten compiled behavioral mutants detected. Final release CD suite149 methods passes. GNU MCU60,888 load (+112),60,312 static DTCM (+8 linked for mode byte/alignment),4,648 ITCM free;56 exact committed inputs/same SDK and final MCU build exact. No record/index/AXI/payload/task growth; reader56 plus nested I/O24,converter40/filter32/disk72 direct frames are not a stack saving. [Sega7.2.5(2)/data5.0 printed62,8.2.3/function3.2 printed85,source credit,reproduction/costs/limits](https://github.com/papsg/SAROO/blob/main/doc/CUE-SECTOR-SAFETY.md#mcu-cdio-004-admit-complete-subcode-windows-under-one-mode). Preserve inherited sidecar origin/converter/ring/overflow policy;native pack timing/physical SD recovery/CD+G/game/original-MDK qualification remain open. |

All four scoped contributions are complete as separate verified linear source
commits: MCU-CDIO-002,MCU-CUE-002,MCU-CDIO-003 andMCU-CDIO-004. The already-public
MCU-CDC-01699-track correction is deduplicated. Reader/compact-metadata support
is the explicit prerequisite; no broader reset/audio/session/raw-format work
or subsequent upstream changes were imported. No PRs were opened or merged.

Final full project suite passes437 Python methods across43 groups and18 Rust
checks, including the unchanged actual-block FPGA controls and workspace
format/lint checks. All149 CD/reader/layout/subcode methods pass optimized
checks as well as final sanitizers. Forty-four compiled behavioral mutants
are detected across the four contributions. Preserve/exclude the equivalent
signed-local attempt, rejected ambiguous preparation and initial zero-filled-
block weak mutation campaign; the corrected current oracles pass. The original
combined baseline's ten failing groups and99-track passing control remain
historical negative evidence, not acceptance of the earlier broken source.

Final complete GNU MCU App/RTX/166-vector build is60,888 loaded bytes (+912
versus59,976 at series start), with4,648 ITCM bytes free. Static DTCM returns
to the starting60,312 bytes after bounded cache-field narrowing and the new
cache-mode byte/alignment. All 100 32-byte ARM records,2,048 index entries,
existing payload/subcode/ring/AXI workspace and task-stack capacities remain.
No new payload buffer, heap, task, wait, SD write or log/mask is added. Current
direct and nested frames are recorded per checkpoint, without claiming whole-
path stack savings or physical cycle/latency/headroom qualification. Exact56
repository MCU input hashes at every checkpoint and final build match their
own committed source; SDK hashes remain identical. The final build is byte-
exact to the subcode checkpoint. No FPGA source/binary changed in this series.

Final menu build retains all36 baseline inputs and its exact preceding ELF/
image:79,572 code/data bytes,9,232 BSS,16,384 reserved stack,25,884 body margin,
357,679 font and488,751 image bytes. Target builds and host I/O/preemption
models are not Saturn bus/cache/concurrency/SD-recovery/game/audio/CD+G timing
or original-MDK qualification. Generated products remain evidence, unshipped
and uninstalled; synthetic fixtures/capacity limits are explicitly documented.

Full proposed files/diffs/messages/public identities and actual source/ledger
blobs pass manual, secret and private-PII review. Exact Sega sections/data/
function numbers/pages and source/FatFs/CUE references distinguish interface
requirements, image conventions, version/translation differences and policy.
Original paps-g/vendor/upstream attribution and notices remain, with public
papsg/noreply commit identities. No credentials/private PII, personal paths,
private accounts/URLs/network details, raw logs or build receipts are published.
No USB integration, runtime diagnostics/probes/telemetry/loaders or subagents.

Public main and all four topics are clean and remotely verified. Default main,
v0.8 ancestry,historical master and unrelated branches/stashes are preserved,
apart from authorized main/progress advances and new topics. Exact private
HEAD/full status/all refs/four stashes match the initial snapshot. Private
source/evidence/research branches,other paused work and Saturn remain untouched.
The final public progress checkpoint is published with these audit results.
Fork issues remain disabled with no open PRs/applicable closures; no original
upstream issue/PR is claimed closed and original requests remain unchanged.

Broader initialization/reset/audio/session/tray/scan,canonical raw MODE1 and
missing sync/FAD/ECC/EDC reconstruction,extra CUE formats/metadata,and inherited
native subcode timing/overrun behavior remain separate. No unmeasured physical
or named-game consequences are claimed resolved.


## CD initialization and selector-reset safety

Active series: selected paps-g `3772326a`, MCU-CDC-017; start `0010a39`.
Revalidated against current main rather than applying the old combined patch.
Direct source contributions; no PRs. Cached Sega initialization/selector-reset
specifications and relevant edition/language differences are cited in
[CD-RESET-SAFETY.md](https://github.com/papsg/SAROO/blob/main/doc/CD-RESET-SAFETY.md).
Preserve newer public directory/request retirement, same-reader cache,
replacement invalidation, playback endpoints/repeats and CUE/subcode fixes.

1. **Bounded initialization handoff — published**
   [`2bbf325`](https://github.com/papsg/SAROO/commit/2bbf3250cae84789d299cea35ff109152f2c53cc),
   topic `codex/cd-init-handoff`; main includes this checkpoint.
   Delay policy changes until acknowledgement; reuse pause_drive, return WAIT
   on failure without reclaiming buffers/FIFO or pretending reset completion.
   Pending requests permit delayed acknowledgement/retry. Existing bit7/report/
   reset-default behavior intentionally follows in checkpoint2.
   73 focused methods pass ASan/UBSan and O2, including21 drive methods, both
   actual worker branches, all3 owner types, complete state during waits,
   final-wait and between-command acknowledgement, ownership/cache and valid
   controls. Six compiled behavioral controls detected; preceding valid exact
   report/log control passes. Corrected a fixture's mistaken low-byte WAIT
   expectation; compiler-only failures do not count as reproduced defects.
   GNU MCU App/RTX/166-vector load60,888→60,872(-16), ITCM free4,664; static
   DTCM60,312 and stack/heap address gap70,760 unchanged. Direct init/disk24/72,
   shared pause16 nested; full call-chain/physical peaks unknown. All56 public
   source hashes match their own checkpoint and85 SDK hashes match baseline.
   No new allocation/task/I/O/payload copy/per-sector work. GNU/host evidence
   does not qualify Saturn/game timing or original-MDK output.
2. **Initialization flags/reports/defaults — published**
   [`98d1314`](https://github.com/papsg/SAROO/commit/98d13147453d1a578e16a7bd465b65d2d99bad75),
   topic `codex/cd-init-policy`; depends on2bbf325 and is on main.
   Keep-settings now pauses and reports without false reset completion; ordinary
   initialization refreshes all4 words; software reset restores supported decode
   defaults. Preserve existing bit7 precedence, positions/ranges/repeats and
   unsupported-control limits. Correct two prior false-completion test oracles.
   75 focused methods pass ASan/UBSan and O2, including1,024 active/idle/branch
   flag combinations,512 missing-media flag cases, keep-settings timeout/retry,
   FIFO/IRQ/default/ownership/cache and exact reports. Eight additional compiled
   behavioral controls detected; preceding valid report/log control passes.
   GNU MCU load60,872→60,888(+16), ITCM free4,648; DTCM/gap and direct frames
   unchanged. All56 public inputs bind to their committed checkpoint;85 SDK
   inputs unchanged. No installation or physical/game/MDK qualification.
   Full-file/diff/message/identity and actual-blob privacy scans pass. Fork
   remains default main, issues disabled, no open PRs; no applicable closures,
   upstream unchanged. Private source and Saturn untouched.
3. **Bounded selector-reset handoff/wakeup — published**
   [`c890af9`](https://github.com/papsg/SAROO/commit/c890af9ed109b808d77538559ed81cf438ae480e),
   topic `codex/cd-selector-handoff`; depends on98d1314/2bbf325, included on main.
   Reuse pause_drive before deleting partition data/changing connectors. Wake
   once, bound failure, retain pending ownership for late acknowledgement/retry.
   Preserve current selective reset/refusal/pause policy, audio/idle behavior,
   output ownership and both original log formats/masks (unlike the old patch).
   78 focused methods pass ASan/UBSan and O2, including1,280 selector-mode/input/
   audio/data/idle combinations, timeout/retry across3 owner types and both real
   acknowledgement branches, complete in-wait state, FIFO/IRQ/output completion,
   directory retirement, cache retention and exact logs. Eight more compiled
   behavioral controls detected; preceding exact valid report/log control passes.
   GNU MCU load remains60,888 with4,648 ITCM free; DTCM/gap unchanged. Selector32
   plus nested pause16 direct frames; full chains/physical peaks unknown. All56
   committed public inputs and85 unchanged SDK inputs verified. No new fields,
   allocations/tasks/I/O/payload copies/per-sector work; one qualifying wakeup.
   Full-file/diff/message/identity and actual-blob privacy scans pass. Fork audit
   remains main/issues-disabled/no open PRs, with no applicable closures and
   upstream unchanged. Final complete suite/build/preservation results are recorded below.

Full source/diff/message/identity and committed-blob scans passed before
publication. Public papsg/noreply identities and paps-g attribution retained.
No secrets/private PII, personal paths/private URLs or raw receipts published.
Fork audit: default main, issues disabled, zero open issues/PRs; no applicable
closure, upstream unchanged. Private repository and Saturn remain untouched;
no USB/runtime diagnostics, unrelated research or subagents. Final full-suite,
MCU/menu, publication and preservation audits follow after the scoped ports.


### CD reset series final verification

All three scoped source contributions are complete on public main `c890af9`;
no PRs were opened. Topic/main remote hashes are verified. The selected paps-g
`3772326a` changes were revalidated and separated rather than applied blindly.
The port preserves the original selector log formats/masks and newer public
ownership/cache/retirement fixes. Only init_cdblock/reset_filter runtime bodies
changed; saturn_main has a reader-comment correction only. No other MCU, menu,
FPGA, USB or research implementation changed.

The complete standard regression run passes **444 Python methods in43 groups**
(415.664 summed test seconds) and **18 Rust checks in4 groups**, with zero failed
or ignored Rust tests. All43 Python groups report OK. The **156 CD methods in16
optimized groups** also pass. All22 compiled behavioral negative controls across
three stages are detected; every preceding-source exact valid report/log control
passes. Failed original/fixture attempts are preserved and excluded from the
accepted results. Actual commands, reports, helpers, acknowledgement branches,
mode/flag matrices, complete in-wait state, FIFO/IRQ/output ownership, directory
retirement, retained positions/ranges/repeats/cache and valid controls are covered.

Final GNU App/RTX/166-vector MCU build binds all56 repository inputs to committed
main and all85 SDK inputs to the unchanged baseline SDK. Its ELF exactly matches
the selector checkpoint. Load60,888/ITCM free4,648; static DTCM60,312;
main-stack/heap address gap70,760. Net load change against the series start is0
bytes, with actual intermediate deltas -16/+16/0. Direct init24/selector32/disk72
and nested shared pause16 are not whole-path or physical stack peaks. No new
persistent field/buffer/task/allocation, payload copy, SD I/O or per-sector work.
Qualifying selector reset adds one worker wakeup; ordinary/keep-settings init
adds the required four-word report. Reset retains existing200-block/24-selector
scan limits. Logging/scheduling/storage failure can exceed inferred wall time.

Final menu build retains all36 inputs, exact prior ELF/image,79,572 code/data,
9,232 BSS,16,384 reserved stack,25,884 body margin,357,679 font and488,751 image
bytes. These host models and GNU builds do not qualify physical Saturn
scheduling/bus/cache/audio/game behavior or original-MDK output. Nothing was
installed; Saturn/USB/SSH/power/capture operations were not used.

Read and preserved seven relevant later patch references. Separate output-reset
mode8 behavior, virtual-tray semantics, cooperative PLAY updates, R-W decode
sessions, file-abort reports, physical SYSRES/FPGA integration and reset sharing
are not prerequisites for this scoped correction and were not imported. Broader
software-reset completeness, standby/ECC/retry, tray/scan/session, all selector
output modes and routing-wide pause policy remain explicit limits. This does not
claim those later features or an audio/named-game defect are resolved.

Citations distinguish Sega §8.2.1/function1.7 printed pp.78–79 and §8.2.5/function5.9
printed p.91 from later English/Japanese editions and SAROO's semaphore policy.
Cached RTX timeout handling supports the finite-wait distinction; it does not
prove a physical deadline. Original paps-g attribution/notices remain; public
papsg/noreply identities identify adaptations. No original upstream issue/PR is
claimed as this self-reported finding's origin. Fork issues remain disabled,
with zero open issues/PRs and no applicable closures; upstream is unchanged.

Full proposed files/diffs/messages/identities, actual committed blobs and the
entire three-commit source history pass secret/private-PII review. No secrets,
private PII, personal paths/accounts/private URLs, raw receipts, runtime
diagnostics/probes/telemetry/loaders or patch framework were published. Source,
mutation, test/build, primary-reference and preservation evidence remains local.

Public main is clean and matches its remote; all three topics match published
commits, default main/v0.8 ancestry/historical master and unrelated refs/stashes
are preserved. The private repository's exact HEAD/full status/all refs/four
stashes match its starting snapshot. Research branches and other paused work
remain untouched. No subagents were used. This final ledger checkpoint records
completion of the scoped port, not physical qualification or broader reset
conformance.


## Audio safety series

Active series starts at public `c890af9`, selected paps-g `e019e57c`, `104b25fb`,
`e8a47c86`, `247c1dbd`, MCU-AUDIO-001–005. Direct original-layout changes;
no PRs, USB reload/handoff work, runtime diagnostics or private platform hooks.
Original source/evidence and later frame/cancellation/continuity corrections are
reviewed before adapting recovery. Private intermediate warning/replay bugs
must not be published as independent improvements. Necessary prerequisites
travel with their introducing behavior; unrelated session/clock policy remains
separately dispositioned. [Citations, attribution, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/AUDIO-SAFETY.md).

1. **MCU-AUDIO-001 retained startup status — published**
   [`7984fe2`](https://github.com/papsg/SAROO/commit/7984fe2980a5c991bb13685bb96fd212826d1c25),
   topic `codex/audio-startup`, included on main. Adapted from paps-g e019e57c.
   Clear only valid I2S flags with SPE zero before enabling initialization and
   DMA playback. Preserve ordinary format/rate/capacity/payload behavior.
   Five actual-source methods pass ASan/UBSan and O2: all16 flag combinations
   at both startup sites,50 restarts,32 sectors/backpressure and ordinary
   preservation control. Preceding source fails four new checks and passes the
   valid control; six compiled behavioral controls detected. A fixture address-
   cast-count setup error is preserved and excluded from defect reproduction.
   The separate automatic-DMA-bank counterexample still fails after001 and
   remains an open source-proven002 defect, not a passing startup result.
   GNU App/RTX/166-vector load remains60,888, ITCM free4,648; static DTCM60,312,
   stack/heap address gap70,760. Direct init/start/producer/IRQ0/12/24/0 unchanged.
   All56 public source inputs bind to their checkpoint;85 SDK inputs match
   baseline. Two cold-path register stores; no added state/buffer/allocation/
   task/poll/wait/copy/SD I/O. No hardware speed/stack peak is measured.
   RM0433 Rev8 §50.9.3/Table417 p.2194, §50.9.7 p.2203, §50.11.1 p.2218 and
   §50.11.7 p.2227 distinguish status acknowledgment from peripheral enable.
2. **MCU-AUDIO-002/003 ownership, FIFO and underrun recovery — published**
   [`bd498fd`](https://github.com/papsg/SAROO/commit/bd498fd184a677ee42965f0d8cb3acd450376251),
   topic `codex/audio-recovery`; depends on7984fe2, included on public main.
   Publish complete sectors before bank selection; retain all32 slots and
   immutable silence. Preserve valid FIFO words/stable residual counts and
   stereo phase; defer asynchronous DMA disable; recover ongoing UDR with
   bounded timer work. Count the real worker's accepted final-sector flush.
   Preceding source reproduces unpublished slot2 and retains UDR through eight
   running completions/publications, with its ordinary preservation control.
   The coherent port folds all necessary004/005 corrections into their
   introducing recovery behavior rather than publishing known regressions.
   Necessary quiet-stop adaptation of paps-g2617a839 adds actual TIM7 grace,
   CSTART/frame completion and (public adaptation) SUSP acknowledgment.
   A reproduced public IRQ snapshot race additionally requires acknowledging
   only observed flags; late TC otherwise replayed a sector, late error stalled.
   Both bank positions pass the corrected final-beat schedule. The earlier
   fixture accidentally delayed two completions; its failed run is preserved
   as a fixture error, not a new hardware finding.
   37 actual-source methods pass ASan/UBSan and O2;26 compiled recovery mutants
   detected. All156 CD/68 MCU save methods pass. Actual DMA1_Stream1/SPI2/TIM7
   ELF vectors resolve to strong handlers. Original startup-only checks remain
   in Git history; maintained coverage expands to all37 cases.
3. **MCU-AUDIO-004 warning and005 residual corrections — incorporated prerequisites**
   Original paps-g e8a47c86/247c1dbd correct private intermediate regressions
   introduced by104b25fb; these are not extra upstream-baseline defects.
   bd498fd retains FEIF backpressure/TC accounting, exact residual offset and
   normal-mode protection, FIFO contents and quiet stereo repair; repeated/
   permanent faults are rate-limited. The all1176-residual-count and both-bank/
   phase tests reject prefix replay, active-bank writes and lost late events.
   No separate broken intermediate implementation is committed.

Full-file/diff/message/identity and actual-blob secret/private-PII scans pass;
public papsg/noreply identities and original paps-g credit retained. No secrets,
private PII, personal paths/accounts/private URLs or raw receipts published.
Fork remains default main, issues disabled, zero open issues/PRs; no applicable
closures, upstream unchanged. Host/GNU evidence does not qualify Saturn/game/
receiver timing or original-MDK output. Private repo and Saturn remain untouched;
no USB/SSH/power/capture operations or subagents. Final full suite/build/privacy/
publication/preservation audits follow the remaining coherent recovery port.


### Audio safety series final verification

All scoped MCU-AUDIO-001–005 ports/dispositions are complete on public main
`bd498fd`, in two verified source checkpoints. Dependencies were folded where
separation would publish a known regression. Precise source/manual citations,
original paps-g attribution and public adaptations are recorded in
[AUDIO-SAFETY.md](https://github.com/papsg/SAROO/blob/main/doc/AUDIO-SAFETY.md).
Both source topics and main are published; no PRs were opened.

The complete standard suite passes **481 Python methods in44 groups** (416.067
summed test seconds) and **18 Rust checks in4 groups**, with no failed/ignored
Rust checks and all44 Python groups reporting OK. The37 audio methods also pass
O2; coupled156 CD/68 save methods pass. Six startup plus26 recovery compiled
behavioral controls are detected. All earlier source/test/mutation failures and
baseline/candidate counterexamples remain preserved; setup errors are excluded.
Exact accepted PCM is checked independently from hardware underrun repetition;
this is not an assertion of inaudible faults or arbitrary IRQ-latency tolerance.

Final GNU App/RTX/166-vector MCU build binds all56 public inputs to their own
committed checkpoint, all85 SDK inputs to the unchanged baseline cohort. Its
ELF exactly matches the recovery checkpoint. Load61,960(+1,072 against series
start), ITCM free3,576; static DTCM60,184(-128), main-stack/heap address gap70,888.
All32 payload sectors retain75,264 fixed SRAM1 bytes; immutable silence adds
2,352 for77,616 total, ending0x30014f30 below0x30020000 and beyond the unchanged
4KiB UART prefix. No other fixed workspace/task-stack reservation changes.
Direct init/producer/DMA IRQ/TIM7 frames8/24/24/8; service16, arm12, start8,
flush8 are nested direct measurements, not physical whole-path peaks.
No extra per-sector payload copy, SD I/O, dynamic allocation, task or sample
checksum is added. Startup fills silence once; temporary quiet DMA and one-shot
retries have explicit costs. No cycle, physical deadline or speedup claim.

Final menu retains all36 inputs and exact prior ELF/image/resources:79,572
code/data,9,232 BSS,16,384 reserved stack,25,884 body margin,357,679 font and
488,751 image bytes. FPGA/menu implementation is unchanged. The only firmware
changes are the audio driver, final-audio worker flush and its header declaration.
No USB stack/reload/handoff, runtime diagnostics/probes/telemetry/loaders,
private platform hooks, patch framework or unrelated research was imported.

Later ordinary-session cancellation and tested continuity-default/retired-
workaround evidence was read and preserved. Those session/indefinite-idle-clock
policies remain separate: this port introduces no ordinary cancellation, and
healthy stops follow a complete zero tail and stereo frame in the source model.
Their prior private game observations do not qualify this public cohort.
ST DMA/I2S/FIFO/W1C contracts, Arm DMB ordering and Sega's separate external
mixer context are cited exactly; receiver behavior under missing clocks,
physical bus/cache/IRQ/audio/game timing and original-MDK output remain open.
The public cold path's non-write-back-cache assumption is explicit. No named-
game or original upstream report is claimed resolved by analogy.

Full proposed files/diffs/messages/identities, actual committed blobs and the
entire two-commit source history pass secret/private-PII review. Public papsg/
noreply identities preserve original credit while publishing no secrets/private
PII, personal paths/accounts/private URLs or raw receipts. Source/evidence is
preserved locally; the old startup-only fixture remains publicly reproducible
at7984fe2. No Saturn/USB/SSH/power/capture or subagent operation occurred.

Public main is clean and matches its remote. Default main,v0.8 ancestry,
historical master and unrelated public refs/stashes are preserved. The private
repository's exact HEAD/full status/all refs/four stashes match the starting
snapshot; research branches and other paused work remain untouched. Fork audit:
issues disabled, zero open issues/PRs, no applicable closure, upstream unchanged.
The final source/progress publication and preservation audit records completion
of this offline port, not hardware or game qualification.

## UART transmit safety series

Series starts at public `bd498fd`. Port only the existing MCU UART transmit
driver. The private references are paps-g `acb824af` (MCU-SCHED-002 and
MCU-UART-001) and `c8335e2f` (STM32-DMA-001). [Primary citations and public
verification](https://github.com/papsg/SAROO/blob/main/doc/UART-TX-SAFETY.md).

1. **STM32-DMA-001 required transfer mode — published**
   [`d24d860`](https://github.com/papsg/SAROO/commit/d24d860c50eca2973fcc08a8c1a1f9e9a35f7d3f).
   Port the independently applicable mode bit first so it is already present
   when changing the queue. Credit paps-g `c8335e2f`. ST RM0433 Rev 8 §15.5.5,
   printed p. 659 specifies TRBUFF for UART. The preceding source writes
   `0x00000441` and fails both actual-source checks. The corrected source writes
   `0x00100441` on the first enable and on a simulated retry. Both checks pass
   ASan/UBSan, and omitting the bit from either write independently is detected.
   The committed GNU App ELF matches the verified candidate ELF. Load 61,960
   bytes, ITCM free 3,576, static DTCM 60,184 and 166 vectors are unchanged.
   The sender's direct frame grows from 72 to 80 bytes. No physical UART, audio
   contention, hardware, game or MDK qualification is claimed.
2. **MCU-SCHED-002 and MCU-UART-001 queue and recovery — published**
   [`9a9910a`](https://github.com/papsg/SAROO/commit/9a9910ab7a6d34cc2af40f384013e8811de9a406).
   Adapted from paps-g `acb824af`, with the transfer mode from `d24d860`.
   Reserve the complete message before copying, publish complete reservations
   in order, and advance the queue using stable DMA residuals. Keep accepted
   UART FIFO bytes and defer failed enables and delayed stops to a 10 ms TIM4
   one-shot. A new message is dropped whole when space runs out. Public
   adaptations preserve BOOT polling, acknowledge observed IRQ flags and keep
   the pending retry deadline. The App queue stays at 4,096 bytes.
   Baseline fails 13 of 20 methods, including wrapped/nested publication,
   premature retirement on UART TC and unbounded waits. The candidate passes
   all 20 with ASan/UBSan and O2, including 300 residual counts and a partial
   transfer at the ring boundary. Eight compiled changes are detected.
   GNU App links all 166 vectors, with strong DMA1 Stream 0 and TIM4 entries
   at 27 and 46. Load 62,012 bytes (+52), ITCM free 3,524, static DTCM 60,176
   (−8). The committed ELF matches the verified candidate. The BOOT UART
   unit compiles with the four original polling routines byte-identical.
   Physical UART/DMA timing, game behavior, and MDK output remain unqualified.

### UART transmit safety final verification

Both source checkpoints are on public main at `9a9910a`. The standard suite
passed 501 Python methods in 45 groups, 18 Rust checks in four groups, and the
native language check. The full run used the existing Pillow-enabled test
environment after the system Python run stopped at the Arabic font check.
Both MCU build receipts bind their committed source and the same 85 SDK
inputs. The final App vectors bind to the verified UART DMA and timer handlers.
The BOOT check covers its UART unit and polling code. Hardware, games and
the original MDK build remain unqualified.

The production diff is confined to the UART transmit driver. The two direct
main commits and the progress entries passed private-data and secret review.
The private HEAD, clean status, all refs and four stashes match the preceding
preservation audit. The Saturn was left untouched.

## Menu status and font safety series

Series starts at public `9a9910a`. The bounded formatter, language additions,
and PR #203 coverage are already public. [Citations, font notices and
verification](https://github.com/papsg/SAROO/blob/main/doc/MENU-STATUS-FONT.md).

1. **SATURN-FONT-002 Romanian s with comma below — published**
   [`de0699b`](https://github.com/papsg/SAROO/commit/de0699b286eb18be9eef294c610c77b43994da91).
   Adapted from paps-g `f9e04ef9` and the retained Adobe/DEC Helvetica BDF.
   Public baseline is missing U+0219. The actual C lookup and raster fixture
   checks its two-byte UTF-8 path. The source-row oracle retains the existing
   base s, a gap and a two-row comma. Complete asset regeneration matches;
   removing U+0219 recovers all 10,795 preceding records, including the
   separate CJK and Arabic contributions, exactly. Twelve focused methods
   pass. The complete pinned menu build adds 19 appended bytes for a
   488,770-byte image and preserves the ELF, body, stack and BSS. Its
   36 built inputs bind to the committed source.
2. **SATURN-FONT-003/004 legacy glyph height and row overhang — published**
   [`5d53919`](https://github.com/papsg/SAROO/commit/5d53919c768be55f65b8fd91f3bf4177592b92f9).
   Adapted from paps-g `4c2cccb4`. The baseline renderer omits U+9515, crops
   U+046E to one row and writes overhanging Latin glyph pixels beyond the row.
   Nonempty zero-height records render all 16 rows. Thirteen curated BDF
   changes keep 637 Latin code points, all distinct strokes, and all 624
   other Latin records intact. The original converter reproduces the header.
   All 11,433 indexed spans fit. Nine font methods and 20 related menu methods
   pass, including guarded actual C raster, an exact-size empty space, and
   unchanged CJK/Arabic/Romanian cases. Both compiled decoder controls fail
   as expected. The 36-input committed menu build links 79,556 code/data
   bytes (−16), with the same 40-byte raster frame, 9,232-byte BSS, 16 KiB
   stack reservation, and 488,770-byte image.
3. **SATURN-MENU-008 status drawing bounds and wording — published**
   [`fb8084d`](https://github.com/papsg/SAROO/commit/fb8084d8aeeb582fcaedbc74e92cfca05cf13459).
   Adapted from paps-g `4c2cccb4` after the font checkpoints. Clear and draw
   use the same 288-pixel area. Twelve shorter messages preserve signed
   error codes and the update/restart instructions. Baseline fails the
   twelve width subcases and both actual framebuffer regressions. Short
   and empty controls pass on both revisions. The candidate passes all
   418 status variants across 19 languages and the existing formatter
   and raster controls, including the independent Arabic oracle.
   The 36-input committed build links 79,380 code/data bytes (−176), with
   a 20-byte status frame and 488,770-byte image.

### Menu status and font safety final verification

All three source checkpoints are on public main at `fb8084d`. The standard
suite passed 511 Python methods in 45 groups, 18 Rust checks in four groups,
and the native language check. The original C converter and existing merge
tool reproduce the complete appended and Latin assets. Each checkpoint's
pinned menu build binds its 36 public inputs and matches the corresponding
verified candidate ELF and image. The final image is 488,770 bytes with
79,380 linked code/data bytes, 9,232 bytes of BSS, a 16 KiB stack reservation
and 26,076 bytes of body margin.

The production diff is confined to the Saturn menu and its font assets.
Citations, the font license, original contribution credit, and public
adaptations are in [MENU-STATUS-FONT.md](https://github.com/papsg/SAROO/blob/main/doc/MENU-STATUS-FONT.md).
The source and progress updates passed private-data and secret review. The
private HEAD, clean status, refs and stashes match the initial snapshot.
The Saturn was left untouched. Native-speaker editorial review, physical
rendering and hardware timing remain unqualified.

## CD+G repair safety series

Series starts at public `fb8084d`, adapting paps-g `fee3293f` directly in
the original CD+G utility. Original tpu and Linux Reed–Solomon credit remain.
[Source citations and validation](https://github.com/papsg/SAROO/blob/main/doc/CDG-REPAIR-SAFETY.md).

1. **HOST-CDG-004 complete packet correction — published**
   [`e03dc5a`](https://github.com/papsg/SAROO/commit/e03dc5adb6dcac15b2e24c9f01693e36d0b1f5cf).
   Stage both parity layers in a packet copy. Retain the whole failed packet,
   rebuild P after Q and preserve logical upper bits. Baseline reproduces
   all three defects, with clean and all 24 single-symbol controls passing.
   Five ASan/UBSan methods and the same O2 methods pass. Original 7,806-byte
   decoder/encoder prefix is byte-identical. Native and x86/x64 MinGW builds
   pass at the original warning policy. The legacy raw path's existing x86
   size warning remains for the next checkpoint.
2. **HOST-CDG-003 raw subchannel and endpoint preservation — published**
   [`898c8b9`](https://github.com/papsg/SAROO/commit/898c8b9d9ab0580dbe116abf3d815de5bbe2dbc5).
   Use the original interleave directly. Repair the last complete packet
   and preserve all untouched bits across the eight subchannels. The previous
   source fails ten raw cases and passes the incomplete single-frame control.
   Six focused methods pass with ASan/UBSan and O2, including eleven raw sizes
   and a clean raw file. Both MinGW architectures now pass strict compilation.
   A Win64 Wine packet smoke test also passed at the preceding checkpoint.
3. **HOST-CDG-001/002 checked file input and replacement — published**
   [`a8f2961`](https://github.com/papsg/SAROO/commit/a8f2961b444b6ad716a50d758dd586a2f0a7ed14).
   Requires both packet checkpoints. The preceding CLI reports success after
   a forced two-byte save of a 24-byte file and accepts an incomplete tail.
   Validate regular-file extents and stream in a 6,336-byte default window.
   Create staged output only on the first correction. POSIX verifies writes,
   close, sync, identity and rename, and reports post-replacement sync failure.
   macOS ACLs and extended attributes pass their tests. The MinGW entry
   point uses native staged replacement and verified backup recovery.
   macOS and Linux focused suites pass; Windows x86/x64 Wine checks cover
   early and late logical repair, raw carry, clean and invalid inputs, and
   the two modeled recovery outcomes. The committed standalone macOS build
   and both MinGW builds pass.

### CD+G repair safety final verification

All four findings are addressed by three verified commits on public main at
`a8f2961`. The standard suite passed 527 Python methods in 46 groups, 18 Rust
checks in four groups, and the native language check. The Windows-only
rollback fixture was skipped on macOS and exercised separately under Wine.
macOS focused checks pass under ASan/UBSan at 1, 16 and 64 raw frames per
batch and under O2. Linux arm64 strict compilation and 16 focused methods
pass. Both original-layout MinGW builds and both release Wine smoke runs pass.
The Wine tests run on the container filesystem. A preliminary Wine
host-volume run returned a permissions failure during staging.
Native Windows metadata, physical media and power-loss durability remain
unqualified.

The original 7,806-byte Reed–Solomon prefix is byte-identical. The port's
source, generated test recipes and documentation stay within the desktop
utility. Public-safe attribution and POSIX/Win32 references are in
[CDG-REPAIR-SAFETY.md](https://github.com/papsg/SAROO/blob/main/doc/CDG-REPAIR-SAFETY.md).
Source and progress commits passed secret and private-data review. The
private HEAD, status, refs and stashes match the initial snapshot, and the
Saturn was left untouched.

## Full cover-index lookup

**MCU-COVER-002 — published**
[`3f02379`](https://github.com/papsg/SAROO/commit/3f023799f42233d7400be2264007d0924af550f0).
Adapted from paps-g `babf1f4ca4fb5cd0a4e6163ea69dba3d23e33e53`. The
port uses the catalog/producer checks in `87ddb00` and the menu DMA
ownership in `0812ccd`. Original SAROO format and cover-tool credit,
related upstream requests, and the FatFs contract are documented.
[Source citations and reproduction](https://github.com/papsg/SAROO/blob/main/doc/COVER-INDEX-CAPACITY.md)

The `a8f2961` baseline renders records 0 and 2,047 and fails all five
tested later positions in the generated index. The port renders through
record 16,383, including catalog slot 2,558. Startup still requests 64 KiB.
A full cold lookup reads eight index pages total, retaining them until the
file reopens. The tests check compact and partial-page layouts, extent and
checksum controls, transient and latched fault recovery, cache invalidation
and the busy maximum GIF canvas. All 76 cases pass under ASan/UBSan and O2.
Both deliberately overlapping SDRAM layouts fail their compile-time guards.
The standard suite passed 532 Python methods in 46 groups, 18 Rust checks
in four groups, and the native language check. The Windows-only CD+G
fixture was skipped on macOS.

The committed GNU MCU build binds 56 public and 85 dependency inputs,
loads 62,156 bytes (+144) and leaves 3,380 ITCM bytes. Static DTCM stays
60,176 bytes and the direct cover lookup frame grows by 16 bytes to 336.
The resident index adds 448 KiB of shared SDRAM. All 36 menu inputs, the
ELF and 488,770-byte image match the baseline. Physical SD throughput,
menu pacing and hardware playback remain unqualified. The private
HEAD, status, refs and stashes match the initial snapshot. The Saturn
was left untouched.

## FatFs write safety and SD initialization

1. **MCU-FS-002 pending file sync — published**
   [`669edd8`](https://github.com/papsg/SAROO/commit/669edd85ccdfa44806be1bc96f2141f69a85b401).
   Adapted from paps-g `1816d52c`. Baseline `3f02379` drops pending sync
   in eight FAT32/exFAT cases; two clean controls pass. A failed sync or
   close now retains the handle and pending changes. All ten scenarios
   and the prior FatFs geometry/seek checks pass with ASan/UBSan and O2,
   including complete files after remount. The rejected directory write
   exercises the current production path. The injected CTRL_SYNC error
   exercises the FatFs disk contract.
   The GNU MCU link binds 56 public and 85 dependency inputs, with 62,156
   load bytes, 3,380 bytes of ITCM margin, 60,176 bytes of static DTCM and
   a 112-byte direct sync frame. [Citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/FATFS-SD-SAFETY.md#mcu-fs-002--keep-a-failed-sync-pending)
2. **MCU-FS-003 failed FAT mirrors and FSInfo — published**
   [`62d993c`](https://github.com/papsg/SAROO/commit/62d993cd1306ee1de1b5ae0a46689147affe48c9).
   Adapted from paps-g `a81828e4`, after the file-sync retry checkpoint.
   Seven mirror and FSInfo cases expose ignored errors on the baseline;
   seven primary-failure and single-FAT controls pass there. All 14
   FAT12/16/32 scenarios and the preceding FatFs/diskio checks pass with
   ASan/UBSan and O2. Both FAT copies, FSInfo and neighboring files match
   after recovery and remount. The 73 MCU save/disc methods also pass.
   The GNU MCU link adds 16 load bytes to 62,172, with 3,364 bytes of ITCM
   margin and 60,176 bytes of static DTCM. The direct sync_fs frame falls
   to 16 bytes. Physical SD durability remains unqualified.
   [Citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/FATFS-SD-SAFETY.md#mcu-fs-003--retry-failed-fat-mirrors-and-fsinfo-writes)
3. **MCU-SD-003 command wait result — published**
   [`12093f0`](https://github.com/papsg/SAROO/commit/12093f0941d4ac7614c8cf08089da3de895753cc).
   Adapted from paps-g `dc3468de` with the existing log text retained.
   Only osOK allows status interpretation and response copying.
   Baseline accepts 26 failed waits, including the null-semaphore case.
   All 43 modeled command cases and the four outer diskio methods pass
   with ASan/UBSan and O2. The GNU MCU still loads 62,172 bytes with
   3,364 ITCM bytes free, 60,176 bytes of static DTCM and a 16-byte
   command frame. Physical IRQ/DMA completion remains unqualified.
   [CMSIS, STM32, source and reproduction](https://github.com/papsg/SAROO/blob/main/doc/FATFS-SD-SAFETY.md#mcu-sd-003--reject-all-failed-command-waits)
4. **MCU-SD-004 card-ready retry budget — published**
   [`946ca0c`](https://github.com/papsg/SAROO/commit/946ca0ccead42ef8071c23e957fbfc5fdba742fb).
   Adapted from paps-g `96c9dd85` after the command-wait guard. Measure two
   seconds from the first ACMD41, counting later command latency with
   wrap-safe arithmetic. Four baseline scenarios exhaust the fixture
   guard; 21 initialization controls and the 43 command cases pass there.
   All 68 pass under ASan/UBSan and O2, and under the 37 Hz model. The
   wrong tick comparison, first-prefix charge, and fixed 200-tick budget
   controls each fail two assertions. The GNU MCU loads 62,220 bytes
   (+48), with 3,316 ITCM bytes free and 60,176 bytes of static DTCM.
   The 40-byte direct identify frame and the menu ELF/image are unchanged.
   The retry budget applies when commands return. Physical timing,
   controller abort, and SD session recovery remain outside this checkpoint.
   [SD/CMSIS/STM32 references, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/FATFS-SD-SAFETY.md#mcu-sd-004--bound-card-ready-polling)

### FatFs and SD final verification

All four firmware checkpoints are on public main at `946ca0c`. The standard
suite passed 550 Python methods in 47 groups, 18 Rust checks in four groups,
and the native language check. The Windows-only CD+G fixture was skipped
on macOS. The committed GNU App and menu builds match the verified
candidates. Both modified MCU units also compile with BOOT and the public
BootLoader RTE configuration. This is an object compile check for those
two units. Full MDK linking, physical SD timing and durability, and
controller abort and media-session recovery remain unqualified.

The series changes only the original FatFs and SD driver in firmware.
Publication scans covered the source, host fixtures, commit metadata and
ledger. The private HEAD, worktree status, refs and stashes match the initial
snapshot. The Saturn was left untouched.

## SD transfer, media launch and storage safety

1. **MCU-SD-004 transfer recovery — published**
   [`9e53888`](https://github.com/papsg/SAROO/commit/9e53888e1da77a77d77479913daf9f4e6b7843c6).
   Adapted from paps-g `433866e8`, with final historical evidence at
   `33916ab5`. Builds on the public wait and card-ready fixes. Twenty
   baseline scenarios fail and three pass. All 23 transfer cases pass
   ASan/UBSan and O2, including a public STOP-enable control. All 68 prior
   SD command/initialization cases and four diskio methods pass in both
   profiles. Eleven disc/save methods pass. The GNU App binds 56 public
   inputs and 85 SDK inputs, loads 62,364 bytes (+144) and leaves 3,172
   ITCM bytes. Static DTCM remains 60,176 bytes. Historical hardware
   evidence reaches startup. Physical SD fault recovery remains unqualified.
   [Citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SD-MEDIA-SAFETY.md#mcu-sd-004--recover-transfer-ownership)
2. **SATURN-CD-001 menu CD transfer and player admission — published**
   [`8bebab7`](https://github.com/papsg/SAROO/commit/8bebab790be64736a2d1f90468807f03e37de700).
   Adapted from paps-g `0b9dc270` and related to upstream issue #175.
   Baseline fails 20 of 33 cases. All 33 pass ASan/UBSan and O2, including
   TOC and DataEnd failures, fast PEND, valid game headers, audio and both
   player callback modes. Existing menu and cover display tests pass.
   The menu build binds 36 public inputs and adds 128 linked bytes to
   79,508. BSS and the 488,770-byte image size stay the same. Physical
   BIOS, drive and player behavior remain unqualified.
   [Sega references, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/SD-MEDIA-SAFETY.md#saturn-cd-001--check-menu-cd-transfers-and-identify-audio-first)
3. **MCU-DISC-005 non-game image and save admission — published**
   [`8e23954`](https://github.com/papsg/SAROO/commit/8e2395467d8c81017f7b3ab87d7fb5104aa5853f).
   Adapted from paps-g `ec0c0e93` after the menu part of upstream #175.
   Five generated CUE/ISO cases and two actual save-session cases fail
   on baseline. Both signed-game controls pass there. All new cases
   pass with ASan/UBSan and O2. The actual parser keeps only global
   runtime settings, and the active save and pending retry survive with
   zero archive I/O. Existing header, runtime and configuration checks
   pass. The GNU App adds 48 bytes to 62,412 with 3,124 bytes of ITCM
   margin. Physical player and reset behavior remain unqualified.
   [Sega IP layout, attribution and tests](https://github.com/papsg/SAROO/blob/main/doc/SD-MEDIA-SAFETY.md#mcu-disc-005--classify-images-before-selecting-a-game-save)
4. **MCU-FS-004 write-abort pending state — published**
   [`4e4bdf0`](https://github.com/papsg/SAROO/commit/4e4bdf03c117897795adaf17b9d1856c3126800f).
   Adapted from paps-g `0e259a3a` after the prior file-sync retry fix.
   Six of 12 FAT32/exFAT scenarios fail on baseline. All pass in both
   profiles after the port, including cached prefix and close retry,
   contiguous and fragmented growth, and complete remounted file and
   neighbor comparisons. The 16-method FatFs suite and 12 save-runtime
   methods pass. The GNU App remains 62,412 bytes and the direct write
   frame remains 56 bytes. Physical SD durability remains unqualified.
   [FatFs references, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/SD-MEDIA-SAFETY.md#mcu-fs-004--keep-completed-writes-pending-after-an-abort)
5. **MCU-SD-005/006 SD identity and completed initialization — published**
   [`9b68a9a`](https://github.com/papsg/SAROO/commit/9b68a9a1f14e9605f6b79c05c002d3fc931a4719).
   Adapted from paps-g `613efd17` on the transfer-recovery checkpoint.
   The address remains local until all required commands succeed. The
   existing filesystem mutex and the disk I/O semaphore now cover automatic
   reidentification. Baseline fails 35 identity, three startup and two
   handoff cases. All 40 identity, 25 transfer, seven new disk-lock cases
   and 13 disc/save methods pass both profiles. Both ownership omissions
   fail their compiled controls. The GNU App adds 208 bytes to 62,620,
   with 2,916 bytes of ITCM margin and 60,184 bytes of static DTCM.
   Physical card replacement and fault recovery remain unqualified.
   [Source, CMSIS reference and reproduction](https://github.com/papsg/SAROO/blob/main/doc/SD-MEDIA-SAFETY.md#mcu-sd-005-and-mcu-sd-006--bind-sector-access-to-an-initialized-card)

### SD transfer, media launch and storage final verification

All five source checkpoints are on public main at `9b68a9a`. The standard
suite passed 578 Python methods in 50 groups, 18 Rust checks in four groups,
and the native language check. The Windows-only CD+G fixture was skipped
on macOS. The committed GNU MCU and menu builds match the verified
candidates. The final MCU load is 62,620 bytes with 2,916 bytes of ITCM
margin and 60,184 bytes of static DTCM. The menu links 79,508 code/data
bytes and retains the 488,770-byte image size. SDIO, diskio and FatFs
also compile as objects with the original BootLoader RTE configuration
and BOOT define. Full MDK linkage, physical SD fault recovery, SD
durability and CD playback remain unqualified.

The public changes add no runtime log formats. Source, fixtures, commit
metadata and ledger additions passed secret and private-data review.
The private HEAD, status, refs and stashes match the starting snapshot.
The Saturn was left untouched.

## Firmware reliability follow-up

1. **Shared CD command filter lookup — published**
   [`87e3a83`](https://github.com/papsg/SAROO/commit/87e3a8375402f05549aaf3fb8d624ee39b489fa9).
   Adapted from paps-g `31326563`. All 156 CD command and buffer methods
   pass on the public baseline and the port. The port passes ASan/UBSan
   and O2. The GNU App load falls 80 bytes to 62,540 and static DTCM is
   unchanged. Physical command timing remains unmeasured.
   [Sega citation, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#shared-cd-command-filter-lookup)
2. **Shared SD sector transfers — published**
   [`3893aa5`](https://github.com/papsg/SAROO/commit/3893aa525b7fb36c4f2d1b6244fce3130b188515).
   Adapted from paps-g `31326563`. The baseline and port pass 16
   direct and bounced copy and error cases. All 28 storage driver
   methods pass ASan/UBSan and O2. The GNU App load falls 64 bytes
   to 62,476. Static DTCM and the 4,096-byte bounce buffer remain
   unchanged. Physical SD timing and fault recovery remain unmeasured.
   [STM32 citation, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#shared-sd-sector-transfers)
3. **Retry aborted save file handles — published**
   [`0b65442`](https://github.com/papsg/SAROO/commit/0b65442a153eeb22031e46c9e9dbf0f8e2b83d11).
   Adapted from paps-g `d5cb72ea` with the public extent and SD ownership
   fixes. The baseline fails 56 of 60 generated FAT32/exFAT recovery
   cases. All pass on the port, including pending writes, failed close
   and open, replacement and structural controls. All 115 save and SD
   methods pass ASan/UBSan and O2. The GNU App load rises 400 bytes to
   62,876 and SDIO and diskio compile with the BootLoader configuration.
   Physical SD recovery and power loss durability remain unqualified.
   [FatFs citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#recover-aborted-save-handles-through-ordinary-requests)
4. **Companion ROM admission — published**
   [`a1f81a2`](https://github.com/papsg/SAROO/commit/a1f81a271af26156a2e298dbc78ceca46e644cb2).
   Adapted from paps-g `198a079f` after the checked file and CD transfer
   ports. The old callbacks fail 21 missing, failed or short-read cases
   and pass three complete reads. All 39 ROM cases, 15 coupled launch
   controls and 29 menu and cover methods pass ASan/UBSan and O2.
   The menu adds 176 code/data and 4 BSS bytes. Both relocated helpers
   fit their aligned 96-byte slots. Physical launch remains unqualified.
   [Sega citation, upstream issues and reproduction](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#check-companion-roms-before-bios-handoff)
5. **FPGA menu timing with corrected CS2 access — published**
   [`03f68cf`](https://github.com/papsg/SAROO/commit/03f68cf78126f2f9079d6f5f31924188b2bcd0ae).
   Adapted from paps-g `fb67ac50` and `86e2af4e` together. The old
   getter loses one full FRT period in the two-wrap control. The old
   CS2 alias fails four read cases. All 38 menu and peripheral methods
   pass with sanitizers and 27 menu methods pass at O2, including the
   public FPGA 05 register simulation. The menu shrinks 96 linked
   bytes and the image size is unchanged. Historical controller
   recovery covers one Saturn. Wider physical timing remains open.
   [Sega references, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#use-the-fpga-counter-for-menu-timing)
6. **Explicit save acknowledgment — published**
   [`fa89104`](https://github.com/papsg/SAROO/commit/fa89104e9440e0e50aafb3414e267397108d4577).
   Adapted from paps-g `74999ab0` as a paired menu and MCU change.
   The baseline accepts cleared registers as success. The new
   protocol passes five host and RTL methods, including every 16-bit
   opcode and full-width MEMS arguments. All 59 save and file transfer
   methods pass at O2. The GNU App adds 64 bytes to 62,940. The menu
   load is unchanged and BSS grows four bytes. Both legacy modes work
   in the host model. Physical reset and session recovery remain open.
   [Original FPGA logic, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#acknowledge-completed-saves-explicitly)

### Firmware reliability final verification

All five checkpoints are on public main as six commits ending at `fa89104`.
The standard suite passed 601 Python methods across 53 groups, 18 Rust
checks and the native language check. The Windows-only CD+G fixture was
skipped on macOS. The menu and GNU App rebuilt from the committed source
match the verified menu image and both ELFs byte for byte.

The GNU App load is 62,940 bytes with 2,596 bytes of ITCM margin and
60,232 bytes of static DTCM. The menu links 79,588 code/data and 9,236 BSS
bytes and retains the 488,770-byte image. The public FPGA 05 timer and
save register simulations pass. Physical SD recovery, game launch, and
timing and reset behavior across hardware variants remain unqualified.

Publication review covered the source, fixtures, attribution, commit
metadata and ledger. The private HEAD, refs, stashes and status match the
initial read-only snapshot. The Saturn was left untouched.

## Firmware I/O and cache follow-up

1. **Shared CUE parser checks — published**
   [`4c30063`](https://github.com/papsg/SAROO/commit/4c3006303e2196a2e1f8f5010cabc25612b5fa15).
   Adapted from paps-g `8ef2ac7f` after the public CUE safety and layout
   ports. The baseline and port pass all 37 host methods, including 42
   indexed-sector and 25 malformed-mode controls. The port passes
   ASan/UBSan and O2. The GNU App load decreases by 8 bytes to 62,932.
   [Contracts, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/IO-CACHE-FOLLOWUP.md#share-cue-parser-checks)
2. **Direct file-read cache invalidation — published**
   [`07424e1`](https://github.com/papsg/SAROO/commit/07424e17c16f5b7bf9cd33d119491015ac2fd0b9).
   Adapted from paps-g `8078e501` on the public bounded file-transfer API.
   The baseline fails 48 stale-line cases. All 415 cache scenarios pass,
   with 29 sanitizer and 28 O2 transfer/menu methods. The linked menu
   grows 80 bytes to 79,668. Physical cache and interrupt timing remain
   unqualified.
   [SH7604 manual, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/IO-CACHE-FOLLOWUP.md#invalidate-direct-file-read-cache-lines)
3. **Shared CD command replies — published**
   [`1bd8773`](https://github.com/papsg/SAROO/commit/1bd8773856dd09fa164d73db8cef4e9ef777366d).
   Adapted from the CD portion of paps-g `48f6a105`. Baseline and port
   pass 1,179,648 ordered replies. All 157 CD command and buffer methods
   pass ASan/UBSan and O2. A reversed tail-write control fails. The GNU App
   load falls 64 bytes to 62,868. Physical command timing remains unmeasured.
   [Sega citation, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/IO-CACHE-FOLLOWUP.md#share-complete-cd-command-replies)
4. **FPGA memory reset acknowledgment — published**
   [`24f1c39`](https://github.com/papsg/SAROO/commit/24f1c39799dd6ea18176e375878608a1ffc254a1).
   Adapted from paps-g `e345a327`. The exact baseline reproduces early
   write completion on both ports and passes six other reset controls.
   All 32 corrected reset/port/mask cases pass. Each omitted assignment
   fails on its own port. Quartus binds all 18 project inputs, fits 1,479
   logic elements and reports positive constrained setup/hold slack.
   External bus timing and physical reset remain unqualified.
   [Original source, reproduction and fit](https://github.com/papsg/SAROO/blob/main/doc/IO-CACHE-FOLLOWUP.md#clear-fpga-memory-acknowledgments-on-reset)
5. **Shared CD filter resets and fixed sector lengths — published**
   [`ce86de1`](https://github.com/papsg/SAROO/commit/ce86de1cabf3de36b5293235082c26e4b3ab52df).
   Adapted from paps-g `bc1f69cc`. The parent and port pass 131,072
   filter command cases and 65,536 sector-length cases. All 159 CD
   command/buffer methods pass ASan/UBSan and O2. The public GNU App
   load falls 72 bytes to 62,796 and static DTCM falls 16 bytes.
   [Sega references, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/IO-CACHE-FOLLOWUP.md#share-filter-condition-resets-and-sector-lengths)
6. **Fixed-length artwork checksum — published**
   [`f0c2398`](https://github.com/papsg/SAROO/commit/f0c239852e926631fa06af977b992a2618549cff).
   Adapted from paps-g `11836b8c`. The old and new implementations
   match zlib for 20,760 title fields at eight alignments. All 20 cover
   methods pass ASan/UBSan and O2. The GNU App and static DTCM sizes
   remain 62,796 and 60,216 bytes. Hardware timing remains unmeasured.
   [Adler-32 reference, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/IO-CACHE-FOLLOWUP.md#simplify-the-fixed-length-cover-checksum)
7. **Explicit file-transfer acknowledgment — published**
   [`f04106d`](https://github.com/papsg/SAROO/commit/f04106d7ed9dc25c800df97728ba8f6c3cf0e8a9).
   Adapted from paps-g `08226bbe` on the public cache and SAVE1
   prerequisites. The parent reports false success in eight reset
   cases and 16 direct-cache cases. The port passes 34 file methods
   and five save methods with ASan/UBSan and O2. Nine controls pass
   for each mixed-version pairing. All 65,536 opcodes are covered.
   The GNU App is 62,812 bytes, and the linked menu uses 79,716
   code/data bytes. The menu image stays at 488,770 bytes.
   Physical reset and recovery remain unqualified.
   [Original register logic, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/IO-CACHE-FOLLOWUP.md#confirm-file-transfers-explicitly)
### Firmware I/O and cache final verification

All seven checkpoints are on public main as seven commits ending at
[`f04106d`](https://github.com/papsg/SAROO/commit/f04106d7ed9dc25c800df97728ba8f6c3cf0e8a9).
The standard `make -C tests check` suite passes 625 Python methods in
58 groups, 18 Rust checks, and the native language check. macOS skips
the Windows-only CD+G fixture.

Rebuilding the committed source reproduces both verified ELFs and the
menu image byte for byte. The GNU App load is 62,812 bytes, with 2,724
bytes of ITCM margin and 60,216 bytes of static DTCM. The menu links
79,716 code/data and 9,240 BSS bytes into a 488,770-byte image.
All 18 fitted FPGA project inputs match the final tree. Quartus reports
positive constrained setup and hold slack. External bus timing and
physical reset and recovery remain unqualified.

Publication review covers the seven commits, tests, citations,
attribution and ledger. The private repository's HEAD, worktree,
refs and stash match the initial read-only snapshot. The Saturn
was left untouched.

## Save ownership and expansion RAM follow-up

1. **Compact configuration parser — published**
   [`b769aa6`](https://github.com/papsg/SAROO/commit/b769aa64c70aea414e0dd246d694e2b057752e59).
   Adapted from paps-g `3ec942a4` after the public configuration safety
   series. The exact parent and the port pass 20 methods, including 328
   loader cases. The port passes ASan/UBSan and O2. Linked Arm code uses
   UMLAL in place of UDIV in the digit loop. The GNU App falls 60 bytes
   to 62,752, and static DTCM remains 60,216 bytes. Hardware load latency
   remains unmeasured.
   [Source contract, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-OWNERSHIP-RAM.md#compact-the-configuration-parser)
2. **Bounded MCU access through primary save handles — published**
   [`3d87029`](https://github.com/papsg/SAROO/commit/3d87029b7bbc0f34c9008bf71a5010f7af18a347).
   Adapted from paps-g `b7720d00` on the public save reopen and tagged
   file protocol. The prior MCU leaves the new commands pending. Both
   filesystems detect omitted revocation on a failed MEMS root read.
   The port passes 44 FatFs image cases and 85 protocol cases. All 20
   focused methods pass ASan/UBSan and O2, with 46 passing in the broader
   O2 run. The GNU App grows to 63,184 bytes and keeps 60,216 bytes of
   static DTCM. Physical recovery remains unqualified.
   [Protocol, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-OWNERSHIP-RAM.md#access-the-existing-save-archive-handles)
3. **Menu archive transport and full-key slot lookup — published**
   [`1b78410`](https://github.com/papsg/SAROO/commit/1b784102cae97fb0f544eda062698c1685dc50e1).
   Adapted from paps-g `3f6e0121` on the bounded MCU owner and public
   file/cache paths. Sixteen paired FAT32 and exFAT cases pass under
   ASan/UBSan and O2. Omitting the exact count accepts four short replies.
   All 32 focused O2 methods pass. The linked menu adds 528 bytes to
   80,244, with unchanged BSS and padded image size. The existing BIOS
   save path and four-argument file ABI are covered by ordinary controls.
   [Protocol, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-OWNERSHIP-RAM.md#pair-the-menu-with-the-primary-save-owners)
4. **Optional menu command completion — published**
   [`3ffee52`](https://github.com/papsg/SAROO/commit/3ffee5246e0124de9c10fdd3d3bd7631debfec31).
   Adapted from paps-g `36c4de4b` after tagged save, file and archive
   support. The parent fails all five new handler checks. The port
   covers all 65,536 opcodes, 131,072 selection arguments and 360
   startup cases. The original public FPGA passes 28 register cases.
   The 37-method ASan/UBSan and 47-method O2 runs pass. The GNU App
   adds 48 bytes to 63,232. Ordinary menu callers continue to use their
   legacy requests. Physical command timing remains unmeasured.
   [Register sources, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-OWNERSHIP-RAM.md#offer-optional-acknowledgments-for-menu-commands)
5. **Cache invalidation for 1 MiB expansion RAM mirrors — published**
   [`39911f6`](https://github.com/papsg/SAROO/commit/39911f6a46c4ee06738807de102e3824404a4c5d).
   Adapted from the menu portion of paps-g `855d0092`. The exact parent
   fails 75 cases. Reading the mode after completion fails all 15 reset
   cases. The port passes 583 cache scenarios and all 24 focused methods
   under ASan/UBSan and O2. The linked menu adds 64 bytes to 80,308.
   Physical cache and bus timing remain unmeasured.
   [FPGA and SH7604 references, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-OWNERSHIP-RAM.md#invalidate-every-expansion-ram-cache-mirror)
6. **MCU expansion RAM address correction — published**
   [`f1ac464`](https://github.com/papsg/SAROO/commit/f1ac4641589fc8390c4f3986b0a54fdd89d8f5f2).
   Adapted from paps-g `d1079400` and the MCU candidate in `855d0092`.
   The exact parent fails 12 named-file mirror cases and all four
   paired archive runs. The port passes 39 named-file cases and all
   four mirrored archive runs. All 58 focused methods pass ASan/UBSan
   and O2, including native buffers and later-span failures. The GNU
   App is 63,424 bytes with the original case table, leaving 2,112
   bytes of ITCM. Physical behavior remains unqualified.
   [Decoder sources, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-OWNERSHIP-RAM.md#map-mcu-transfers-to-the-saturn-visible-ram)

   **Separate FatFs case table commit within checkpoint 6 — published**
   [`2fd9f3a`](https://github.com/papsg/SAROO/commit/2fd9f3ad530b12f157e54682f61e5fe2362259e2).
   Adapted from the table portion of paps-g `d1079400`. All 1,114,112
   Unicode values and five out-of-range controls match the compiled
   pinned routine byte for byte. All 874 changed mappings are retained.
   Three methods pass under ASan/UBSan and O2. The mapping-only image
   fits with 2,112 bytes to spare. The table saves another 180 bytes
   for a 63,244-byte GNU App and 2,292 bytes of ITCM margin.
   [Pinned source, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/SAVE-OWNERSHIP-RAM.md#compact-the-pinned-fatfs-case-table)

   **Disc test fixture correction**
   [`a557905`](https://github.com/papsg/SAROO/commit/a557905dbcfff904e8b7e22c3b83cd4fbf12b21b).
   The full suite exposed a duplicate transfer-helper definition in
   the disc fixture's configuration slice. The slice now ends at
   the actual parser. All 13 disc and save session methods pass
   after the correction. This commit changes the host fixture.

   **Shared configuration fixture boundary**
   [`dc5e079`](https://github.com/papsg/SAROO/commit/dc5e0798b4992b4afb59641581da2daf5ebda615).
   The standalone loader and disc fixtures now extract the same matcher
   and parser through the end of parse_config. All 28 combined methods
   pass ASan/UBSan and O2. The preceding parser also passes all 15
   standalone loader methods through the shared fixture.

### Save ownership and expansion RAM final verification

All six checkpoints are on public main as seven firmware commits.
Two test fixture corrections bring main to
[`dc5e079`](https://github.com/papsg/SAROO/commit/dc5e0798b4992b4afb59641581da2daf5ebda615).
The full `make -C tests check` suite passes 662 Python tests in
62 groups, 18 Rust checks and the native language check.
macOS skips the Windows-only CD+G fixture.

The MCU and SH-2 menu rebuilds reproduce both ELFs and the menu image
byte for byte. The final public tree matches all 56 tracked MCU inputs
and all 36 menu inputs from the rebuilds. The GNU App is 63,244 bytes,
with 2,292 bytes of ITCM margin and 60,216 bytes of static DTCM.
The menu links 80,308 code/data and 9,240 BSS bytes into the
488,770-byte image. The mapping-only image fit with 2,112 bytes
of ITCM margin before the separate FatFs compaction.

The original FPGA command register model passes its simulated
transport cases. Physical transfer timing, save recovery, and game
behavior remain unqualified. Publication review covered the source,
fixtures, provenance, commit metadata and ledger. The private HEAD,
worktree, refs and stash match the initial snapshot. The Saturn was
left untouched.

## CD startup, transfer and save metadata follow-up

1. **Stopped-drive subcode — published**
   [`36e82b0`](https://github.com/papsg/SAROO/commit/36e82b0f65411f632837f9b873e1857196c26424).
   Adapted from paps-g `555eb231`. The parent fails three new checks and
   passes the valid-payload control. The port passes all four under
   ASan/UBSan and O2 and all 163 CD group checks. The GNU MCU App falls
   to 63,212 bytes, with 2,324 bytes of ITCM margin. Physical report timing
   and multisession behavior remain unqualified.
   [Sega references, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-STARTUP-TRANSFER-SAFETY.md#read-subcode-safely-after-a-drive-stop)

2. **CD query and authentication failures — published**
   [`e411cc4`](https://github.com/papsg/SAROO/commit/e411cc44342373228a0e2ac73926f43a0f191fe6).
   Adapted from paps-g `544f2b10`. The parent fails four new methods
   and passes the successful decoding control. All 13 query and CD
   player methods pass ASan/UBSan and O2, including 3,072 successful
   decodes and first-error cleanup precedence. The linked menu is
   80,452 bytes with unchanged BSS and padded image size. Physical
   authentication and wait timing remain unqualified.
   [Sega reference, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-STARTUP-TRANSFER-SAFETY.md#propagate-cd-query-and-authentication-failures)

3. **Save overwrite metadata — published**
   [`a9c36ba`](https://github.com/papsg/SAROO/commit/a9c36ba789c24abf09e943de326ccdaac89f29d2).
   Adapted from paps-g `d00d9efe`. The parent fails 23 overwrite
   cases and ten coupled retries while eleven rejections preserve both
   archives. All 44 focused methods pass ASan/UBSan and O2. The SH-2
   menu stays at 80,452 code/data bytes, with a 44-byte direct writer
   frame. Physical recovery and the Policenauts disc change remain
   unqualified.
   [Sega and issue references, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-STARTUP-TRANSFER-SAFETY.md#refresh-comment-and-language-on-save-overwrite)

4. **CD transfer bounds and failures — published**
   [`6546ba6`](https://github.com/papsg/SAROO/commit/6546ba6b88800c375d45190b88ddd8f04633f5fc).
   Adapted from paps-g `4f7cd16b`. The parent fails eight of nine
   transfer methods, including the 405-byte extent, and passes the
   ordinary command control. All 22 CD methods pass ASan/UBSan and O2.
   Cleanup requires a transfer request in the accepted reply. The menu
   links 80,756 code/data and 9,244 BSS bytes. Physical transfers and
   Kaitei Daisensou music remain unqualified.
   [Sega and upstream sources, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-STARTUP-TRANSFER-SAFETY.md#bound-cd-transfers-and-preserve-failures)

5. **CD startup admission — published**
   [`23be646`](https://github.com/papsg/SAROO/commit/23be646fe08246381743da35a908462f0f7f9e73).
   Adapted from the production loader portion of paps-g `56980390`.
   The parent fails four of six startup methods and passes both
   healthy controls. All 28 CD methods pass ASan/UBSan and O2,
   covering 90 failed boot combinations and 36 observed media
   transitions. The menu links 80,772 code/data and 9,244 BSS bytes.
   Physical BIOS and drive transitions remain unqualified.
   [Sega and upstream sources, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-STARTUP-TRANSFER-SAFETY.md#stop-cd-startup-after-failed-initialization)

6. **BIOS executable request admission — published**
   [`e8f6830`](https://github.com/papsg/SAROO/commit/e8f6830b073ce497c6104f40a138b1e2818b9fd2).
   Adapted from paps-g `d3d124d3`. The parent fails three of five
   handoff methods and passes the healthy and early-failure controls.
   The port passes all 33 CD methods under ASan/UBSan and O2 and five
   companion-ROM methods. The menu links 80,788 code/data and 9,244
   BSS bytes. BIOS execution and asynchronous read completion remain
   unqualified on hardware.
   [Sega and upstream sources, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-STARTUP-TRANSFER-SAFETY.md#gate-bios-handoff-on-executable-request-admission)

### CD startup, transfer and save metadata final verification

All six checkpoints are on public main at
[`e8f6830`](https://github.com/papsg/SAROO/commit/e8f6830b073ce497c6104f40a138b1e2818b9fd2).
The full `make -C tests check` run with the pinned Pillow dependency
passes 693 Python methods, 18 Rust checks and the native language check.
The Windows-only CD+G replacement fixture is skipped on macOS.

The final committed sources reproduce the SH-2 ELF and image and the
GNU MCU ELF byte for byte. All 36 tracked menu inputs and 56 tracked
MCU inputs match the build receipts. The 488,770-byte menu image contains
80,788 code/data bytes and 9,244 BSS bytes, with 24,656 bytes of body
margin. The GNU MCU App loads 63,212 bytes with 2,324 bytes of ITCM
margin and 60,216 bytes of static DTCM.

Existing serial callers check query results and use the received file
count before reading those outputs. Physical CD timing, game music,
media recovery and BIOS execution remain unqualified. The private
HEAD, worktree, refs and stash match the initial read-only snapshot.
The Saturn was left untouched.

## CD events and game launch

1. **Terminal CD waits — published**
   [`6be7daa`](https://github.com/papsg/SAROO/commit/6be7daa428eb0664c365fb0a1bbb3d2bbbef8fb5).
   Adapted from the production paths of paps-g `80364f8f`. Two parent
   fixtures observe extra polls. All 35 CD methods pass ASan/UBSan and O2,
   including partial output, buffered sectors and authentication cleanup.
   The menu links 80,820 code/data and 9,244 BSS bytes. Physical wait time
   and drive recovery remain unqualified.
   [Sega citation, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-EVENTS-AND-GAME-LAUNCH.md#exit-cd-waits-on-terminal-drive-states)

2. **VDP1 game patch order — published**
   [`9cd3b4b`](https://github.com/papsg/SAROO/commit/9cd3b4b87f73f1f605898c07fda1fd194719e7db).
   Adapted from paps-g `74bee684`. The parent erases Croc and configured
   writes and passes five controls. Seven whole-VRAM methods pass both
   profiles, including Ultraman takeover and KOF95. Ten adjacent ROM and
   BIOS handoff methods pass. The menu links 80,852 code/data and 9,244 BSS
   bytes. Physical game rendering remains unqualified.
   [Sega and upstream citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-EVENTS-AND-GAME-LAUNCH.md#preserve-vdp1-writes-during-game-launch)

3. **DataEnd readiness ownership — published**
   [`51fa0c1`](https://github.com/papsg/SAROO/commit/51fa0c12bfb355c51309f811b7d527485cc99ecb).
   Adapted from paps-g `3d10ea73`. The parent fails three methods,
   including late DRDY during a timed-out cleanup. The successful-count
   control passes on both. All 38 CD methods pass ASan/UBSan and O2.
   The menu retains 80,852 code/data and 9,244 BSS bytes. Physical CD
   interrupt timing remains unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-EVENTS-AND-GAME-LAUNCH.md#preserve-readiness-when-dataend-fails)

4. **Sector-read completion at DataEnd — published**
   [`982ab77`](https://github.com/papsg/SAROO/commit/982ab7748a338698ee592cb59403ae5777bf812b).
   Adapted from paps-g `35cf0b88`. The parent publishes EHST during GET
   admission. The port passes 12 early and full transfer scenarios and
   both 164-method CD runs. The GNU MCU App retains its 63,212-byte
   load and 2,324-byte ITCM margin. Physical game event timing remains
   unqualified.
   [Sega citation, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-EVENTS-AND-GAME-LAUNCH.md#signal-sector-read-completion-at-dataend)

5. **Signed disc-launch errors — published**
   [`850065c`](https://github.com/papsg/SAROO/commit/850065c24f6567589b765b85d3350f06b557d674).
   Adapted from the menu portion of paps-g `218abca2`. Both parent
   launch paths display 65530 for MCU error −6. Each corrected path
   passes all 65,536 reply words under ASan/UBSan and O2. The 66-method
   menu safety group passes. The menu links 80,836 code/data and 9,244
   BSS bytes. Physical disc loading remains unqualified.
   [Upstream report, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-EVENTS-AND-GAME-LAUNCH.md#preserve-signed-disc-launch-errors)

6. **File completion across a full CD buffer — published**
   [`21d9187`](https://github.com/papsg/SAROO/commit/21d918794c04cc854cb2981cce9b4b2d1c8b5a8e).
   Adapted from paps-g `a506699c`. The parent signals EFLS during
   three pressure scenarios and passes normal completion. The port
   passes all 168 CD methods and all 13 XA methods at O2, including
   38,208 full-buffer pauses. The GNU MCU App loads 63,196 bytes,
   with 2,340 bytes of ITCM space. Physical scheduling remains unqualified.
   [Sega citation, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-EVENTS-AND-GAME-LAUNCH.md#keep-a-file-read-pending-while-the-cd-buffer-is-full)

**Save boot fixture follow-up — published**
[`43a2fb8`](https://github.com/papsg/SAROO/commit/43a2fb8a19c5ca1a776374100030e902440cda12). The first
full run found that the older CD player fixture expected the raw mailbox
word for −1. The fixture now checks signed results and retains its boot
gates. Both save boot methods pass ASan/UBSan and O2. This updates the
host fixture.

### CD events and game launch final verification

The six firmware ports are published through
[`21d9187`](https://github.com/papsg/SAROO/commit/21d918794c04cc854cb2981cce9b4b2d1c8b5a8e).
The full `make -C tests check` run on
[`43a2fb8`](https://github.com/papsg/SAROO/commit/43a2fb8a19c5ca1a776374100030e902440cda12)
passes 712 Python methods, 18 Rust checks and the native language check.
The Windows-only CD+G replacement fixture is skipped on macOS.

The clean firmware revision reproduces the menu ELF and image and the
GNU MCU ELF byte for byte. All 36 tracked menu inputs and 56 tracked MCU
inputs match the receipts. The final test commit changes one host
fixture. The 488,770-byte menu contains 80,836 code/data bytes and 9,244
BSS bytes, with 24,608 bytes of body margin. The GNU MCU App loads 63,196
bytes with 2,340 bytes of ITCM margin and 60,216 bytes of static DTCM.

The private HEAD, worktree, refs and stash match the starting snapshot.
Physical CD event timing and game rendering remain unqualified. The
Saturn was left untouched.

## CD playback and media state

1. **Retained SEEK playback range — published**
   [`fa6dabb`](https://github.com/papsg/SAROO/commit/fa6dabb12eb4eba5019fcc349ba8db973d871427).
   Adapted from paps-g `4f123d38` (MCU-CDC-023) after
   `3a68c3d` and `d78c74f`. Three parent methods reproduce range
   replacement and three valid controls pass. Both public CD profiles
   pass 170 methods, including 63 endpoint/seek combinations and 162
   authored resume sequences. The GNU MCU App loads 63,164 bytes
   with 2,372 bytes of ITCM space. Named game behavior and physical
   timing remain unqualified.
   [Sega and upstream citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-PLAYBACK-STATE.md#preserve-the-playback-range-across-seek)

2. **SEEK completion and BUSY reply — published**
   [`de03534`](https://github.com/papsg/SAROO/commit/de035340203933076c8734fed613b72ec8837fc5).
   Adapted from paps-g `13f35e65` (MCU-CDC-024/025) after the
   retained SEEK range. Three parent cases miss PEND or report the
   settled status in the command reply. The no-event control passes.
   Both CD profiles pass 174 methods. The GNU MCU App loads
   63,196 bytes with 2,340 bytes of ITCM space. Actual interrupt
   timing and game waits remain unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-PLAYBACK-STATE.md#report-seek-completion-and-status)

3. **Pickup-preserving PLAY and delayed resume — published**
   [`3117a02`](https://github.com/papsg/SAROO/commit/3117a022d1cc121d0bd3032a0e052cd7a59e2978).
   Adapted from paps-g `b8c71ee2` (MCU-CDC-026/027/028) after
   the SEEK changes. Eight parent continuity methods fail and the
   delayed-start/read-error control passes. All nine methods exercise
   the actual disk task and command with modeled host preemption.
   Both CD profiles pass 183 methods and 13 disc/save handoff methods
   pass. The GNU MCU App loads 63,228 bytes with 2,308 bytes of ITCM
   space. Physical scheduling, audio timing and game playback remain
   unqualified.
   [Sega and upstream citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-PLAYBACK-STATE.md#continue-playback-through-a-pickup-preserving-play)

4. **File-abort reply and reader cache — published**
   [`45a952d`](https://github.com/papsg/SAROO/commit/45a952dff5a4966081c389395867746d2c3a2048).
   Adapted from paps-g `6deb9795` (MCU-CDC-029/030) using the
   public same-disc pause. The parent leaves stale reply words and
   invalidates read-ahead. The timeout and late-acknowledgment control
   passes on both. Both CD profiles pass 186 methods. The GNU MCU App
   loads 63,228 bytes with 2,308 bytes of ITCM space. Physical command
   and interrupt timing remain unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-PLAYBACK-STATE.md#return-the-complete-file-abort-response)

5. **Disc-default range after file access — published**
   [`b937ded`](https://github.com/papsg/SAROO/commit/b937ded80df4ac42a435103c1c2afe59fc6a9fb7).
   Adapted from paps-g `0b8d2172` (MCU-CDC-031) on the public
   SEEK, PLAY and file-abort behavior. The parent reuses file bounds
   for PLAY and SEEK and on media retry. Two ownership and
   background-audio controls pass on both revisions. Both CD profiles
   pass 191 methods. The GNU MCU App loads 63,276 bytes with 2,260
   bytes of ITCM space. Physical game behavior remains unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-PLAYBACK-STATE.md#use-the-disc-playback-range-after-file-access)

6. **Unavailable TOC and session data — published**
   [`932a0d2`](https://github.com/papsg/SAROO/commit/932a0d2cadfa359e3515713564dd288a88fe8512).
   Adapted from paps-g `748bfd8d` (MCU-CDC-032/033). The parent
   returns stale metadata in two methods. Mounted TOC, busy-transfer
   and valid session controls pass on both revisions. The port checks
   19,456 session replies and 204-word FIFO publication. Both CD profiles
   pass 196 methods. The GNU MCU App loads 63,308 bytes with 2,228
   bytes of ITCM space. Physical tray transitions and multi-session
   behavior remain unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-PLAYBACK-STATE.md#return-unavailable-toc-and-session-metadata)

### CD playback and media state final verification

The six direct checkpoints are published through
[`932a0d2`](https://github.com/papsg/SAROO/commit/932a0d2cadfa359e3515713564dd288a88fe8512). Both CD
profiles pass 196 methods. The full `make -C tests check` run passes
740 Python methods, 18 Rust checks and the native language check. The
Windows-only CD+G replacement fixture is skipped on macOS.

The published source reproduces both ELFs and the SH-2 menu image.
All 36 tracked menu inputs and 56 tracked MCU inputs match the build
receipts. The 488,770-byte menu contains 80,836 code/data bytes and
9,244 BSS bytes with 24,608 bytes of body space. The GNU MCU App loads
63,308 bytes with 2,228 bytes of ITCM space and 60,216 bytes of static
DTCM.

The private checkout's HEAD, branch, refs, stash and worktree status
match the initial read-only snapshot. Validation was on the host.
Physical disc timing, audio behavior and game compatibility remain
unqualified. [Sega citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/CD-PLAYBACK-STATE.md)

## CD power and timing

1. **CD power and verified takeover — published**
   [`680f73f`](https://github.com/papsg/SAROO/commit/680f73f6f5b61070d82b2f9f71e6d338faf65094).
   Adapted from paps-g `ce4de2ff` (SATURN-CD-003). Twelve parent
   methods reproduce stalled waits, ignored replies or caller errors.
   Two direct healthy controls pass. The full menu safety group passes
   80 sanitizer methods and the CD and launch groups pass 54 optimized
   methods. The SH-2 menu links 81,172 code/data bytes with 24,272 bytes
   of body space. Physical timing and game behavior remain unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-POWER-AND-TIMING.md#bound-physical-cd-power-and-cartridge-takeover)

2. **Stalled game sector reads — published**
   [`b345952`](https://github.com/papsg/SAROO/commit/b3459526a308d88b01d6db7e81c1a17d715421ac).
   Adapted from the game-facing portion of paps-g `63e0e2b4`
   (SATURN-CD-003). The parent keeps polling past the modeled
   deadline and passes the other 15 transfer methods. Both CD
   profiles pass 54 methods, including wrap, partial output, and
   a read that makes progress across 27 modeled seconds. The SH-2
   menu links 81,236 code/data bytes with 24,208 bytes of body
   space. Physical drive timing remains unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-POWER-AND-TIMING.md#bound-a-stalled-game-sector-read)

3. **BIOS clock reset enable — published**
   [`5eca24a`](https://github.com/papsg/SAROO/commit/5eca24a10ae7dc81eefcf40a1e9ad72604d92761).
   Adapted from paps-g `cc2cafa2` and the clock-hook portion of
   `ff3f9d2f` (SATURN-SMPC-005). Four parent methods fail and the
   healthy BIOS/SCU control passes. Five focused methods pass both
   profiles and 88 menu safety methods pass under sanitizers. The
   linked 96-byte and 36-byte fragments pass 48 relocated checks.
   The menu links 81,380 code/data bytes with 24,064 bytes of body
   space. Physical reset behavior and BIOS variants remain unqualified.
   [Sega citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-POWER-AND-TIMING.md#preserve-reset-enable-after-bios-clock-changes)

4. **CD worker pacing — published**
   [`0a36729`](https://github.com/papsg/SAROO/commit/0a36729944b5c37b608dea693bc7992afdd58c5a).
   Adapted from paps-g `77b8f027` and `dca425eb` for the public CD
   worker. Fourteen timer cases fail on the parent and two pass.
   The parent also fails the startup ownership and worker failure
   cases, while 14 other methods across those two groups pass.
   Both CD profiles pass 205 methods. Both menu command profiles
   pass six methods. The GNU MCU App link uses 64,460 bytes with
   1,076 bytes of ITCM space. The linked TIM2 vector is active,
   TIM6/DAC is retired, and TIM4 and TIM7 remain linked.
   Physical timer latency and game pacing remain unqualified.
   [STM32 citations, reproduction and limits](https://github.com/papsg/SAROO/blob/main/doc/CD-POWER-AND-TIMING.md#use-tim2-for-cd-worker-pacing)

The full run caught a stale audio fixture source boundary after the
TIM2 port. The test-only follow-up
[`81212bd`](https://github.com/papsg/SAROO/commit/81212bd69e1918e96166e574f907b2cd00b2fab4)
recognizes both worker forms. It preserves the exact admission,
backpressure and flush code in the extracted fixture. All 37 audio
methods pass under sanitizers and O2, and the parent worker-flush
control passes.

### CD power and timing final verification

All four production checkpoints and the audio fixture correction are
published through [`81212bd`](https://github.com/papsg/SAROO/commit/81212bd69e1918e96166e574f907b2cd00b2fab4).
The full `make -C tests check` run passes 770 Python methods, 18 Rust
checks, and the native language check. The Windows-only CD+G
replacement fixture is skipped on macOS. Both CD profiles pass 205
methods and both audio profiles pass 37 methods.

The published source reproduces both ELFs and the SH-2 menu image.
All 36 tracked menu inputs and 56 tracked MCU inputs match their
build receipts. The 488,770-byte menu contains 81,380 code/data bytes
and 9,244 BSS bytes, with 24,064 bytes of body space. The linked
ROM-cartridge clock hook passes 48 cases at its relocated addresses.
The GNU MCU App loads 64,460 bytes and leaves 1,076 bytes of ITCM
space, with 60,216 bytes of static DTCM. Its vector table uses TIM2
for CD pacing and preserves TIM4 and TIM7.

The public main worktree is clean. The private checkout's HEAD,
branch, refs, stash, status and tracked diff match the initial
read-only snapshot. Physical SMPC timing, drive behavior, and game
pacing remain unqualified.
[Citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/CD-POWER-AND-TIMING.md)

## Menu startup, GIF display, and CD handoff

1. **Startup language bounds — published**
   [`8031a28`](https://github.com/papsg/SAROO/commit/8031a28aec71496212e504c70da57e7494680178).
   Adapted from paps-g `1ad9fb40` (SATURN-MENU-010). The parent
   indexes outside the table for a negative ID, and its normal
   19-language/2,280-lookup control passes. The port passes 65,540
   signed ID cases and the existing controls under sanitizers and
   O2. The pinned SH-2 build changes one image byte and retains
   81,380 code/data bytes with 24,064 bytes of body space.
   [Upstream references, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/STARTUP-GIF-CD-HANDOFF.md#validate-the-startup-language-id)

2. **Fully clipped GIF frames — published**
   [`eb134b0`](https://github.com/papsg/SAROO/commit/eb134b072c46fa9561e0933ef16a1b2c1565665f).
   Adapted from paps-g `a3a14724` (SATURN-MENU-011). Four parent
   cases submit invalid counts and 13 controls pass. The port
   passes 17 display scenarios and all nine cover/display methods
   under sanitizers and O2. The SH-2 menu links 81,396 code/data
   bytes with 24,048 bytes of body space. The actual SCU and VDP
   behavior remains unqualified.
   [Sega and upstream citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/STARTUP-GIF-CD-HANDOFF.md#skip-row-dma-for-a-fully-clipped-gif)

3. **Original-drive command retirement — published**
   [`1002ed1`](https://github.com/papsg/SAROO/commit/1002ed1f93006d5b003fd10b24d1a7b25cff35f7).
   Adapted from paps-g `4e024188` (MCU-CDC-034). The parent fails
   the actual-C disabled dispatcher and coupled FPGA cases while
   passing 44 drive methods. The candidate covers 1,024 disabled
   combinations and restores readiness at takeover in the stock
   FPGA register simulation. Both CD profiles pass 207 methods.
   The GNU MCU App links 64,460 load bytes with 1,076 bytes of
   ITCM space. Physical bus timing and games remain unqualified.
   [Sega and FPGA citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/STARTUP-GIF-CD-HANDOFF.md#retire-original-drive-commands-in-the-emulated-registers)

4. **Unanswered CD command admission — published**
   [`c0a6535`](https://github.com/papsg/SAROO/commit/c0a6535ec933dbff7c4f1f3bd75ceeb9cf1d23e4).
   Adapted from paps-g `b782c9b7` (SATURN-CD-003), after original-drive
   command retirement. Both new parent methods fail and 16 controls
   pass. The port protects all 65,536 busy state/wait combinations
   and admits a distinct request after a late reply. Both menu runs
   pass 90 methods, including the CD player, startup and launch
   controls. The menu retains 81,396 code/data bytes and 24,048
   bytes of body space. Physical behavior remains unqualified.
   [Sega citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/STARTUP-GIF-CD-HANDOFF.md#protect-an-unanswered-menu-cd-command)

5. **Asynchronous file event admission — published**
   [`f90cc54`](https://github.com/papsg/SAROO/commit/f90cc546239b268bdbb2e1bf90667e3c15f1d34c).
   Adapted from paps-g `e515b7c5` and the shared helper from
   `5ef543a2` (SATURN-CD-003). Two parent methods reproduce stale
   EFLS and 19 pass. The port checks all 65,536 HIRQ patterns,
   fresh events on reply/error and all five boot handoff methods.
   Both menu runs pass 93 methods. The pinned menu links 81,428
   code/data bytes with 24,016 bytes of body space. Actual BIOS
   and game execution remain unqualified.
   [Sega and upstream citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/STARTUP-GIF-CD-HANDOFF.md#acknowledge-the-previous-file-event-on-read-admission)

6. **BIOS disc-change acknowledgment — published**
   [`0d7459c`](https://github.com/papsg/SAROO/commit/0d7459ce4d74f3d24cada43dd22fe6737fb37b72).
   Adapted from paps-g `200a447b` (SATURN-CD-003). The parent
   retains stale DCHG at its first command and passes nine
   controls. The port passes all 65,536 initial flag states and
   25 new-event orderings across the five stages. Both menu
   runs pass 95 methods. The pinned menu links 81,444 code/data
   bytes with 24,000 bytes of body space. Physical disc and BIOS
   behavior remain unqualified.
   [Sega and upstream citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/STARTUP-GIF-CD-HANDOFF.md#acknowledge-the-old-disc-change-before-bios-initialization)

### Menu startup, GIF display, and CD handoff final verification

The six direct checkpoints are published through
[`0d7459c`](https://github.com/papsg/SAROO/commit/0d7459ce4d74f3d24cada43dd22fe6737fb37b72).
The final `make -C tests check` run passes 780 Python methods, 18 Rust
checks, and the native language check. The Windows-only CD+G
replacement fixture is skipped on macOS. Both focused menu runs
pass 95 methods and both CD profiles pass 207 methods. The coupled
host/Verilog case covers original-drive command retirement using the
public FPGA register logic and actual MCU dispatcher output.

The published source reproduces both ELFs and the SH-2 menu image.
All 36 menu and 56 MCU input hashes match the receipts. The
488,770-byte menu contains 81,444 code/data bytes and 9,244 BSS bytes
with 24,000 bytes of body space. The linked ROM clock hook passes
48 relocated cases. The GNU MCU App loads 64,460 bytes and retains
1,076 bytes of ITCM space and 60,216 bytes of static DTCM.

The public main worktree is clean. The private checkout's HEAD,
branch, refs, stash, worktree status and diff match the initial
read-only snapshot. Physical SCU timing, cold starts, and game
behavior remain unqualified.
[Citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/STARTUP-GIF-CD-HANDOFF.md)

## Save archives, CD workers, and reset ownership

1. **Live primary save archives — published**
   [`d41a25a`](https://github.com/papsg/SAROO/commit/d41a25a5bb043236ea99dac9d68dae4a826e1cdd).
   Adapted from paps-g `6b728c6d` (MCU-SAVE-003). The parent fails
   12 filesystem/device subcases and passes six existing methods.
   The port checks both archives on FAT32 and exFAT, including 72
   conflicting named writes, alternate paths, directory boundaries,
   read-only and reopen controls. The paired owner and reopen groups
   each pass nine methods in both profiles. Both transfer runs pass
   52 methods. The GNU MCU App links 64,572 load bytes and leaves
   964 bytes of ITCM space. Physical SD behavior remains unqualified.
   [FatFs references, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/SAVE-CD-RESET-OWNERSHIP.md#protect-live-primary-save-archives)

2. **Failed CD reader retirement — published**
   [`f3b6514`](https://github.com/papsg/SAROO/commit/f3b65148aadc11018ce72b36f1b1d02c04538f94).
   Adapted from paps-g `c3bfa9ef` (MCU-CDC-035). The parent fails five
   input methods and one worker method while passing 12 controls.
   The port checks lookup and input failure across reader types,
   preserves buffered payloads, and reports before acknowledging a
   pending PLAY. Both CD profiles pass 208 methods. The GNU MCU App
   links 64,572 load bytes with 964 bytes of ITCM space.
   [Sega citations, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/SAVE-CD-RESET-OWNERSHIP.md#report-cd-input-failure-before-retiring-the-reader)

3. **Settled drive state after retirement — published**
   [`b560381`](https://github.com/papsg/SAROO/commit/b560381bcbc3c8b4e7dd6bbfb0f87483125cd778).
   Adapted from paps-g `218dcb45` (MCU-CDC-036). Both new worker
   methods fail on the parent and 11 controls pass. The port preserves
   three idle states and a newly published index at retirement.
   Both CD profiles pass 210 methods. The GNU MCU App links 64,572
   load bytes with 964 bytes of ITCM space.
   [Sega and upstream references, attribution and reproduction](https://github.com/papsg/SAROO/blob/main/doc/SAVE-CD-RESET-OWNERSHIP.md#keep-settled-drive-state-after-reader-retirement)

### Atomic retained PLAY acknowledgment

- `c6dc9847` (paps-g, MCU-CDC-037) → [9bee114](https://github.com/papsg/SAROO/commit/9bee11499cfd7301df601486485fd73c888b3289) on `main`.
- Reuses the ordinary pause guard. The parent lost a replacement command in the new fixture and passed its other 46 drive methods. The port services both requests and passes 211 CD methods in each host profile.
- GNU MCU App: 64,556 load bytes, 980 bytes free in ITCM. The modeled command-preemption boundary still needs physical qualification. [Implementation and primary references](https://github.com/papsg/SAROO/blob/9bee11499cfd7301df601486485fd73c888b3289/doc/SAVE-CD-RESET-OWNERSHIP.md#acknowledge-retained-play-as-one-operation).

### CD pacing and handoff

- `39a45215` (paps-g, MCU-CDC-038) → [137caf5](https://github.com/papsg/SAROO/commit/137caf52f453731ac9eb26e5f5bec376ef31db40) on `main`.
- The parent timer misses both modeled long-wait handoffs. The actual worker parent also misses its seek and PEND handoff checkpoints. All 217 CD methods pass under sanitizers and O2 after the port.
- GNU MCU App: 64,636 load bytes before the layout change and 63,628 after it, leaving 1,908 bytes of ITCM. The layout preserves static RAM and capacities, and three of 737 matched direct frames shrink. Physical timing remains unqualified. [Implementation and primary references](https://github.com/papsg/SAROO/blob/137caf52f453731ac9eb26e5f5bec376ef31db40/doc/SAVE-CD-RESET-OWNERSHIP.md#wake-cd-pacing-for-drive-handoff).

### Saturn reset and retained command ownership

- `90ff71c0` (paps-g, MCU-SAVE-001) → [db18718](https://github.com/papsg/SAROO/commit/db18718454003f3a0a914fb17317e733e4013176) on `main`. Credits the `dfa2f1d1` investigation and uses the later mailbox fence directly.
- The stock menu uses the public MCU's existing v1 completion path. The parent overwrites a retained request and reuses an in-flight preview argument. Host and FPGA models verify retained command/argument state, ordered completion, a bounded timeout across counter wrap, retry and the legacy sticky-opcode controls. Both focused profiles pass 189 methods, plus 48 linked menu clock cases.
- GNU MCU App: 63,628 load bytes with 1,908 bytes of ITCM space. Menu code/data: 82,052 bytes, 608 above the parent. BSS grows four bytes. The pre-v1 MCU still cannot distinguish live and retired untagged STARTUP/SELECT values. Physical reset behavior remains unqualified. [Implementation and primary references](https://github.com/papsg/SAROO/blob/db18718454003f3a0a914fb17317e733e4013176/doc/SAVE-CD-RESET-OWNERSHIP.md#preserve-an-in-flight-mailbox-across-saturn-reset).

- Full-suite integration follow-up [cc1a6f6](https://github.com/papsg/SAROO/commit/cc1a6f6b3a39cb52fc5aa4abaf60cc6357972da4) models idle command admission in the save-owner fixture after `db18718`. All nine owner methods pass with sanitizers and O2. Firmware source and artifacts are identical to the hot-reset checkpoint.

### Final verification

The six production checkpoints and the archive-owner fixture follow-up are published on [`main` at cc1a6f6](https://github.com/papsg/SAROO/commit/cc1a6f6b3a39cb52fc5aa4abaf60cc6357972da4). The full `make -C tests check` run passed from a clean archive of that commit, with 798 Python passes, one Windows-only skip, 18 Rust passes and the native language check. The separate linked menu clock run passed 48 cases.

The exact archive rebuild reproduced the previous MCU ELF and menu ELF/image byte for byte. The GNU MCU App verifies 166 vectors, links 63,628 load bytes and leaves 1,908 bytes of ITCM. The menu links 82,052 code/data bytes and retains 23,388 bytes of body margin. All 56 MCU and 36 menu repository source hashes match. The public working tree is clean and the private reference checkout matches its baseline. Physical Saturn and game qualification remain open. [Attribution, primary references and reproduction details](https://github.com/papsg/SAROO/blob/cc1a6f6b3a39cb52fc5aa4abaf60cc6357972da4/doc/SAVE-CD-RESET-OWNERSHIP.md).

## FPGA memory, executable folders, and CD selector decoding

### Accepted writes to both FPGA caches

- `eefa5aca` (paps-g, FPGA-CACHE-001) → [36174ac](https://github.com/papsg/SAROO/commit/36174aca6fa36a89b7739592e96b4bbc773b7666) on `main`. Uses the original arbiter's accepted write order and keeps the published memory-ack reset.
- The exact parent fails five concurrent-write/fill subcases and passes three valid controls. All 416 cache scenarios and four existing reset methods pass.
- The complete Quartus fit binds all 18 inputs and passes 21 reported constrained timing checks. Logic elements go from 1,479 to 1,426 and registers from 816 to 814. Physical bus timing remains unqualified. [Attribution, original source and reproduction](https://github.com/papsg/SAROO/blob/36174aca6fa36a89b7739592e96b4bbc773b7666/doc/FPGA-MEMORY.md#apply-accepted-writes-to-both-caches).

### SDRAM refresh acknowledgment reset

- `eefa5aca` (paps-g, FPGA-REFRESH-001) → [2b7fd30](https://github.com/papsg/SAROO/commit/2b7fd306133a90ec85be2daa1fb49a474ce88db5) on `main`. Both exact parent ports stall with the acknowledgment held. The adjacent controls and all 48 selected reset combinations pass. The six cache methods retain 416 passing cases.
- The explicit-register candidate missed a setup constraint by 0.202 ns. The selected public port decodes the refresh-acceptance state and passes the complete five-stage fit and all 21 reported constrained checks. It uses 1,413 logic elements and 813 registers. Physical timing remains unqualified. [Attribution, original source and reproduction](https://github.com/papsg/SAROO/blob/2b7fd306133a90ec85be2daa1fb49a474ce88db5/doc/FPGA-MEMORY.md#clear-refresh-acknowledgment-on-reset).

### MCU executable folder catalog

- `6f1ab00f` (paps-g, BIN-DIRS-001) → [fc083e4](https://github.com/papsg/SAROO/commit/fc083e43a24f4208830fef0be0c18e6d30cbd937) on `main`. Adds a tagged capability and bounded BIN-path listing while preserving the legacy flat command. [Original request](https://github.com/tpunix/SAROO/issues/84).
- The parent passes the flat-order control and fails the six folder subcases and tagged command check. The port passes 108 MCU save/catalog and 58 file-transfer methods under sanitizers. The optimized catalog and file-transfer groups pass 10 and 58 methods.
- The GNU MCU App links 63,864 load bytes with 1,672 bytes of ITCM space. Static RAM is unchanged. The next menu checkpoint consumes this protocol. Physical media behavior remains unqualified. [Attribution, FatFs references and reproduction](https://github.com/papsg/SAROO/blob/fc083e43a24f4208830fef0be0c18e6d30cbd937/doc/BIN-FOLDERS.md#list-executable-folders-on-the-mcu).

### Executable folder browser in the stock menu

- `a09e734b` (paps-g, BIN-DIRS-001) → [e44efab](https://github.com/papsg/SAROO/commit/e44efab959b4d76ce677c139734f6abf2798fdc7) on `main`, following `fc083e4`. [Original request](https://github.com/tpunix/SAROO/issues/84).
- The actual parent passes flat fallback and fails the negotiated workflow. The port passes 160 menu and file-transfer methods in each host profile, 10 catalog methods and 48 linked clock cases. The new fixtures exercise nested launch, bounded paths, late and invalid replies, queued exit, and recovery from folder or catalog-restore failure.
- The linked menu uses 84,164 code/data bytes and 9,488 BSS bytes, with 21,036 bytes of body space. This includes a 240-byte remembered path. Physical media, menu and game behavior remain unqualified. [Attribution, public protocol and reproduction](https://github.com/papsg/SAROO/blob/e44efab959b4d76ce677c139734f6abf2798fdc7/doc/BIN-FOLDERS.md#browse-and-launch-files-from-the-menu).

### Shared CD filter selector decoding

- `7c92d827` (paps-g) → [a9a64b3](https://github.com/papsg/SAROO/commit/a9a64b32fe8fa4bdcf99c8338ad57786b64b9c23) on `main`. Seven command handlers share the existing bounds-checked lookup of the serial task's CR3 snapshot.
- The exact parent and port pass 217 methods in each CD/CUE run, including valid selector, reply and routing controls. Both GNU App builds use the same 63,864 load bytes, 1,672 bytes of ITCM headroom and 60,208 static DTCM bytes. The two changed direct frames each shrink from 16 to 8 bytes. Physical performance and peak stack usage remain unqualified. [Attribution, Sega references and reproduction](https://github.com/papsg/SAROO/blob/a9a64b32fe8fa4bdcf99c8338ad57786b64b9c23/doc/MCU-SHRINK.md#shared-cd-filter-selector-decoding).

### FPGA cache word validity

- `6b156a19` (paps-g, FPGA-CACHE-002) → [4b5a86b](https://github.com/papsg/SAROO/commit/4b5a86bff1fb1dc5ba91f46eba8b51a32e028b51) on `main`, following `36174ac`. A partially filled line waits for the requested word in the existing burst. A valid word completes immediately.
- Both ports in the exact parent return stale words in each targeted regression and pass the four valid controls. The port passes 200 distinct read scenarios, 416 accepted-write scenarios and six memory reset methods. The command-count control rejects a refetch variant on both ports.
- The five-stage Quartus build passes all 21 constrained checks with the same compiler, clock constraints and 17 other project inputs. Logic elements increase from 1,413 to 1,435, with 813 registers and 32,768 memory bits. Minimum reported setup slack goes from 0.968 ns to 1.093 ns. The reset-tag address is outside the v0.8 top-level mapping, and physical partial-fill timing remains unqualified. [Attribution, original RTL and reproduction](https://github.com/papsg/SAROO/blob/4b5a86bff1fb1dc5ba91f46eba8b51a32e028b51/doc/FPGA-MEMORY.md#wait-for-the-requested-cache-word).

### Final verification

All six checkpoints are published in order on [`main` at `4b5a86b`](https://github.com/papsg/SAROO/commit/4b5a86bff1fb1dc5ba91f46eba8b51a32e028b51). A fresh archive of that commit passes `make -j4 -C tests check` with 822 Python passes, one Windows-only skip, 18 Rust passes and the native language check. All 48 linked menu clock cases pass.

The archive rebuild matches all 36 menu, 56 MCU and 18 FPGA repository input hashes. The menu ELF/image and the MCU load image reproduce the checkpoint bytes. The MCU's unstripped ELF differs in debug metadata and its stripped ELF matches. The GNU MCU App links 63,864 load bytes, verifies 166 vectors and retains 1,672 bytes of ITCM space. The menu links 84,164 code/data bytes and retains 21,036 bytes of body space.

The original five-stage Quartus project reproduces the RBF and JIC, with 1,435 logic elements, 813 registers and 32,768 memory bits. The same 21 reported constrained checks pass. Physical bus, media and game behavior remain unqualified. The public working tree is clean. The private checkout matches its starting HEAD, branch, refs and worktree state, and the Saturn was untouched.

## September 14 storage and catalog audit

### SD DMA buffer alignment

- `b483124c` (paps-g, AUD-MCU-001) → [992be39](https://github.com/papsg/SAROO/commit/992be39127e83a6fd42f4295e4affea010c4982f) on `main`. Unaligned sector buffers use the existing eight-sector bounce path.
- The exact parent fails both modeled DMA regions and passes the existing controls. The port passes 48 new transfer cases, 16 direct and bounce controls, and all 30 storage methods in each host profile.
- The GNU MCU App links 63,864 load bytes with 1,672 bytes of ITCM headroom. All 738 direct frames match. Physical DMA behavior remains unqualified. [Attribution, STM32 reference and reproduction](https://github.com/papsg/SAROO/blob/992be39127e83a6fd42f4295e4affea010c4982f/doc/STORAGE-AUDIT.md#align-direct-sd-transfers).

### Shared disk read and write retries

- `b483124c` (paps-g, AUD-RES-001) → [8999992](https://github.com/papsg/SAROO/commit/89999923ae5fe5a21b6099d822478ec1220c2292) on `main`. Both FatFs directions share the existing disk owner and four-attempt policy.
- Both exact revisions pass nine disk adapter methods and 3,840 matrix cases in each host profile, including all drive IDs, all retry positions, exact messages and read-only linking without a write provider. Both port storage runs pass 32 methods.
- The GNU MCU App links 63,800 load bytes, 64 below the parent. The shared helper uses 40 direct stack bytes while active, eight above the preceding wrappers. Static DTCM stays at 60,208 bytes. [Attribution, CMSIS and reproduction](https://github.com/papsg/SAROO/blob/89999923ae5fe5a21b6099d822478ec1220c2292/doc/STORAGE-AUDIT.md#share-disk-read-and-write-retries).

### exFAT allocation bitmap backing

- `b483124c` (paps-g, AUD-MCU-021) → [73a9ce6](https://github.com/papsg/SAROO/commit/73a9ce65b2ea26fb66dab6dded0335b85c70d89b) on `main`. Mount checks declared length, contiguous backing and volume-relative address bounds before publishing the bitmap.
- The exact parent changes the neighboring file's first byte from zero to one on the short-chain image and fails six subcases. The port refuses seven invalid layouts with zero writes. Seven valid layouts and three read-error retries pass. The FatFs and MCU save runs pass 130 methods in each profile, with the final partitioned control also verified on the parent.
- GNU MCU App load rises by 96 bytes to 63,896, leaving 1,640 ITCM bytes. The mount frame grows from 40 to 48 bytes and static DTCM stays at 60,208 bytes. [Attribution, exFAT specification and reproduction](https://github.com/papsg/SAROO/blob/73a9ce65b2ea26fb66dab6dded0335b85c70d89b/doc/STORAGE-AUDIT.md#validate-exfat-allocation-bitmap-backing).

### Bounded game catalog sort

- `b483124c` (paps-g, AUD-MCU-006 and AUD-RES-021) → [f6c8ab5](https://github.com/papsg/SAROO/commit/f6c8ab50e2113299afb9e315355703642a707ffd) on `main`. The in-place heap orders the existing offsets with the stock comparator.
- The exact parent uses 3,272,961 comparisons in three full-catalog cases. The port uses at most 52,436 across the same shapes in both directions. Both revisions pass 100 ordering and offset controls. The port passes 24 discovery and cover methods in each host profile.
- GNU MCU App load falls by 96 bytes to 63,800, static DTCM by eight to 60,200, and the direct sorter frame from 48 to 40 bytes. Physical startup latency remains unmeasured. [Attribution, original SAROO source and reproduction](https://github.com/papsg/SAROO/blob/f6c8ab50e2113299afb9e315355703642a707ffd/doc/STORAGE-AUDIT.md#bound-game-catalog-sorting).

### Read-only seek allocation

- `b483124c` (paps-g, AUD-MCU-015) → [5ec3454](https://github.com/papsg/SAROO/commit/5ec3454818db6fc332c8e1154686b17f4fa1a499) on `main`. A read-only handle gets the existing internal-error result before the missing-chain branch can allocate.
- The exact parent persists an allocation across FAT12, FAT16, FAT32 and exFAT in all 12 malformed cases. Both revisions pass 16 valid controls. The port passes all 24 FatFs methods in both profiles.
- The GNU MCU App links 63,816 bytes, up 16, leaving 1,720 ITCM bytes. All 739 direct compiler frames and static DTCM match the parent. [Attribution, FatFs and exFAT references, and reproduction](https://github.com/papsg/SAROO/blob/5ec3454818db6fc332c8e1154686b17f4fa1a499/doc/STORAGE-AUDIT.md#refuse-allocation-during-a-read-only-seek).

### FatFs and CD object layout

- `b483124c` (paps-g, AUD-RES-007) → [79eedd3](https://github.com/papsg/SAROO/commit/79eedd31e50f1cb1a438721ef570e48ae11d68c5) on `main`. Use natural alignment to reduce FatFs object padding and move the subcode file behind the CD tables.
- Linked ARM types measure 48 to 40 bytes for `FFOBJID`, 80 to 72 for `FIL` and `DIR`, and 8,168 to 8,160 for `CDBLOCK`. The rebuilt GNU App saves 112 load bytes to 63,704 and 832 static DTCM bytes to 59,368. Twenty-two direct frames shrink by 8 to 24 bytes.
- The exact parent passes 217 CD methods. Both port profiles pass all 351 CD, FatFs and MCU save methods. All 13 enabled BootLoader C units compile with the original BOOT and RTE settings. Peak stack use and physical timing remain unqualified. [Attribution, Arm ABI and reproduction](https://github.com/papsg/SAROO/blob/79eedd31e50f1cb1a438721ef570e48ae11d68c5/doc/STORAGE-AUDIT.md#reduce-padding-in-fatfs-and-cd-state).

### Final verification

All six checkpoints are published in order on [`main` at `79eedd3`](https://github.com/papsg/SAROO/commit/79eedd31e50f1cb1a438721ef570e48ae11d68c5). The clean archive passes `make -j4 -C tests check` with 832 Python passes, one Windows-only skip, 18 Rust passes and the native language check. All 48 linked menu clock cases pass.

The menu ELF and image reproduce the prior build. The GNU MCU App reproduces the checkpoint load bytes and stripped ELF. It links 63,704 load bytes with 1,832 bytes of ITCM headroom and uses 59,368 static DTCM bytes. The menu and MCU receipts match all 36 and 56 repository input hashes. All 13 enabled BootLoader C units compile with the original BOOT and RTE settings. The final 18 FPGA project inputs match the previous verified fit.

The public working tree is clean. The private checkout matches its starting HEAD, branch, refs and worktree. The Saturn was untouched. Physical DMA, media and timing behavior and a full MDK link remain unqualified.

## Startup, CD, and storage audit follow-up

### Stock menu file admission

- `7f77b9d5` (paps-g, AUD-MCU-002) → [d938595](https://github.com/papsg/SAROO/commit/d938595974e995f48fa41aeaef48e11bca22a54a) on `main`. The stock loader validates the full extent before converting it to a read count, then requires a complete read before startup.
- The exact parent fails 21 invalid-extent and nine incomplete-read cases. All 43 cases pass with sanitizers and at O2, including boundary, close and missing-file controls.
- The GNU MCU App links 63,752 load bytes with 1,784 bytes of ITCM space and unchanged static DTCM. Physical startup remains unqualified. [Attribution, FatFs contract and reproduction](https://github.com/papsg/SAROO/blob/d938595974e995f48fa41aeaef48e11bca22a54a/doc/STARTUP-CD-STORAGE-AUDIT.md#validate-the-complete-menu-file).

### CD subcode queue ownership

- `49b2bce5` (paps-g, AUD-MCU-007/008) → [8bbed27](https://github.com/papsg/SAROO/commit/8bbed274c2021254fc513ed07f53b23085b04a41) on `main`. The shared cursors are protected and R–W packs become visible after audio or data admission. The early Q hint stays in place.
- The exact parent fails five targeted methods and passes normal admission controls. The port passes all 223 CD/CUE methods in both profiles, including full ring, track reset, retry, directory and terminal notification.
- The GNU MCU App links 63,800 load bytes with 1,736 bytes of ITCM space. Static DTCM is unchanged. Linked code confirms copies and HIRQ writes are outside the masked cursor transaction. Physical playback and timing remain unqualified. [Attribution, Sega and Arm references, and reproduction](https://github.com/papsg/SAROO/blob/8bbed274c2021254fc513ed07f53b23085b04a41/doc/STARTUP-CD-STORAGE-AUDIT.md#publish-rw-subcode-with-accepted-sectors).

### Directory recovery across storage errors

- `18ed08a6` with the `b483124c` growth prerequisite (paps-g, AUD-MCU-023/024/025/026) → [c2a1561](https://github.com/papsg/SAROO/commit/c2a156123790c69da8bf3a5e03f61dd2546abbea) on `main`. Clears free directory clusters before allocation and retains one bounded recovery record for exFAT growth and name changes.
- The exact parent fails 78 of 214 directory cases. The port passes all 284 cases in both profiles, including long names, split parent headers, partial writes, retryable allocation and name retirement, and valid classic FAT controls. Both filesystem suites pass 30 methods.
- The GNU MCU App links 64,808 load bytes with 728 bytes of ITCM space and 59,456 static DTCM bytes. All 13 enabled BootLoader C units compile. Physical media, power-loss recovery and a full BootLoader link remain unqualified. [Attribution, exFAT references and reproduction](https://github.com/papsg/SAROO/blob/c2a156123790c69da8bf3a5e03f61dd2546abbea/doc/STARTUP-CD-STORAGE-AUDIT.md#recover-directory-updates-after-storage-errors).

### FAT mount geometry

- `8da2769d` (paps-g, AUD-MCU-009/029) → [3fb78c7](https://github.com/papsg/SAROO/commit/3fb78c79c98f7473cac993cb7ea2fc6e5c0da7fe) on `main`. Validate classic FAT arithmetic and the complete exFAT table before following the mounted layout.
- The exact parent mounts five invalid geometries. The port passes all 11 geometry cases and both 32-method filesystem runs, including valid endpoint, one/two FAT and exFAT allocation controls.
- The GNU MCU App links 64,872 load bytes with 664 bytes of ITCM space and unchanged static DTCM. The checks cover declared geometry with a host block device. [Attribution, Microsoft FAT and exFAT references, and reproduction](https://github.com/papsg/SAROO/blob/3fb78c79c98f7473cac993cb7ea2fc6e5c0da7fe/doc/STARTUP-CD-STORAGE-AUDIT.md#validate-fat-geometry-before-mount).

### Obsolete CD position history

- `1206dfc8` (paps-g, AUD-RES-012) → [e897dd4](https://github.com/papsg/SAROO/commit/e897dd407c09876e12943dcc6ec5970a4e1735ac) on `main`. Remove the unused previous-FAD field and the excluded delay experiment.
- The exact parent and port pass all 223 CD/CUE methods; both port profiles pass. Drive timing controls, repeat, pause, file and subcode behavior remain covered.
- The GNU MCU App saves 64 load bytes to 64,808, with 728 bytes of ITCM space, and eight static DTCM bytes. The linked CD object shrinks from 8,160 to 8,152 bytes. Physical timing remains unmeasured. [Attribution, Sega references and reproduction](https://github.com/papsg/SAROO/blob/e897dd407c09876e12943dcc6ec5970a4e1735ac/doc/STARTUP-CD-STORAGE-AUDIT.md#remove-unused-cd-position-history).

### Shared CD partition updates

- `fb924aac` (paps-g, AUD-RES-011/004) → [9f36b43](https://github.com/papsg/SAROO/commit/9f36b43beba9aaa4a956e7ffe011ec585b07a8f5) on `main`. Remove unused byte totals and share the append path, keeping the last destination and the list under the same outer mask.
- The exact parent and the port pass 224 CD/CUE methods; both port profiles pass. The new test checks masked publication across all 24 partitions. The separate historical directory fixture uses its matching header and retains six reproduced failures and one valid control.
- The GNU MCU App links 64,680 load bytes with 856 bytes of ITCM space and 59,448 static DTCM bytes. Compact records save 96 static bytes but add 80 load bytes; a separate locked helper adds 16 load bytes. Physical timing remains unmeasured. [Attribution, Sega and Arm references, and reproduction](https://github.com/papsg/SAROO/blob/9f36b43beba9aaa4a956e7ffe011ec585b07a8f5/doc/STARTUP-CD-STORAGE-AUDIT.md#simplify-cd-partition-updates).

- Full-suite fixture follow-up [081ec64](https://github.com/papsg/SAROO/commit/081ec64f9e6eca24020d4fd9d928ce5d7248f1af) models the FAD passed by the worker after `8bbed27`. The initial suite stopped at the host compile; both audio profiles now pass 37 methods. Production source and firmware bytes are unchanged.

### Final verification

All six production checkpoints and the audio fixture follow-up are published in order on [`main` at `081ec64`](https://github.com/papsg/SAROO/commit/081ec64f9e6eca24020d4fd9d928ce5d7248f1af). The final suite used an archive matching all 663 blobs of that commit. `make -j4 -C tests check` passed with 851 Python passes, one Windows-only skip, 18 Rust passes and the native language check. All 48 linked menu clock cases pass separately.

The GNU MCU App reproduces the selected image byte for byte, binds all 56 repository MCU inputs and verifies 166 vectors. It links 64,680 load bytes with 856 bytes of ITCM space and 59,448 static DTCM bytes. All 13 enabled BootLoader C units compile with the original BOOT and RTE settings. The stock menu ELF and image match the prior build and all 36 menu inputs match. All 18 FPGA project inputs match the previous fit.

The public worktrees are clean. The private checkout matches its starting branch, refs and worktree. No Saturn operation was performed. Physical playback, media behavior, power-loss recovery and a complete BootLoader link remain unqualified. [Attribution, specifications and reproduction](https://github.com/papsg/SAROO/blob/081ec64f9e6eca24020d4fd9d928ce5d7248f1af/doc/STARTUP-CD-STORAGE-AUDIT.md).

| SDRAM refresh budget (`cd22aeb1`, AUD-FPGA-002) | `0a6734c` | Parent 783/792 clocks idle/active; port 719/728. All 23 FPGA simulation methods and 21 constrained Quartus checks pass (worst setup 1.103 ns, +15 logic elements). Reset, external and zero controls pass. Physical retention, throughput and board timing remain unmeasured. [Evidence and Samsung datasheet citation](https://github.com/papsg/SAROO/blob/0a6734c2e704e695ef09d20eca500c3a9afadf7c/doc/FPGA-MEMORY.md#bound-autonomous-refresh-spacing). |

| FatFs UTF-8 filename decoding (`6e52b09c`, AUD-MCU-014) | `3eb69ba` | Four overlong forms per filesystem reproduced on the parent. All 35 FatFs methods pass under ASan/UBSan and O2; invalid paths produce no writes. GNU App +32 load bytes (824 free); 13 BootLoader C objects compile. [Tests and RFC citation](https://github.com/papsg/SAROO/blob/3eb69ba0cee2f04a8647a7b787406942d10bf5eb/doc/FATFS-SD-SAFETY.md#aud-mcu-014--reject-non-shortest-utf-8-filenames). |

| LED stack and common RTX pool (`7ec67882`, `e4953e40`, AUD-RES-008/017) | `1502442` | Both GNU target links recover 3,584 static DTCM bytes with no loss in later allocation admission. QEMU with the linked RTX allocator passes reuse and exhaustion; the 20 KiB variant fails. Linked 264-byte LED bound leaves 248 bytes of margin. App 64,728 load; BootLoader 28,704. No physical stack or MDK claim. [Evidence and Arm citations](https://github.com/papsg/SAROO/blob/1502442f1c006101a24fef9fa98b799d1ac02709/doc/MCU-SHRINK.md#size-the-permanent-led-stack-and-common-rtx-pool). |

| CD word reversal (`fa0a8f9d`, AUD-RES-006) | `39bbf09` | Seven linked `REV` instructions; no helper symbol or calls. All 225 CD methods pass on the parent and port, including 65,536 independent word values. Port also passes O2. App saves 64 image bytes; static DTCM unchanged. [Evidence and Sega/Arm citations](https://github.com/papsg/SAROO/blob/39bbf098588ab37c9526f1e1c9accba36745a64e/doc/MCU-SHRINK.md#inline-the-cd-word-byte-swap). |

| CUE subindex layout (`c39ce448`, AUD-RES-003) | `b5b07c6` | Full 2,048-slot capacity; seven parent and port CUE layout methods cover high FADs, preview and overflow. All 226 CD methods pass in both profiles. Static DTCM saves 2,048 bytes (53,816 used); App image adds 32, search frame adds four. [Validation and Sega citation](https://github.com/papsg/SAROO/blob/b5b07c6d1dca629c195da25fe202f109d7a06aae/doc/MCU-SHRINK.md#store-cue-subindices-in-24-bits). |

| CD directory storage and FIFO (`556a6a29`, AUD-RES-005) | `340fc6f` | All 227 CD methods pass on parent and port; port also passes O2. Covers every word of the 1,524-word reply, startup placement, busy and DataEnd behavior, and preserved older negative controls. The CD state saves 3,072 static DTCM bytes and the App image saves 16 bytes. [Validation and Sega citations](https://github.com/papsg/SAROO/blob/340fc6f940bdfe9c1ff051aa3829a3e60d036bad/doc/CD-FILESYSTEM-SAFETY.md#reuse-the-cd-directory-reservation). |

| LED startup fixture follow-up (test only) | `d3d82ba` | The full suite found the GIF fixture lacked the CMSIS `stack_size` member. Adding that field restores all 16 GIF methods under ASan/UBSan and O2. Production files are unchanged. See [LED stack attribution and CMSIS reference](https://github.com/papsg/SAROO/blob/d3d82bac2e26b46be7b6b75167b813c2d1831cc3/doc/MCU-SHRINK.md#size-the-permanent-led-stack-and-common-rtx-pool). |

## SDRAM, UTF-8 and MCU memory series complete

All six production changes were verified and pushed in order. [Public main at `d3d82ba`](https://github.com/papsg/SAROO/commit/d3d82bac2e26b46be7b6b75167b813c2d1831cc3) includes the test-only GIF fixture follow-up. The full suite ran from an archive whose 667 tracked files match the published commit. It passed 862 Python methods and 18 Rust tests; the Windows replacement fixture was skipped on macOS. All 48 linked SH-2 menu clock cases also passed.

The GNU App uses 64,680 load bytes (856 free), unchanged from the start of this series. Static DTCM falls from 59,448 to 50,744 bytes, a saving of 8,704. The GNU BootLoader comparison links its 13 enabled C units and common providers with all 166 vectors at the original flash base. The stock menu ELF and binary match the parent. All 18 FPGA project inputs match the five-stage Quartus fit with 21 passing constrained checks. These are source, host, simulation and link/fit results. The MDK image, physical timing and game behavior were not qualified.

The public checkout is clean. The private checkout's HEAD, branch, refs, stash, status and tracked diff match their starting fingerprints. No Saturn operation or PR was performed.

| CD audio session retirement (`30f343ff`, `2617a839`, `fb0bf193`, AUD-MCU-030) | `c094756` | Parent reproduces retained old audio after replacement. Both 42-method audio profiles pass, including 3,528 cancellation positions and delayed startup/copy/disable. All 227 CD/CUE and 13 disc/save checks pass. GNU App 65,048 load bytes, 488 free; no hardware qualification of the public build. [Attribution, STM32 and Sega citations](https://github.com/papsg/SAROO/blob/c09475658aaf540c260220be6be7ac683cb370b9/doc/AUDIO-SAFETY.md#retire-cd-audio-while-keeping-the-i2s-link-running). |

- [x] `ddb07367` (AUD-MCU-031): [`18297b1`](https://github.com/papsg/SAROO/commit/18297b1fe773a9569ecfd0d4f09cb1ea97f2baed) — [plain CD audio completion pacing](https://github.com/papsg/SAROO/blob/18297b1fe773a9569ecfd0d4f09cb1ea97f2baed/doc/AUDIO-SAFETY.md#pace-plain-cd-audio-by-queue-room). Actual parent worker reproduces the unnecessary wait; both 43-method audio profiles and 227 CD/CUE checks pass. Full-depth, refusal/retry and R–W timer/error controls pass. GNU App 65,064 bytes, 472 free. Physical timing, receiver and game behavior unqualified.

- [x] `7b9e51b1` + `e265bd35` (AUD-FPGA-001): [`b7ddaf2`](https://github.com/papsg/SAROO/commit/b7ddaf23438c5627b24d3003a1f9c01a02c88ada) — [physical cache identity and same-edge geometry safety](https://github.com/papsg/SAROO/blob/b7ddaf23438c5627b24d3003a1f9c01a02c88ada/doc/FPGA-MEMORY.md#match-cache-addresses-to-the-configured-sdram-cells). 168 parent failures, 32 valid controls, 200 candidate cases and 27 FPGA methods. Quartus passes five stages and all 21 constrained checks; 1,491 LEs, 818 registers, setup/hold minimum 0.559/0.153 ns. External bus timing and physical SDRAM remain unqualified.

- [x] `07b96476` (AUD-RES-030): [`da9ee58`](https://github.com/papsg/SAROO/commit/da9ee586071302f1b809ae732cc450c9f514d397) — [reuse the linked bounded NUL scanner](https://github.com/papsg/SAROO/blob/da9ee586071302f1b809ae732cc450c9f514d397/doc/MCU-SHRINK.md#reuse-the-existing-bounded-nul-scan). 41 CUE/config/file-transfer methods pass on the parent and in both candidate profiles. GNU App 65,064 → 64,912 bytes, 624 free, unchanged RAM and direct compiler frames; no physical performance claim.

- [x] `7b6f6ebd` (AUD-MCU-035): [`7f05303`](https://github.com/papsg/SAROO/commit/7f0530316b32991c45539cf8c914fe7d608922b0) — [already addressed by the continuous link](https://github.com/papsg/SAROO/blob/7f0530316b32991c45539cf8c914fe7d608922b0/doc/AUDIO-SAFETY.md#deliver-a-lone-accepted-audio-track-tail). Pre-continuity parent strands a single accepted sector; current source emits all 1,176 words. Both 44-method audio profiles, 228 CD/CUE methods and 17 optimized worker methods pass. No runtime change; rebuilt GNU App load is byte-identical. Mixed-mode hardware playback unqualified.

- [x] `bf429abf` (AUD-MCU-046): [`fca7da1`](https://github.com/papsg/SAROO/commit/fca7da17877aaae61814d7879a446e870684cb7c) — [whole-span SD address admission](https://github.com/papsg/SAROO/blob/fca7da17877aaae61814d7879a446e870684cb7c/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#reject-a-sector-span-that-the-card-address-cannot-represent). The parent begins a bounce transfer past the representable span; valid endpoints pass. Both focused 70-method runs and 204 new transfer cases pass. GNU App 64,960 bytes, 576 free. Physical card capacity, timing and media behavior unqualified.

### Final verification: CD audio, SDRAM identity and SD span

All six checkpoints are recorded above and published in order through [`fca7da1`](https://github.com/papsg/SAROO/commit/fca7da17877aaae61814d7879a446e870684cb7c). The fifth checkpoint verifies the existing continuous audio link and adds no runtime workaround. The attribution and primary sources are linked from each entry.

The full `make -j6 -C tests check` passes on the published source: 877 Python methods and 18 Rust tests pass, with one Windows-only fixture skipped. The final GNU App links with all 166 vectors and uses 64,960 load bytes (576 free) and 50,752 static DTCM bytes. The GNU BootLoader comparison links all 13 enabled C units and common providers with 166 vectors at the original flash base. The stock menu ELF and binary match the earlier build exactly, and all 48 linked menu clock cases pass.

All 18 final FPGA project inputs match the completed five-stage Quartus fit. All 21 constrained checks pass; minimum setup and hold slacks are 0.559 ns and 0.153 ns. These are host, simulation, link and fit results. They do not qualify MDK output, external bus timing, physical audio or game behavior.

The public checkout is clean. Private checkout fingerprints match for HEAD, branch, status, all refs (including stash) and tracked diff. The Saturn was untouched. No PR, USB work or runtime diagnostic was added.

- [x] `72ac7fab` (AUD-MCU-047): [`b1bf264`](https://github.com/papsg/SAROO/commit/b1bf264b68ab8908a7f4c405e368d1ea10565c3b) — [preserve live children on fragmented exFAT](https://github.com/papsg/SAROO/blob/b1bf264b68ab8908a7f4c405e368d1ea10565c3b/doc/STARTUP-CD-STORAGE-AUDIT.md#preserve-live-children-when-deleting-an-exfat-directory). The parent deletes both nonempty fragmented directories under patterned automatic storage. The port passes both 38-method FatFs runs, including eight deletion and preservation cases. The GNU App remains 64,960 load bytes with 576 free and the BootLoader load adds four. Host media and linked images checked.

- [x] `2d8ba306` (AUD-MCU-039): [`f0135c1`](https://github.com/papsg/SAROO/commit/f0135c1438c4ea438ce3eb8dcc72ecfa87af1f82) — [recover clusters after failed file growth](https://github.com/papsg/SAROO/blob/f0135c1438c4ea438ce3eb8dcc72ecfa87af1f82/doc/STARTUP-CD-STORAGE-AUDIT.md#recover-clusters-after-failed-file-growth). The exact parent leaves one orphan in each FAT32 and exFAT witness. The port passes 358 growth and recovery cases in each profile and both 43-method FatFs runs. GNU App 65,136 load bytes, 400 free, with unchanged static DTCM. GNU BootLoader links with all 166 vectors. Fault-injected host media only.

- [x] `4c24b1d8` (AUD-MCU-018): [`27ec0fc`](https://github.com/papsg/SAROO/commit/27ec0fc698fe3841ee59783c136bc8959ab3a6d8) — [bound direct SD command size](https://github.com/papsg/SAROO/blob/27ec0fc698fe3841ee59783c136bc8959ab3a6d8/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#bound-the-size-of-direct-sd-commands). The parent issues a 16 MiB command whose minimum wire time exceeds the stock wait. Both 37-method disk/SD runs and 83 new cases pass, including failure at every chunk. GNU App 65,172 load bytes with 364 free, unchanged static DTCM, and linked BootLoader. Physical SD timing remains unmeasured.

- [x] `8a31651a`: [`39b3724`](https://github.com/papsg/SAROO/commit/39b3724bb96f671d1ac23f56a2ace5a6d931a625) — [place SDRAM refreshes in FC1 idle windows](https://github.com/papsg/SAROO/blob/39b3724bb96f671d1ac23f56a2ace5a6d931a625/doc/FPGA-MEMORY.md#schedule-sdram-refresh-in-saturn-fc1-idle-windows). The parent refreshes 722 clocks outside the window. The port passes the eight FC1 methods, prior cache/reset/clock controls and revised sparse-collision expectation. The worst 8,192-row interval with fallback traffic is 5,956,422 clocks. Quartus passes all 21 constrained checks with 1.076/0.168 ns minimum setup/hold. Issue #349 hardware evidence applies to the historical development build.

- [x] `8345fa51` (MCU-FMC-001): [`623efc6`](https://github.com/papsg/SAROO/commit/623efc6bfe047478da6350d75c342b5d35ad5c1a) — [set the required FMC reserved bit](https://github.com/papsg/SAROO/blob/623efc6bfe047478da6350d75c342b5d35ad5c1a/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#set-the-required-fmc-reserved-bit-during-boot). Parent fails the reserved-bit check, mode and timing controls pass. All three checks pass on the port. The linked 28,964-byte GNU BootLoader differs at one byte, and the GNU App load is byte-identical. Verified against ST RM0433 Tables 184–185.

- [x] `cb70560a` (STM32-AXI-001): [`31e917f`](https://github.com/papsg/SAROO/commit/31e917fb70288310cbe3e9a6782a4ccadc2a2010) — [limit AXI SRAM read issuing on Y and W](https://github.com/papsg/SAROO/blob/31e917fb70288310cbe3e9a6782a4ccadc2a2010/doc/FIRMWARE-RELIABILITY-FOLLOWUP.md#limit-axi-sram-read-issuing-on-stm32-revisions-y-and-w). The parent fails 16 affected cases, and 64 other revision controls pass. Both current 80-case profiles pass. Linked App and BootLoader verify the early conditional register update. GNU App 65,208 load bytes, 328 free, with unchanged static DTCM. No physical Y/W result for the public build.

### Final verification for storage and startup series

All six checkpoints landed in order through [`31e917f`](https://github.com/papsg/SAROO/commit/31e917fb70288310cbe3e9a6782a4ccadc2a2010). The links above record the original changes, their references, and the public adaptations.

The full `make -j6 -C tests check` passes on the archived published tree with all 676 tracked files matched. It passes 900 Python methods and 18 Rust tests, with one Windows-only fixture skipped. The final GNU App and BootLoader loads reproduce their verified candidate bytes. The App uses 65,208 bytes with 328 free and 50,752 static DTCM bytes. The BootLoader uses 29,000 bytes, and both builds retain 166 vectors. The stock menu binary and ELF match the previous build. All 48 linked menu clock cases pass.

All 18 final FPGA project inputs match the completed Quartus fit. The design passes all 21 constrained checks with minimum setup and hold slacks of 1.076 and 0.168 ns. The RTL tests cover the FC1 pair, power-on sequencing, collisions and full-row refresh after signal loss. External bus timing and physical behavior of the public build remain unqualified. The issue #349 hardware result belongs to the historical development build.

Public main is clean and synced. The private checkout fingerprints match for HEAD, branch, status, refs including the stash, and tracked diff. The Saturn was untouched. No PR, USB change or runtime diagnostic was added.

## Saturn menu hardware series

Six ordered stock-menu checkpoints after [`31e917f`](https://github.com/papsg/SAROO/commit/31e917fb70288310cbe3e9a6782a4ccadc2a2010). Each linked note records provenance, applicable Sega documentation, focused verification and hardware limits.

- [x] `4b80469e` (SATURN-SCU-001) — [`0de8086`](https://github.com/papsg/SAROO/commit/0de808682cfb202f68fc6f9b7f22339fd35c853e), [correct level-1 SCU DMA status](https://github.com/papsg/SAROO/blob/0de808682cfb202f68fc6f9b7f22339fd35c853e/doc/SATURN-MENU-HARDWARE.md#correct-level-1-scu-dma-status). The parent fails two of 30 register scenarios. Both corrected profiles pass all 30. The linked 488,770-byte menu and 48 ROM clock controls pass. Current menu callers use levels 0 and 2.

- [x] `25e04166` (SATURN-CD-003 continuation) — [`122beac`](https://github.com/papsg/SAROO/commit/122beac449c70be23f5f49b17b8c80df3a0cca39), [bound the outer physical-disc waits](https://github.com/papsg/SAROO/blob/122beac449c70be23f5f49b17b8c80df3a0cca39/doc/SATURN-MENU-HARDWARE.md#bound-the-outer-waits-for-physical-disc-authentication). The parent fails six stalls and passes seven query controls. Both corrected CD profiles pass 64 methods each, including rollover and cleanup. The linked menu uses 84,292 code/data bytes, with only address relocations in the existing copied routine. No physical disc qualification.

- [x] `bfc3463f` (SATURN-VDP1-001) — [`bb75abf`](https://github.com/papsg/SAROO/commit/bb75abf37e36e0cde8380df76a8899686a069fd1), [initialize the VDP1 erase rectangle](https://github.com/papsg/SAROO/blob/bb75abf37e36e0cde8380df76a8899686a069fd1/doc/SATURN-MENU-HARDWARE.md#initialize-the-vdp1-erase-rectangle). The seeded EWRR case fails on the parent. The port passes both five-method palette runs and seven game-handoff checks. Linked store confirmed at the hardware register. Adds 16 code/data bytes and retains the padded image size.

- [x] `50d580c8` (SATURN-VDP2-001) — [`d6a8a0a`](https://github.com/papsg/SAROO/commit/d6a8a0abb02f450e828278d08cab991715ebef4c), [correct VDP2 VRAM cycle assignments](https://github.com/papsg/SAROO/blob/d6a8a0abb02f450e828278d08cab991715ebef4c/doc/SATURN-MENU-HARDWARE.md#schedule-vdp2-vram-cycles-for-bitmap-and-cpu-access). The parent fails the partition schedule case. Both palette profiles pass all six methods, and nine cover controls pass. The linked menu uses 84,324 code/data bytes with eight cycle-register writes. Physical arbitration remains unqualified.

- [x] `8cf9992e`, superseded by `370d5cbd` (SSR-002) — [`9f57ef6`](https://github.com/papsg/SAROO/commit/9f57ef6e4bc28dfe7b0765f34051125d75ec1918), [enable the NTSC menu display during measured blanking](https://github.com/papsg/SAROO/blob/9f57ef6e4bc28dfe7b0765f34051125d75ec1918/doc/SATURN-MENU-HARDWARE.md#enable-the-ntsc-menu-display-during-measured-blanking). The parent fails three NTSC methods. Both port profiles pass all ten methods, including 263 modeled starting lines, refusal and retry. The linked menu uses 84,628 code/data bytes. Historical hardware evidence is NTSC only, and the public PAL path is unchanged.

- [x] `fa537859` (SATURN-SMPC-001, remaining CD power path) — [`757ebbe`](https://github.com/papsg/SAROO/commit/757ebbed6a5231cb4a8ca81b9fa082aedb5b54da), [admit physical-drive power commands at a safe scan phase](https://github.com/papsg/SAROO/blob/757ebbed6a5231cb4a8ca81b9fa082aedb5b54da/doc/SATURN-MENU-HARDWARE.md#admit-physical-drive-power-commands-at-a-safe-scan-phase). The parent fails four timing methods and passes 11 controls. Both corrected CD profiles pass 68 methods. The linked menu uses 84,900 code/data bytes and keeps 20,296 bytes of body margin. Physical SMPC timing and real-disc behavior remain unqualified.

### Final verification for the Saturn menu hardware series

All six firmware checkpoints landed in order through [`757ebbe`](https://github.com/papsg/SAROO/commit/757ebbed6a5231cb4a8ca81b9fa082aedb5b54da). The complete suite exposed a scrolling fixture that needed to model an already-ready display. The test-only follow-up [`9b7bfb1`](https://github.com/papsg/SAROO/commit/9b7bfb100716e051419a86f93cc53e2c427c8f18) passes both eight-method timer runs and leaves the firmware build inputs unchanged.

The full `make -j6 -C tests check` passed on an archive matching all 680 tracked files. It discovered 917 Python methods and passed 915, with the Windows replacement fixture and optional RTX pool fixture skipped in that invocation. The RTX pool fixture passed separately in QEMU with the current source and the archived GNU RTX objects. All 18 Rust tests passed. The independent atlas oracle used the cached Pillow 13.0.0. All six original checkpoints also passed their focused tests and a complete SH-2 build before publication.

The final menu archive reproduces the verified 488,770-byte image and ELF byte for byte and passes all 48 linked ROM clock controls. The menu has 84,900 code/data bytes, 9,488 static BSS bytes, a 16,384-byte stack and 20,296 bytes of body margin. MCU and FPGA source inputs match the previous series. Historical NTSC phase captures support the display algorithm. This public build and PAL display admission remain physically unqualified.

The private checkout's HEAD, branch, refs and stash, status and tracked diff match the starting fingerprints. The Saturn was untouched. The published deltas passed the secret scan and public identity review.

## Saturn menu handoff and rendering series

Six ordered checkpoints after [`9b7bfb1`](https://github.com/papsg/SAROO/commit/9b7bfb100716e051419a86f93cc53e2c427c8f18). The linked notes record provenance, Sega documentation, verification and hardware limits.

- [x] `f822fc1e` + `33014804` (SATURN-SMPC-002/004) — [`35c06d1`](https://github.com/papsg/SAROO/commit/35c06d11c22ae0b29cf38c80e7d009d237e94179), [complete reset disable before BIOS player handoff](https://github.com/papsg/SAROO/blob/35c06d11c22ae0b29cf38c80e7d009d237e94179/doc/SATURN-MENU-HARDWARE.md#complete-reset-disable-before-handing-off-to-the-bios-player). The parent fails both new methods. The port passes both seven-method profiles, 38 adjacent checks and 48 linked ROM clock checks. The historical private Astal test reached the player. Physical qualification of the public image remains open.

- [x] `99806a35` (SATURN-VDP1-003) — [`82e4ec7`](https://github.com/papsg/SAROO/commit/82e4ec7b0d971acb004fdcc4e73ca6678ecf1166), [terminate the active draw before replacing command RAM](https://github.com/papsg/SAROO/blob/82e4ec7b0d971acb004fdcc4e73ca6678ecf1166/doc/SATURN-MENU-HARDWARE.md#stop-the-active-vdp1-draw-before-replacing-command-ram). The parent fails the ordered replacement check. Both eleven-method profiles pass on the port, with ten adjacent controls and 48 linked ROM clock cases. The linked menu grows by 32 code/data bytes. Physical draw timing remains unqualified.

- [x] `db0f31aa` (SATURN-VDP2-003) — [`b6ca1ab`](https://github.com/papsg/SAROO/commit/b6ca1ab6443f8026abe71c9f6cfa436ec675fde7), [initialize both VDP2 menu layer transforms](https://github.com/papsg/SAROO/blob/b6ca1ab6443f8026abe71c9f6cfa436ec675fde7/doc/SATURN-MENU-HARDWARE.md#initialize-both-vdp2-menu-layer-transforms). The parent fails the seeded coordinate case. Both twelve-method profiles pass across cold and logo paths in both models. Nine cover checks, 48 linked clock controls and all eight target coordinate stores are verified. Physical rendering remains unqualified.

- [x] `b0177efe` (SATURN-VDP2-004) — [`d2b0a97`](https://github.com/papsg/SAROO/commit/d2b0a9722c634da75e1c5457f941eae79df95153), [set VDP2 menu compositing defaults](https://github.com/papsg/SAROO/blob/d2b0a9722c634da75e1c5457f941eae79df95153/doc/SATURN-MENU-HARDWARE.md#set-vdp2-menu-compositing-defaults). The parent fails the seeded state case. Both thirteen-method profiles pass with logo and cover controls, plus 20 adjacent and 48 linked clock cases. The linked menu adds 48 code/data bytes. Physical output remains unqualified.

- `232758b3` (SATURN-CD-003 continuation) — excluded. This change would improve the existing disc-authentication bypass. The previously published outer-wait handling remains as recorded in the preceding series.

- [x] `268ce1cc` + `eb4826d3` (SATURN-SCSP-001/002) — [`5b5409b`](https://github.com/papsg/SAROO/commit/5b5409bb58814ac59e223c03b934994a4b85aa10), [initialize the stereo CD audio mixer with word writes](https://github.com/papsg/SAROO/blob/5b5409bb58814ac59e223c03b934994a4b85aa10/doc/SATURN-MENU-HARDWARE.md#initialize-the-stereo-cd-audio-mixer-with-word-writes). Both three-method profiles pass. The parent fails width and stereo fields, and the old aligned left constant fails only stereo. All 55 adjacent and 48 linked clock checks pass. The linked menu stays at 85,076 code/data bytes. Physical stereo output remains unqualified.

### Final verification for the Saturn menu handoff and rendering series

Five firmware checkpoints landed in order through [`5b5409b`](https://github.com/papsg/SAROO/commit/5b5409bb58814ac59e223c03b934994a4b85aa10). The raw disc-authentication disposition is recorded above. Each port passed focused checks in both sanitizer and optimized profiles and a full SH-2 build before publication.

The archived public tip matches all 682 tracked files. The full `make -j6 -C tests check` passed 924 of 925 Python methods with the Windows replacement fixture skipped. The RTX pool fixture passed with the unchanged MCU source and archived GNU RTX objects. All 18 Rust tests passed. The independent atlas check used the cached Pillow 13.0.0.

A fresh SH-2 link reproduces the 488,770-byte menu image and its ELF byte for byte. The final image uses 85,076 code/data bytes, 9,488 static BSS bytes, a 16,384-byte reserved stack and 20,120 bytes of body margin. All 48 linked ROM clock cases pass. The private Astal result is historical evidence for the reset admission approach. Public-build hardware qualification remains open.

The published delta and ledger passed the secret scan and public identity review. The private checkout's HEAD, branch, refs and stash, status and tracked diff match the starting fingerprints. The Saturn was untouched.

## Saturn CPU and FPGA bus timing series

Six ordered checkpoints after [`5b5409b`](https://github.com/papsg/SAROO/commit/5b5409bb58814ac59e223c03b934994a4b85aa10). The linked notes record attribution, primary references, verification and hardware limits.

- [x] `aece1be2` (SATURN-CACHE-001) — [`41ce86f`](https://github.com/papsg/SAROO/commit/41ce86f3ca04bc140afb246b331cd1d8659ba893), [purge the SH-2 cache during BIOS player handoff](https://github.com/papsg/SAROO/blob/41ce86f3ca04bc140afb246b331cd1d8659ba893/doc/SATURN-MENU-HARDWARE.md#purge-the-sh-2-cache-during-bios-player-handoff). The parent links only the enable store. Both eight-method profiles and 48 linked clock checks pass on the port with all three CCR stores. Menu image and allocations retain their sizes. Hardware remains unqualified.

- [x] `33b51a8c` + `59239df4` (SATURN-DMAC-001/002) — [`179e9af`](https://github.com/papsg/SAROO/commit/179e9afc51e030f9cee0b147222aca98af77576e), [stop SH-2 DMA before replacing channel state](https://github.com/papsg/SAROO/blob/179e9afc51e030f9cee0b147222aca98af77576e/doc/SATURN-MENU-HARDWARE.md#stop-sh-2-dma-before-replacing-the-channel-state). The parent and wrong-address control fail the ordered access check. Both nine-method profiles, 15 adjacent checks and 48 linked clock cases pass. The linked menu has 85,092 code/data bytes. Physical DMA handoff remains unqualified.

- [x] `843c51bf` + `d604fb88` (SATURN-SCU-003/005) — [`fff4c31`](https://github.com/papsg/SAROO/commit/fff4c31), [restore cartridge timing from WRAM](https://github.com/papsg/SAROO/blob/fff4c31/doc/SATURN-MENU-HARDWARE.md#restore-cartridge-timing-from-wram-after-a-clock-change). Both ten-method profiles, 15 adjacent methods, the ordinary linked tail and 48 linked ROM cases pass. The parent misses the dummy read in host and both linked fixtures. The menu links with 20,064 bytes of body margin. An earlier development RAM trial reached the affected game after the WRAM correction. Physical qualification for the public build remains open.

- [x] `41a2fe2f` + `3abb5c6c` (FPGA-SDRAM-001/003) — [`a73cb1e`](https://github.com/papsg/SAROO/commit/a73cb1e), [capture SDRAM read bursts at the pins](https://github.com/papsg/SAROO/blob/a73cb1e/doc/FPGA-MEMORY.md#capture-sdram-read-bursts-at-the-pins). The parent fails all four CL2 delay cases, and an extra validity stage drops the first word. The selected source passes both new methods and 35 adjacent FPGA methods. All five Quartus stages and 30 reported constraints pass. Minimum setup/hold are +0.578/+0.159 ns. A prior development build passed scoped register, cache and SD-transfer checks. The public bitstream has no physical qualification.

- [x] `bcd2ae55` + `f491d0ee` + `f1f20c0e` + `44fe8abd` (FPGA-FMC-001/002/003 and throughput) — [`5fa8c2e`](https://github.com/papsg/SAROO/commit/5fa8c2e), [keep MCU halfwords coherent across the FMC bus](https://github.com/papsg/SAROO/blob/5fa8c2e/doc/FPGA-MEMORY.md#keep-mcu-halfwords-coherent-across-the-fmc-bus). All 30 complete pin-model streams, both negative controls, 37 adjacent FPGA methods and 17 related command and save methods pass. The parent fails early NWAIT assertion. Modeled write rate is 13.20–14.19 MB/s. The final fit passes all five Quartus stages and 39 timing checks after pin-adjacent placement restores +0.210 ns on the 5 ns release path. Physical transfer timing for the public bitstream remains open.

- [x] `9ba50fc5` (FPGA-ABUS-001) — [`4dc4b66`](https://github.com/papsg/SAROO/commit/4dc4b665338e448271706b29b1e4d65611314927), [capture Saturn writes on the A-bus data phase](https://github.com/papsg/SAROO/blob/4dc4b665338e448271706b29b1e4d65611314927/doc/FPGA-MEMORY.md#capture-saturn-writes-on-the-a-bus-data-phase). The parent emits a write without a TIM1 event. The port passes 40 phase, polarity and skew runs. All ten delayed live-address controls fail at the SDRAM pins. The FPGA05 map remains in use. All five Quartus stages and 39 timing checks pass, with +0.197 ns minimum setup, +0.164 ns minimum hold and +5.867 ns minimum reported A-bus bundle slack. Board and game timing remain unqualified.

The final archived source matches all 691 tracked files. `make -j6 -C tests check` passed 934 of 935 Python methods and all 18 Rust tests. The Windows replacement fixture was skipped. The fresh SH-2 menu and ELF reproduce the verified SCU checkpoint byte for byte. The linked ordinary WRAM tail and all 48 relocated ROM cases pass. The final Quartus build matches all 21 FPGA input hashes.

All six deltas passed secret scanning and public identity review. Private HEAD, branch, refs and stash, status and tracked diff match the starting fingerprints. No Saturn operation was performed.

## Saturn bus reset, menu DMA and CD drive behavior

Six ordered checkpoints after [`4dc4b66`](https://github.com/papsg/SAROO/commit/4dc4b665338e448271706b29b1e4d65611314927). Private source IDs are attribution. The public notes record source, verification and hardware limits.

- [x] `92f619b0` (FPGA-ABUS-002) — [public commit](https://github.com/papsg/SAROO/commit/22d3c835e6de54a71a056d1f1cdd1be7aea950e1), [set Saturn data direction before chip select](https://github.com/papsg/SAROO/blob/22d3c835e6de54a71a056d1f1cdd1be7aea950e1/doc/FPGA-MEMORY.md#set-saturn-data-direction-before-chip-select). The parent fails idle direction. The port passes 13 direction and isolation cases and six adjacent FPGA methods. All five Quartus stages and 39 reported timing checks pass. RD-to-DIR is 6.390 ns, with 7.045 ns worst across the three inputs against 12 ns. Board-level switching and game behavior remain unqualified.

- [x] `5e95a7cc` + `bd0aab2e` (FPGA-RESET-001/003) — [`2402006`](https://github.com/papsg/SAROO/commit/2402006c779c2a7dfe1d3d092a2c5ea262e08865), [quiet the Saturn bus during reset and retain CD state](https://github.com/papsg/SAROO/blob/2402006c779c2a7dfe1d3d092a2c5ea262e08865/doc/FPGA-MEMORY.md#quiet-the-saturn-bus-during-reset-and-retain-cd-state). The parent drives during SYSRES and a mapping-reset control loses the CD selection. Three full-top phase runs pass, including a chip select held across release, MCU completion, mailbox retention and full reset. All eight FC1 checks and the adjacent A-bus, FMC, HIRQ and menu checks pass. All five Quartus stages and 39 timing checks pass, with minimum setup and hold of +0.197 and +0.162 ns. Physical qualification remains open.

- [x] `8b86806f` (FPGA-CART-001) — [`5ff0b5e`](https://github.com/papsg/SAROO/commit/5ff0b5eb0a5189aedc3092939cf285dc5270ec09), [terminal extended-RAM cartridge ID](https://github.com/papsg/SAROO/blob/5ff0b5eb0a5189aedc3092939cf285dc5270ec09/doc/FPGA-MEMORY.md#decode-the-terminal-extended-ram-cartridge-id). The parent falsely returns the 1 MiB ID at the lower edge of the old alias. The pin fixture covers both byte forms and all 32,767 neighboring words in each of four modes. All 46 FPGA methods and five Quartus stages pass with 39 nonnegative timing rows. Minimum setup and hold are +0.210 ns and +0.186 ns. Physical RAM-cartridge behavior remains open.

- [x] `591f7c2b` (SATURN-SCU-004) — [`dbef306`](https://github.com/papsg/SAROO/commit/dbef306ac2a956fb0260c0440ff58f8e7811eda8), [scrolling-selection DMA ownership](https://github.com/papsg/SAROO/blob/dbef306ac2a956fb0260c0440ff58f8e7811eda8/doc/SATURN-MENU-HARDWARE.md#finish-scrolling-selection-dma-before-buffer-reuse). The parent returns with the last row still pending in all three level-0 states. Five DMA methods pass in each of two profiles, with 23 adjacent timer, cover and clock methods. The SH-2 link passes the ordinary WRAM clock tail and 48 relocated ROM cases. Code/data grows by 32 bytes to 85,164, leaving 20,032 bytes of body margin. Physical display timing remains open.

- [x] `b768af07` (MCU-CDC-R04) — [`fe5853e`](https://github.com/papsg/SAROO/commit/fe5853ebc24440b0571ab635e1881b6477ebbd38), [virtual tray and observed removal](https://github.com/papsg/SAROO/blob/fe5853ebc24440b0571ab635e1881b6477ebbd38/doc/CD-DRIVE-SAFETY.md#open-the-virtual-tray-and-wait-for-observed-sd-removal). The parent ignores OPEN and can reload the queued image before removal. The corrected drive passes both 203-method CD profiles, both 14-method disc/save profiles, and the 31 CUE, 11 timer, 44 audio and 11 menu-command methods. The exact timer conversion at 100 Hz removes the GNU 64-bit divide. The App links 64,760 load bytes, with 776 ITCM bytes free and 166 vectors. Physical swaps, timing and MDK remain open.

- [x] `32a363af` + `55dc3521` (MCU-CDC-R05/R23/R24) — [`e880fb2`](https://github.com/papsg/SAROO/commit/e880fb298b2992ed01f20b8077f0b2cd7ae15c83), [forward and reverse CD scan](https://github.com/papsg/SAROO/blob/e880fb298b2992ed01f20b8077f0b2cd7ae15c83/doc/CD-DRIVE-SAFETY.md#implement-forward-and-reverse-cd-scan). The parent acknowledges without entering SCAN. Restoring each historical error independently fails the timing or Q-report control. All 207 optimized CD and 45 optimized audio methods pass. Full check passes 949 Python methods and 18 Rust tests with one Windows-only skip. The final GNU App uses 65,396 ITCM load bytes and leaves 140 free, with 50,768 DTCM bytes and 166 vectors. Fast-scan pickup cadence is approximate. Physical playback and MDK remain open.

## SD media and CD report and sector correctness

Six ordered checkpoints after [`e880fb2`](https://github.com/papsg/SAROO/commit/e880fb298b2992ed01f20b8077f0b2cd7ae15c83). The linked notes record provenance, primary references, reproducible validation and hardware limits.

- [x] `3f0e15ae` (MCU-SD-007) — [`8a33c14`](https://github.com/papsg/SAROO/commit/8a33c14dfc2b221a6eb919bc760ae7f8bbb3e8af), [reject SD card errors and failed recovery STOP](https://github.com/papsg/SAROO/blob/8a33c14dfc2b221a6eb919bc760ae7f8bbb3e8af/doc/FATFS-SD-SAFETY.md#reject-sd-card-status-errors-and-failed-recovery-stop). The parent accepts 179 rejected card responses and misses the STOP retry. Both corrected 40-method storage profiles pass. The GNU App links 65,448 bytes with 88 ITCM bytes free, unchanged DTCM and 166 vectors. Physical recovery and MDK remain open.

- [x] `3b44556a` + `5c15670e` (MCU-SD-008) — [`4fc9ea3`](https://github.com/papsg/SAROO/commit/4fc9ea36fb2548f5af2889a195f46d0136dbef89), [complete SD programming before sync and reads](https://github.com/papsg/SAROO/blob/4fc9ea36fb2548f5af2889a195f46d0136dbef89/doc/FATFS-SD-SAFETY.md#complete-pending-sd-programming-before-sync-and-reads). The parent starts reads early and falsely acknowledges filesystem sync. The port passes both 44-method SD/disk profiles and 43 FatFs methods. The public size adaptation removes verbose SD identification prints and their reporting-only CSD calculations. The GNU App links 65,036 bytes, with 500 ITCM bytes free and 50,736 bytes of DTCM span. Physical programming and durability remain open.
- **SD media readiness and retained FatFs session** — paps-g `de274082`, test `371f5369`, MCU-SD-R09 → [`c832021`](https://github.com/papsg/SAROO/commit/c832021e1e8fa2b69f71afa6fa07ecf219450cdc). Depends on shared SD transfer/recovery and write completion. Parent fails sampled removal, post-CMD13 sync, absent disk status and FAT32/exFAT retained mount controls. Sanitized disk/FatFs/save suite passes 207 methods and the optimized disk/FatFs suite passes 91. GNU App 65,184 bytes, 352 free, DTCM span 50,736. Card-detect pulses between samples, physical write durability and MDK remain unqualified.
- **Invalid CD status, periodic and WAIT reports** — paps-g `6f4f8b90` and `0657ffd1`, MCU-CDC-R06 → [`432c428`](https://github.com/papsg/SAROO/commit/432c42848f1399eda0c7d5347d360e365cacb1d7). Both parent regressions expose stale fields. Both 237-method CD and CUE profiles pass with the shared report formatter, retained pickup and valid WAIT/REJECT controls. Sega ST-162 §7.2.4, printed pp. 59–61. GNU App 65,244 bytes, 292 free, DTCM span 50,736. Physical report timing and MDK remain unqualified.
- **SDRAM power-on startup refresh** — paps-g `3c0fd621` is already covered by [`39b3724`](https://github.com/papsg/SAROO/commit/39b3724bb96f671d1ac23f56a2ace5a6d931a625). Public RTL gates external refresh events during `M_POR`. The early-FC1 countdown regression passes on the public source.
- **Raw Mode 1 image and CD audio buffer layouts** — paps-g `7d543dac`, MCU-CDC-R11/R13 → [`e4ca31f`](https://github.com/papsg/SAROO/commit/e4ca31fcf4af6574df78517a7e45299491df279c). Parent fails raw-image preservation, directory buffer bytes and the audio/XA sizing control. Both 239-method CD and CUE profiles pass, including all four FIFO widths and mixed image sectors across direct, COPY and MOVE paths. Sega ST-162 §5.4 p. 48 and §8.2.7 p. 95. GNU App 65,260 bytes, 276 free, DTCM span 50,736. Host PUT follow-up `ba472965` is separate. Physical playback and MDK remain unqualified.
- **Cooked image padding isolation** — paps-g `6901e3c2`, MCU-CDC-R14 → [`ec8f8f6`](https://github.com/papsg/SAROO/commit/ec8f8f6865cfffd93a44f646a132675077ccb4ea). Parent returns seeded old pool bytes in the wide FIFO reply. Both 240-method CD/CUE profiles and the final 15-method directory checks pass. Sega ST-162 §5.4 and ECMA-130 §14. GNU App 65,280 bytes, 256 free, DTCM span 50,736. The synthetic container supplies padding. Host PUT and missing raw-field synthesis remain separate work. Final repository-wide host suite is running.
- **Series complete.** The full `make -j6 -C tests check` run used an archive of the published `ec8f8f6` tree with all 699 tracked blobs checked. It passed 966 Python tests and 18 Rust tests, with one Windows-only CD+G fixture skipped. All six staged GNU App links were checked against their committed MCU sources. The final App uses 65,280 bytes of the 65,536-byte ITCM image limit. Physical Saturn and MDK qualification remain open.

## Mode 2 headers, SD initialization and CD selector output

Six ordered checkpoints after [`ec8f8f6`](https://github.com/papsg/SAROO/commit/ec8f8f6865cfffd93a44f646a132675077ccb4ea). The linked public notes record provenance, verification and remaining limits.

- **Reverted host PUT padding** — paps-g `a47a15db` / `d2b9867a`. [Sega ST-162 §5.4(c), printed p. 48](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=36) leaves the tail undefined for host-supplied user data. The private branch reverted this separate experiment.
- **Mode 2/2336 header** — paps-g `669d3301`, MCU-CDC-R16 → [`4a9fb8e`](https://github.com/papsg/SAROO/commit/4a9fb8e3c75a6b16de346337c68e2bd3cc733a64). The parent fails the complete-container comparison. Both 241-method CD/CUE profiles pass, including 64 XA form, address and FIFO-width cases and all routed-copy controls. The public port retains the existing prefix outside the two-digit minute range. Sega ST-162 §5.4, ECMA-130 §§14.1–14.2 and ECMA-168 Annex C §C.2.1. GNU App 65,396 bytes, 140 free, DTCM span 50,736. Physical reads and MDK remain unqualified.
- **SD CMD8 echo** — paps-g `8e5ea8ce`, MCU-SD-009 → [`5f78144`](https://github.com/papsg/SAROO/commit/5f781445bbbcea384057a4fdfaa658c37a97a2cb). The parent proceeds on six malformed echoes. Both 48-method disk profiles pass, including exact and reserved-upper-bit echoes. SD Physical Layer 3.01 §4.3.13. GNU App 65,424 bytes, 112 free, DTCM span 50,736. Physical negotiation and MDK remain unqualified.
- **Legacy SD byte addressing** — paps-g `76c94c6c`, MCU-SD-010 → [`8a436ea`](https://github.com/papsg/SAROO/commit/8a436ea69a0a6fe87953d10d9b4f7d1dc19fe64e). The parent selects block addressing for a legacy card with the otherwise unusable capacity bit set. The corrected driver passes both 49-method disk profiles and the other three initialization/capacity pairs. SD Physical Layer 3.01 §4.2.3. GNU App 65,436 bytes, 100 free, DTCM span 50,736. Physical addressing and MDK remain unqualified.
- **SD application-command acknowledgment** — paps-g `56e9f763`, MCU-SD-011 → [`cefe207`](https://github.com/papsg/SAROO/commit/cefe20778f54f68fef8e52e8d5c913088619d60a). The parent accepts missing acknowledgment at the R1, initialization and write boundaries. Both 52-method disk profiles pass, including legal ACK and ACMD41 R3 controls. SD Physical Layer 3.01 §4.3.9.1. GNU App 65,464 bytes, 72 free, DTCM span 50,736. Physical card behavior and MDK remain unqualified.
- **CMD8 no-response classification** — paps-g `b25cbc7e`, MCU-SD-012 → [`a99a3f9`](https://github.com/papsg/SAROO/commit/a99a3f9ff99258f29169d945f2d3a9b7a323cd27). The parent enters legacy negotiation on nine non-timeout failures. Both 53-method disk profiles pass, including a response timeout with an unrelated status bit. SD Physical Layer 3.01 §4.2.3 and STM32 RM0433 Rev 8 §55.10.11. GNU App 65,484 bytes, 52 free, DTCM span 50,736. Physical card timing and MDK remain unqualified.
- **CD partition output reset** — paps-g `aade02ea`, MCU-CDC-R17 → [`73a8864`](https://github.com/papsg/SAROO/commit/73a8864a63849cd2657e5d1da6ddc78d80929060). The parent leaves GET and GETDEL connected. Both 243-method CD/CUE profiles pass, including 48 partition/read pairs, preserved ownership through DataEnd, and input/condition/PUT controls. Sega ST-162 §8.2.5, data 5.9, printed p. 91. GNU App 65,500 bytes, 36 free, DTCM span 50,736. Physical HIRQ timing and MDK remain unqualified.

### Final verification for Mode 2, SD initialization and selector output

All six checkpoints landed in order through [`73a8864`](https://github.com/papsg/SAROO/commit/73a8864a63849cd2657e5d1da6ddc78d80929060). The archived tree matched all 699 tracked blobs. The full `make -j6 -C tests check` passed 975 Python and 18 Rust tests with one Windows-only fixture skipped.

All six GNU App links match their committed MCU inputs and contain 166 vectors. The final App uses 65,500 of 65,536 ITCM load bytes and 50,736 bytes of DTCM data and BSS span. The GNU BootLoader comparison used the 13 enabled project C units, original BOOT and RTE settings and common providers. The SD baseline and each SD checkpoint link at the original 0x08000000 flash base with 166 vectors. The final load is 29,080 bytes, up from 28,992. Original MDK and physical Saturn behavior remain unqualified.

The public deltas passed staged secret scanning and the public commit identity check. Private checkout fingerprints match and the Saturn was untouched.

## CD commands, STM32 I2S and menu cover visibility

Six ordered checkpoints after [`73a8864`](https://github.com/papsg/SAROO/commit/73a8864a63849cd2657e5d1da6ddc78d80929060).

- **CD frame-address search** — paps-g `a42241e3`, MCU-CDC-R18 → [`b7bdd73`](https://github.com/papsg/SAROO/commit/b7bdd734e9f7801645e77c2872645f31c4c8afd3). The parent leaves stale registers for the query. Both 246-test CD/CUE profiles pass, including 24 selectors, a full 200-block partition, result lifetime, GET/GETDEL ownership and existing trace labels. [Sega ST-162-062094 §8.2.6, functions 6.6 and 6.7, printed p. 94](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=82); [reproduction and limits](https://github.com/papsg/SAROO/blob/b7bdd734e9f7801645e77c2872645f31c4c8afd3/doc/CD-DRIVE-SAFETY.md). The straight link overflowed by 288 bytes. Moving the two high trace labels out of the dense lookup reduces its table from 904 to 472 bytes. GNU App 65,408 bytes, 128 free, DTCM span 50,744. Physical command timing and MDK remain unqualified.
- `043223c0` (paps-g, MCU-CDC-R19) → [`de502b9`](https://github.com/papsg/SAROO/commit/de502b990c3b93351739882ed9b07987e6fce28e). Both selectors are checked before either sector length changes; all four sizes and the no-change value retain their behavior. [Sega ST-162-062094 §§3.3, 8.2.7, printed pp. 31, 95](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=83); [reproduction and limits](https://github.com/papsg/SAROO/blob/de502b990c3b93351739882ed9b07987e6fce28e/doc/CD-DRIVE-SAFETY.md). The parent fails on invalid input; 65,536 pairs and both 246-method CD/CUE profiles pass. GNU App: 65,436 bytes, 100 free, 166 vectors. No hardware or MDK qualification.
- `bf703240` (paps-g) → [`ce2b71d`](https://github.com/papsg/SAROO/commit/ce2b71d1baec49aea9c98429ccc78869a8198b3e). The I2S transmit-DMA write now retains the documented reset fields. [ST RM0433 Rev. 8 §§50.9.3, 50.11.3, printed pp. 2194, 2219](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf); [reproduction and limits](https://github.com/papsg/SAROO/blob/ce2b71d1baec49aea9c98429ccc78869a8198b3e/doc/AUDIO-SAFETY.md). The parent fails the cold-start register assertion. Both 45-method audio profiles pass, including delayed initialization and exact PCM controls. GNU App: 65,440 bytes, 96 free, 166 vectors. No hardware or MDK qualification.
- `14b12636` (paps-g) → [`0f9a0a6`](https://github.com/papsg/SAROO/commit/0f9a0a6a481ac662439a4a7470e47c20ee9b1aef). Restoring a cover layer keeps its VDP2 window within the drawn viewport. [Sega ST-058-R2-060194 §§8.1–8.2, printed pp. 180–181, 190](https://antime.kapsi.fi/sega/files/ST-058-R2-060194.pdf#page=198); [reproduction and limits](https://github.com/papsg/SAROO/blob/0f9a0a6a481ac662439a4a7470e47c20ee9b1aef/doc/SATURN-MENU-HARDWARE.md). Four clipped cases fail on the parent. All nine toggle controls, 157 focused methods and 10 optimized cover methods pass. Stock SH-2 menu: 488,770-byte image, 85,212 code/data bytes, 19,988-byte body margin, linked ordinary clock tail and 48 ROM clock cases pass. No physical display qualification.
- `7360fa4c` (paps-g) → [`b1826d3`](https://github.com/papsg/SAROO/commit/b1826d3a432c60f01e5a1d3847c4a653a996eeb3). An unsupported emulated command returns REJECT before CMOK, retaining buffer-ready and passthrough behavior. [Sega ST-162-062094 §§3.3, 7.2.4, printed pp. 31, 59–60](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=47); [reproduction and limits](https://github.com/papsg/SAROO/blob/b1826d3a432c60f01e5a1d3847c4a653a996eeb3/doc/CD-DRIVE-SAFETY.md). The parent returns stale PAUSE; five opcodes with both buffer states and both 247-method CD/CUE profiles pass. GNU App remains 65,440 bytes, 96 free, 166 vectors. No hardware or MDK qualification.
- `34ad28b3` (paps-g, MCU-CDC-R21) → [`82edd43`](https://github.com/papsg/SAROO/commit/82edd435fa48dfe37f1e26221fd4d076cbebdaa4). The empty drive starts in NODISC before command service; the hardware signature and mounted-disc transitions remain unchanged. [Sega ST-162-062094 §§7.2.3–7.2.4, printed pp. 58–60](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=46); [reproduction and limits](https://github.com/papsg/SAROO/blob/82edd435fa48dfe37f1e26221fd4d076cbebdaa4/doc/CD-DRIVE-SAFETY.md). The parent fails the first session query with an uninitialized TOC. The status, absent-session and mounted-session controls and both 248-method CD/CUE profiles pass. GNU App: 65,440 bytes, 96 free, 166 vectors. No physical startup or MDK qualification.

### Final verification for CD commands, I2S and cover visibility

All six changes landed in order through [`82edd43`](https://github.com/papsg/SAROO/commit/82edd435fa48dfe37f1e26221fd4d076cbebdaa4). The final archive matched all 699 tracked files. The full `make -j6 -C tests check` passed 981 Python methods and 18 Rust tests, with one Windows-only fixture skipped on macOS.

All five MCU links and the stock SH-2 menu match their committed source hashes. The final GNU App has 166 vectors, uses 65,440 of 65,536 ITCM load bytes and spans 50,744 DTCM bytes for data and BSS. The BootLoader target excludes the changed Saturn source group. The stock menu links with 19,988 bytes of body margin and passes the ordinary clock tail and 48 linked ROM clock cases. Original MDK output and physical Saturn behavior were not qualified.

The published changes passed secret and commit identity checks. The private repository and Saturn were untouched.

## BIOS player return, disc reader ownership and CD transfers

Six ordered checkpoints after [`82edd43`](https://github.com/papsg/SAROO/commit/82edd435fa48dfe37f1e26221fd4d076cbebdaa4).

- `7f626020` (paps-g, SATURN-MENU-013) → [`28bb157`](https://github.com/papsg/SAROO/commit/28bb1577f025e6578e78ccb83ba9954de1bc95ef). Mask the retiring player's SCU interrupts and restore the BIOS callback before menu BSS initialization. The parent and a restore-only control fail independently. All 143 focused methods, 10 optimized player methods and 48 linked ROM clock cases pass. Stock menu has 85,244 code/data bytes and 19,956 bytes of body margin. A historical private RAM trial reached SAROO after Astal and the BIOS player. Public hardware behavior remains unverified. [Sega ST-162-R1-092994, printed pp. 2 and 10](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=15). [Reproduction and limits](https://github.com/papsg/SAROO/blob/28bb1577f025e6578e78ccb83ba9954de1bc95ef/doc/SATURN-MENU-HARDWARE.md).
- `ff3f9d2f` (paps-g, MCU-MENU-001) → [`1269bc2`](https://github.com/papsg/SAROO/commit/1269bc21b532febedf5054670d868902b454ab9d). Pause and close the old reader before startup uses the shared cache. A failed handoff leaves the tagged request unacknowledged, so the current menu can retry. Successful legacy startup and catalog replies retain their format. The BIOS clock portion is already in `5eca24a`. The parent reaches the catalog with an active reader. All 212 focused methods and 12 optimized menu command methods pass. GNU App uses 65,472 bytes and leaves 64 free. [Sega ST-169-R1-072694 §1.1, printed p. 2](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=12). [Reproduction and limits](https://github.com/papsg/SAROO/blob/1269bc21b532febedf5054670d868902b454ab9d/doc/SAVE-CD-RESET-OWNERSHIP.md).
- `ba472965` (paps-g, MCU-CDC-R25) → [`f0b8ed7`](https://github.com/papsg/SAROO/commit/f0b8ed7dab0615c02f54b68344ded4705a425ef2). Retain the Mode 1 marker when DataEnd publishes a header-bearing PUT. The parent returns the wrong bytes. All 128 PUT/COPY/MOVE/GET combinations and both 249-method CD/CUE profiles pass. GNU App uses 65,488 bytes and leaves 48 free, with unchanged static DTCM. [Sega ST-162-062094 §§5.4(2), 8.2.7, printed pp. 48 and 95](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=36). [Reproduction and limits](https://github.com/papsg/SAROO/blob/f0b8ed7dab0615c02f54b68344ded4705a425ef2/doc/CD-DRIVE-SAFETY.md).
- `482d27d8` (paps-g, SATURN-CD-004) → [`8e29b65`](https://github.com/papsg/SAROO/commit/8e29b6574a8287568a4c6f14ead7b6055a06672e). Accept DataEnd counts from the consumed words through the Form 2 limit. The parent rejects a successful partial copy. All 96 boundary cases, the 896-word Kaitei Daisensou final reply, 143 focused methods, 70 optimized CD menu methods and 48 linked ROM clock cases pass. Stock menu remains at 85,244 code/data bytes. [Sega ST-162-062094 functions 1.10 and 7.1, printed pp. 81 and 95](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=69). [Reproduction and limits](https://github.com/papsg/SAROO/blob/8e29b6574a8287568a4c6f14ead7b6055a06672e/doc/CD-EVENTS-AND-GAME-LAUNCH.md).
- `6bcc069b` (paps-g, GAME-REG-004) → [`576d8bb`](https://github.com/papsg/SAROO/commit/576d8bba42b71560f04827f016b4ae4f1288a1f7). Guard the US v1.000 soft-reset correction by product, version, loaded executable CRC and original instruction. The parent leaves the prohibited instruction in place. All 11 synthetic dispatch cases, 144 focused methods, eight optimized methods and 48 linked ROM clock cases pass. Stock menu adds 108 code/data bytes and retains 19,848 bytes of body margin. Historical private hardware accepted the reported reset sequence. [Sega ST-210-110194 precaution 14, printed p. 5](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf#page=9). [Hitachi SH7604 §8.4.2, printed p. 219](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=235). [Reproduction and limits](https://github.com/papsg/SAROO/blob/576d8bba42b71560f04827f016b4ae4f1288a1f7/doc/CD-EVENTS-AND-GAME-LAUNCH.md).
- `340f77b8` (paps-g, MCU-CDC-R26) → [`6c7c5cd`](https://github.com/papsg/SAROO/commit/6c7c5cdc77b6018c2064521c9fe510522914dae0). Discard the retired disc's directory extents and file page on unload, including close errors. The parent retains the old seven-sector root. The replacement reads its own one-sector root and admits its first file. Both 250-method CD/CUE profiles and 35 adjacent parser, disc/save and header methods pass. GNU App uses 65,504 bytes and leaves 32 free. Physical Daytona USA recovery remains unverified. [Sega ST-162-062094 functions 8.1–8.5, printed pp. 99–100](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=87). [ECMA-119 §8.4.18, printed p. 20](https://ecma-international.org/wp-content/uploads/ECMA-119_2nd_edition_december_1987.pdf). [Reproduction and limits](https://github.com/papsg/SAROO/blob/6c7c5cdc77b6018c2064521c9fe510522914dae0/doc/CD-FILESYSTEM-SAFETY.md).

### Final verification for BIOS player return and CD transfers

All six checkpoints landed in order through [`6c7c5cd`](https://github.com/papsg/SAROO/commit/6c7c5cdc77b6018c2064521c9fe510522914dae0). The final source archive matches all 699 committed files. The full `make -j6 -C tests check` passed 987 Python methods and 18 Rust tests, with one Windows-only fixture skipped on macOS.

The three GNU App links and three stock SH-2 menu links match their committed source hashes. The final App contains 166 vectors and uses 65,504 of 65,536 ITCM load bytes. Its DTCM data and BSS span remains 50,744 bytes. The stock menu has 85,352 code/data bytes and 19,848 bytes of body margin. It passes the ordinary linked clock tail and all 48 ROM clock cases. The public builds have host and link verification. Historical hardware observations apply to the corresponding private trials.

The public source and ledger changes pass secret and public commit identity checks. The private repository fingerprint matches the starting checkout.

## Menu callback and stack, backup compatibility, CD FIFO and FAT32

Six ordered checkpoints after [`6c7c5cd`](https://github.com/papsg/SAROO/commit/6c7c5cdc77b6018c2064521c9fe510522914dae0).

- `a1f65f2a` (paps-g, MENU-GAME-VANDAL-STUB-001) → [`45e78c9`](https://github.com/papsg/SAROO/commit/45e78c98a463e436480d7677e4d86addf84c433e). Declare all 48 bytes copied by the inherited yzb callback. The parent fails the declared-payload check. The linked guards, 12 stores and full payload match after the change. The earlier 44-byte version is superseded. All 144 focused checks and 48 linked ROM clock cases pass. Menu code/data is 85,368 bytes with 19,832 bytes of body margin. The exact modified game revisions remain unverified. [Original yzb contribution](https://github.com/tpunix/SAROO/commit/f6a35a4). [Hitachi SH7604 tables 2.12 and 2.16, printed pp. 33 and 39](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=48). [Reproduction](https://github.com/papsg/SAROO/blob/45e78c98a463e436480d7677e4d86addf84c433e/doc/CD-EVENTS-AND-GAME-LAUNCH.md#declare-the-complete-vandal-hearts-and-bio-hazard-callback-payload).
- `75919b29` (paps-g) → [`cb0f705`](https://github.com/papsg/SAROO/commit/cb0f70513ee0e1787b7fefb4010571115b65fbc2). Align the stock menu's initial stack pointer and preserve the fixed reservation. A BSS tail of five bytes misaligns the parent. Removing alignment with the new assertions in place is rejected. All four BSS tail alignments pass, the stock ELF/image hashes are unchanged, and the 48 linked ROM clock cases pass. This is link verification. [Hitachi SH7604 §§2.2.2 and 4.8.1, printed pp. 18 and 76](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=33). [Reproduction](https://github.com/papsg/SAROO/blob/cb0f70513ee0e1787b7fefb4010571115b65fbc2/doc/SATURN-MENU-HARDWARE.md#align-the-menus-initial-sh-2-stack).
- `24a4b069` (paps-g, CART-BUP-001–003) → [`f40702b`](https://github.com/papsg/SAROO/commit/f40702b06750521c3eee0adb4d5148c84e3d4acf). Report all blocks occupied by each cartridge save, the 8,192 total 1 KiB blocks and 960 available bytes with one block free. The parent fails nine cases. All 156 save methods, six optimized methods and 48 linked ROM clock cases pass. Menu code/data is 85,416 bytes with 19,784 bytes of body margin. Existing save formats and allocator capacity are preserved. [Sega ST-162-R1-092994 Backup Library §§2.1 and 2.3, printed pp. 41–42, 45 and 47](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=46). [Reproduction](https://github.com/papsg/SAROO/blob/f40702b06750521c3eee0adb4d5148c84e3d4acf/doc/FIRMWARE-SAVE-SAFETY.md#keep-extended-backup-cartridge-counts-consistent-with-allocation).
- `24a4b069` (paps-g, BUP-ABI-001) → [`b52b40d`](https://github.com/papsg/SAROO/commit/b52b40d8fda34ab5844eee68bfa776f729ce7179). Preserve MACH, MACL and PR across game-facing backup calls. The linked parent routes the date call to code that changes MACH/MACL without preserving them. All eleven public entries, 64 modeled clobber cases, three broken restore controls, 40 save methods, 15 optimized methods and 48 linked clock cases pass. The shared call adds a 12-byte stack frame. Menu code/data is 85,560 bytes with 19,640 bytes of body margin. No retail failure or physical stack measurement is claimed. [Hitachi SH C Compiler table 2-5, printed p. 35](https://antime.kapsi.fi/sega/files/shccomp.pdf). [Sega ST-162-R1-092994 Backup Library §2.3, printed pp. 43–48](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=48). [Reproduction](https://github.com/papsg/SAROO/blob/b52b40d8fda34ab5844eee68bfa776f729ce7179/doc/FIRMWARE-SAVE-SAFETY.md#preserve-the-games-sh-2-registers-across-backup-calls).
- `d5fbaaea` (paps-g, SSR-001) → [`4ca9656`](https://github.com/papsg/SAROO/commit/4ca9656db33c05bef4ffae11e5c926b399bce2af). Use only native 32-bit FIFO reads for TOC and unaligned/tail copies. Seven parent methods observe the forbidden halfword access. The corrected source passes 146 focused methods, 71 optimized CD methods and 48 linked clock cases. Direct byte stores avoid a compiler-generated copy call. The stock menu retains 85,560 code/data bytes and 19,640 bytes of body margin. Hardware timing and gameplay are unverified. [Sega Technical Bulletin #8 function 9.1, PDF pp. 17–18](https://antime.kapsi.fi/sega/files/Sattechs.pdf#page=17). [Reproduction](https://github.com/papsg/SAROO/blob/4ca9656db33c05bef4ffae11e5c926b399bce2af/doc/CD-EVENTS-AND-GAME-LAUNCH.md#use-longword-reads-for-the-native-cd-data-fifo).
- `a3ca5238` (paps-g, SSR-011) → [`cd24706`](https://github.com/papsg/SAROO/commit/cd24706bf01b04175b1de834b6b40c9749ba0107). Use the active FAT32 copy when mirroring is disabled. The parent fails 17 selector and stale-copy cases. All 20 cases and 43 optimized FatFs methods pass, including remount, mirrored error recovery and the other supported formats. The GNU App links at 65,536 bytes, with 166 vectors and unchanged static DTCM. The port reuses the write-count field after validating physical geometry. Physical media is untested. Microsoft FAT On-Disk Format 1.03, p. 12, `BPB_ExtFlags`. [Reproduction](https://github.com/papsg/SAROO/blob/cd24706bf01b04175b1de834b6b40c9749ba0107/doc/FATFS-SD-SAFETY.md#honor-the-selected-fat32-copy).

### Final verification for menu, backup, CD FIFO and FAT32

All six commits landed in order through [`cd24706`](https://github.com/papsg/SAROO/commit/cd24706bf01b04175b1de834b6b40c9749ba0107). The suite archive matches all 702 tracked blobs. The complete `make -j6 -C tests check` run discovered 994 Python methods and passed 992 in that run, along with all 18 Rust tests. The optional RTX pool fixture passed separately in QEMU against the final MCU objects. The Windows replacement fixture remains skipped on macOS. The first suite attempt used a Python environment without Pillow. The corrected run used the cached test environment.

The SH-2 menu and GNU MCU link inputs match the committed source hashes. The menu has 85,560 code/data bytes and 19,640 bytes of body margin. The 48 linked ROM clock cases, four stack alignments, callback byte comparison and backup ABI checks pass. The GNU MCU has 166 vectors and uses exactly 65,536 ITCM load bytes with 50,744 bytes of static DTCM span. Both MCU build attempts with the final code produced the same load bytes. The separate write-count candidate exceeded the limit by eight bytes. The final port reuses the existing count after mount validates the physical geometry. A later MCU change may require further size savings.

Source and ledger deltas passed secret and public identity checks. This batch used host, target-link and model verification. The private checkout stayed at its starting revision and the Saturn was not used.

## FAT32-only MCU App and GNU link-time optimization

Two ordered checkpoints after [`cd24706`](https://github.com/papsg/SAROO/commit/cd24706bf01b04175b1de834b6b40c9749ba0107). The SD overlay work is a separate series.

- `205e0f61` and `495794eb` (paps-g, MCU-FS-005/006) → [`f9c3949`](https://github.com/papsg/SAROO/commit/f9c3949784189243fb6f6fd43e72b4ca74fcb5d3). The MCU App now requires FAT32. Its allocation rollback and seek-prefix recovery run without exFAT. A bound check keeps CD header offsets from wrapping with 32-bit file positions. The earlier multi-format App accepts six media cases that the new App refuses without writes; isolated rollback, seek-prefix and header controls reproduce failures. All 288 focused methods pass. The GNU App uses 60,512 bytes, leaving 5,024 free, and static DTCM falls from 50,744 to 46,688 bytes. A GNU BootLoader comparison retains multi-format support and the 29,112-byte size, with linked changes confined to `mount_volume`. Microsoft *FAT: General Overview of On-Disk Format* 1.03, printed pp. 10–12 and 14–15; [FatFs f_lseek](https://elm-chan.org/fsw/ff/doc/lseek.html) and [f_close](https://elm-chan.org/fsw/ff/doc/close.html). [Reproduction and limits](https://github.com/papsg/SAROO/blob/f9c3949784189243fb6f6fd43e72b4ca74fcb5d3/doc/FATFS-SD-SAFETY.md#use-fat32-in-the-mcu-application) and [card migration](https://github.com/papsg/SAROO/blob/f9c3949784189243fb6f6fd43e72b4ca74fcb5d3/README_EN.md#images-and-configuration). No physical media, games or MDK output were tested.
- `2aaf465c` (paps-g, AUD-RES-028) → [`d631a9b`](https://github.com/papsg/SAROO/commit/d631a9bc4a85a55494b9aaf3d6b0319e27b10236). Default the GNU MCU App build to single-partition link-time optimization. `--no-lto` provides the comparison with the same 141 staged inputs. Arm GNU 14.3.Rel1 uses 56,412 instead of 60,512 load bytes, leaving 9,124 free; static DTCM is 46,680 bytes, down 8. Both links pass all 166 vectors and startup/linker checks, retain the 29,184-byte RTX common pool and pass the compiled provider's QEMU admission test. The single linked partition reports a 568-byte direct `load_cover` frame (was 336); its `disk_task` caller remains at 88. The original MDK project is unchanged. [GCC 14.3 optimization options](https://gcc.gnu.org/onlinedocs/gcc-14.3.0/gcc/Optimize-Options.html#index-flto-partition). [Reproduction and limits](https://github.com/papsg/SAROO/blob/d631a9bc4a85a55494b9aaf3d6b0319e27b10236/tools/README.md#original-mcu-app-compilelink-check). Final combined host suite is running.

### Final verification for FAT32 and GNU LTO

Both checkpoints landed through [`d631a9b`](https://github.com/papsg/SAROO/commit/d631a9bc4a85a55494b9aaf3d6b0319e27b10236). All 703 files in the suite archive match that commit. The combined `make -j6 -C tests check` run found 1,008 Python methods: 1,006 passed, the Windows replacement fixture was skipped on macOS, and the optional RTX pool fixture was run separately and passed with both GNU object sets. All 18 Rust tests passed. The App profile passed all 133 growth and recovery cases, including fault reinjection.

Both final GNU links match the committed MCU sources and reproduce their earlier load bytes. Combined ITCM saving from the starting 65,536-byte image is 9,124 bytes: 5,024 from the FAT32 change and 4,100 from LTO. The original Keil build is outside the LTO change. No physical media or Saturn behavior was qualified. Both source commits and ledger updates passed secret and public identity checks; the private checkout was left untouched. SD overlays remain a separate port.

## Guarded SD sector overlays

Four ordered checkpoints after [`d631a9b`](https://github.com/papsg/SAROO/commit/d631a9bc4a85a55494b9aaf3d6b0319e27b10236).

- `f7791566` and standalone CRC from `7afd1bea` (paps-g) → [`aaea43a`](https://github.com/papsg/SAROO/commit/aaea43a1e53d53330beaa52176de127b12e16e46). Add the ABI 1 pack decoder and an offline Rust compiler using the public CUE geometry and exact source checks. Three sanitizer methods exercise 13 structural refusals, mixed-width and later-track geometry, read retry retention and independent whole-sector parity. Workspace format and lint checks pass. The MCU decoder has no caller in this checkpoint. [Format, Sega and ECMA-130 references](https://github.com/papsg/SAROO/blob/aaea43a1e53d53330beaa52176de127b12e16e46/doc/SD-SECTOR-OVERLAYS.md).
