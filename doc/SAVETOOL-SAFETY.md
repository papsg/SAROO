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
