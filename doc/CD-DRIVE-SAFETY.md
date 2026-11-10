# CD drive playback, seek and repeat safety

Starts at public `64fc106`. Port selected **paps-g** `47ec46a4`, `6a00df29`
and necessary `659fe4ee`/`cd83b371` corrections as individual verified changes.
Keep original notices, log formats/masks and compatibility exceptions. These
self-reported source findings are not attributed to a named game's failure.
Host models and GNU builds do not qualify physical Saturn timing, concurrency,
games or original-MDK firmware.

## MCU-CDC-014: position admission, final sectors and bounded handoff

Original: selected paps-g `47ec46a4`. The old track helpers and play/seek commands
can form pointers outside the loaded track table, dereference absent indices
or use the failed FAD lookup sentinel as a track. They truncate position/count
bits, pass inclusive ends to an exclusive worker, and wait indefinitely for
pause acknowledgement. The worker looks up a track before admitting its range.

Sega [ST-162 CdcPos data No6.4, printed pp.65–66](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=53)
defines position forms, FAD/track clamping and missing-index exceptions.
The [later CdcPos/CdcPly data No6.3/6.4](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p07_12.htm)
retains those contracts, count-based FAD ends, mixed-form rejection and reversed
ranges. The numbering differs between editions; the English archive is a
translation, checked alongside its cached source-language page.
[§4.2, printed pp.37–38](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=25)
defines completion after the last sector and repeat/completion outside a range.
[§8.2.2 functions2.1/2.2, printed pp.82–83](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=70)
specifies play and seek/pause/stop forms. Both language copies' pickup-bit diagram
conflicts with their zero-valued default/resume examples. Preserve existing wire
polarity and explicit data-track restart/seek-resume exceptions; this port does
not establish those compatibility policies as fully conforming hardware behavior.

Normalize into local positions before waits/mutation, validate loaded media and
both track/index bounds before pointer formation, and retain all23 address bits
below the FAD flag. Clamp counts to lead-out and use exclusive worker ends.
Keep the inclusive internal track-to-FAD helper convention. Admit sector-play
ranges before track lookup; empty/reversed ranges complete without a fetch,
including infinite-repeat requests. Background audio includes its final frame.

Factor the existing bounded reader-stop loop into pause_drive, used by play/seek:
at most500 two-tick semaphore waits plus scheduling/wakeup overhead. A timeout
returns WAIT without replacing positions/ranges or releasing buffers; the worker
still owns its pending request. This is a wait-count bound, not a physical
response-time guarantee. Reader replacement keeps pause_disc and invalidates
the cache only after a successful handoff, including an already-idle reader.

Two necessary corrections accompany the original port. Selected `659fe4ee`
MCU-CDC-016 sets the shared limit to99, matching the existing parser/TOC and100
reserved metadata records; blindly importing the original32-track helper guard
would introduce a private regression. No track storage/layout or CUE geometry
changes here. Selected `cd83b371` MCU-CDC-022 removes cache clearing from the
actual shared worker acknowledgement, preserving both real pause branches.
Keep public main's atomic idle/request publication and unfinished-directory
invalidation. The original private fixture modeled acknowledgement and did not
prove this cache claim. MCU-CDC-021 retained endpoint-type admission follows
separately; this checkpoint checks explicit mixed forms only.

All original log formats/masks remain, checked against the preceding source.
Unlike the private patch, retain play/seek traces before playback publication,
and check the valid trace/report control exactly. No new runtime diagnostic,
buffer, table, allocation, payload copy, SD write or valid-input I/O is added.
Command normalization adds bounded work and position reporting scans the loaded
index list. Worker admission adds comparisons, avoids invalid-range reads, and
same-reader acknowledgement removes two cache-bound stores. Logs can still wait.

Twelve actual-source methods pass ASan/UBSan and O2: all65,536 track/index words
for starts/ends and nonzero seeks; lower/upper helpers and absent index storage;
120 FAD/count shapes and high addresses; mixed-form refusal; absent/invalid media;
500-wait failure/late retry across sector/file/directory owners; all16 repeat
limits at range/empty/outside boundaries; exact150-sector admission for a track;
background final frame; pause/stop/retained compatibility controls; both actual
worker pause branches and reader replacement; exact baseline-valid reports/logs;
and all99 loaded tracks. Worker read/notification/IRQ boundaries are host models.
The compiled preceding source fails eleven groups and passes the valid control.
The initial control lacked an existing control/address value, and a retry
assertion incorrectly compared a retained low report byte; those fixture errors
are corrected and preserved, not counted as production findings.

Twenty-one compiled behavioral mutants are detected, including the32-track
regression, actual-worker cache loss, empty-range repetition and unbounded
waits. One initial compile-only mutation attempt is preserved and excluded.
All124 CD methods pass both profiles; eleven disc/save handoff methods also pass.
The disc fixture extracts both actual pause branches and the new shared helper;
cache expectations now distinguish acknowledgement from reader replacement.
The directory fixture retains its new-request ownership checks and expects
same-reader cache preservation. No old invalidation policy is asserted unchanged.

Complete GNU App/RTX/166-vector MCU load grows176 bytes:59,640→59,816, leaving
5,720 ITCM bytes. Static DTCM stays60,312, with unchanged pool/workspace/stacks.
All56 repository input hashes match the receipt and cached SDK hashes match the
preceding build. Direct play/seek frames32/32; position resolution/update16/16;
pause_drive16 and replacement wrapper8 are nested, not a whole-path saving.
Track helpers0, producer48, disk72, command24 and atomic acknowledgement16.
Those frames exclude other callees/exceptions; physical peaks/cycles are unknown.

```sh
python3 tests/test_cdc_drive.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_drive.py
make -C tests check-cdc-command-buffer
# Baseline/mutant directory with saturn_main.c and saturn_cdc.c:
SAROO_CDC_DRIVE_DIR=/path/to/source python3 tests/test_cdc_drive.py
```

Repeat/home lifetime and retained endpoint types follow as separate changes.
CUE geometry/raw layouts, broad initialization/reset, uninterrupted live-range
updates, session/tray/scan and broader drive-state behavior remain separate.
No device or installed firmware is accessed.
