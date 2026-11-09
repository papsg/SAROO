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

Original: **paps-g**, `66c197b3` (2026-09-06), adapted after public `a3959f4`.
Validate the filename within240 bytes including NUL, reject buffer starts inside
the live256-byte request, and reject extents crossing it or the16 MiB mapped
window. Set the completed count to zero before validation. Rejected creates and
overwrite requests therefore perform no open/truncate/transfer. Bounds use
subtraction after address normalization, avoiding wrapping end-address sums.

The address contract comes from [FPGA05 `SSMaster.v`, SDRAM mapping](https://github.com/papsg/SAROO/blob/2664d51/FPGA/SSMaster.v#L669):
the MCU path exposes the low24 address bits. [MCU request constants](https://github.com/papsg/SAROO/blob/2664d51/Firm_MCU/Saturn/cdc.h#L12)
and [menu producers](https://github.com/papsg/SAROO/blob/2664d51/Firm_Saturn/main.c#L581)
define the request at offset `0x0c7000`, name at `+0x10` and bounce payload at
`+0x100`. This is a transfer-envelope contract, not a claim that every mapped
address is caller-owned. The FPGA's expansion-RAM folding, CPU cache aliases and
cross-CPU ownership require separate work and are not repaired by this port.

For size-zero reads, compute `f_size-f_tell` at full64-bit `FSIZE_t` width and
compare the remaining extent before narrowing or reading. The [configured
FatFs types](https://github.com/papsg/SAROO/blob/2664d51/Firm_MCU/FatFS/ff.h)
and `FF_FS_EXFAT=1` establish that width; the actual `f_lseek` clips ordinary
read-only seeks to EOF. Exact end/request/name boundaries and valid tail/EOF
reads remain supported. The original address aliases, command fields, partial
I/O semantics and direction-specific log formats remain. Legacy producer-side
name copies and caller allocation limits follow in the separate menu checkpoint.

The fixture grows by28 bounds scenarios:12 invalid fixed/name cases rejected
before open, four invalid whole-file extents rejected after seek with one close,
and12 valid boundary/tail controls. Together with prior completion cases, all61
scenarios in12 methods pass ASan/UBSan and O2. The preceding completion-corrected
baseline fails all16 invalid cases and passes45 controls. Generated FAT32 files
verify data/guard/request preservation and rejected-write targets. Two cases
inject only full-width size metadata (`2^32` and `2^32+17`) after real open; these
are not actual multi-gigabyte exFAT media qualification. A compile-time width
check and actual address-header assertions bind those modeling assumptions.
Six mutations detect missing name/request/fixed-size/whole-size checks,
window-only capacity and narrowing before comparison.

The complete App/RTX/166-vector link grows64 load bytes,57,332→57,396, leaving
8,140 ITCM bytes. Static DTCM remains60,296 and IRQ/dispatcher/helper direct
frames remain24/8/120. No allocation, task, buffer or normal filesystem operation
is added. Valid requests add a bounded filename scan and integer checks; whole-
file reads add a wide extent calculation. Invalid requests avoid file/data work.
The same public reproduction command above runs completion and bounds controls;
no physical latency, stack-peak, malformed-filesystem or game claim follows.
MCU-FILE-002 still has the legacy allocation/producer/coherence limits described
in the menu checkpoint; this is not blanket arbitrary-pointer safety.

## SATURN-WORD-001: unsigned word decoding

Original: **paps-g**, selected `4df2e67a` (2026-09-06), independently ported
before the menu continuation. Cast shift operands to `u32` in `BE32` and `LE32`.
The [public baseline readers](https://github.com/papsg/SAROO/blob/e1e6fdb/Firm_Saturn/main.c#L24)
promote bytes to signed `int`, so a high byte of128 or greater shifted by24
cannot be represented. The unsigned return type does not repair that intermediate
operation. C11 [N1570 §6.5.7 paragraphs3–4, printed94–95](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
defines operand promotions and signed/unsigned shift semantics; the cached primary
clauses were read. Existing byte reads preserve unaligned access behavior.

Two actual-helper methods pass sanitizers/O2:8,256 full-width checks across both
byte orders/four alignments and4,128 baseline-valid low-byte controls. Independent
byte/write guards also check `LE16` and `LE32W`. Both old full-width readers fail
UBSan while the low-byte cohort passes. Reverting either reader independently
fails its signed-shift check. The nine existing binary-selection cases pass.

The complete pinned SH-2 menu link is byte-identical, including ELF and final
image hashes. Code/data remains78,884, BSS9,232, reserved stack16,384, font357,679
and image488,751 bytes; direct frames remain identical. All36 menu inputs match
the receipt. MCU sources and its prior56-input build remain unchanged. This is a
source-semantics/portability correction; no physical game failure, speed change
or hardware acceptance is claimed. `check-file-transfer` now includes these tests;
`SAROO_MENU_WORD_SOURCE` selects a preserved baseline or mutation.

## MCU-FILE-002: capacity-aware menu and binary loading

Original: **paps-g**, selected `4df2e67a` (2026-09-06), after the separately
verified unsigned readers at `ae59ff3`. Preserve earlier catalog/path checks and
extend their actual implementation. The public adaptation also preserves the
existing controller suspend → jump → resume ordering, and checks it in the
modeled-jump fixture. No title-specific code, runtime diagnostics or new loader
is introduced; this bounds the existing BIN loader and file APIs.

`read_file_bounded(name, offset, size, buffer, capacity)` requires caller-owned
writable storage. Reject negative read offset/size, fixed size above capacity,
zero whole-file capacity, missing/overlong names and invalid shared extents
before publication/copy. Check returned counts against the requested extent
before copying bounced data. Names retain239 bytes plus NUL; the shared bounce
span stops before the existing cover/GIF workspace. Preserve fixed-size partial/
EOF semantics and zero-byte write/create behavior. The unused request word at
`+0x0c`, command IDs and all earlier export slots are unchanged.

For a size-zero whole-file read, issue a capacity-sized ordinary request. A short
result proves EOF within capacity. If it fills exactly, probe one byte at the
checked following offset: EOF accepts the exact fit; extra data or a probe error
rejects it. No unbounded read is sent. Smaller files use one command/open/seek/
transfer/close; full-capacity reads add one sequential one-byte command and file
sequence. Failure can leave a bounded copied prefix, which callers must discard.
The probe uses one local byte and existing transfer storage, with no heap or
retained buffer. Two commands are not an atomic snapshot of a mutable file.

The four-argument `read_file` and `write_file` exports at `0x02000f08/0x02000f0c`
remain. Legacy whole-file calls use remaining supported physical-bank capacity;
they cannot infer a smaller C allocation. New homebrew can check `SFB1`
(`0x53464231`) at `0x02000f14` before using the new five-argument reader at
`0x02000f10`. Those slots were unused in the baseline. The producer uses existing
fixed-size MCU requests and works with the original handlers for the tested
valid transfers; it does not backport MCU error/bounds repairs into old firmware.

The existing BIN loader checks catalog count/offset/string extent, joined-path
length and optional `@hex` syntax/overflow before file I/O. Destination capacity
comes from these supported ranges, with ends exclusive:

| Permitted physical start | End | Basis |
| --- | --- | --- |
| `0x00200000` | `0x00300000` | Low work RAM |
| `0x06002000` | `0x06100000` | High work RAM above BIOS work/vectors |
| `0x020c7100` | `0x02400000` | Cartridge scratch after the live request |
| `0x02800000` | `0x03000000` | Supported free cartridge span |

Sega [SCU User's Manual ST-097-R5-072694, §1.2, Figure1.3, printed4/PDF20](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=20)
and [Figure1.5, printed6/PDF22](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=22)
define the two1 MiB work-RAM banks and cache-through aliases. Both cached pages
were read and their memory-map diagrams rendered/inspected. They do not define
SAROO's reserved buffers: [the linker](https://github.com/papsg/SAROO/blob/ae59ff3/Firm_Saturn/ldscript)
places menu/BSS/stack in cartridge memory; [shared constants](https://github.com/papsg/SAROO/blob/ae59ff3/Firm_Saturn/main.h#L59)
reserve catalogs/saves/request; [`game_load.c`](https://github.com/papsg/SAROO/blob/ae59ff3/Firm_Saturn/game_load.c#L296)
uses BIOS work and the high-RAM IP start; [cover/GIF use](https://github.com/papsg/SAROO/blob/ae59ff3/Firm_Saturn/main.c)
reserves `0x02400000`; [FPGA05's mapping](https://github.com/papsg/SAROO/blob/ae59ff3/FPGA/SSMaster.v#L669)
supplies the supported cartridge decode. These project contracts define the
restricted loader policy, not a universal allocation map or cache repair.

Keep the default load address `0x06004000`. Accept its cache-through counterpart
and supported alternative regions; reject unsupported classes and odd execution
addresses. A jump requires at least one complete two-byte instruction. Inspect
the eight-byte `SEGA SEG` prefix only when all eight bytes arrived, using a byte
comparison; preserve the existing `+0x0f00` entry rule and require at least
`0x0f02` loaded bytes. Load-without-execute retains zero/one-byte loads. Bounds
and headers do not authenticate executable contents or authorize file paths.

Nine coupled actual menu/MCU/FatFs methods cover30 transfer scenarios; one
actual-loader method covers40 cases. All pass sanitizers and O2. The prior
unsigned-reader menu fails15 transfer and21 loader cases, with15/19 preservation
controls passing. The same30 transfer cases also pass with the original MCU
handlers. Coverage includes exact/oversized/tail/EOF reads, invalid args/names,
bad/high-bit response counts, probe errors, writes/zero-byte creates, direct/
bounce/request boundaries, legacy function-pointer calls and actual ABI-slot
assignments, default-capacity BIN/2 MiB companion transfers, malformed catalogs/
addresses and short executable/header refusal. Jumps/data/storage/registers are
modeled; source/controller order is checked without executing a real BIN.
Seven real-FatFs discovery and seven cover regression methods pass with the
updated actual loader, preserving catalog publication and earlier controls.

Nine meaningful mutations are rejected: unchecked replies, missing EOF proof,
ignored capacity, weakened full name bound, oversized bounce extent, missing
header length, ignored address overflow, omitted controller suspension and
unchecked catalog count. One initial loop-only name mutation passed the tested
boundary because its rejecting-length guard was unchanged; that incomplete
negative control is preserved and not counted as a rejected mutation.

The complete SH-2 menu grows688 code/data bytes,78,884→79,572. BSS9,232,
reserved stack16,384, font357,679 and padded image488,751 bytes remain; body
margin is25,884. The MCU is unchanged at57,396 load/60,296 static DTCM bytes.
Compiler direct frames: old read/write20/20; new legacy read24, bounded reader36,
command helper36, write8; BIN loader144→148. The BIN read caller sum changes
164→220 (+56), before utilities/interrupt frames; legacy read/write sums are
96/44. Sequential probes do not accumulate their frames. These source/compiler
caller sums and preserved target disassembly are not physical stack peaks.
Added scans/checks and the occasional extra command have unmeasured device
latency; no speed, audio, game, original-MDK or hardware qualification is claimed.

```sh
make -C tests check-file-transfer
SAROO_FILE_PROFILE=release make -C tests check-file-transfer
python3 -B -m unittest discover -s tests -p test_menu_binary_bounds.py -v
# Overrides: SAROO_MENU_FILE_SOURCE, SAROO_MENU_MCU_SOURCE,
#            SAROO_MENU_BINARY_SOURCE; all accept local source files.
```

MCU-FILE-002 retains its ownership limitation: mapped addresses and a capacity
argument do not prove ownership of an arbitrary allocation. Legacy whole-file
callers must provide sufficient storage or use the opt-in API. Names/buffers
must remain valid and exclusively owned during the nonreentrant command.
Expansion-RAM folding, calling/other-CPU cache coherence, reset generations,
unbounded command waits, pending file recovery and atomic snapshots/writes are
separate later work. This scoped port does not close those findings or any
unattributed game/audio/save-loss report. Private repo and Saturn stay untouched.
