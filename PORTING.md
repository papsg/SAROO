# SAROO contribution porting progress

The development baseline is v0.8 commit
[`ba387b08`](https://github.com/papsg/SAROO/commit/ba387b083bb9d2a001ae6a22a24d6e6785e515bd).
The fork's default `main` starts there. The previous `master` is preserved;
subsequent upstream changes are not automatically imported.

Each contribution is a direct source PR targeting `main`. Original authors,
related requests and relevant primary documentation are credited in the PR.
USB integration, custom diagnostics and private patch-application machinery
are excluded. Pull requests are not merged automatically.

Last updated: 2026-11-08.

| Contribution | Original request | Fork PR / status | Dependencies and qualification |
| --- | --- | --- | --- |
| Language hash-table bounds | [#377](https://github.com/tpunix/SAROO/pull/377) | [#1](https://github.com/papsg/SAROO/pull/1), open | Independent. ASan/UBSan passes 1,680 lookups over 14 languages; original and individual old-mask controls fail. SH-2 objects compile; complete link lacks the original libc in the available toolchain. No new hardware qualification. |
| MEMS overwrite corruption | [#372](https://github.com/tpunix/SAROO/pull/372) | Preparing | Independent source fix; fresh native CLI regression pending. |
| Controller/multitap parsing | [#380](https://github.com/tpunix/SAROO/pull/380) | Queued | Deduplicate #378/#379. Port the corrected protocol implementation and exact Sega references. |
| Missing CJK coverage | [#203](https://github.com/tpunix/SAROO/pull/203) | Queued | Preserve existing font records and source notices; missing-glyph subset only. |
| European Portuguese | [#256](https://github.com/tpunix/SAROO/pull/256) | Queued | Retain existing language IDs; bounds fix is a prerequisite. |
| Welsh | [#299](https://github.com/tpunix/SAROO/pull/299) | Queued | Retain existing language IDs; document language-series ordering. |
| Dutch | [#324](https://github.com/tpunix/SAROO/pull/324) | Queued | Retain reviewed wording and verify complete labels. |
| Catalan | [#360](https://github.com/tpunix/SAROO/pull/360) | Queued | Retain existing language IDs and prior additions. |
| Arabic | [#107](https://github.com/tpunix/SAROO/pull/107) | Queued | Required licensed glyphs, fixed-label shaping and language-series prerequisites. Native-speaker review remains a separate limit. |
| English documentation/comments | [#68](https://github.com/tpunix/SAROO/pull/68), [#352](https://github.com/tpunix/SAROO/pull/352) | Queued | Deduplicate #343 and overlapping content; exclude private operational instructions. |
| Browser save editor | [#374](https://github.com/tpunix/SAROO/pull/374) | Queued | Separate application PR with the reviewed format/safety corrections. |
| Browser cover editor | [#374](https://github.com/tpunix/SAROO/pull/374) | Queued | Separate application PR with the reviewed format/safety corrections. |

Historical implementation evidence guides these ports; it does not qualify a
newly assembled public branch. Every PR records fresh checks and outstanding
limits. Host tests and compilation do not establish physical Saturn behavior.

After a verified merge, fully resolved issues and fully superseded PRs in this
fork can be closed with a link and resolution summary. Partial requests stay
open. Requests in `tpunix/SAROO` are left unchanged. No closures have been made.
