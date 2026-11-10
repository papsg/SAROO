# CUE layout and sector-reader safety

Selected paps-g `659fe4ee`, revalidated against public `bd4dc09`. This series
separates reader support, CUE mapping, accepted MODE2 payloads and optional
subcode I/O. The MCU-CDC-016 99-track correction is already public and is not
reapplied. No original upstream PR or named-game audio diagnosis is asserted.
Historical private USB-profile resources are not current build measurements.

## MCU-CDIO-002: extent-bounded reads and coherent reports

The original [reader](https://github.com/papsg/SAROO/blob/bd4dc0914d1de4d9fdf0b6b95189f1f1b63d4fa6/Firm_MCU/Saturn/saturn_main.c)
prefetches across a known track boundary, then changes track metadata while
reusing the previous cache's bytes/width. It reports index 1 on misses and leaves
index/control unchanged on hits and position-only calls. Its signed byte-offset
intermediate sign-extends at the 64-bit FatFs boundary above 2 GiB. A successful
clipped seek can read the wrong bytes. These are independently reproduced with
authored metadata and synthetic data; they do not depend on the CUE mapper
being correct or establish any physical game's symptom.

Sega [ST-162 §2.1 access keys, printed p.24](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=12)
and [§2.2 disk layout, printed p.26](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=14)
define FAD/LSN and stored index/track relationships. [Status/report data No. 4.0(3),
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
index lookup with seek reporting: ordinary index 1-only tracks need no table
read;98 higher indices need at most seven comparisons. Invalid track lookup
still reports ERROR, preserves directory ownership and retains the old logs.
Read failure invalidates the cache and leaves the output block unmodified.

Generated-gap support is a narrow prerequisite for the following CUE mapper.
Reuse one zeroed frame with zero cache stride; distinguish generated gaps from
stored INDEX 00 bytes and bound each cache window. Generated data gaps use the
existing zero user-payload container, not complete synthesized raw sync/ECC/EDC.
No parser gap/mixed-width behavior is claimed fixed by this checkpoint.

Preserve the 32-byte ARM track record by replacing its pointer with an offset in
the existing 2,048-entry index pool. Zero means absent; offsets 0..2048 use values
1..2049, with the unused one-past value never dereferenced for a higher index.
24-bit gap fields and 8-bit mode/index fields fit the old record. Derive
control/ADR from the three already-supported modes; CUE FLAGS remain unsupported.
Parser/TOC/drive consumers adapt together; cartridge commands and wire formats
are unchanged.
The old parser initializes gap fields to zero. Full parser ordering/size checks
remain;99 tracks,100 records and every index entry are preserved.

Static target assertions enforce the 32-byte record and AXI layout:3,200 bytes
for 100 records plus existing two 1,536-byte subcode/ring buffers leave3,072 bytes
in the existing metadata region. No payload buffer, heap, task, SD write, extra
wait or interrupt-mask change is added. Bounds may shorten reads at transitions;
warm hits perform no I/O. All original log formats/masks are preserved.

Five actual-source groups pass ASan/UBSan and O2: six mixed-width combinations
with independent metadata/full payload checks forward, reverse and permuted;
all 100 index values 00..99 with position-only calls; above 2 GiB seek and
clipped-seek refusal; generated gaps/stored pregap bytes; and an unchanged
single-track/warm-cache control. Compiled preceding source fails the three
applicable defect groups (eight cases), passes the valid control and explicitly
skips the new gap-metadata invariant absent from its old representation.
Thirteen compiled behavioral mutants are detected. One signed-local variant is
preserved/excluded as equivalent: the unchanged unsigned helper boundary repairs
its representation before FatFs; it does not reproduce the original defect.
The replacement sign-extension-at-FatFs-boundary mutant is detected.

All 131 existing CD methods pass both profiles and 68 MCU save/discovery/parser/
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
position setter16 and disk 72 bytes; checked I/O is inlined into the reader.
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

## MCU-CUE-002: map stored frames and generated gaps separately

Original: selected paps-g `659fe4ee`, after the reader/metadata prerequisite
above. The preceding parser multiplies a FILE-relative index by the new track's
width. After seven 2,048-byte frames, an AUDIO track should start at byte14,336;
the old calculation chooses16,464. It also shifts the first INDEX 01 away from
FAD 150 when stored prefix frames exist, and ignores PREGAP/POSTGAP entirely.

The original finding references [GNU ccd2cue's CUE command conventions](https://www.gnu.org/software/ccd2cue/manual/ccd2cue.html)
for FILE-relative INDEX times, stored INDEX 00 and unbacked PREGAP/POSTGAP.
These image-format conventions are separate from Sega's interface contract;
no parser code is imported from that reference. The source-only port retains
SAROO's supported nonzero FILE-relative offsets. The cached [ST-162 CdcPos data
No6.4, printed pp.65–66](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=53)
defines disc-start FAD 150 and position exceptions; the disk-layout/access-key
pages cited above show index/track and FAD/LSN relationships. Sega does not
specify CUE text grammar, generated-gap representation or malformed-input policy.

Accumulate bytes using the preceding track's width before beginning the new
track. Separate stored INDEX 00 bytes from unbacked time and include generated
gaps in subsequent track positions and lead-out. Normalize the first INDEX 01 to
FAD 150 while retaining its actual FILE offset; earlier stored/gap time below
FAD 0 stays unexposed. Preserve complete supported24-bit gap lengths, rejecting
unrepresentable sums before bitfield assignment. Enforce gap ordering/uniqueness,
monotonic indices, file extents and the existing24-bit end/lead-out ceiling.
Unknown unrelated metadata remains ignored. Failed parsing closes owned readers
and publishes no tracks; previews preserve the live metadata/index pool.

Six actual parser/TOC/reader groups pass ASan/UBSan and O2: six mixed-width
layouts with complete payload checks forward/reverse/permuted; stored/generated
gaps and multiple files; first stored prefixes 0/2/150/300 frames; thirteen
malformed/overflowing/overlapping-index cases; pool sizes 2,046/2,047/2,048 plus
2,049 refusal and preview preservation; all 99 tracks addressable/readable.
The preceding parser fails24 cases across five groups while the 99-track control
passes. Capacity checks use a virtual file-size boundary and position-only reads;
they prove pool/report ownership, not actual backing for every synthesized frame.
The ordinary payload checks use real generated files and actual public FatFs.
Ten pre-existing parser, six preview/header and five reader groups also pass.
Thirteen compiled behavioral mutants are detected, covering wrong-width offsets,
gap/index ordering, overflow, prefix loss and pool bounds. One initial ambiguous
replacement was rejected before compilation, preserved and excluded.

GNU MCU App/RTX load grows400 bytes:60,248→60,648, leaving4,888 ITCM bytes.
Static DTCM remains60,304; the 100 32-byte track records, all 2,048 indices and AXI
workspace are unchanged. All56 source inputs bind to the receipt; cached SDK
hashes match the reader checkpoint. Direct parser frame 152→160, offset/end
helpers 16/16 and finishing wrapper 8; these can nest and exclude other callees/
exceptions. Reader 64/disk 72 frames remain. Parsing adds bounded per-record
arithmetic/checks and no payload allocation, task, wait, SD write or log/mask
change. These builds/models do not qualify MDK, physical timing or games.

```sh
python3 -B tests/test_cue_layout.py
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B tests/test_cue_layout.py
# Reader checkpoint source directory demonstrates the preceding failures:
SAROO_MCU_SAVE_DIR=/path/to/reader-checkpoint/Saturn python3 -B tests/test_cue_layout.py
```

MODE2 container normalization and optional subcode I/O follow independently.
The same commit's already-public99-track correction is deduplicated; replacing
CUE_TRACKS's literal99 with the shared limit changes no capacity. No source,
installed firmware or physical device in the private workspace is changed.

## MCU-CDIO-003: normalize accepted MODE2 image payloads

Original: selected paps-g `659fe4ee`. The parser already accepts MODE2/2336 and
MODE2/2324, but the reader loses their subheader/form metadata and filter_sector
has no payload-copy branch for either width. Allocating a block can therefore
publish its unrelated previous bytes. Both producer metadata and direct filter
copy defects reproduce independently against the preceding source.

Sega [ST-162 §5.4(1), Figure 5.8, printed p.48](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=36)
defines the XA sector's16-byte sync/header prefix,8-byte repeated subheader,
2,048/2,324-byte user regions and trailer sizes. [Submode constants, printed
p.71](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=59)
define the 0x20 Form 2 bit. Those describe native sector content; the accepted
headerless image widths and zero-filled missing fields are SAROO image-adapter
policy, not a Sega requirement to support those image files.

Extract the 2336-byte image's subheader from its beginning and copy all 2336
bytes at container offset 16 after initializing the missing prefix/mode byte.
For 2324-byte payload-only input, use Form 2 metadata, place all user bytes at
byte 24, initialize both subheaders/form markers and clear the four-byte tail.
Unknown file/channel/coding fields are zero; removed image information cannot
be recovered. Preserve configured Form 1 transfer sizes and existing 2324-byte
Form 2 reporting. This is the existing normalized buffer convention, not a
claim of synthesized canonical raw sync/FAD/ECC/EDC or authentication data.
Raw/cooked MODE1 and 2352-byte/audio behavior remain outside this change.

Two new actual-source groups each pass 96 combinations: reader→filter→routed
copy, and direct filter→copy with independently supplied metadata. They cover
2336-byte Form 1/Form 2 and 2324-byte input, all four configured transfer sizes,
eight patterns, every 2352 container byte, FAD/metadata, selected routing and
block ownership. Free payloads are poisoned before allocation. Both preceding-
source groups fail; six existing routing controls pass. Eight compiled
behavioral mutants are detected. The initial fixture left block 0 zero-filled,
so header/tail-clear omissions survived; those results are preserved/excluded.
The corrected poison-buffer fixture detects both and the other six variants.

All 144 current CD/reader/layout methods pass sanitizers and O2; final poisoned
routing groups also pass both profiles. Existing sector-input metadata fixtures
now seed real 2336-byte subheaders and expect 2324-byte Form 2 flags. The unchanged
payload bytes remain independently checked. No old zero-metadata expectation
for these newly handled formats is asserted preserved.

GNU MCU load grows128 bytes:60,648→60,776, leaving4,760 ITCM bytes. Static DTCM
remains60,304; all 56 input hashes and identical SDK bind to the receipt. Reader 64,
filter 32 and disk 72 direct frames remain unchanged. No buffer, state, allocation,
I/O, wait, log or mask is added. The new branches copy the supplied payload once
and initialize absent header/tail bytes outside the existing brief ownership
masks. Physical timing, game behavior and MDK equivalence remain unqualified.

```sh
python3 -B tests/test_cdc_routing.py
SAROO_CDC_PROFILE=release python3 -B tests/test_cdc_routing.py
# To reproduce both old defects, select the preceding source functions:
SAROO_CDC_ROUTING_SOURCE=/path/to/preceding/saturn_cdc.c \
SAROO_CDC_ROUTING_MAIN=/path/to/preceding/saturn_main.c \
  python3 -B tests/test_cdc_routing.py
```

Optional subcode I/O/mode/lookahead safety remains the last scoped contribution.
No private source, device or installed firmware is changed.

## MCU-CDIO-004: admit complete subcode windows under one mode

Original: selected paps-g `659fe4ee`. The preceding optional reader ignores
sidecar seek/read errors and partial frames, can reuse a sector-only cache after
enabling sidecodes, and can change publication policy during I/O. Its planar
converter consumes 16 input frames but yields 14; publishing a 16-frame window
reads stale conversion tail bytes. These source defects reproduce separately
from the parser and MODE2 payload work.

Sega [ST-162 §7.2.5(2), data No. 5.0, printed p.62](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=50)
defines 96 bytes per frame in four 24-byte R–W packs. [§8.2.3 function 3.2,
printed p.85](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=73)
defines pack retrieval and error/overrun reporting. The fourteen/sixteen-frame
limits come from SAROO's existing converter and 1,536-byte staging allocation,
not a Sega prefetch requirement. The cached FatFs seek contract cited above
requires checking the resulting offset after success. Sidecar origins/formats,
EOF lookahead padding and error admission are image-adapter policy.

Snapshot the selected mode once before cache/I/O work and include it in the
cache identity. Reuse the checked locked read helper for sidecars. Bound planar
publication to 14 frames with two lookahead frames; raw sidecars retain up to 16
frames. Require complete requested output frames and whole 96-byte input frames,
zero absent EOF lookahead, then convert/copy. A subcode-enabled request below
FAD 150 is refused before I/O; preserve the existing FAD 150 to sidecar-byte-0 map.
Only successful sector completion publishes cache bounds/mode and ring data.
If sector I/O fails after valid sidecodes, conversion scratch may change but
output/cache/ring publication does not. Retry must reload the invalid cache.

The entire operation uses the captured mode, including conversion and ring
publication if initialization changes iflag during either read. The next
operation observes the new mode and reloads as needed. This does not import
broader initialization/reset changes or prove every RTOS interleaving.
Preserve the inherited deinterleaver/permutation, ring allocation and overflow
policy; native pack-buffer/read-rate timing and physical CD+G remain unqualified.
No error flag or waveform fidelity beyond this file-I/O contract is asserted.

Five actual-source groups / 16 scenarios pass sanitizers and O2: planar/raw EOF
windows with exact conversion-byte oracles and 14/16-window controls; enabling
sidecodes over a warm sector-only cache; six enable/disable changes during each
I/O; failed/clipped/short/partial-error sidecar reads without ring/output
publication; and both payload-failure-after-valid-sidecodes/retry controls.
The preceding source fails 13 cases across four groups; raw EOF and the payload-
failure controls already pass. Ten compiled behavioral mutants are detected,
including missing lookahead/padding, wrong frame limits, ignored mode identity,
live-mode conversion/publication and short/clipped admission. I/O/preemption and
retry failures are injected models, not physical SD recovery or timing proof.

GNU MCU App/RTX load grows112 bytes:60,776→60,888, leaving4,648 ITCM bytes.
Static DTCM grows60,304→60,312: one cache-mode byte plus linked alignment costs 8
bytes. Track/index/AXI/payload/task-stack allocations remain unchanged. All56
source inputs and identical SDK bind to the checkpoint/final build receipts.
Direct reader frame 64→56, checked-I/O 24 now out of line, converter40, filter 32
and disk 72. The nested I/O path is not a stack saving; physical peaks/cycles are
unmeasured. Ordinary warm hits add a mode comparison and perform no I/O; mode
changes intentionally reload. Planar read windows may be shorter and request
two lookahead frames. No new payload buffer, task, wait, SD write, log or mask.

```sh
python3 -B tests/test_cue_subcode.py
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B tests/test_cue_subcode.py
SAROO_MCU_SAVE_DIR=/path/to/mode2-checkpoint/Saturn python3 -B tests/test_cue_subcode.py
```

All four scoped source contributions are implemented independently. Final full
suite, release CD controls and publication/preservation audits are recorded in
the public progress ledger. Physical Saturn/game/original-MDK qualification and
all broader reset/audio/session/raw-format behavior remain separate. No device
or installed firmware is accessed.
