# CD routing and write safety

Start at public `b686232`, preserving the earlier command/buffer safety ports.
Port selected contributions from **paps-g** `50100444` and `3f7bba1a`
(2026-09-06) as separate, freshly verified source changes. Original notices and
logging remain. No USB, new runtime diagnostics or device operation is included.
These source findings are not attributed to a named game's observed failure.

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

Queued: admission from `50100444` plus the lifecycle in `3f7bba1a`. Reserve before
readiness, consume actual available words, retain private ownership until
DataEnd/reset, route completed sectors, and distinguish zero transfer from error.
Undefined untransferred payload must not be fabricated by empty FIFO reads.
