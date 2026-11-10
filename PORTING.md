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

Last updated: 2026-11-09.

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
| MCU-CDC-010; selected paps-g `3d2c405e` | Queued | Stable GET output/private GETDEL ownership, conflicting cancellation, safe detach/release and proven nonblocking/scheduling prerequisites. |
| MCU-CDC-012; selected paps-g `3d2c405e` plus corrected `ac0aeac1` | Queued | Queued-window and CD-produced read counts; preserve distinct PUT-consumed semantics and errors. Superseded host-consumed read policy is excluded. |

Two source checkpoints are published. Two ports and final integrated/build/
privacy/preservation audit remain. Directory/ISO/XA filesystem011/013 and broader
async copy/move/MPEG ownership remain separate; no hardware/game qualification.
