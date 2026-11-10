# CUE layout and sector-reader safety

Selected paps-g `659fe4ee`, revalidated against public `bd4dc09`. This series
separates reader support, CUE mapping, accepted MODE2 payloads and optional
subcode I/O. The MCU-CDC-01699-track correction is already public and is not
reapplied. No original upstream PR or named-game audio diagnosis is asserted.
Historical private USB-profile resources are not current build measurements.

## MCU-CDIO-002: extent-bounded reads and coherent reports

The original [reader](https://github.com/papsg/SAROO/blob/bd4dc0914d1de4d9fdf0b6b95189f1f1b63d4fa6/Firm_MCU/Saturn/saturn_main.c)
prefetches across a known track boundary, then changes track metadata while
reusing the previous cache's bytes/width. It reports index1 on misses and leaves
index/control unchanged on hits and position-only calls. Its signed byte-offset
intermediate sign-extends at the64-bit FatFs boundary above2GiB. A successful
clipped seek can read the wrong bytes. These are independently reproduced with
authored metadata and synthetic data; they do not depend on the CUE mapper
being correct or establish any physical game's symptom.

Sega [ST-162 §2.1 access keys, printed p.24](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=12)
and [§2.2 disk layout, printed p.26](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=14)
define FAD/LSN and stored index/track relationships. [Status/report data No4.0(3),
printed p.60](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=48)
defines binary track/index and control/ADR reporting. These contracts do not
specify SAROO's file cache or optional-image format policy. The cached
[FatFs f_lseek description](http://elm-chan.org/fsw/ff/doc/lseek.html#description)
requires checking the resulting position after success and describes read-only
EOF clipping. The retained public FatFs implementation and64-bit configuration
corroborate that requirement; no dependency implementation is imported.

Limit each stored read/publication to its extent and use unsigned positions
through the checked locked-I/O helper. Resolve current index/control before
returning a cache hit or position-only request. Share a bounded upper-bound
index lookup with seek reporting: ordinary index1-only tracks need no table
read;98 higher indices need at most seven comparisons. Invalid track lookup
still reports ERROR, preserves directory ownership and retains the old logs.
Read failure invalidates the cache and leaves the output block unmodified.

Generated-gap support is a narrow prerequisite for the following CUE mapper.
Reuse one zeroed frame with zero cache stride; distinguish generated gaps from
stored INDEX00 bytes and bound each cache window. Generated data gaps use the
existing zero user-payload container, not complete synthesized raw sync/ECC/EDC.
No parser gap/mixed-width behavior is claimed fixed by this checkpoint.

Preserve the ARM32-byte track record by replacing its pointer with an offset in
the existing2,048-entry index pool. Zero means absent; offsets0..2048 use values
1..2049, with the unused one-past value never dereferenced for a higher index.
24-bit gap fields and8-bit mode/index fields fit the old record. Derive
control/ADR from the three already-supported modes; CUE FLAGS remain unsupported.
Parser/TOC/drive consumers adapt together; cartridge commands and wire formats
are unchanged.
The old parser initializes gap fields to zero. Full parser ordering/size checks
remain;99 tracks,100 records and every index entry are preserved.

Static target assertions enforce the32-byte record and AXI layout:3,200 bytes
for100 records plus existing two1,536-byte subcode/ring buffers leave3,072 bytes
in the existing metadata region. No payload buffer, heap, task, SD write, extra
wait or interrupt-mask change is added. Bounds may shorten reads at transitions;
warm hits perform no I/O. All original log formats/masks are preserved.

Five actual-source groups pass ASan/UBSan and O2: six mixed-width combinations
with independent metadata/full payload checks forward, reverse and permuted;
all100 index values00..99 with position-only calls; above2GiB seek and
clipped-seek refusal; generated gaps/stored pregap bytes; and an unchanged
single-track/warm-cache control. Compiled preceding source fails the three
applicable defect groups (eight cases), passes the valid control and explicitly
skips the new gap-metadata invariant absent from its old representation.
Thirteen compiled behavioral mutants are detected. One signed-local variant is
preserved/excluded as equivalent: the unchanged unsigned helper boundary repairs
its representation before FatFs; it does not reproduce the original defect.
The replacement sign-extension-at-FatFs-boundary mutant is detected.

All131 existing CD methods pass both profiles and68 MCU save/discovery/parser/
header/cover controls pass sanitizers. Fixtures extract actual track layout and
helpers rather than duplicate obsolete structs. The read fixture now updates
the modeled file position on successful seek and seeds a real two-track count
for its second-track control; those are fixture corrections, not more defects.
The full original combined baseline corpus also compiles: ten groups fail and
the already-correct99-track control passes; remaining scopes are not yet ported.

Complete GNU MCU App/RTX/166-vector load grows272 bytes:59,976→60,248, leaving
5,288 ITCM bytes. Static DTCM falls60,312→60,304 by narrowing bounded cache offset/
stride globals; AXI/pool/task-stack capacities are unchanged. All56 source
inputs bind to the receipt and cached SDK hashes match the preceding build.
Direct reader frame is64, index lookup16, track lookup0, TOC32, parser152,
position setter16 and disk72 bytes; checked I/O is inlined into the reader.
These exclude callees/exceptions and physical stack peaks, cycles or latency.
No claim of byte equivalence to MDK or hardware/game qualification follows.

```sh
python3 -B tests/test_cue_geometry.py
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B tests/test_cue_geometry.py
make -C tests check-cdc-command-buffer
# Complete preceding source directory containing cdimg.c, cdc.h,
# saturn_main.c and saturn_utils.c:
SAROO_MCU_SAVE_DIR=/path/to/preceding/Saturn python3 -B tests/test_cue_geometry.py
```

Reader support precedes MCU-CUE-002 so adding generated gaps never requires an
intermediate firmware to read them as file data. CUE mapping, MODE2/2336/2324
normalization and optional sidecode error/mode/lookahead handling follow as
separate changes. Broader initialization/reset, raw-MODE1, session/tray/scan
and physical audio/CD+G timing remain outside this checkpoint. No device,
installed firmware, private source or other paused work is changed.
