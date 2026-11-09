# Firmware save-safety ports

This series starts at public `7d1d461`. It independently ports the original
v0.8 firmware save corrections, preserving layouts/capacities. The desktop
[save-tool series](SAVETOOL-SAFETY.md) is separate. Source changes are direct,
with no private patch framework, USB integration, added runtime diagnostics,
native-backup migration or experimental journal/transaction system.

## SATURN-SAVE-002: unsigned dirty-window shifts

`mbuf_mark` tracks modified blocks in two 32-bit words for the 64-block MEMS
window. At positions 31 and 63 it evaluates signed `1 << 31`, outside the
representable positive range. The two shift operands are now unsigned. No
bitmap layout, buffer, command or normal allocation is added.

The primary C requirement is
[WG14 N1570 §6.5.7 paragraphs 3–4, printed pp. 94–95](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).
The operand's promoted type determines shift arithmetic; signed left shift has
a representability condition, while unsigned arithmetic is reduced modulo its
range. This is a C source defect rather than a Sega register requirement.
The project-specific window is established by
[`get_block_addr` and `mbuf_mark` at the public starting point](https://github.com/papsg/SAROO/blob/7d1d461/Firm_Saturn/game_save.c).

Our original correction was authored by `paps-g` in `d8fe6875` on 2026-09-05,
alongside save resizing. This public port separates the two findings. The dirty
mask is an explicit prerequisite for sanitizer qualification of large resize
cases, so it is ported first. Original source notices remain unchanged.

Three public methods include the actual firmware C with test-only MMIO stubs.
They exercise every bit at three window starts (192 cases), repeated marks,
combined words and a slot-device control that leaves MEMS tracking unchanged.
The baseline has seven failed subcases under UBSan: six single-bit cases at
positions 31/63 and one combined-window case. Other bit positions and the
slot-device control pass there. All 194 invocations now pass ASan/UBSan and
normal O2. The fixture issues no storage command.

The preceding and corrected public trees both fully compile/link all sixteen
menu units with the pinned GCC 13.2.0 / Yaul environment. The actual
`game_save.o`, ELF and complete menu image are byte-identical between them.
Code/data remain 75,056 bytes, BSS 7,180, reserved stack 16,384 and body margin
32,452. The image remains 488,751 bytes with the same 357,679-byte font. This
compiler already emitted the intended shift instructions; source correction
removes undefined arithmetic without adding instructions or storage in this
qualified build. Existing unrelated target warnings remain.

From the repository root:

```sh
make -C tests check-firmware-save
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-firmware-save
cargo build --offline --locked --manifest-path tools/Cargo.toml -p firmware-build
tools/target/debug/firmware-build menu --repo "$PWD" --out tests/build/menu-dirty
```

`SAROO_GAME_SAVE_SOURCE` selects an independently extracted `game_save.c`, for
example from `7d1d461`, to reproduce the old failures. The test stubs/model are
not linked into firmware. The public MIT host builder is a verification
prerequisite for repeated isolated builds; it uses the existing Makefile and
cached toolchain, records inputs/resources, and refuses existing output paths.

These are host C/sanitizer and target compile/link checks. They do not establish
physical endian encoding, interrupt/reentrancy/cache/bus behavior, storage
durability, game compatibility or a historical crash cause. In particular,
[issue #363](https://github.com/tpunix/SAROO/issues/363) is related investigation
context, not a report resolved by this two-line correction. No device, private
source/cache/branch or installed firmware is changed.

Save resizing, metadata validation, successful dirty-flush bookkeeping, separately
evaluated header batching, checked MCU startup/runtime, incomplete-tail admission
and Saturn error/retry propagation remain following series members.

## SATURN-SAVE-001: resize saves and flush changed allocation

At public `1239a7d`, overwrite stores a new date but uses the old header size and
allocation for copying. Growth is truncated; shrinking can read beyond the
caller's smaller buffer. MEMS allocation changes also need the global header
flushed, so correct RAM behavior alone does not establish correct reloads.

The replacement path plans allocation in private scratch, reuses the selected
save's old data blocks, checks the selected old layout/capacity and retains its
header/list position. Only after the plan succeeds does it publish size, data
allocation and actual free count, then write the complete supplied payload.
MEMS final flush includes changed global allocation metadata. The 960-byte
boundary stays inline; new saves above table capacity and a full MEMS directory
are refused before allocation. Existing same-size overwrite semantics remain.

The public API references are Sega's
[Saturn System Library User's Guide, ST-162-R1-092994](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf),
Backup Library §2.1 `BupDir` (printed p. 42, PDF p. 47: byte data size), and
§2.3 function No. 5 `BUP_Write` (printed p. 45, PDF p. 50: directory/data inputs,
overwrite-check mode and failure/success results). The original unchanged PDF's
text and rendered write specification were checked. Those API rules do not
define SAROO's bitmap/table layout, which comes from the starting
[`BUPHEADER`, `MEMSHEADER`, `access_data`, `find_save` and writer](https://github.com/papsg/SAROO/blob/1239a7d/Firm_Saturn/game_save.c).
Maximum payloads remain 510 × 128 bytes for an otherwise empty slot and
479 × 1,024 bytes for MEMS; neighboring saves can reduce actual capacity.

Our correction was originally authored by `paps-g` in `d8fe6875` on 2026-09-05.
This public source port independently revalidates it after the separate
SATURN-SAVE-002 prerequisite; no private preparer is used. Source notices and
the prior language/controller/host-tool contributions remain intact.

Two actual-C model methods execute 32 cases: 21 successful growth/shrink/zero/
inline/table/maximum/same-size round trips and eleven refusals covering capacity,
oversize, selected old chains, overwrite protection and full directory. They
check payload/read/verify, metadata, output sentinels, neighbors, allocation/free
counts and complete refused disk images. Discarding model RAM and reloading
checks the MCU flush contract. The preceding public source fails 28 cases;
all 32 pass ASan/UBSan and normal O2. Removing only the changed-allocation flush
flag makes eight cases fail after reload. This control distinguishes updated
RAM from allocation metadata actually sent through the modeled storage commands.

The model includes the real firmware module and synchronously implements the
existing MCU slot/MEMS load/flush behavior, including overlapping header/data
windows. Stored typed fields use host byte order. It is not a physical reset,
SD/controller, bus, cache, interrupt or concurrency simulation. Checks establish
the logical API/allocation/reported-command behavior. They do not authenticate
arbitrary input archives or establish power-loss atomicity. The MCU still needs
the later series members to check actual I/O results and propagate them.

The complete pinned SH-2 build qualifies all sixteen menu C units. Code/data
grow 960 bytes to 76,016, and BSS grows 2,028 to 9,208 for the private bitmap and
block list. The fixed 16,384-byte menu stack and 488,751-byte image/font layout
remain, with 29,464 body bytes free. The two large arrays live outside the
calling game's stack, under the service's existing synchronous ownership.
GCC inlines the planner: the write function's direct frame grows from 52 to
104 bytes (eight four-byte saves plus 20/72 local bytes). This is partial
compiled stack accounting, excluding callees/interrupts; the menu stack reserve
does not prove a calling game's stack headroom. Same-size writes skip planning;
different-size writes scan at most 512 slot or 8,064 MEMS blocks. No new I/O
command type or whole-archive copy is added. No physical latency is claimed.

Run `make -C tests check-firmware-save` for coupled dirty/resize checks; the
source/profile overrides and isolated target-build command above also apply.
Full metadata/ownership admission, dirty-flush bookkeeping, separately evaluated
batching and MCU/error/retry changes remain following scope. This correction
does not establish that a named game's historical save report is resolved.

## SATURN-SAVE-003: validate metadata and block ownership

At public `6e9c217`, read, verify, directory, delete and same-size overwrite still
trust stored references. Malformed chains can loop or address outside storage;
one save can claim another save's header or payload. A stale free counter can
also admit an allocation that the bitmap cannot satisfy. Resize-only checks do
not protect these other entry points.

The service now checks device bounds, signature, fixed geometry, reserved bits,
all live headers, representable payload sizes, allocated block references,
table termination and unique ownership before exposing or modifying saves.
Headers are claimed first, bounding slot-list cycles and preventing payloads
from claiming another header. Inline MEMS bytes remain payload. Allocated orphan
blocks stay reserved. Queries recount available space without repairing storage;
successful mutations persist the corrected counter. Status respects requested
payload size and directory capacity without underflowing at zero free blocks.

Read/write/delete/verify use exact bounded names, so `ABC` cannot select
`ABCDEF`; directory search retains prefix matching and its signed hit-count
convention. Names are scanned for at most eleven bytes. Returned name/comment
fields are terminated after their eleven/ten stored bytes. MEMS directory/header
identity compares all eleven stored bytes, including bytes following a NUL.
This storage consistency rule is separate from the API's terminated name lookup.
Directory failure returns zero without changing the output table, rather than
exposing a positive backup-error number as a count. The other APIs retain their
unformatted/broken-storage error results. Zero-length payloads need no buffer.

Sega's [ST-162-R1-092994 System Library User's Guide](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf)
documents Backup Library §2.1 `BupDir` (printed p. 42, PDF p. 47: eleven-byte name
and ten-byte comment plus terminators) and §2.3 functions No. 4–9 (printed
pp. 45–47, PDF pp. 50–52: requested-size status, write/read/delete/verify results,
and directory prefix/count behavior). The original local PDF and relevant
pages were checked. Sega does not define SAROO's private archive bitmap/table or
raw directory/header identity: those contracts come from the
[`6e9c217` firmware layout and writer](https://github.com/papsg/SAROO/blob/6e9c217/Firm_Saturn/game_save.c).

The original correction was authored by `paps-g` in `c97d2d70` on 2026-09-05.
This public source port independently reproduces it after resizing. Added public
identity controls incorporate the later full-field test insight: the original
validator already used `memcmp`, so this is stronger coverage, not another
firmware identity fix. Source notices and preceding contributions are preserved.

Six actual-module methods execute 357 scenarios. The original 349-scenario
matrix covers 329 malformed-storage operations, stale counts, exhaustion,
maximum directory, exact/prefix/fixed-width names, API boundaries, initialization
and status-size controls. The preceding public source produces 322 failures and
twelve two-second case deadlines in that matrix; deadlines alone are not proof
of infinite loops. Eight added identity cases fail on the preceding source
under a longer ten-second observation. All 357 pass ASan/UBSan and normal O2.
Changing only full-field `memcmp` to `strncmp` makes seven mismatch operations
fail; an equal raw-field case still passes. Refusals check complete unchanged
model disk images, allocation metadata and caller output. Valid mutations check
neighbors and orphan reservation after modeled RAM reload.

All eleven coupled dirty/resize/validation methods pass in both profiles.
The complete pinned SH-2 build adds 1,200 code/data bytes and eight BSS bytes:
77,216 code/data, 9,216 BSS, 16,384 reserved stack and 28,256 body margin. Image
size remains 488,751 bytes with the unchanged font. Validation reuses the
existing bitmap. GCC direct frames include 28 bytes for the archive check,
36 for its per-save check and 104 for write; these exclude callees and interrupts
and do not establish a calling game's available stack. The host builder now
preserves compiler stack reports. Existing unrelated target warnings remain.

Validation is uncached. A full 448-entry zero-size MEMS directory requires 448
one-KiB header requests for status and 449 for a cold read, including reloading
the chosen header: 458,752/459,776 modeled bytes and no payload-window loads.
Slot validation reads resident RAM. These are command counts, not measured
Saturn latency. Batching is evaluated separately; this checkpoint makes no
claim of preserved game responsiveness.

Startup preserves a nonblank damaged signature rather than formatting it.
Automatic formatting still accepts a uniformly zero/FF inspected region: the
whole 64 KiB slot, but only the loaded 8 KiB MEMS metadata. That MEMS predicate
cannot distinguish a new archive from lost metadata with surviving payload;
the later MCU startup member must reject existing invalid archives before menu
service. The initialization fixture executes MEMS `bup_init` with a BIOS stub;
the fixed-address slot BIOS startup path is source/target checked only.

Use `make -C tests check-firmware-save`, its release profile and the isolated
target-build command above. `SAROO_GAME_SAVE_SOURCE` reproduces the preceding
source controls. The synchronous host model uses native typed-field byte order
and reported-success MCU loads/flushes. It does not establish physical encoding,
cache/IRQ/reentrancy/bus behavior, SD error recovery or power-loss atomicity.
Dirty bookkeeping, actual MCU I/O errors, pending-data ownership and retry
propagation follow in separate members. No particular game report is closed.

## SATURN-SAVE-005: release dirty bits after a successful buffer flush

`bup_update` previously left both payload-window dirty words set after a
successful MEMS flush. A later read that changed windows could therefore write
already committed bytes again. It now clears those words only when the requested
flush included `MEMS_BUF` and the completed command reports zero. Header-only
and slot flushes do not release payload dirty state; a nonzero result preserves
both words for retry. Cache start/end/header ownership is unchanged.

This independently ports the dirty-bookkeeping part of `paps-g`'s `a9ddfcc6`
(2026-09-05). Header batching is a separate following evaluation. No new command,
runtime diagnostic or storage format is introduced. The relevant contract is
the existing [`SSCMD_SMEMS` dispatcher](https://github.com/papsg/SAROO/blob/b084a3c/Firm_MCU/Saturn/saturn_main.c)
and [`flush_smems`](https://github.com/papsg/SAROO/blob/b084a3c/Firm_MCU/Saturn/saturn_utils.c):
publish the helper result in `SS_ARG`, then clear `SS_CMD`. Sega's API result
rules do not specify these SAROO-private dirty words or command registers.

The current MCU still ignores actual seek/read/write/sync errors and reports
success. Clearing on its acknowledgement does not establish SD durability.
FatFs's [`f_sync` return-values and description](https://elm-chan.org/fsw/ff/doc/sync.html)
explicitly allow errors and describe flushing an open file's cached state.
Checked MCU transfers and full Saturn error/eviction/retry ownership follow in
later members; this change alone does not fix a caller that discards state
after a failed eviction.

Two actual-module methods run 33 scenarios: all eight valid MEMS/slot flush flag
combinations with zero/negative/two positive reported statuses, plus a 65-block
write followed immediately by read/verify across windows. Failed-status cases
retry the same flush successfully and check both dirty words and owner indices.
The model publishes completion status in the real dispatcher's order. Its
injected flush failure occurs after copying bytes, representing uncertain late
failure rather than rollback. The preceding source fails seventeen scenarios.
All 33 now pass ASan/UBSan and O2. Removing the success guard fails twelve
cases; removing the buffer-flag guard fails four header/slot controls.

All thirteen coupled firmware methods pass in both profiles. Complete SH-2
compilation adds 32 code/data bytes to 77,248, with BSS 9,216, reserved stack
16,384 and body margin 28,224. The image/font sizes remain unchanged. Direct
compiler frames are unchanged, including zero bytes for `bup_update`; this
does not account for transitive/IRQ use or game stack headroom. No extra flush
command is issued by this change, and the read-after-write case issues none.
These are synchronous host logic and target-link checks, with the physical,
concurrency and durability limits stated above. Use the same public firmware
test/build commands and source/profile overrides to reproduce them.

## SATURN-SAVE-005: batch full runs of validation headers

The uncached ownership check can reduce command exchanges without reading extra
header bytes. When the payload window is unused, it orders live MEMS directory
indices by physical header block in existing resize scratch. Already ordered
indices need a linear check; a bounded heap sort handles other orders. Only
complete runs of 64 consecutive headers use the existing 64 KiB loader. Shorter
runs retain one-KiB header loads. Directory storage/output order is unchanged,
and every loaded header receives the same layout/ownership validation.

A populated payload cache keeps the single-header path, avoiding a later extra
64 KiB payload read. An unused window with unexpected dirty bits is refused.
Batch requests leave Saturn payload indices invalid: a later payload access
must reload both sides' window state before modifying bytes. A reported failed
batch load is rejected even if the buffer already contains valid old headers.
No validation result is cached across API calls; existing per-header/payload
cache coherence assumptions remain.

This separately ports the header-batching portion of `paps-g`'s `a9ddfcc6`
(2026-09-05), after the successful-flush dirty-state prerequisite. The original
[`load_smems` file/window contract](https://github.com/papsg/SAROO/blob/0c76466/Firm_MCU/Saturn/saturn_utils.c)
supplies the loader and overlap behavior. FatFs
[`f_open`, Description: duplicate opens involving write access](https://elm-chan.org/fsw/ff/doc/open.html)
prohibits another open handle for the existing writable `sm_fp` file. Therefore
batching reuses its loader rather than issuing a generic file-read command.
These private cache/command rules are not Sega register requirements. Original
notices remain, with no MCU/protocol change or second file handle.

Thirteen before/after cold-status profiles check identical header-byte traffic.
Representative measured model counts are:

| Header layout | Saves | Before commands | After commands | Header bytes |
| --- | ---: | ---: | ---: | ---: |
| Consecutive | 63 | 63 | 63 | 64,512 |
| Consecutive | 64 | 64 | 1 | 65,536 |
| Consecutive | 65 | 65 | 2 | 66,560 |
| Consecutive | 127 | 127 | 64 | 130,048 |
| Consecutive | 447 | 447 | 69 | 457,728 |
| Consecutive | 448 | 448 | 7 | 458,752 |
| Sparse | 448 | 448 | 448 | 458,752 |
| Reversed/permuted directory, consecutive blocks | 448 | 448 | 7 | 458,752 |

In the zero-size full-directory fixture, a cold read uses eight commands instead
of 449, with the same 459,776 header bytes. A full warm-cache status control still uses 448 header commands
and zero window loads, preserving all 64 KiB of the live payload cache. Sorting
adds bounded CPU work; sparse/warm worst cases remain material latency limits.
These are model command/byte counts, not Saturn timing or game responsiveness.

Seven actual-module methods run 57 scenarios: ordered/unordered/high/short/full
ranges, sparse layouts, live cache and unexpected dirty state, all six mutation
forms after batching, three reported-load errors, four later-request metadata
changes, and eight corruption cases within batches. The latter include complete
stored-name mismatches after a NUL at both ends of a batch. An added full-directory
control grows a save, preserves its live payload window through validation and
reads it without another payload-window load. Corruption and load errors leave
caller status output untouched; valid mutations preserve neighbors after reload.

The preceding source fails 35 cases in the initial 54-scenario matrix; the later
two identity and one full warm-cache controls supplement that reproduction.
All 57 scenarios pass ASan/UBSan and O2, together with all twenty coupled firmware
methods. Omitting only the load-status guard fails three cases; omitting only
payload-cache protection fails one; skipping batch validation fails twelve
metadata/corruption cases. Valid preseeded buffers make the load-error controls
independent of accidental invalid stale bytes.

Complete SH-2 compilation adds 624 code/data bytes to 77,872, with unchanged
9,216 BSS, 16,384 stack reserve and image/font sizes; body margin is 27,600.
There is no new persistent allocation or whole-archive buffer. Compiler direct
frames include eight bytes for the heap helper and 48 for the archive check
(previously 28); write remains 104. Those figures exclude callees/IRQs and do
not qualify game stack headroom. Existing target warnings remain unrelated.
Use the same public tests/build command above; the standalone batching fixture's
`profile` mode prints command/byte counts without enforcing the new threshold.

The MCU still needs checked I/O and the public APIs need full pending/error/retry
ownership. A short transfer falsely acknowledged as success is not detected by
the new status guard. These host/model/target-link checks do not establish
physical cache/IRQ/bus behavior, SD durability, power-loss atomicity or games.

## GNU MCU build prerequisite: malloc size type

The public MCU source declares/defines `malloc(uint32_t)` while including the C
library from other units. Under the complete Arm GNU 14.3.Rel1/newlib toolchain,
`uint32_t` is `unsigned long` and `size_t` is `unsigned int`. Both are 32 bits,
but their C function types conflict: the unchanged `Saturn/cdimg.c` fails to
compile when `<stdlib.h>` meets that declaration. A compiler-only Homebrew
installation first failed earlier for missing newlib; that setup failure is
not counted as the source defect.

The declaration and definition now use `size_t`, with an explicit `<stddef.h>`
include. The allocator body, RTOS pool and ownership policy are unchanged.
[C11 N1570 §7.22.3.4 paragraph 1, printed p. 349](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
specifies the prototype. This is a GNU build portability prerequisite, not a
claim that the original MDK typedefs were incompatible or a Sega register fix.
The source contract is the original public
[`inc/main.h`](https://github.com/papsg/SAROO/blob/a62eb2c/Firm_MCU/inc/main.h)
and [`Main/main.c`](https://github.com/papsg/SAROO/blob/a62eb2c/Firm_MCU/Main/main.c).
The same size-type adaptation was present in `paps-g`'s original GNU integration;
only this necessary declaration correction is ported here.

The full GNU App compile/link check succeeds after correction, including all
nineteen project C units, the configured RTE provider, RTX and tick/assembly
providers. The allocator's complete 20-byte `.text.malloc` section is identical
before/after under the same GCC, with a zero direct frame; debug type records
are not claimed identical. No buffer, command or diagnostic is added.
The original MDK project remains available; this check is not an MDK image,
physical allocator/reentrancy, peak stack or game qualification.
