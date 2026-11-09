# Native cover utility safety — COVERTOOL-001

This is a direct source port after `87ddb00`. The existing browser editor stays
unchanged. The original native tool is Bucanero's GPL-3.0-or-later contribution
in [PR #321](https://github.com/tpunix/SAROO/pull/321), improved in
[PR #332](https://github.com/tpunix/SAROO/pull/332), following
[issue #215](https://github.com/tpunix/SAROO/issues/215). Those features are
already in the v0.8 baseline. These additional safety corrections are **paps-g**'s
`2231f4b5ad9cd29e43aedabde77f221e09d58252` (2026-09-06), independently revalidated
here. The narrow protected-destination refinement from
`3151c6f54175f0493a3b168520cca3ca53a39031` is included because the original
staging implementation can copy flags that prevent removal of its temporary.
Original GPL notices remain intact. No external SDK/dependency is added.

## Build, format and supported inputs

```sh
make -C tools/covertool
# Run in the intended output directory:
/absolute/path/to/covertool -p coverlist.txt
/absolute/path/to/covertool -u /path/to/cover.bin
/absolute/path/to/covertool -i /path/to/disc.bin
```

The host format retains a 16,384×32-byte (512 KiB) index. Each record has a fixed
12-byte serial, 32-bit IP checksum/offset and 16-bit dimensions, explicitly
encoded little-endian. Payloads contain 1,024 palette bytes and width×height
indexed pixels. Fields and the identity rule originate in SAROO's
[baseline native structures and hash function](https://github.com/papsg/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/tools/covertool/main.c).
The serial plus Adler-32 of 64 IP bytes starting at `0x30` identifies a cover;
the disc's filename/folder does not. This is artwork identity, not a complete
disc fingerprint. The MCU's separate first-2,048-record lookup limit remains;
this host port does not remove it or autodetect old 64 KiB-only indexes.

The supported image domain is uncompressed 8-bit indexed Windows BMP, a 256-entry
palette, width 1–128 and height 1–192, with DIB sizes 40/52/56/108/124. Validate
signature, planes, dimensions, palette count, compression and declared/actual
extents. Normalize top-down/bottom-up rows, four-byte row padding and gaps before
pixels; export a 40-byte DIB with zeroed row padding. This is a supported subset,
not a claim that other BMP variants are invalid. The BMP field reference is
[Microsoft's BITMAPINFOHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader);
the port uses local source/byte fixtures and does not claim a fresh online
review or Windows qualification. Independent encoders/oracles exercise all
accepted layouts and complete palette/pixel bytes.

List entries may include paths/spaces; identity is decoded from the basename
`cover_SERIAL~12345678.bmp`. Empty lists create an empty index. Blank entries,
overlong/embedded-NUL lines, invalid names and duplicate identities fail.
Serials contain 1–12 nonzero bytes. Exports escape unsafe bytes and lowercase
ASCII as `%HH`, retaining distinct names on case-insensitive filesystems.
Packing decodes that spelling. Each hash match additionally requires matching
payload length and complete palette/pixel equality. Different dimensions can
share equal payload bytes when their byte counts agree; record dimensions remain
separate. Deliberately colliding hashes can still force quadratic comparisons
within the fixed 16,384-record limit.

`-i` retains the existing cooked/raw-at-offset-16 heuristic and serial trimming.
Truncated/invalid input fails; it does not parse CUEs or discover data tracks.
Input paths must resolve to regular files. Explicit symlinks to regular inputs
remain supported; nonblocking opens reject FIFOs without hanging.

## Preserve output and report incomplete operations

Previously packing truncated `cover.bin` before checking inputs; both directions
ignored several I/O/allocation/close failures. Extraction trusted unbounded
serial strings and unchecked dimensions/offsets. Padding/orientation could
scramble valid images, and a pixel checksum alone could reuse the wrong palette
or colliding payload. Public baseline reproductions confirm these defects;
they do not diagnose any particular upstream user's missing cover.

Packing builds a checked temporary beside `cover.bin`, verifies all inputs and
identities, closes the input list, then publishes the replacement. Extraction
validates the whole active index first, stages each complete BMP and publishes
`coverlist.txt` last. Per-file sync, close, rename and directory sync are checked.
Short/interrupted transfers resume; zero/error transfers fail. A post-rename
sync failure reports that replacement already happened. Earlier complete images
can remain after a later extraction or source-close error; the old manifest
remains. This is a per-file transaction, not a whole-directory rollback.

Existing output symlinks, special files, directories and aliases of the active
source are refused. New files use mode 0600. Replacements retain owner/group
and macOS metadata; the non-macOS path retains permissions. On macOS, refuse
user/system immutable or append-only destinations **before** creating a temporary.
Ordinary flags, extended attributes and permissions are preserved. Removing
just that early check reproduces nine abandoned protected temporaries in the
native filesystem controls. These tests touch only their own disposable files.

Contracts were checked against local Xcode SDK/manual sources: `pread(2)` /
`pwrite(2)` DESCRIPTION, RETURN VALUES and ERRORS for counts/interruptions;
`rename(2)` DESCRIPTION for replacement;
[`copyfile(3)` DESCRIPTION, COPYFILE_STAT / COPYFILE_METADATA](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/copyfile.3.html),
and [`chflags(2)` DESCRIPTION, UF_IMMUTABLE / UF_APPEND](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/chflags.2.html)
for metadata/protection. Local `fsync(2)` DESCRIPTION explicitly limits physical
persistence: this port does not add `F_FULLFSYNC` or establish power-loss/media
qualification. WG14 [N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
§7.21.7.1 p3/footnote289 (printed330) and §7.21.10.3 (339) distinguish list EOF
from read errors; §7.24.4.1 (365) defines the complete byte comparison used after
hash matches. These C and system-call references were inspected locally, not fetched during
this port.

Concurrent input changes are not a coherent snapshot; concurrent destination
replacement is not protected by a directory ownership protocol. Ordinary errors
clean up staged files; abrupt process termination can leave unreferenced
staging. Native acceptance is Apple Silicon macOS. The source requires POSIX
interfaces; Linux fallback and Windows portability are not qualified here.

## Reproduce the checks

```sh
make -C tests check-native-cover check-web-cover
SAROO_COVERTOOL_PROFILE=release make -C tests check-native-cover
# Retained artifacts include copied inputs, hashes, compile commands and results:
# tests/build/native-cover/{cli,faults}-*/
```

Seventeen focused methods pass ASan/UBSan and O2. They cover **102 functional/
metadata invocations** and **39 injected I/O/allocation/publication cases**.
Inputs include all accepted DIBs, padding/orientation/gaps, maximum image size,
full/overcapacity, 12-byte and escaped serials, malformed extents and deliberate
pixel/palette/different-length hash collisions. Faults cover short/interrupted/
zero/error transfers, staging, allocation, metadata, close, rename and file/
directory synchronization. The extra final source-close control verifies the
old manifest survives even after a complete BMP was published. Test-accounted
heap is released on every injected path. Native metadata controls include
nine protected and six ordinary destinations.

Ten nonblocking functional methods run against the unchanged baseline produce
53 failed assertions and two filename errors; no FIFO timeout is needed to
establish the defects. These are failed checks, not distinct defect counts.
The full-size baseline's verbose output and all failures remain archived.
Ignoring complete byte equality fails the collision control (four distinct
payloads collapse to two). Removing only the protected-file check fails all
nine protected metadata controls. The current source passes both.

The existing browser codec/worker/linked-export checks pass, including **seven**
browser→native→browser-compatible archive/BMP round trips with padded widths.
The initial expanded interoperability assertion exposed the fixture's deliberate
`0xDA` padding expectation; the final oracle explicitly expects zeroed output
padding while retaining the exact pixel/palette comparison. No browser product
source or asset was changed. Firmware/menu artifacts remain those recorded in
[COVER-ARTWORK-SAFETY.md](COVER-ARTWORK-SAFETY.md).

## Resource accounting

Uninstrumented native `-O2` builds, same compiler and flags:

| Application resource | Public baseline | Port |
| --- | ---: | ---: |
| Executable file | 34,600 B | 35,464 B |
| `__text` | 3,044 B | 8,364 B |
| Large zero-initialized static workspace | 65,536 B | 25,738 B |
| Compiler direct `main` stack frame | 524,688 B | 8,624 B |
| Requested heap peak, 256 unique 128×128 | 16,384 B | 6,144 B |
| Application allocations, that workload | 256 | 8 |
| Requested heap peak, 256 identical 128×128 | 16,384 B | 6,144 B |
| Application allocations, that workload | 256 | 5 |
| Requested heap peak, full 16,384 identical 1×1 | 1 B | 393,216 B |
| Application allocations, that workload | 16,384 | 11 |

The index lives in the staged file rather than a 512 KiB stack object. Growable
8-byte dictionary slots reference staged records; image and identity dictionaries
have disjoint lifetimes. Maximum capacity is 32,768 slots (262,144 bytes); growth
briefly retains old/new arrays, giving the 393,216-byte requested peak. Stack
frames exclude callees and heap counters exclude libc; RSS is not measured.
All three measured workloads produce independently expected, byte-identical
archives before/after. The full tiny-image case uses more heap, not less.

The normal BMP path reads a bounded prefix once and rearranges rows within the
25,738-byte buffer; large gaps use a checked offset read. Repeated payloads now
cost complete comparisons. On the full tiny-image case, the instrumented new
path makes 65,534 checked offset reads / 35,568,607 bytes and 16,385 checked writes /
525,313 bytes, excluding other system calls and index truncation. Old stdio I/O
is not instrumented, so these are not before/after syscall counts. No speedup,
physical-card latency, artwork-on-Saturn or game qualification is claimed.

The final integrated public suite passes **212 Python methods**, authored C
checks, **14 Rust tests**, Rust formatting and clippy. Current source hashes
match the complete MCU/menu qualification artifacts from this series. No Saturn
or installed firmware is touched.
