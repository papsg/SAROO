# Native save-tool safety

This series ports independently reviewed maintenance corrections to the native
`tools/savetool` CLI. The public v0.8 archive formats and capacities remain the
compatibility target. Browser, console firmware and physical storage acceptance
have separate scopes.

## SAVETOOL-002: resizing an existing MEMS save

On public starting commit `54d0c7244db481b9218447dc902b928b5e6bb760`, overwriting
a save replaces its date and copies its payload using the **old stored size and
allocation**. A 32-byte save replaced by 64 bytes exports only 32; replacing 64
bytes with 32 exports an extra 32 bytes. The same error crosses the 960-byte
inline boundary and affects zero-length and maximum-size replacements.

The primary format/behavior reference is the baseline
[`sr_mems.c`](https://github.com/papsg/SAROO/blob/54d0c7244db481b9218447dc902b928b5e6bb760/tools/savetool/sr_mems.c),
specifically `MEMSHEADER`, `access_data`, `sr_mems_import` and the block bitmap.
MEMS is a project-defined archive: its 1,024-byte blocks, eight reserved blocks,
8,064 bitmap positions, 960 inline bytes and 479-entry terminated data table
establish the layout and 490,496-byte maximum payload.

The selected save's header and directory position remain stable. A private
bitmap and bounded block list plan replacement storage, allowing reuse of its
old data blocks even when no other free space exists. Header/old-data references
and capacity are checked before publication. On success, the size, block table,
bitmap and recounted free-block total are committed, then the replacement data
and date are written. Existing name/comment/language overwrite semantics remain.
Equal-size overwrites retain their previous byte layout.

The temporary plan contains a 1,008-byte bitmap and 958-byte block list; there
is no extra full-archive copy or firmware allocation. Capacity/layout refusal
preserves the complete archive. This protects the resize path; full cross-save
metadata admission, incoming-file validation, new-save allocation and checked
file replacement are separate subsequent members of this series. The current
legacy writer still has its previously documented persistence limits.

### Origin and attribution

Resizing is our independently discovered correction, first implemented in the
maintenance workspace on 2026-09-05 as `3eada90f` (author `paps-g`). This public
port revalidates it against public main and additionally bounds the selected
overwrite header before accessing it. It does not import the private build or
patch-application framework.

Damián Parrino / Bucanero's
[original PR #372](https://github.com/tpunix/SAROO/pull/372) fixes the separate
date-pointer error and is already integrated. Its correction, original credit
and same-size preservation controls remain intact. SRO/BUP support also remains
the existing implementation from
[#269](https://github.com/tpunix/SAROO/pull/269) and
[#270](https://github.com/tpunix/SAROO/issues/270)/
[#271](https://github.com/tpunix/SAROO/pull/271); those contributions are not
imported again. Existing source notices are preserved.

### Reproduce and verify

From the public repository root, with Python 3 and a C compiler. The browser
compatibility target additionally requires Rust 1.85+ and Node.js 22+:

```sh
make -C tests check-native-save check-save-endian check-web-save
cc -std=gnu11 -O2 -g -Wall -fno-strict-aliasing tools/savetool/*.c -o tests/build/savetool/sst-release
SAROO_SAVETOOL_BINARY="$PWD/tests/build/savetool/sst-release" python3 -B -m unittest discover -s tests -p 'test_mems_*.py' -v
```

The new three-method CLI suite exercises thirteen successful resize scenarios
and six rejected layouts/capacity cases. It checks old-block reuse, inline/table
transitions, zero and maximum lengths, stable identity/metadata, complete
allocation/free-count consistency, unchanged neighbors and all bytes outside
the permitted mutation extents. Fresh CLI processes export the exact payload,
date and CRC; read-only exports and incoming saves remain unchanged. The five
existing MEMS overwrite methods continue to cover same-size and directory-edge
controls. Both ASan/UBSan and the normal O2 build pass.

To exercise the same assertions against the public starting point:

```sh
mkdir -p tests/build/save-baseline
git archive 54d0c7244db481b9218447dc902b928b5e6bb760 tools/savetool | tar -x -C tests/build/save-baseline
cc -std=gnu11 -O1 -g -fno-strict-aliasing -fsanitize=address,undefined -fno-omit-frame-pointer tests/build/save-baseline/tools/savetool/*.c -o tests/build/save-baseline/sst
SAROO_SAVETOOL_BINARY="$PWD/tests/build/save-baseline/sst" python3 -B -m unittest discover -s tests -p test_mems_resize.py -v
```

The baseline command is expected to fail the new resize assertions. Its
malformed-header control also exposes the old out-of-bounds access. Existing
same-size overwrite controls pass at that baseline. The corrected path passes
all nineteen new scenarios. Existing unsigned-field and browser/native
interchange checks also pass; the browser runtime sources are unchanged.

These are generated-file host checks on macOS. ASan/UBSan bounds and undefined
behavior checks remain enabled; leak reporting is disabled for the original
CLI's process-lifetime buffers. No user saves, physical SD card, Saturn or
firmware are accessed. Host checks do not establish power-loss durability or
Saturn game acceptance.

## SAVETOOL-003: input extents, integrity and complete reads

The public `de8715b` starting point accepts inconsistent incoming sizes and merely
warns about a bad recorded CRC. Short headers can trigger invalid reads/copies;
oversized input can overwrite the shared payload buffer. A header-only BUP save
enters the SRO decoder. The archive dispatcher also invokes readers on incomplete
inputs, accepts unsupported larger plain backups and checks only four bytes of
an odd-byte backup signature. File loading ignores seek/read/close failures and
narrows unchecked lengths before allocation.

The new loader checks both seeks, the reported length, allocation arithmetic,
the complete read, error indicator and close result. Failure releases the input
and leaves the returned length zero. The existing two-times-file allocation,
including spare capacity for SAROO slot creation, remains; host allocation limits
and whole-file memory costs still apply.

Before copying payloads, SRO/BUP imports now require complete 64-byte headers,
at most 1 MiB of transport payload and an exact declared/actual payload match.
Nonzero SRO CRCs must match; zero retains the legacy absent-checksum convention.
Input allocations are released after copying validated metadata/data. Header-only
BUP saves use the correct parser. The already-integrated unsigned-field reader
`9f41183` is unchanged.

SRO's fixed eight-byte magic and separate four unused bytes follow Bucanero's
[revised format description in issue #270](https://github.com/tpunix/SAROO/issues/270#issuecomment-2615753729).
That description specifies big-endian CRC32 at `0x0c`, covering metadata/payload
from `0x10` through EOF. The unused bytes are accepted without requiring a C
string terminator. The existing BUP layout is documented by
[PR #269](https://github.com/tpunix/SAROO/pull/269) and the public
[`bup_format.h`](https://github.com/papsg/SAROO/blob/de8715b/tools/savetool/bup_format.h).
[PR #271](https://github.com/tpunix/SAROO/pull/271) supplies the existing SRO
extension/CRC feature. Their authorship/notices remain; this change corrects
validation and ownership around those existing formats.

Archive dispatch requires complete SAROO 64 KiB units through the 4,095-game
capacity, at least the fixed 8 MiB MEMS region, or exactly 32 KiB plain / 64 KiB
odd-byte Saturn dumps. Odd-byte detection checks all sixteen decoded signature
bytes. Opaque MEMS trailers remain supported and survive mutation/export.
These bounds follow the public
[`sr_bup.c`, `sr_mems.c` and `ss_bup.c` readers](https://github.com/papsg/SAROO/tree/de8715b/tools/savetool)
and existing
[browser admission/preservation behavior](https://github.com/papsg/SAROO/blob/de8715b/tools/savetool/web/src/save-codec.mjs).
Larger cartridge-backup formats remain unsupported by this internal-backup reader.
Complete block-chain/cross-save metadata validation is subsequent SAVETOOL-005
scope; extent checks alone do not establish that an archive is well formed.

The I/O requirements are independently checked against cached primary
[WG14 N1570, C11 committee draft](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf):
§7.21.5.1 (`fclose`, printed p. 304), §7.21.8.1 (`fread`, p. 335),
§7.21.9.2 (`fseek`, pp. 336–337), §7.21.9.4 (`ftell`, pp. 337–338) and
§7.21.10.3 (`ferror`, p. 339). The checks respect nonzero seek/error indicators,
short element counts, failed length queries and disassociation after closing.

### Origin, verification and limits

This is our independently discovered maintenance correction, originally authored
by `paps-g` as `4bbc849a` on 2026-09-05. This public port additionally follows the
eight-byte SRO magic field, complete odd-byte signature and public format limits.
The browser/native control caught an overstrict MEMS extent candidate during
porting; that rejected candidate's failure is preserved in local evidence, and
explicit trailer controls plus all 24 native/browser exports now pass.

Run the public native suite and compatibility controls with the first command in
the preceding section. Five new Python methods exercise malformed and missing
inputs, supported raw formats/checksums, archive extents/signatures,
the full 256 MiB SAROO extent and deterministic errors around the actual C loader.
The suite's incoming-file matrix contains 22 malformed files; the missing-name
case is covered directly at the loader API. Twelve valid format/size combinations
include empty BUPs, high-bit CRCs and nonzero SRO unused bytes. Nineteen malformed
archive cases are rejected without writes. Empty archives and opaque MEMS tails
are preserved; a complete unit above SAROO's maximum is rejected. The actual
loader shim checks fourteen normal/error/maximum-payload controls and ownership.

The four end-to-end CLI methods pass with ASan/UBSan and normal O2. The loader
shim always uses ASan/UBSan, including positive and negative seek failures,
short reads, allocation/close/error-indicator failures and full 1 MiB SRO/BUP
decoding. The baseline fails the new corpus; updated direct-I/O and full-unit
boundary controls also fail independently there. Earlier malformed-output
decoding errors in the test collector are preserved; the collector now retains
replacement characters rather than mistaking binary CLI output for UTF-8.
Existing eight MEMS methods, two unsigned-reader methods and seven web-save
methods pass. The web runtime remains unchanged.

The input suite can target a separately compiled baseline with the existing
`SAROO_SAVETOOL_BINARY` override. Set `SAROO_SAVETOOL_SOURCE` to that baseline's
`tools/savetool` directory too when running the directly included loader tests.
Generated fault controls are confined to the test executable. The release CLI
has no fault-injection switch. No physical-device, power-loss or game acceptance
is claimed; checked output persistence remains SAVETOOL-006 scope.

## SAVETOOL-004: plan new MEMS allocation before mutation

At public `bb64566`, a full directory leaves the destination index at `-1`.
An exhausted bitmap returns block zero, and the inline-save path skips the
free-count guard. A stale high count also bypasses that guard for larger saves.
The resulting operations can report success after changing the bitmap/header or
destroying block zero. An oversized data-block table crosses the header boundary.
A stale low free count incorrectly refuses a save that actually fits.

The new path admits the directory position, nonempty identity, representable
payload size and complete header/data allocation before writing archive bytes.
It plans from the real bitmap, starts at block eight, and preserves allocated
orphans. Payload/header preparation and updated bookkeeping precede publication
of the complete directory entry. Refusal leaves the entire archive unchanged.
The old incremental allocator becomes unused and is removed.

The temporary arrays contain a 1,008-byte bitmap and 960-byte block list,
including the header plus up to 479 data blocks. The 490,496-byte payload maximum
and existing inline representation are unchanged. The reference is the starting
[`sr_mems.c`](https://github.com/papsg/SAROO/blob/bb64566/tools/savetool/sr_mems.c),
specifically `MEMSHEADER`, `get_free_block`, `access_data` and the new-save branch
of `sr_mems_import`. These are SAROO format/allocation rules. Existing SRO/BUP
feature authorship and all source notices remain as described above.

This independent maintenance correction was originally authored by `paps-g` as
`5a000472` on 2026-09-05. The public port revalidates it after the resizing/input
changes and adds explicit fragmented/last-block and BUP boundary controls.

Five new actual-CLI methods cover thirteen successful creations and eleven
refusals. Successes include zero/inline/table/maximal payloads, SRO and BUP,
directory holes/final entry, stale low counts and a complete fragmented
allocation ending at block 8,063. Tests independently check allocation bits,
free counts, table termination, preserved old saves and every byte outside the
new save's permitted extents. Fresh-process exports verify data/date/CRC.
Refusals include full directory/bitmap, oversize, empty name, stale high count,
insufficient space and a bitmap with only reserved blocks free.

The prior public source fails ten scenarios and passes the remaining controls.
All new checks and previous MEMS regressions pass under ASan/UBSan and normal O2.
Run `make -C tests check-native-save check-save-endian check-web-save` for the
coupled controls. To run against the previous implementation, use
`SAROO_SAVETOOL_SOURCE` with a separately extracted `bb64566` source directory.
No private workspace or hardware is required.

This changes in-memory admission/commit ordering. Full metadata ownership checks
and checked on-disk replacement remain the subsequent SAVETOOL-005/006 scope.
No power-loss or physical Saturn qualification is claimed.

## SAVETOOL-005: MEMS metadata and ownership

The preceding public `be30f6b` follows directory/header/table pointers without
admitting the whole archive. Invalid, shared or unallocated blocks can reach
payload access, listing and deletion. Deletion also trusts a stale stored free
count. Unbounded printing and inferred input names can consume bytes outside the
eleven-byte directory identity.

Initialization now checks the declared 8 MiB region, eight reserved blocks,
every occupied directory header, payload size, exact terminated chain and
cross-save allocation ownership before a command can follow those pointers.
Headers are claimed before data, so a payload cannot borrow a later save header.
The single 1,008-byte temporary ownership bitmap bounds this check. Allocated
orphans remain allocated; the bitmap supplies the actual free count locally.
Read-only commands leave stale stored counts and all archive bytes untouched.
Names used as strings are bounded to eleven bytes.

The format source is
[`MEMSHEADER`, `access_data`, `sr_mems_delete` and `sr_mems_init` at be30f6b](https://github.com/papsg/SAROO/blob/be30f6b/tools/savetool/sr_mems.c).
The already-public
[`validateMems` browser contract](https://github.com/papsg/SAROO/blob/be30f6b/tools/savetool/web/src/save-codec.mjs)
independently admits the same block ownership and complete eleven-byte stored
identity. Matching raw bytes after NUL are preserved; divergent stored suffixes
reject the archive. Sega's public backup-name API does not specify this private
MEMS padding, so it is not cited as requiring that identity policy.

This is our maintenance correction originally authored by `paps-g` as
`9e486906` on 2026-09-05, with the necessary complete-name correction `0dce8f0c`
of 2026-10-20 (SSR-102). The public port independently revalidates both. It does
not import the later console validator or private patch framework. Existing
MEMS/SRO/BUP notices and feature attribution described above remain intact.

Five new actual-CLI methods cover 105 whole-archive metadata refusals,
ten deletion/recount controls with orphans and neighbors, a fixed-width padding
control, and 78 name operations: 36 divergent-suffix refusals and 42 equal-field
list/export/import/delete controls. The preceding public source fails 145
subcases. Restoring only `strncmp` in the corrected validator causes all 36
divergent-suffix cases to fail while the equal-field controls still pass.
Refusals preserve every archive/input byte and create no output file; successful
operations retain exact payloads, complete raw names and neighboring records.
Ten read-only stale-count controls also preserve the original complete archive.
All eighteen MEMS methods and five input methods pass under ASan/UBSan and
normal O2; the direct loader shim remains sanitized. Two unsigned-reader and
seven web-save methods pass, including all 24 native/browser exports.

Run `make -C tests check-native-save check-save-endian check-web-save` for the
coupled suite. `SAROO_SAVETOOL_SOURCE` selects an independently extracted prior
source directory; `SAROO_SAVETOOL_BINARY` can select a normal O2 build. This is
host archive admission, not physical Saturn or power-loss qualification. SAROO
slot validation, Saturn-reader correction and checked output persistence remain
the following independent series members.

## SAVETOOL-005: SAROO slots, resizing and creation

At public `f922ed8`, selection checks a directory location before bounding its
index and has no archive extent. A missing slot signature triggers an implicit
in-memory format that a later import can persist over damaged data. Linked
headers can cycle; block sizes and payload bitmaps are not validated. Resizing
has the obsolete-size defect already corrected for MEMS. Creation can reuse a
directory hole but still grow the file, miss duplicates beyond that hole, copy
beyond a short ID or write past a full directory.

The slot reader now receives the complete archive extent. It bounds selected
indices to stored slots 1–4,095, rejects populated directory entries beyond
storage, and validates the selected slot's signature, 64 KiB extent, 128-byte
blocks and reserved allocation. Header links are bounded and unique; payload
bits must have complete, exclusive ownership and match the declared size.
Damaged slots are refused without formatting. Allocated orphans remain reserved;
free counts are recounted locally without changing read-only inputs.

Different-size overwrites plan replacement allocation in two private 64-byte
bitmaps before mutation, keeping the header and list position. New imports
require a nonempty name, representable size and sufficient real free space.
The 510 × 128-byte maximum payload is unchanged. Display/inferred names are
bounded to eleven bytes. Creation pads one-to-sixteen-byte IDs, checks the whole
directory for duplicates, reuses holes without extending storage, and appends
one complete unit only when needed. A full directory refuses before access.
Its internal return value now supplies the complete required archive length.

These are project-defined archive rules, established by
[`BUPHEADER`, `sr_bup_format`, `access_data` and the starting reader](https://github.com/papsg/SAROO/blob/f922ed8/tools/savetool/sr_bup.c)
and independently used by the existing public
[`validateSlot` browser reader](https://github.com/papsg/SAROO/blob/f922ed8/tools/savetool/web/src/save-codec.mjs).
The dispatcher and creation contract are in the starting
[`main.c`](https://github.com/papsg/SAROO/blob/f922ed8/tools/savetool/main.c).
Sega's internal-backup layout does not define these SAROO archive fields. This
port retains the public v0.8 layout; it excludes our later native-backup migration.

Our maintenance correction was originally authored by `paps-g` as `a6d7e31e`
on 2026-09-05. This port independently revalidates it after the preceding series,
retains existing source/format attribution, and adapts the historical tests to
the public source build rather than importing private preparers.

The preceding source fails 117 subcases and reaches five bounded timeouts in
the seven-method historical corpus. All eight public slot methods now pass
with ASan/UBSan and normal O2. Controls cover invalid indices/storage, 100
malformed selected-slot operations, resize growth/shrink/zero/full boundaries,
unchanged allocation refusals, stale counts/orphans, IDs/holes/duplicates/append
and the full 256 MiB directory limit. Additional controls exercise six successful
SRO/BUP creations through the full 65,280-byte payload, allocation at fragmented
blocks 100/510/511, creation from an empty 64 KiB archive, and four unchanged
read-only stale-count inputs. The fragmented stale-count control also fails
independently against the preceding public source. Fresh exports, neighbors,
allocation/free counts, raw metadata and complete refusal identity are checked.

Run the coupled suite above; the standard source/binary overrides also apply.
This is host validation of the selected archive/slot, not physical Saturn or
storage power-loss qualification. The Saturn reader and output persistence are
the following separate contributions.

## SAVETOOL-005: read-only Saturn backup tables and payloads

The preceding public `b1f3ac4` does not validate table pointers, cross-save
ownership or payload capacity. For saves of at most thirty bytes it bypasses
the table altogether, starting the payload two bytes before the required zero
terminator has been consumed. Its two-byte copy also writes past an exact-size
destination when the payload length is odd.

The reader now parses every terminated table, including one-block saves and
tables spanning noncontiguous blocks. It bounds references and table positions,
rejects duplicate/shared ownership, reserves all save headers before payloads,
and proves the payload fits the complete allocation before copying. Bounded byte
extents handle the final odd byte. Listing respects eleven-byte stored names.
Initialization receives the actual extent, decodes exactly 32 KiB plain or
64 KiB odd-byte dumps, and validates the full decoded signature. The earlier
SAVETOOL-003 dispatch bounds remain; larger cartridge formats remain unsupported.
Import, deletion and creation remain unavailable for Saturn-format inputs.

The starting source is
[`BUPINFO`, `load_blist`, `read_data` and `scan_save`](https://github.com/papsg/SAROO/blob/b1f3ac4/tools/savetool/ss_bup.c).
The raw format is independently cross-checked against Slinga's
[`SAT_START_BLOCK_HEADER`, tags and internal-block constants](https://github.com/slinga-homebrew/libslinga/blob/7083be5d5a135a3553bbf9f51432cffb7889ca21/devices/sat/sat.h)
and
[`read_sat_table_from_block` / `read_save_from_sat_table`](https://github.com/slinga-homebrew/libslinga/blob/7083be5d5a135a3553bbf9f51432cffb7889ca21/devices/sat/sat.c).
The latter explicitly skips the terminating zero word before payload access.
These are format references; no libslinga implementation is imported. Cached
reference sources and their MIT notice are preserved in local evidence. The
existing public browser `validateSaturn` is an additional compatibility control.

Our correction was originally authored by `paps-g` as `75ccd91b` on 2026-09-05.
This public port independently revalidates it, uses the checked extent to select
the encoding, and preserves existing BUP/SRO feature authorship and notices.

The five-method historical corpus has 185 scenarios: 100 SRO/BUP exports across
25 lengths and both encodings, two fragmented multi-save cases, 78 malformed
operations, four unsupported extent/signature cases and an exact three-byte
destination harness. The preceding public source fails 113 subcases, including
an ASan heap overwrite in that harness; the four dispatch-limit cases already
pass because of SAVETOOL-003. A sixth public method adds six unchanged-input
controls proving mutation commands remain unsupported. All pass with ASan/UBSan
and normal O2; the directly included exact-size reader stays sanitized in both
runs. Checks retain payload, size, comment, language, date, CRC and source bytes.
The independent fixture reaches the 29,550-byte full-allocation boundary; this
is a raw-layout calculation, not a game's measured save capacity.

Run the coupled suite above. It executes the actual CLI and included reader;
no private toolchain or device is required. These are host-format checks, not
physical Saturn or game qualification. Checked file replacement and derived
paths remain the final separate contribution.

## SAVETOOL-006: checked file replacement and derived paths

At public `b6df5de`, `write_file` truncates the destination before writing it.
Short writes and close errors can leave partial data; archive mutation and all
three exporters ignore the result and can print success. Save-derived names are
also interpreted as host paths, allowing traversal, symlink following and export
over the input archive or a hardlink alias.

The writer now prepares a private temporary in the destination directory, writes
all bytes while handling short/interrupted writes, synchronizes/closes it and
renames it over the destination. It then synchronizes the parent directory.
Checked errors before replacement leave the previous destination intact. A
failure after rename reports that replacement completed and returns failure;
the complete new file is already visible. Mutation and export callers propagate
that result, and success messages follow checked persistence.

Ordinary owner/group/mode are retained. On macOS, metadata copying also retains
ACLs and extended attributes. Immutable/append-only destinations are refused
before staging; the code includes both user and system flag masks. New exports
use private `mkstemp` permissions. Explicit archive symlinks are resolved at load
and their targets replaced while preserving the links. Derived output symlinks,
directories and special files are refused. Input-archive aliases cannot be
overwritten by export. Inputs must be regular files; inferred `.bin` paths do
not follow links or block opening a FIFO. Explicit import paths retain support
for regular-file symlinks and subdirectories.

Derived names preserve ASCII letters/digits, spaces, dots, underscores and
hyphens. Other bytes, including percent, are percent-escaped: `A/B` produces
`A%2FB.SRO`, while literal `A%2FB` produces `A%252FB.SRO`. The same rule applies
to BUP export and inferred `.bin` import, with a buffer covering all eleven
bytes at maximum expansion. Exported metadata retains its original raw name.
This retains existing overwrite behavior for an already chosen export filename;
it does not resolve filesystem case-folding or equal visible-name collisions.

### Sources and attribution

The original behavior/callers are in
[`write_file`, `bup_flush` and archive dispatch at b6df5de](https://github.com/papsg/SAROO/blob/b6df5de/tools/savetool/main.c)
and its three archive modules. Host publication semantics are checked against
Apple's primary manuals:
[`rename(2)`, DESCRIPTION](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/rename.2.html),
[`fsync(2)`, DESCRIPTION](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html),
and
[`copyfile(3)`, DESCRIPTION / COPYFILE_METADATA](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/copyfile.3.html).
Bundled Darwin `open(2)`, `write(2)` and `close(2)` manuals additionally establish
the checked descriptor flags, return values and errors. Cached Apple metadata
implementation evidence corroborates flag propagation; it is not asserted to be
the installed library's exact source. These are host filesystem requirements,
not Sega firmware rules.

This is our maintenance correction originally authored by `paps-g` as
`c8ee25f7`, with the necessary `3151c6f5` macOS protection refinement (SSR-042).
The public port independently revalidates both, retains original SRO/BUP/MEMS
attribution, builds the stage name with bounded copies, preserves an early
creation error across cleanup, and adapts the earlier loader fault fixture to
`fdopen`, including descriptor closure. A whitespace-only correction removes a
misleading-indentation warning in the preceding slot port without changing C
tokens. No private framework, console service or cover-tool writer is imported.

### Verification and limits

Ten actual-CLI methods cover write/create/sync/close/rename/metadata failures,
new-file refusal, short/interrupted completion, post-rename errors, all archive
exporters, escaped paths, aliases/special files, explicit/inferred inputs, long
archive names, archive symlinks and native metadata. The preceding public source
has 218 failed assertions, 24 missing escaped-output errors and six FIFO timeouts
in this corpus. An incomplete initial integration and a failed control setup are
retained separately and excluded from acceptance.

Removing only the protection guard from the corrected writer produces eighteen
failed cleanup assertions and eighteen retained stages, with all originals
intact. Twelve ordinary native metadata controls still pass. The corrected CLI
passes all thirty cases under both sanitizer and optimized profiles, retaining
tested owner/group/mode/xattrs and refusing protected files without stages.
The existing four ACL/archive-link controls also pass. Only generated disposable
files receive user flags; system/privileged flags are compiled but not applied.

All 47 combined native methods pass ASan/UBSan and normal O2. Fault interception,
the exact-size reader and direct loader shims remain separate sanitized test
executables in both profiles; the release binary has no fault-environment hook.
Two unsigned-reader and seven web-save methods pass, including all 24 native/
browser exports. Public reproduction remains
`make -C tests check-native-save check-save-endian check-web-save`.

For the ordinary optimized CLI profile, from the repository root:

```sh
mkdir -p tests/build/savetool
cc -std=gnu11 -O2 -g -Wall -fno-strict-aliasing \
  tools/savetool/main.c tools/savetool/sr_bup.c \
  tools/savetool/ss_bup.c tools/savetool/sr_mems.c \
  -o tests/build/savetool/sst-release
SAROO_SAVETOOL_BINARY="$PWD/tests/build/savetool/sst-release" \
  make -C tests check-native-save
```

This new port is qualified on Apple Silicon macOS as a host utility. No new
Intel/Linux/Windows, physical storage, Saturn or game qualification is claimed.
The non-Apple fallback preserves owner/group/mode, with no ACL/xattr claim.
Atomic replacement changes the inode; other hardlinks retain the old file.
Concurrent writers/metadata changes, arbitrary cleanup failures and termination
are outside this contract. A killed process can leave its private temporary.
There is no rollback after rename or arbitrary power-loss guarantee: Apple's
`fsync` manual explicitly distinguishes host flushing from drive persistence.

All seven requested native safety contributions are now represented here. The
earlier sections retain each checkpoint's then-remaining scope; their following
series members are implemented above. These bounded changes do not claim general
save-tool safety or alter the console firmware's separate storage behavior.
