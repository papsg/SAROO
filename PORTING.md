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
| SATURN-SAVE-003 archive validation; `c97d2d70` | Next | Bounds, ownership and damaged-archive preservation; account for validation traffic. |
| SATURN-SAVE-005 dirty clear; `a9ddfcc6` | Queued | Clear only confirmed flushes; preserve failure/retry ownership. |
| SATURN-SAVE-005 header batching; `a9ddfcc6` | Separate evaluation | Independently qualify cache ownership, header traffic and resource costs. |
| MCU-SAVE-001 startup; `aa74eb3e` | Queued | Include necessary `169cb468` incomplete-tail correction; omit private startup diagnostic instrumentation. |
| MCU-SAVE-001 runtime; `d05062a1` | Queued | Checked transfers, full identity, bounds/ownership and publication; include `169cb468`. |
| MCU-SAVE-001 Saturn API; `1f90bc28` | Queued | Propagate failure and preserve pending data/retry ownership. |

Dirty-mask and resizing corrections plus the build prerequisite are integrated;
the remaining firmware work is active. All verified branches/main are pushed.
Fork issues remain disabled and no open fork PR needs closure. No upstream
request was modified. Private sources/branches/stashes/caches, installed firmware
and Saturn remain untouched. New checks establish host/source and target-link
behavior only, with no bus/cache/IRQ, SD durability or game qualification.
