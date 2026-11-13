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

## MCU-CDC-015: repeat history and home-position recovery

Original: selected paps-g `6a00df29`, requiring014 and its actual worker handoff.
The preceding PLAY clears repeat history on every resume, so repeated pause/
resume can replenish a finite budget. Seek also clears the count; stop writes
0xff into it to generate an invalid report. Home-position pause/resume retains
an invalid pickup FAD and can terminate or consume a repeat before playback.

Sega [ST-162 §4.2(3–5), printed pp.37–38](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=25)
defines count/range retention, clearing on changed range/limit and outside-range
completion/repeat. The [later §7.2.5(5), invalid reports](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p07_11.htm)
distinguishes retained counts from wire sentinels and restores FAD150 on home-to-
pause. Cached English/source-language pages agree. These requirements do not
endorse the existing explicit-seek range rewrites, retained separately as
compatibility policy pending further investigation.

After successful handoff, compare normalized range and repeat limit with current
settings. Clear history only when they change. Equivalent explicit/default/
NOCHG requests retain the count, including progress the worker made during the
wait. Failed handoff publishes no settings. Seek/stop preserve the counter;
the existing invalid-options sentinel generates the invalid low report nibble.
No extra retained state is required. Home pause/direct resume resolves FAD150
without consuming a repeat or replacing the range/limit; ordinary worker rules
then apply if that position is outside the retained range. Validate media for
home-to-pause before forming a track pointer.

Four new actual-source groups pass ASan/UBSan and O2:2,880 counter/limit/form/
mode combinations, actual finite-budget exhaustion, modeled repeat progress
before real acknowledgement, changed settings and failed handoff/retry, all15
reportable counts through pause/seek/stop with exact invalid words, home pause/
direct resume and missing media. Compiled preceding source fails three groups
and passes the changed-setting/failure control. Eleven compiled behavioral
mutants are detected, including observing history before the wait. Actual
completion/report/pause functions run; notification/preemption boundaries remain
models. All128 current CD methods pass both profiles.

GNU MCU load grows112 bytes:59,816→59,928, leaving5,608 ITCM bytes; static DTCM
remains60,312. All56 repository inputs match the receipt and SDK hashes remain
identical. Direct play/seek32/32, report8, disk72 and command24 frames are
unchanged. The report adds a conditional inside its existing mask; command
admission adds comparisons. No buffer, allocation, payload copy, I/O, extra wait,
cache invalidation or new log format/mask is added. Individual frames exclude
other callees/exceptions; physical cycles, latency and peaks are unmeasured.

The existing public drive commands above reproduce these checks. Retained
endpoint types021 and the separate FPGA clear contribution remain next. No
physical game/audio or original-MDK qualification or installation is claimed.

## MCU-CDC-021: retain endpoint forms through NOCHG

Original: selected paps-g `cd83b371`, requiring014/015. Once track positions
are normalized into FADs, the preceding command cannot distinguish a retained
track endpoint from a retained FAD endpoint. It accepts a new FAD start with a
retained track end, or the reverse. Sega's [later CdcPly data No6.4 note2](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p07_12.htm)
explicitly rejects mixed track/FAD forms even when one endpoint is NOCHG.
The cached source-language page agrees. The earlier [ST-162 CdcPly data No6.5,
printed pp.67–68](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=55)
provides the underlying position/no-change forms; the later note makes this
retention requirement explicit.

Reuse one formerly unused CDBLOCK byte for two bits per endpoint: default,
track or FAD. Resolve NOCHG from the previous form and reject mixed forms before
waiting, changing settings, waking the worker or clearing interrupts. Default
removes only its endpoint's form constraint. Publish new forms only after
successful handoff. File playback and background-audio range replacement clear
these play-command constraints. Existing seek-range compatibility policies are
retained; this does not add a new seek-form rule or broader reset behavior.

Three new actual-source groups pass ASan/UBSan and O2:112 old/new endpoint pairs,
failed handoff followed by NOCHG and default-removal controls, and actual file/
background replacement followed by valid NOCHG/FAD commands. The preceding
source fails the first two defect groups. The third also fails when compiled
with the new field, but establishes a new metadata-lifetime invariant rather
than a separately demonstrated old hardware defect. Eight compiled behavioral
mutants are detected. All131 CD methods pass both profiles; the drive group now
contains19 methods. Notifications, preemption and I/O remain host models.

GNU MCU load grows48 bytes:59,928→59,976, leaving5,560 ITCM bytes. Static DTCM
remains60,312; all56 repository inputs match the receipt and cached SDK hashes
are identical. Direct play32, file-start32, disk72, drive-pause16 and replacement-
pause8 frames remain unchanged; frames exclude callees/exceptions. The field
reuses existing padding, with no object/pool/stack allocation growth. Only
bounded command classification/comparison and range-replacement stores are
added; no payload copy, I/O, wait, log format/mask or interrupt-mask change.
Physical timing, memory peaks, concurrency, games and original-MDK qualification
remain unmeasured. Use the existing public drive-test commands above.

The scoped MCU drive contracts are now implemented. The separate FPGA clear
contribution follows; CUE geometry and broader drive/reset behavior remain
outside this series. No device or installed firmware was accessed.

## FPGA-HIRQ-001: combine simultaneous clears on the original FPGA

Original: selected paps-g `ad7c8307`. Its private correction was made on FPGA06;
this port revalidates and adapts only the clear/clear expression to the public
[original FPGA05 source](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L254).
No FPGA06 tree, revision/profile selection or binary is imported. This is a
self-reported collision, with no original upstream PR or game failure asserted.

Sega [ST-162 §3.1, HIRQREQ, printed p.28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=16)
specifies zero-written bits as clear requests while ones preserve other bits.
[§8.2.9 function9.3, CDC_ClrHirqReq, PDF p.90](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=90)
specifies that mask and reserves CMOK clearing to the communication interface.
These describe the Saturn interface; they do not define SAROO's internal MCU
register arbitration. The selected internal contract preserves both independent
clear requests when their decoded strobes meet on one main-clock edge.

The actual source generates MCU and Saturn write strobes independently, without
mutual exclusion. MCU HIRQ_CLR at offset0x2c is used by the command handlers;
Saturn HIRQ writes decode register address bits5:2=2. Initial0005, MCU mask0004
and Saturn maskfffe should clear both pending bits. The preceding else-if chain
leaves0001 by discarding the Saturn clear. Apply the Saturn mask in the MCU-clear
branch as well, usingffff when the Saturn clear is absent or not selected.
Ordinary single-writer updates, unrelated bits, decode, hold/reset and original
MCU-SET priority are preserved. No new state, ports, buffer or MCU instruction
is added; combinational gating grows. Collision frequency and game consequences
are unmeasured.

The actual-block baseline fails the collision method with0001 instead of0000;
its1,857-update control method passes. Corrected source passes both methods:
5,121 collision updates across all16x16 same/different-bit pairs, four initial
patterns and4,096 deterministic multibit masks;1,857 single-writer/hold/decode/
SET-priority controls; asynchronous reset in both. Ten compiled HDL behavioral
mutants are detected, including removed gating, wrong polarity/address/neutral
mask and an unwanted SET-priority change. Rust orchestration and an independent
per-bit Verilog oracle replace the old private version/patch preparer. Public
reproduction and tool requirements are in [tests/README.md](../tests/README.md#fpga-interrupt-clear-regression).

FPGA05's separate MCU-SET/Saturn-CLEAR loss deliberately remains: old0001 with
SET0004 and Saturnfffe still gives0005. The private06 test expected0004 and cannot
be copied as this port's valid control. Broader priority/crossing/timing repairs
remain separate; this series does not claim universal interrupt correctness.
The [isolated build tool](../tools/README.md#original-fpga-project-comparison)
compares complete original-device projects with equal verification-only Standard
Fit/uncertainty settings. Reported constrained timing and complete artifacts do
not qualify unconstrained external interfaces or hardware/game behavior.

Both complete five-stage Quartus builds pass, including SOF/RBF/JIC generation
and all21 reported constrained-timing summaries. Worst reported slack is
+0.042 ns before and+0.167 ns after; this is a fit comparison, not a guarantee
of external/physical timing. Final use changes1,467→1,464 logic elements and
1,332→1,333 combinational functions through different packing, with816 registers,
32,768 memory bits,150 pins and one PLL unchanged. RBF grows121,706→121,905 bytes;
SOF358,702 and JIC2,097,376 bytes are unchanged in size. These generated products
remain build evidence and are not shipped or installed.

All18 public project inputs bind to the baseline/candidate source cohorts;
SSMaster.v is the sole changed input. Runtime, wrapper, compiler manifest and
verification settings match. Each compiler manifest binds96,211 file/link
entries and9,405,944,612 regular-file bytes, unchanged after each read-only build.
The first tool run completed compiler stages but rejected the timing report's
spacing; its exact source/logs/receipt remain archived and excluded. The corrected
reader's fresh baseline and candidate both pass. Host process peaks and physical
cycles are unmeasured; two CPUs/four GiB are container caps, not measured usage.
The final GNU MCU image remains byte-identical to the021 checkpoint; menu source
and image are unchanged. No MCU/menu layout, stream I/O, installation or runtime
instrumentation accompanies this FPGA source change.
## Open the virtual tray and wait for observed SD removal

MCU-CDC-R04, adapted from paps-g `b768af07`. Command `0x05` previously
returned the old drive status without stopping the reader. It now waits
for the reader to retire, replies BUSY, signals DCHG and EFLS, and enters
OPEN. A timed-out handoff returns WAIT without invalidating the reader
or its media. Opening invalidates TOC, session, directory and pickup
reports while preserving host selectors, buffered sectors and the
retained play range. Repeating OPEN leaves the change event latched only
once. A drive initialization, play or seek can close the tray on the
current card. A software reset leaves the tray open.

The disk worker waits for a sampled removal followed by reinsertion
before replacing the image. It stops any old reader and invalidates its
cache before reading the replacement. It selects the configured next
image or remounts the previous one while preserving the live save session.
Failed identification or load leaves NODISC and waits for a new removal
before retrying. An explicit menu unload clears the pending selection.

[Sega CD Block User's Manual ST-162-062094, §4.1, printed pp. 33–34](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21),
[§5.5, printed p. 49](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=37),
[§6.2, printed pp. 51–53](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=39)
and [CDC_CdInit and CDC_CdOpen, printed pp. 79–80](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=67)
specify drive transitions, initialization and the disc-change events.

```sh
python3 -B -m unittest discover -s tests -p 'test_cdc*.py' -v
python3 -B -m unittest discover -s tests -p test_mcu_save_disc.py -v
python3 -B -m unittest discover -s tests -p test_cd_pacing_timer.py -v
```

The parent ignores OPEN and remounts a queued image while the card is
still inserted. To reproduce those failures, point `SAROO_CDC_DRIVE_DIR`
and `SAROO_MCU_SAVE_DIR` to its `Firm_MCU/Saturn` directory and use
`-k open_retires` in the drive fixture and `-k observed_removal` in the
disc/save fixture. Both corrected CD profiles pass 203 methods. Both
disc/save profiles pass 14 methods. The adjacent CUE, timer, audio and
menu-command checks also pass.

The public port also reduces the existing CD timer's 10 µs to kernel-tick
conversion exactly at the configured 100 Hz rate. This preserves every
integer result while avoiding a 64-bit divide in the GNU build. The
general conversion remains available for other rates. All 11 timer
methods pass, including the largest positive delay, timeout/retry and
tick wrap. The GNU App links 64,760 load bytes with 776 bytes of ITCM
space, 50,768 bytes of DTCM data and BSS span, and 166 vectors.

Card presence is polled. Fast swaps can escape observation, and
physical media, timing and original MDK firmware remain unqualified.

## Implement forward and reverse CD scan

MCU-CDC-R05, MCU-CDC-R23 and MCU-CDC-R24, adapted from paps-g
`32a363af` and `55dc3521`. Command `0x12` previously fell through
the unknown-command branch. The port takes over the pickup after the
old reader acknowledges, updates FAD and subcode Q together, and
leaves host CD-ROM sectors and selectors alone. Scan across an audio
track is audible only if it began during CD play. The shared audio
queue scales signed samples to one quarter before publishing the
DMA slot. A direction change keeps the scan's audio policy. The
worker yields on audio backpressure, ends at the retained play
range, and yields the pickup when another drive command arrives.

[Sega CD Block User's Manual ST-162-062094, §§4.1–4.3, printed pp. 33–40](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21),
[CD report fields, printed pp. 60 and 63](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=48)
and [CDC_CdScan, printed p. 84](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=72)
define the command, the Q report and the muted or −12 dB audio policy.
[Sega Technical Bulletins, CD Block specification changes, Table 4.5, PDF p. 15](https://antime.kapsi.fi/sega/files/Sattechs.pdf#page=15)
describes range exit and PEND. The raw direction bytes `0` and `1`
were independently corroborated by the [Mednafen CD block source](https://sources.debian.org/src/mednafen/1.32.1%2Bdfsg-3/src/ss/cdb.cpp).

The final private correction replaced kernel ticks with the existing
microsecond timer and cleared the CD-ROM flag throughout SCAN. Six
steps request 40,002 µs over data or 79,998 µs over audio. The
worker then jumps 101 sectors. The [Arm CMSIS-RTOS2 wait reference](https://arm-software.github.io/CMSIS_6/main/RTOS2/group__CMSIS__RTOS__Wait.html)
defines `osDelay` in kernel ticks. The fast-scan motion is an
approximation. The requested timer periods do not establish physical
pickup or audio cadence.

```sh
python3 -B -m unittest discover -s tests -p 'test_cdc*.py' -v
python3 -B -m unittest discover -s tests -p 'test_audio*.py' -v
```

Set `SAROO_CDC_DRIVE_DIR` to the preceding checkout's
`Firm_MCU/Saturn` directory and select `-k scan_command` with
`test_cdc_drive.py` to reproduce the missing command. The actual
worker fixtures cover both directions, mixed audio and data tracks,
FAD and Q, the report flag, range completion, backpressure, timer
and read errors, and interruption by a new drive request. The DMA
fixture covers signed attenuation, publication and cancellation
during the copy.

The GNU MCU App links 65,396 load bytes, leaving 140 bytes of ITCM
space, with the same 50,768-byte DTCM data and BSS span and 166 vectors.
Physical Saturn playback and original MDK firmware remain unqualified.

## Report unavailable CD positions as invalid

MCU-CDC-R06, adapted from paps-g `6f4f8b90` and its WAIT
follow-up `0657ffd1`. OPEN, NODISC and ERROR now put `ff` in
each of the seven report bytes, including the flag/repeat byte.
This applies to Get CD Status, periodic reports and status-only
WAIT replies. The drive-state byte and its qualifiers are kept.
The retained pickup and repeat count can still be reported when
the drive returns to PAUSE. Valid-position WAIT and REJECT keep
their previous reply behavior.

Sega defines these report values, the WAIT qualifier and retained
position in [ST-162 §7.2.4, printed pp. 59–61](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=47).
The unconditional formatter also appears in [the v0.8 baseline](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_cdc.c).

Run `make -C tests check-cdc-command-buffer`. For the negative
controls, set `SAROO_CDC_DRIVE_DIR` to the parent's Saturn source
directory and select `reports_invalidate` in `test_cdc_drive.py`.
Both tests fail on the parent. The corrected fixture checks every
reply word, the retained state, tray-close refusals, and valid
PAUSE, PLAY and REJECT controls. The GNU App uses 65,244 load bytes,
with 292 bytes of ITCM free. Physical report timing and MDK are
unqualified.

## Preserve raw Mode 1 image sectors and CD audio length

MCU-CDC-R11 and MCU-CDC-R13, adapted from paps-g `7d543dac`.
The image-to-partition path retains all 2,352 source bytes for raw
Mode 1. Two layout bits in the existing block-size field identify
raw Mode 1 and audio without expanding the sector pool. The
2,048-byte read and directory paths take Mode 1 user data at
offset 16. Wider reads use the selected header or full-sector
range. The audio marker keeps sample bytes resembling an XA
header from changing a 2,048-byte read to 2,324 bytes.

Sega documents these layouts, widths and size calculations in
[ST-162 §5.4, printed p. 48](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=36)
and [§8.2.7, printed p. 95](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=83).
The older shifted Mode 1 layout appears in [the v0.8 baseline](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_cdc.c).

Run `make -C tests check-cdc-command-buffer`. The regression
compares every FIFO word for all four read lengths using a mixed
Mode 1, CD audio and XA list through direct, copy and move paths.
It also checks that the directory parser uses the original Mode 1
payload. The parent fails the sector-byte and audio-size checks.
The GNU App uses 65,260 load bytes, with 276 bytes of ITCM free.
This checkpoint covers sectors from mounted images. Host PUT
normalization, cooked-image raw-field synthesis, playback timing
and MDK are outside its verification.

## Clear the unused tail of cooked image sectors

MCU-CDC-R14, adapted from paps-g `6901e3c2`. A 2,048-byte cooked
image sector or generated data-track gap fills bytes 24–2,071 of
the internal buffer. Clear the 280 bytes after it before linking
the block into its partition. This prevents wider reads from
returning bytes left by a previous occupant of that block.

[Sega ST-162 §5.4, printed p. 48](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=36)
defines the read widths and distinguishes user data from the
complete sector. [ECMA-130 §14 and §§14.1–14.6, printed pp. 14–16](https://ecma-international.org/wp-content/uploads/ECMA-130_2nd_edition_june_1996.pdf#page=24)
defines the sync, header, error detection and parity fields.
Those fields require a raw source or separate format-aware
construction. The cleared tail is padding.

Run `make -C tests check-cdc-command-buffer`. To reproduce the
stale FIFO bytes, set `SAROO_CDC_DIRECTORY_SOURCE` to the parent
`saturn_cdc.c` and select `wide_reads_clear` in
`test_cdc_directory.py`. The test seeds two pool slots and checks
all four read widths, including the ordinary 2,048-byte payload,
the gap and the three wider replies. The GNU App uses 65,280 load
bytes, with 256 bytes of ITCM free. Physical playback and MDK are
unqualified.
