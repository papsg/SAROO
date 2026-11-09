# Shared-memory file-transfer safety

Start at public `2664d51`. Port selected September6 contributions from **paps-g**
as separate verified source commits. These are SAROO's shared-memory file
commands, used by BIN/companion-ROM loading and the existing shell's file export.
They are distinct from ordinary game CD streaming and save-archive persistence.
No upstream issue/game failure is attributed to these self-reported findings.
USB, new diagnostics, private tooling and unrelated later protocols are excluded.
Original source and FatFs notices remain.

## MCU-FILE-001: report positioning and completion errors

Original implementation: `52ba5498` (2026-09-06). The public adaptation shares
open/seek/transfer/close setup in `ss_file_transfer`, retains both command numbers,
request fields, existing log strings/masks and partial/EOF transfer semantics.
The original direction-specific logger text is preserved instead of importing
the private combined format. One outlined helper keeps its file workspace out
of ordinary command-handler paths.

The [baseline command handlers](https://github.com/papsg/SAROO/blob/2664d51/Firm_MCU/Saturn/saturn_main.c#L668)
ignore `f_lseek` and `f_close` results; open failure leaves the requested count.
Pinned public FatFs R0.15 w/patch1 supplies reachable failure behavior:
[`f_lseek`](https://github.com/papsg/SAROO/blob/2664d51/Firm_MCU/FatFS/ff.c#L4434)
can return disk/chain errors and can clip a write extension at allocation
exhaustion while returning success. [`f_close`](https://github.com/papsg/SAROO/blob/2664d51/Firm_MCU/FatFS/ff.c#L4217)
calls [`f_sync`](https://github.com/papsg/SAROO/blob/2664d51/Firm_MCU/FatFS/ff.c#L4136),
whose data/metadata flush can fail. These source/API contracts supply the
requirement; Sega does not define these custom file commands.

Stop before data I/O after a failed seek. For writes also require the resulting
position to equal the requested offset. After a successful open, attempt close
exactly once, preserving any earlier error; otherwise propagate the close error.
Publish the actual count, initialized to zero, before completing the command.
Open/seek failures therefore cannot expose the request as completed bytes.
Partial successful reads/writes return their counts; partial errors retain their
counts and report the error. Negative write offsets and create/overwrite/extend/
zero-byte behavior retain the existing contract. Whole-file bounds follow in
the separate MCU-FILE-002 checkpoint.

Public [`ffconf.h`](https://github.com/papsg/SAROO/blob/2664d51/Firm_MCU/FatFS/ffconf.h#L227)
selects `FF_FS_TINY=1`, `FF_FS_LOCK=0` and `FF_FS_REENTRANT=0`. The private volume-
mutex timeout reproduction is inapplicable to this standard configuration and
is not reused as evidence. Tests compile all public FatFs settings unchanged.
A real FAT-chain seek read error, real disk-write failure during close and the
real allocation-exhaustion branch reproduce reachable failures instead. A forced
free-cluster cache value exercises exhaustion without filling a physical card.

Nine actual-C/FatFs methods cover33 scenarios under ASan/UBSan and O2. Baseline
fails13 and passes20 preservation controls. Cases cover ordinary/whole/empty/
EOF/beyond-EOF/short reads; overwrite/create/negative/extension/zero-byte/short
writes; open, seek, short-position, transfer, partial-error and close failures;
earlier-error precedence; real disk failures; unchanged file/buffer guards;
actual returned counts, command completion, I/O order and close attempts.
The fixture compiles the actual protocol/log macros and validates exact existing
log text with logging enabled and disabled. Six mutations fail their intended
checks: ignored seek, ignored clipped seek, ignored close, last-error precedence,
requested count on failure and duplicate close. Eleven coupled disc/save methods
and six GIF scheduling methods also pass.

The complete GNU App/RTX/166-vector link uses57,332 load bytes, down16 from
57,348, leaving8,204 ITCM bytes. Static DTCM remains60,296. The persistent IRQ
frame stays24 bytes; command-handler direct frame falls112→8, and the outlined
file helper uses120. The active IRQ/command/file caller frames therefore total
136→152 (+16) before filesystem/logging callees and exception frames; ordinary
command-handler paths avoid the old file workspace. Required4 KiB task stacks,
allocator pool and all static capacities remain. These are compiler frames,
not physical peaks. Normal successful transfers still use one open, seek,
transfer and close, with no new allocation, extra file I/O or retry. Added
position/error checks and changed call layout do not establish a speedup.

```sh
make -C tests check-file-transfer
SAROO_FILE_PROFILE=release make -C tests check-file-transfer
# Baseline/mutation override:
# SAROO_FILE_TRANSFER_SOURCE=/path/to/saturn_main.c
```

Failed seek/create/transfer/close operations can already have changed allocation,
file length, data or pending filesystem state. Reporting failure does not roll
that back, retry close, retain the local file handle or guarantee durability.
This checkpoint does not establish atomic writes, safe malformed request extents,
reentrancy, reset generations, cache coherence or physical/game qualification.
Public fixtures model the shared address base and storage errors on synthetic
media; they do not access a device. Exact sources, controls, builds/disassembly
and privacy receipts remain local and ignored.

## MCU-FILE-002: MCU request bounds

Queued: bounded names, request/window overlap and full-width whole-file extents.

## SATURN-WORD-001: unsigned word decoding

Queued separately before dependent menu transfer tests.

## MCU-FILE-002: capacity-aware menu and binary loading

Queued: preserve legacy exports while adding bounded reads, checked shared hops
and binary destination/header limits. Existing catalog checks will be deduplicated.
Legacy callers must still own valid storage; later cache/reset/ownership work
remains separate.
