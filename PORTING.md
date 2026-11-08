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
| Language hash-table bounds | [#377](https://github.com/tpunix/SAROO/pull/377) | [6fb42d0](https://github.com/papsg/SAROO/commit/6fb42d0), on main | Independent. ASan/UBSan passes 1,680 lookups over 14 languages; original and individual old-mask controls fail. SH-2 objects compile; complete link lacks the original libc in the available toolchain. No new hardware qualification. |
| MEMS overwrite corruption | [#372](https://github.com/tpunix/SAROO/pull/372) | [5920eb9](https://github.com/papsg/SAROO/commit/5920eb9), on main | Independent. Nine overwrite scenarios fail on v0.8; read-only export passes. All five actual-CLI methods pass after correction under ASan/UBSan and normal O2. No physical Saturn qualification. |
| Bounded menu text prerequisite | Our correction for the language series | [f6bdc5e](https://github.com/papsg/SAROO/commit/f6bdc5e), on main | Existing Greek code-1 error reproduces a 40-byte stack overflow. Six error paths and two titles now use actual capacities; custom formatter termination/integer/width handling is bounded. Three ASan/UBSan methods cover 252 errors, titles, 12,880 capacity/length pairs and valid controls. Independent old-caller and old-formatter controls fail. All 16 menu units compile; final SH-2 formatter preserves null/size guards with scoped compiler flags. Complete-link and hardware limits remain. |
| Controller/multitap parsing | [#380](https://github.com/tpunix/SAROO/pull/380) | Queued | Deduplicate #378/#379. Port the corrected protocol implementation and exact Sega references. |
| Missing CJK coverage | [#203](https://github.com/tpunix/SAROO/pull/203) | Queued | Preserve existing font records and source notices; missing-glyph subset only. |
| European Portuguese | [#256](https://github.com/tpunix/SAROO/pull/256) | [a494e69](https://github.com/papsg/SAROO/commit/a494e69), on main | Original 20 labels at ID 14; IDs 0–13 unchanged. Requires language bounds and bounded menu text. ASan/UBSan: 1,800 lookups, 270 complete errors, title/formatter controls. Shipped-font glyphs and 288-pixel width checks pass. SH-2 language object compiles. Editorial, full-link and hardware limits remain. |
| Welsh | [#299](https://github.com/tpunix/SAROO/pull/299) | [73c1fbe](https://github.com/papsg/SAROO/commit/73c1fbe), on main | Original 20 labels at ID 15 after Portuguese; existing IDs unchanged. ASan/UBSan: 1,920 lookups, 288 complete errors, title/formatter controls. All new glyph/288-pixel checks and SH-2 language compilation pass. No related open fork PR; issues disabled. Editorial, full-link and hardware limits remain. |
| Dutch | [#324](https://github.com/tpunix/SAROO/pull/324) | [461d658](https://github.com/papsg/SAROO/commit/461d658), on main | ID 16. Four documented wording adaptations; sixteen labels unchanged. Original wording fails at 328 pixels; corrected glyph/288-pixel checks pass. ASan/UBSan: 2,040 lookups, 306 complete errors and title/formatter controls. SH-2 language compilation passes. Editorial, full-link and hardware limits remain. |
| Catalan | [#360](https://github.com/tpunix/SAROO/pull/360) | [360d03d](https://github.com/papsg/SAROO/commit/360d03d), on main | Original 20 labels at ID 17; IDE workspace files excluded. ASan/UBSan: 2,160 lookups, 324 complete errors and title/formatter controls. All 80 added labels/full signed-error variants pass shipped-glyph and 288-pixel checks. SH-2 language compilation passes. Editorial, full-link and hardware limits remain. |
| Arabic | [#107](https://github.com/tpunix/SAROO/pull/107) | Queued | Required licensed glyphs, fixed-label shaping and language-series prerequisites. Native-speaker review remains a separate limit. |
| English documentation/comments | [#68](https://github.com/tpunix/SAROO/pull/68), [#352](https://github.com/tpunix/SAROO/pull/352) | [73922db](https://github.com/papsg/SAROO/commit/73922db), on main | Deduplicates #343. 77 ASCII comments preserve every original source byte; all eight affected SH-2 objects are byte-identical. Source-checked English README/developer notes and local links verified. The integrated tree and all compiled inputs/hashes match the verified branch. No runtime or hardware claim. |
| Browser save editor | [#374](https://github.com/tpunix/SAROO/pull/374) | Queued | Separate application contribution with the reviewed format/safety corrections. |
| Browser cover editor | [#374](https://github.com/tpunix/SAROO/pull/374) | Queued | Separate application contribution with the reviewed format/safety corrections. |

Seven of the twelve requested contributions are integrated. Controller/multitap
handling, CJK coverage, Arabic, the browser save editor and browser cover editor
remain. Bounded menu text is an additional explicit prerequisite, not an eighth
completed requested contribution. All topic branches and this ledger are pushed.

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
the English and Latin-language contributions integrated; the fork has no open PRs.
