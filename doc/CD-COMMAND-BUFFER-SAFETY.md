# CD command and buffer safety

Start at public `de549dc`. Port selected MCU-CDC-001–004 contributions from
**paps-g**, oldest first, with independent current-source verification. Existing
logging, source/dependency notices and prior public ports remain. No USB, new
diagnostics, private preparer or device operation is included. These self-reported
source findings are not attributed to a named game, audio or save-loss report.

## MCU-CDC-001: separate filter reply status and channel

Original: `f0da2b0b` (2026-09-06). Correct the one expression in command0x43,
`get_filter_subheader`: CD status occupies CR1's high byte and channel occupies
its low byte. The [baseline getter](https://github.com/papsg/SAROO/blob/de549dc/Firm_MCU/Saturn/saturn_cdc.c#L1074)
ORs both into the low byte. Status1/channel2 therefore yields `0x0003` instead
of `0x0102`, losing status and altering the returned condition.

Sega's [CD Communication Interface, §8.2.5 functions5.3/5.4](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_15.htm)
defines the setter/getter of the same subheader conditions. The current
[0x42 setter](https://github.com/papsg/SAROO/blob/de549dc/Firm_MCU/Saturn/saturn_cdc.c#L1052)
places the channel in the low request byte; the [common menu reply reader](https://github.com/papsg/SAROO/blob/de549dc/Firm_Saturn/cdblock.c#L86)
reads status from the high response byte. These actual source contracts plus
an independent set/get round trip establish the selected byte correction.
The local primary HTML/text and source identities are preserved. No exact
hardware register trace or new online observation is claimed.

The manual marks the getter `[S-]` and lists no end flag, while the adjacent
setter lists ESEL. A historical command-wiki ESEL listing is insufficient reason
to change the getter's existing zero return here. Preserve its other three
reply words, complete CD state and interrupt-return mask. This does not qualify
physical HIRQ timing or every aspect of command0x43. Invalid-selector guards
remain later work; this fixture uses the24 valid selectors only.

The public adaptation captures actual current types, functions and inputs
without the private preparer. Three methods pass ASan/UBSan and O2:71,680
independent status/channel/other-field replies,6,144 baseline-valid zero-status
replies and6,144 actual0x42→0x43 round trips. The old getter has71,400 byte-field
failures in the first sweep and fails the round-trip method; the zero-status
control passes. All256 status-byte values are an encoding-domain sweep, not
256 asserted real hardware states. Complete CD-state snapshots and other reply
words are checked. Four mutations detect unshifted status, shifted channel,
changed state and an added ESEL return. Three prior CD setup methods also pass.
The setter's existing log invocation is suppressed only in the host fixture;
production logging is unchanged.

The complete GNU MCU App/RTX/166-vector link grows16 load bytes:
57,396→57,412, leaving8,124 ITCM bytes. Static DTCM remains60,296. Compiler
IRQ/command-dispatch/getter direct frames remain24/16/8 bytes. No new call,
allocation, buffer, payload copy, I/O or interrupt-mask work is introduced.
The historical private zero-growth result is not reused. Source/build receipts
and target resource checks remain separate from original-MDK, physical timing,
stack peaks and game qualification. Menu source/assets remain unchanged.

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
# Preserved baseline/mutation: SAROO_CDC_SUBHEADER_SOURCE=/path/to/saturn_cdc.c
```

## MCU-CDC-002: retained copy/move error and software reset

Original: paps-g `93391efb` (2026-09-06), together with its reset continuation
from `8432f7d8`. Command0x65/0x66 already record allocator failure, but
[the old0x67 getter](https://github.com/papsg/SAROO/blob/1d9a126/Firm_MCU/Saturn/saturn_cdc.c#L1564)
always reports success. Return the retained error in CR1's low byte, preserving
status in its high byte. Keep the result across repeated and unrelated queries;
clear it on a subsequent successful copy/move or accepted software reset. Move
the existing state before initialization and narrow its storage from int to u8.

Sega's [CD Communication Interface §8.2.7 function7.8, CDC_GetCopyErr](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm)
specifies retained results until the next copy/move. Function7.6 explicitly
describes partial failure when another buffer consumer exhausts space.
[§5.5 Table5.2 and host-information list](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
include copy/move errors in software reset, while initialization without reset
retains them. [§8.2.1 function1.7, CDC_CdInit](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_11.htm)
defines flag bit0 reset and bit7 unchanged settings. The preserved Sega
[SEGA_CDC.H CdcCopyErr enumeration](https://github.com/johannes-fetz/joengine/blob/master/Compiler/COMMON/SGL_302j/INC/SEGA_CDC.H)
assigns OK=0, NG=1 and BUSY=255; this header is reference evidence, not an imported
dependency. The local source identity is preserved; the moving URL is not a
claim of new online verification. The present synchronous worker does not offer
an asynchronous BUSY-query window; no asynchronous engine is added.

Four actual-source methods/eight native cases pass ASan/UBSan and O2. An
independent pool fixture invokes the real allocator and makes another modeled
consumer take the remaining capacity after two allocations: copying stops after
two sectors with intact source, payload and metadata, and reports NG repeatedly.
All256 status-byte encodings preserve the low-byte result. Successful copy/move
replaces NG; soft reset clears it, while ignored and nonreset initialization
retain it. Power-on/success are valid baseline controls; six other native cases
fail against the previous getter. Four mutations reject hidden/consumed error,
missing reset and unconditional initialization clearing. This allocator schedule
is host fault injection, not proof of physical concurrency. Production logging
and command IRQ returns are unchanged; no host-only assertion enters firmware.

The full MCU link adds32 load bytes:57,412→57,444, leaving8,092 ITCM bytes.
Static DTCM remains60,296; IRQ/dispatch/init/getter direct frames remain
24/16/24/0 bytes. No payload copy, allocation, I/O, new call or mask operation is
added by this correction. Buffer admission/refusal semantics, byte accounting
and zero-copy moves are the separate003 checkpoint below. No physical
concurrency/timing/game or original-MDK qualification is claimed.

```sh
python3 tests/test_cdc_copy.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_copy.py
# Preserved baseline/mutation: SAROO_CDC_COPY_SOURCE=/path/to/saturn_cdc.c
```

## MCU-CDC-003: buffer admission, accounting and existing-block moves

Original: selected paps-g `8432f7d8` (2026-09-06); depends on002's retained
outcome/reset. Revalidate the original public helpers and all callers: append
is used only by copy/move; removal also serves delete and transfer completion.
The old move duplicates then frees, so a full buffer cannot move even one
sector. The old append omits partition byte accounting. Both old commands
index selectors before validating and can traverse beyond the requested data.

Sega's [CD Communication Interface §5.3.4(4)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
requires WAIT for empty partitions, zero count and out-of-range position/count.
[§8.2.7 functions7.6–7.8](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm)
define last-sector/through-end sentinels, initial insufficient-copy-space WAIT,
partial exhaustion error, retained outcomes and moves when the buffer is full.
The shared implementation validates the two selector bytes before indexing,
normalizes sentinels, checks ranges without adding overflowing endpoints and
keeps refusal from replacing an existing error. REJECT for invalid selector
bytes and WAIT while an existing host transfer owns data are bounded admission
policies consistent with local ownership, not newly measured hardware replies.

Appending updates count, bytes and tail together. Removal shares a masked unlink
that keeps the detached block allocated; moving unlinks and appends that same
block under one short nested mask, preserving pointer/payload/metadata and
free capacity. It scans the source prefix once. Fixed iteration count plus
captured next pointers handles same-partition moves and copies without walking
the newly appended tail. Copy still uses the real allocator and2352-byte payload
copy outside the mask. Keep both original direction-specific log formats,
normalization log and masks; the private consolidated log is not imported.

Eleven actual-source methods/nineteen native cases pass sanitizers and O2,
including928 invalid-selector combinations,18 invalid range combinations,
all eight active host-transfer types in both directions,72 full-pool move shapes,
eight copy/sentinel shapes and six removal indices. Independent pool/list,
byte/free counts, block identity, all payload bytes and metadata are checked.
Previous error is generated by real allocator exhaustion, not injected directly;
reset and repeated/unrelated-query controls remain. The baseline has16 failed
native cases; power-on, ordinary copy-payload/reset and removal controls pass.
Several baseline cases stop at the known accounting defect before later guards;
seven targeted mutations separately reject omitted accounting, capacity exemption,
selector/range/busy admission, refusal retention and source unlink. No hardware
concurrency or game qualification follows from the host IRQ model.

The full MCU link adds32 load bytes:57,444→57,476, leaving8,060 ITCM bytes.
Static DTCM remains60,296; no workspace or persistent allocation is added.
The shared direct frame is56 bytes versus48 in either old command; wrappers
are tail calls with zero direct frame. Append grows0→8 and removal shrinks16→8;
IRQ/dispatch remain24/16. Copy's allocator call chain therefore adds eight
compiler-accounted frame bytes; these are not measured stack peaks. Move
performs zero payload copies/allocations and retains the free count, including
full pools. The per-block mask covers only unlink/append metadata, not traversal,
logging or payload copying. All caller scheduling/cancellation and physical
preemption/stack/timing limits remain unqualified.

This is a **partial disposition** of broader MCU-CDC-003. The destination still
uses a partition directly; Sega specifies an aperture/filter route. Later005–007
routing, sector metadata and host-PUT work, interrupted-move erase semantics
from §5.3.4(5), cancellation and general output ownership remain separate.
No complete command, MPEG or hardware-compatibility claim is made.

## MCU-CDC-004: bounded incoming filter traversal

Original: paps-g `661fd2d4` (2026-09-06), revalidated after003. The old
incoming router follows false connectors without a bound and indexes device,
next-filter and true destinations unchecked. A legal disconnected true output
(`0xff`) reaches partition255; a false cycle never returns to disc service.
The setter already permits disconnection, so this is reachable without an
invalid command. Production predicates and sector-format conversions remain.

Sega's [CD Communication Interface §5.2, §5.3.1–5.3.3](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
describes false connections to apertures, true connections to partitions,
disconnected outputs erasing data, append order and last-sector reporting.
[§8.2.4 function4.3, CDC_CdGetLastBuf](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_14.htm)
requires the disconnected sentinel when the last sector was discarded.
Validate every byte-valued index, evaluate at most24 filters, capture the valid
destination before allocation, and publish it with completed append. Discard
publishes0xff without allocating/copying. A full-buffer retry keeps the last
completed destination until storage or discard. The unchanged disk-task caller
uses return0 for stored,1 for discard and2 for retry; no caller IRQ/scheduling
change is included.

Discarding invalid/cyclic topology after the bound and retaining last completed
destination during retry are bounded local policies, not measurements of real
hardware topology or CSCT/query timing. The original false-disconnect log still
prints exactly once for mode1 and stays silent for audio mode; unlike the private
patch, this port does not remove that existing log. No new logging is added.

Twelve actual-source methods pass sanitizers and O2:24 disconnected true outputs,
all invalid byte-valued device/false/true indices,300 cycle/start combinations,
576 full24-filter routes, discard after storage, full200-block pool plus retry,
five existing sector-format conversions, payload ordering, metadata/byte/free/
list ownership, enabled Mode2 subheader conditions/masks/inversion and FAD edges.
A modeled connection change between allocation and append keeps the already
validated destination. The baseline fails eight methods and times out in the
cycle method; three valid route/format/delivery groups pass. Failures share
causes rather than representing nine independent defects. Seven targeted
mutations reject unvalidated input/output, unbounded/shortened chains, stale
discard, premature destination and removed original logging. Host interruption
models and timeouts do not establish physical concurrent safety or elapsed time.

The full MCU link adds64 load bytes:57,476→57,540, leaving7,996 ITCM bytes.
Static DTCM remains60,296, with disk-task/router direct frames80/32 unchanged.
There is no new persistent state, workspace, allocation, payload copy or I/O.
Valid routes gain bounded index/hop checks; routing and payload work remain
outside the existing append mask. Only final destination publication is added
inside it. The private20-byte image saving depended on removed logging and a
different link context; it is not reused. Physical stack peaks, ESEL-setting
timing, CSCT/query timing, games and original-MDK builds remain unqualified.
Inversion with no subheader condition, non-Mode2 metadata normalization,
filter-reset/connection ownership and005–007 copy/move/host-PUT routing are
separate work.

```sh
python3 tests/test_cdc_filter.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_filter.py
# Preserved baseline/mutation: SAROO_CDC_FILTER_SOURCE=/path/to/saturn_cdc.c
```
