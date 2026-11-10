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

Current host publication before DataEnd, byte accounting, unchecked allocation,
empty/short FIFO service, transfer counts and reset/connection ownership remain
explicitly separate007/006 work. This metadata port does not make the entire
host-write command compliant. Shared filtering of normalized non-Mode2 fields
is the later003 routing continuation; current incoming predicates remain.

## MCU-CDC-006: connection ownership and reset/default distinctions

Queued: selected `50100444` and the defaults/reset continuation from `3f7bba1a`.
Keep input exclusivity distinct from permitted shared true outputs; validate
before mutation and preserve ignored-argument behavior.

## MCU-CDC-003 continuation: shared destination-filter routing

Queued: selected `50100444`, with producer and connection prerequisites explicit.
Retain zero-copy full/same-pool moves, original selection, outcome and last-CD
reporting. General asynchronous/output ownership remains separate.

## MCU-CDC-007: reserved host writes and bounded completion

Queued: admission from `50100444` plus the lifecycle in `3f7bba1a`. Reserve before
readiness, consume actual available words, retain private ownership until
DataEnd/reset, route completed sectors, and distinguish zero transfer from error.
Undefined untransferred payload must not be fabricated by empty FIFO reads.
