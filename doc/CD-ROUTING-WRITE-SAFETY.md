# CD routing and write safety

Start at public `b686232`, preserving the earlier command/buffer safety ports.
Port selected contributions from **paps-g** `50100444` and `3f7bba1a`
(2026-09-06) as separate, freshly verified source changes. Original notices and
logging remain. No USB, new runtime diagnostics or device operation is included.
These source findings are not attributed to a named game's observed failure.
Per-port measurements describe their own checkpoints; final verification is
recorded below. Host writes here use the Saturn CD interface.

## MCU-CDC-005: coherent producer metadata

Original: selected paps-g `50100444`; the public adaptation isolates producer
normalization from later connection, routing and write-lifecycle changes.
The [preceding disc producer](https://github.com/papsg/SAROO/blob/b686232/Firm_MCU/Saturn/saturn_main.c#L259)
only sets FN/CN/SM/CI for raw Mode2. Other formats can retain previous or
uninitialized values. Raw audio whose byte15 happens to equal2 is also mistaken
for Mode2. Clear the four cached fields for cooked, Mode1 and audio data, while
preserving raw Mode2 fields and the disc's logical FAD. Cooked images have no
usable raw header from which to reconstruct that address.

The [preceding host producer](https://github.com/papsg/SAROO/blob/b686232/Firm_MCU/Saturn/saturn_cdc.c#L230)
does not initialize absent header bytes or populate cached address/subheader
fields. Reused blocks can therefore contain unrelated metadata. For each new
host block clear only its absent prefix:24/16/12/0 bytes for the four existing
2048/2336/2340/2352 write lengths. On sector completion derive FAD from the
stored header and copy Mode2 subheaders, otherwise zero them. Payload and
unwritten trailing bytes are retained; no extra payload copy is added.

Sega's [CD Communication Interface §5.4(2)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
defines zero subheaders outside Mode2 and a zero24-byte prefix for user data
from non-CD devices. §5.4(3) defines header BCD-to-FAD conversion and explicitly
does not validate the BCD digits. [§8.2.7 function7.1](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm)
defines the four transfer lengths. Zeroing other absent prefixes is the local
deterministic representation used to avoid reused-header classification; no
physical trace of unspecified header fill is claimed. Cached primary pages and
their identities are preserved locally. No new BCD rejection is introduced.

The public fixture compiles actual current producers and allocator with current
CD/FatFs types. Cache-hit disc checks cover five formats, preserve the logical
FAD and all source bytes, and include valid Mode2 controls. Scripted complete
host input covers all four lengths, two poison patterns, three sectors followed
by a separate append, and all three legacy allocation branches. It independently
checks transmitted bytes, absent prefixes, untouched trailing bytes, cached
fields and addresses. A header matrix covers modes0/1/2 and three literal
BCD/address pairs, including an invalid-digit pair that must not be rejected.
The fixture's FIFO is a complete-input script, not a physical occupancy model.

Five methods pass ASan/UBSan and O2. Baseline fails three metadata groups;
its Mode2 and transmitted-payload controls pass. Seven compiled mutations reject
stale fields, audio misclassification, lost logical FAD, each missing prefix
clear and a dropped host field. All31 current CD methods also pass. No private
preparer is used. The host fixture supplies valid lengths and ample capacity;
it neither approves nor hides the separate pending write-lifecycle defects.

The complete GNU MCU App/RTX/166-vector link adds208 load bytes:
57,540→57,748, leaving7,788 ITCM bytes. Static DTCM remains60,296; no retained
state, workspace or allocation is added. Direct FIFO-handler frame grows40→48;
disc producer/disk task/command dispatcher/command task remain48/80/16/24.
The command DataEnd frame remains32. Caller-plus-handler sums therefore grow
eight bytes before allocator, memset, other callees and exception state; these
are compiler frames, not physical stack peaks. Added work is bounded prefix
clearing (at most24 bytes per new sector) and header/metadata normalization.
No new I/O, payload copy or interrupt-mask operation is introduced. Original
offset selection is relocated intact, retaining its existing fallback rather
than assuming a new valid-length invariant. No speed improvement, physical
timing/concurrency/game or original-MDK qualification is claimed.

```sh
python3 tests/test_cdc_metadata.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_metadata.py
make -C tests check-cdc-command-buffer
# Reproduce preserved source/mutations with:
# SAROO_CDC_METADATA_SOURCE=/path/to/saturn_cdc.c
# SAROO_CDC_METADATA_MAIN=/path/to/saturn_main.c
```

At the005 milestone, host publication before DataEnd, byte accounting, unchecked allocation,
empty/short FIFO service, transfer counts and reset/connection ownership were
explicitly separate007/006 work. This metadata port does not make the entire
host-write command compliant. Shared filtering of normalized non-Mode2 fields
is the later003 routing continuation; current incoming predicates remain.

## MCU-CDC-006: connection ownership and reset/default distinctions

Original: selected paps-g `50100444`, plus defaults/reset prerequisites from
`3f7bba1a`. Depends on005's producer representation; prepares later routing and
private writes. The public adaptation preserves original log strings/masks,
including the two spaces in the connection-setter format.

Sega's [CD Communication Interface §5.2 and §5.3.3 Table5.1](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
defines one-to-one device/false-output connections to aperture inputs while
allowing multiple true outputs to one partition. A new input owner disconnects
prior CD/false sources; true outputs remain shared. Setters validate all selected
fields before mutation, preserve ignored output fields, and allow0xff
connection disconnection. Invalid array selectors return existing REJECT status
without mutation; exact malformed-command hardware replies are unmeasured.

[§8.2.5 functions5.5,5.7 and5.9](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_15.htm)
define condition initialization, selected connection fields and reset flags.
Condition reset now clears FN too. Input reset disconnects CD and false inputs
without changing true outputs. A nonzero reset ignores the supplied partition;
pause depends on actual incoming data erasure, with a valid data-track check,
instead of comparing that ignored argument. Preserve the existing pause
handshake; the host fixture acknowledges it without physical scheduling.

Startup restores2048-byte read/write defaults, identity true connections and
zero ranges. Accepted software reset restores lengths and FN, ends transfer
pointer/count ownership and disables FIFO data direction/IRQ. Nonreset and
no-change initialization retain state. These defaults follow §5.3's initial
selector state, §8.2.5 function5.9 and
[§8.2.7 function7.1](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm).
[§5.5 Table5.2](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
and [§8.2.1 function1.7](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_11.htm)
define software reset versus retained/no-change initialization. Other drive/init
parameters and active partition-output reset are not completed by this port.

Remove the blanket pool-free pass after global partition erasure. Partition
release already frees its owned blocks; repeating a blanket reset would make
an allocated, unpublished block reusable. An actual allocator/reset control
proves that ownership violation in the model. This is also a necessary
prerequisite for007's private reservations, not a claim of a captured hardware
interleaving. Complete CD software reset still owns and releases the whole pool.

Nine actual-source methods pass sanitizers/O2. They cover576 ownership pairs,
invalid/ignored selected fields and atomic rejection, all24 condition resets,
510 nonzero flag/ignored-argument combinations, pause/no-track/audio controls,
actual relocated startup, software/nonreset/no-change initialization, an actual
unpublished allocation and baseline-valid configuration. The flag sweep checks
encoding/unchanged fields, not255 asserted valid hardware reset modes. Only two
physical buffer address expressions are relocated in startup; the initializer
itself is extracted unchanged. The baseline fails eight groups; its ordinary
configuration control passes. Thirteen compiled mutations reject ownership,
validation, reset/default and unpublished-block regressions. All40 current CD
methods pass; original002 outcome/reset and005 producer controls remain.

The full MCU link adds160 load bytes:57,748→57,908, leaving7,628 ITCM bytes.
Static DTCM remains60,296, with no extra retained state, workspace, allocation,
payload copy or I/O. CD-connection direct frame grows8→16, connection setter
stays40, reset shrinks48→32, startup/init remain24/24 and IRQ/dispatch24/16.
The new disconnect helper has an8-byte direct frame; caller sums include that
additional frame before other callees/exceptions. Setters mask at most24
connection checks and selected field writes; they do not mask payload/routing
work or pause waits. The existing global reset loop is not promoted to a new
atomic transaction. No physical latency, concurrency, peak-stack or game
qualification follows from these host/build checks.

```sh
python3 tests/test_cdc_connections.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_connections.py
# Overrides: SAROO_CDC_CONNECTION_SOURCE and SAROO_CDC_CONNECTION_MAIN
```

Active host-input theft and private-list lifecycle completion remain007;
general partition-output/cancellation/GET ownership and later initialization
corrections remain outside this selected slice. This is not full command or
hardware reset qualification.

## MCU-CDC-003 continuation: shared destination-filter routing

Original: selected paps-g `50100444`; depends on005 producer metadata and006
connection/reset rules. The destination is an aperture, not a same-number
partition. The preceding direct copy/move bypasses file/channel/submode/coding
conditions, FAD range, false chains and intentional discard.

Sega's [CD Communication Interface §5.3.1–5.3.3](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
and [§8.2.7 functions7.6–7.8](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm)
define filter-based copy/move, initial full-request capacity and retained results.
One bounded helper now consumes existing normalized metadata for incoming and
copy/move paths, evaluates up to24 selectors, validates the destination and
returns a partition or discard. Range comparison subtracts after the lower-bound
check, avoiding unsigned addition wrap in the cached representation. No payload
header reparsing replaces the logical disc FAD.

After admission, take the requested filter input from previous sources. Copy
allocates/copies only accepted sectors; discarded copies do neither. Move
unlinks the original block and appends or frees it without copying or needing
spare capacity. Fixed original selection length and captured next pointers
preserve split, discard and return-to-source ordering. The existing requested-
count capacity check still applies even if all copies would later be discarded.
Copy/move do not change last incoming-CD destination or their IRQ-return masks.

The public helper's logging argument preserves the existing incoming mode1
false-disconnect print, including its audio silence, while copy/move stay silent
on that path. Its null-track guard retains the old disconnected-input discard
without needing a track or sector context. A baseline-valid control catches the
initial uncommitted logging-adaptation regression; that failure is retained.
Existing direction-specific logs/masks remain; no private log
consolidation or new logging is imported. Incoming sector conversion and last-
stored/retry policy remain as before. Normalized non-Mode2 subheaders are now
compared as zero, as §5.4(2) requires, rather than bypassing conditions.
[§8.2.5 function5.5](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_15.htm)
defines inversion before range combination. Retain the prior Mode2 interpretation
when inversion is enabled without another subheader condition; the historical
emulator disagreement does not establish physical behavior for that combination.

Six new actual-source methods pass sanitizers/O2:144 split copy/move selection
shapes, discard/capacity/logging controls, five real cache-hit producer formats
followed by incoming storage and routed copy,4,096 baseline-valid Mode2 predicate
combinations, normalized non-Mode2 predicates and full-width range boundaries.
Independent expected lists validate all payload bytes, metadata, byte/free counts,
block identity and outcomes. The actual allocator symbol is wrapped only in the
host fixture to count calls, with its body and decisions intact. Discarded copy
has zero calls/copy bytes; accepted copy calls equal accepted sectors and moves
make zero allocation calls. Existing pool setup now uses real startup's identity
connections. Complete source/dependency snapshots accompany both profiles.

The baseline fails five groups and passes the4,096-combination control. Those
failures share causes; the full-width range case is a defensive representation
check, not a physical24-bit packet reproduction. Eleven compiled mutations reject
routing bypass, even allocate-then-free waste on discard, leaks, lost original
selection, last-CD corruption, missed input takeover, predicates/range and added
copy logging and lost disconnected-input guarding. All47 current CD methods pass
in both profiles, including the
prior full-pool/same-partition and incoming-cycle/format/log controls. Host IRQ
and cache/FIFO substitutes do not qualify physical concurrency or game playback.

The full MCU link adds80 load bytes:57,908→57,988, leaving7,548 ITCM bytes.
Static DTCM remains60,296 with no new retained state, workspace or I/O. Shared
route direct frame is32, copy/move grows56→64, incoming stays32, disconnect8,
disk task80 and IRQ/dispatch24/16. The copy route caller sum rises96→136 and
incoming sum112→144 before other callees/exception state; these are compiler
frame sums, not measured peaks. The added input takeover masks at most24
connection checks once per accepted command. Predicate walks and payload work
remain outside the masks; existing per-move-block link/free work stays masked.
Correct filtering adds CPU work that was previously skipped; no elapsed-time or
game-speed gain is claimed.

```sh
python3 tests/test_cdc_routing.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_routing.py
# Overrides: SAROO_CDC_ROUTING_SOURCE and SAROO_CDC_ROUTING_MAIN
```

This completes the selected synchronous routing port, not broader MCU-CDC-003.
General output ownership, asynchronous cancellation/erase contracts, physical
scheduling and games remain unqualified. Host writes are the separate007 port.

## MCU-CDC-007: reserved host writes and bounded completion

Original: paps-g admission in `50100444` plus lifecycle in `3f7bba1a`; depends
on005 metadata,006 ownership/defaults and the shared003 router. The preceding
host handler allocates unchecked buffers as input arrives, exposes them in a
same-number partition, and reads up to2,048 bytes without checking availability.
DataEnd loops until the requested count is exhausted even when no further data
exists. Write word counts are absent and valid zero transfer looks like error.

Sega's [CD Communication Interface §8.2.7 function7.5, CDC_PutSctData](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm)
requires an aperture destination, sufficient initial capacity and incorporation
only after DataEnd. Initial shortage returns WAIT; capacity lost while preparing
signals EHST without DRDY. Interrupted ordinary input still incorporates the
requested sector count, with untransferred contents undefined.
[§8.2.1 functions1.9/1.10, CDC_DataReady/CDC_DataEnd](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_11.htm)
defines preparation-error handling and actual write words capped at the request.
The cached Sega header's CdcDend constant `CDC_DEND_ERR=0xffffff` is corroborated
by the existing [public menu decoder](https://github.com/papsg/SAROO/blob/a66041a/Firm_Saturn/cdblock.c#L158)
and preceding MCU reply layout. Its [Sega header reference](https://github.com/johannes-fetz/joengine/blob/master/Compiler/COMMON/SGL_302j/INC/SEGA_CDC.H)
is a cached identity, not a new online observation or imported dependency.

Before DRDY, reserve the complete request into an unpublished linked list using
the checked allocator. Failed preparation releases every acquired block. Existing
next pointers and transfer-start/current/count fields carry ownership; the start
byte is a pool index only for PUT and retains its partition-position meaning
for GET. All consumers of that field were reviewed. Request count and length
are captured locally; no input-time allocation or extra buffer is needed.
Partitions remain unchanged until DataEnd, and another producer may consume
all remaining free space without invalidating the reservation.

The unchanged pinned [FPGA05 status mapping, SSMaster.v line424](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L424)
exports11 used-word bits plus full bit11. The [FIFO declaration, lines83–94](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/cdcfifo.v#L83)
sets2,048 sixteen-bit words. [Control bits9/8](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L224)
select input direction/reset; [FIFO routing](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L579)
and [latched threshold/ack logic](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L314)
establish the actual interface. Current RTL matches that baseline byte for byte.
Each host service acknowledges first, reads at most one occupancy snapshot and
consumes at most4,096 available/requested bytes. New arrivals wait for a later
service. This bound is per invocation, not an entire command-task wake cycle.

DataEnd drains one final available snapshot and stops input, then normalizes and
routes every reserved block before EHST. Each block is appended or freed,
retaining original order and last incoming-CD reporting. Report actual consumed
words; ordinary zero is valid, while disconnected/no-active transfer uses the
error sentinel. Unknown unfilled payload remains untouched, not read from empty
FIFO or zeroed to conceal a shortage. The absent prefix remains005's selected
representation policy; poison-byte tests do not prescribe hardware fill values.

The [selector stream-error rule, §5.3.4(1)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
and buffer-I/O connection rules govern losing the active input. CD/false source
replacement or all-input reset signals EHST and stops consumption, retaining the
unpublished chain until DataEnd/software reset. Error DataEnd frees it without
publishing or stealing back the new connection. Partition-only resets preserve
the reservation; software reset releases it through006's whole-CD reset. Idle
FIFO service is harmless and repeated DataEnd reports no-active-transfer error.
Exact physical interrupt timing and global output ownership remain unqualified.

The public adaptation preserves every original log format/mask, including valid
PUT, DataEnd's conditional legacy wait message and GET/idle logs. That historic
message now precedes a single available-data service; it causes no wait for future
input. Host routing uses the shared helper's silent role. No new diagnostics,
logging API, payload copy, thread, heap object or static state is added.

Thirteen new actual-source groups pass ASan/UBSan and O2. Coverage includes:

- All200 reservation counts, ownership before DataEnd, exhaustion of remaining
  capacity by a modeled producer, and zero subsequent input/end allocation.
- All8,196 combinations of four lengths and0–2,048-word prefixes, including
  valid zero, exact bytes/counts, untouched remainder, all requested sectors,
  repeated DataEnd and stale service.
- Empty/single/threshold/full/cross-block/excess FIFO input, at-most-one snapshot,
  acknowledge-before-read order, sparse chains and pool head199.
- Raw host metadata, split/discard output at DataEnd, absence of premature
  publication and extra host logs, and preserved last-CD reporting.
- Real allocation loss at six preparation boundaries, rollback, atomic invalid/
  capacity/busy admission, reset kinds, four input-theft paths, retained new
  owner, recovery and unrelated-input/shared-true-output controls.
- Actual relocated startup/default reset followed by writes and captured-length
  defense. Changing sector length during transfer is prohibited by Sega; that
  synthetic invariant test does not approve a forbidden command sequence.

The baseline fails twelve groups; its complete transmitted-payload control
passes. Failures share causes and are not twelve independent hardware defects.
Twenty-one compiled mutants fail actual behavior/resource/sanitizer checks,
including empty/full/count errors, overrun, rollback/leak/premature publication,
wrong head/next, theft/reset/idle behavior, metadata routing/logging, masked FIFO
work, duplicate snapshots and manufactured zero remainder. IRQ masks, occupancy,
capacity loss and acknowledgment are host models, not Quartus/bus/concurrency
or hardware-timing simulation. Their source/dependency identities are retained.

The earlier005 fixture now calls actual PUT→windowed FIFO service→DataEnd,
followed by an append request. All original payload, prefix, trailing-byte,
poison, metadata and logical-FAD oracles remain. Its five groups also pass
against the preceding source; original005 artifacts remain in their commit.
All60 current CD methods pass in both final profiles.

The complete MCU link adds240 load bytes:57,988→58,228, leaving7,308 ITCM bytes.
Static DTCM remains60,296; the200-block pool and required stacks are unchanged.
Direct host-service frame shrinks48→24, preparation grows16→48 and DataEnd
32→40. IRQ/dispatch remain24/16, route32, disconnect8 and reset32. The host
service caller sum falls from at least80 (task/handler/allocator) to48 before
exceptions; preparation now needs96 (task/dispatch/preparation/allocator) and
the route part of DataEnd112 before other callees. These are compiler-frame
paths, not physical peaks. The whole compiled handler still calls the existing
GET fill_fifo; it is not claimed to be a leaf.

Reservation moves work before readiness and holds up to all200 blocks until
completion. Prefix clearing is at most4,800 bytes per200-sector user-data request;
allocator scans remain bounded by the fixed pool. Input updates metadata per
block span instead of each word. The4,096-byte service cap is twice the old
unconditional2,048-byte chunk cap. Routing and FIFO/prefix/payload work are
outside whole-operation masks; only existing allocation/append/free critical
sections and bounded input-ownership changes are masked. No physical latency,
IRQ-peak or game-speed improvement is claimed.

```sh
python3 tests/test_cdc_put.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_put.py
make -C tests check-cdc-command-buffer
# Overrides: SAROO_CDC_PUT_SOURCE and SAROO_CDC_PUT_MAIN
```

## Final verification and scope

The full project check passes351 Python methods and14 Rust tests. All60 final
CD methods also pass ASan/UBSan and O2 after the final host I/O-budget assertions;
all seven CD cohorts bind to current source, fixture and dependency hashes.
MCU/menu receipts bind56/36 current source inputs. Menu image/resources match
preceding public main exactly:79,572 code/data,9,232 BSS,16,384 reserved stack,
357,679 font and488,751 total image bytes.

Across the four scoped ports, MCU load grows688 bytes:57,540→58,228. Static DTCM
stays60,296 and no persistent buffer/state is added. Public original-layout
source, focused topics, attribution/citations, failing baselines/valid controls,
mutations and resource limits remain separate from original-MDK, physical stack,
bus/IRQ/concurrency/timing, game and Saturn qualification. No Saturn operation
or installed-firmware change was made.

The selected005/006/synchronous003/007 ports are complete at source/build level.
Broader MCU-CDC-003 asynchronous/cancellation/erase/output-connection contracts,
later008 GET/delete/size/ownership and initialization work remain outside this
series. Inversion-only hardware ambiguity and unspecified absent-header fill
are not converted into confirmed defects or claimed physical behavior.
