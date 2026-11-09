# Disc and CUE safety ports

This series starts at public `6135642`, after the firmware save-safety ports.
Historical private fixes are independently reproduced against each public
checkpoint. Changes are direct source commits, without USB integration,
additional runtime diagnostics, a patch framework or device operations.

## MCU-DISC-001: keep the active save session during automatic changes

The previous automatic `next_disc` branch calls the ordinary launch loader.
That loader reloads the 64KiB slot buffer even when the new disc has the same
ID. A different ID can select/create a different slot. Both paths can replace
live or pending save bytes while the running Saturn-side service still owns its
continuation. The automatic caller also ignores reset/load failure, and SD reset
always reports success after discarding card identification's result.

`load_disc(index, start_save)` now separates selecting an image from starting a
save session. Ordinary menu and existing shell loads pass one; automatic changes
pass zero. Automatic changes still read the new disc ID and apply its
configuration, but retain slot selection, save bytes, MEMS cache ownership and
pending writes. Directly starting a later disc still selects that disc's own
full-ID slot. No archives are merged, migrated or fingerprinted.

Reset returns the identification result. A failed automatic reset/load unloads
the presented image, enters the existing NODISC state and retains the requested
index. Polling an inserted card does not immediately retry; another removal and
insertion can retry. The existing disc-change notification remains a media
condition notification, not an assertion that loading succeeded. Loader and
preview reject negative/out-of-count indices before indexing and reject a failed
256-byte filename allocation before discovery or image/save mutation.

### Source contracts and attribution

The baseline [automatic/menu callers](https://github.com/papsg/SAROO/blob/6135642/Firm_MCU/Saturn/saturn_main.c),
[loader and preview](https://github.com/papsg/SAROO/blob/6135642/Firm_MCU/Saturn/cdimg.c),
[full-ID slot selection](https://github.com/papsg/SAROO/blob/6135642/Firm_MCU/Saturn/saturn_utils.c)
and [SD reset](https://github.com/papsg/SAROO/blob/6135642/Firm_MCU/Main/sdio_h7.c)
establish this SAROO-specific ownership defect. Sega's
[ST-162-R1-092994, Backup Library §2.3 No. 5, printed p. 45/PDF p. 50](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf)
defines write inputs and success/failure; it does not prescribe SAROO's per-ID
archive mapping or an automatic multidisc save policy. Preserving the running
session is a project contract, not an invented Sega requirement. FatFs
[`f_read`, Description/Return Values](https://elm-chan.org/fsw/ff/doc/read.html)
and [`f_write`, Description/Return Values](https://elm-chan.org/fsw/ff/doc/write.html)
remain the checked I/O contracts used by the existing save backend.

Original correction and fixtures: `paps-g`, `82125993a3c5d2661af86525e8012e64bedc6476`,
2026-09-05. Public adaptations omit the historical `disc-change-error` logger,
reuse public test preparation, and update the existing shell's declaration/call
to preserve its explicit-launch behavior under the new signature. Original
notices and legacy comment bytes remain. No shell feature is added. Later SD
media-session, startup-retirement and command-completion work was inspected and
remains outside this finding; this port does not claim those recovery contracts.

### Reproduction and qualification

Five public methods cover **31 scenarios** using actual loader/ISO/header/
unloader, automatic/menu/reset fragments, Saturn backup APIs, MCU save functions
and public FatFs on synthetic FAT32 files. They test clean/live/pending slot
state, incomplete large MEMS writes, identical/different IDs, explicit launch,
reset/discovery/parser/short-header failures, 100 inserted-card polls before
reinsertion, invalid indices and allocation failure. Pending payload retries
and neighboring records are checked after buffer reload. Baseline fails **25**
scenarios; all 31 pass ASan/UBSan and O2. Six isolated regressions produce
6/2/4/2/9/2 failed cases for automatic save reload, skipped menu selection,
ignored reset/load results and removed index/allocation guards. Valid controls
in those same methods remain useful; these are scenario counts, not bug counts.

All **32 coupled MCU methods / 469 scenarios** pass both profiles. Test-only
address/register/card-detection/scheduling/discovery/configuration/CUE stubs do
not enter firmware. The actual ISO reads use generated headers, never game
images or user saves. SD reset's register sequence is source-executed with
register stubs, not exercised on a controller/card. Host byte-order backup
structures and serialized commands are not a physical endian/bus simulation.

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_disc.py -v
make -C tests check-mcu-save
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-mcu-save
```

`SAROO_MCU_SAVE_DIR` can select an extracted baseline's `Firm_MCU/Saturn`
directory with its sibling `Main` directory. Tests stage public sources and
record unique source/hash/log/result cohorts under ignored `tests/build`.
No private preparer, hardware or mounted card is required.

The complete [GNU MCU App/RTX target check](../tools/README.md#original-mcu-app-compilelink-check)
passes all 166 vectors and memory bounds. Load occupancy grows **84 bytes**,
59,732→59,816; ITCM free becomes **5,720**. BSS/data remain **60,656/504**,
DTCM span 61,160 and main-stack/heap address gap 69,912. Compiler direct frames
are unchanged: loader 312, preview 280, disk task 80, dispatcher 112, reset 8
and shell 248 bytes. No static buffer or heap allocation is added. These are
GNU link/direct-frame figures, not MDK equivalence or transitive/IRQ stack peaks.
The compiler, builder and every qualified input except the five intended MCU
files match the preceding MCU build. Saturn menu inputs remain unchanged.

Measured save traffic per automatic swap becomes zero. The old same-ID path
read 131,072 bytes in two requests; the new-ID path read 65,536 and wrote 65,552
in two write requests, unnecessarily creating a slot. Image/configuration I/O
continues. Eliminated native operations are not a Saturn latency measurement.

This does not restore state after MCU reset/reload, coordinate an old pending
operation with a new menu launch, reopen latched files, identify replacement
media or make partial in-place writes atomic. Card changes are detection stubs
on one mounted synthetic filesystem. Discovery/CUE/header/reader-handoff defects
are subsequent members, not repaired by this test's stubs. No physical storage,
DMA/audio/RTOS timing, concurrency, game or upstream issue resolution is claimed.

## SATURN-MENU-002: bound BIN selection before loading

The old `run_binary` indexes a catalog entry without checking its published
count and formats its name into a 128-byte local buffer. An empty catalog can
retain an old entry; negative/out-of-count selections can read unrelated data.
Names longer than the existing path buffer can overwrite the stack.

The consumer now rejects negative/out-of-count indices before reading their
offset. It rejects a name longer than 116 bytes before formatting: the fixed
`/SAROO/BIN/` prefix occupies 11 bytes, leaving one byte for the terminator.
The maximum valid name, ordinary filename and existing `@address` parsing are
preserved. Rejected inputs return the existing `-1` without a file command.
This consumer prerequisite lands before removal of producer catalog clearing;
the original MCU producer remains unchanged in this checkpoint.

The [baseline consumer](https://github.com/papsg/SAROO/blob/f23a7d4/Firm_Saturn/main.c)
and [catalog producer](https://github.com/papsg/SAROO/blob/f23a7d4/Firm_MCU/Saturn/saturn_utils.c)
define the project-specific count/path contract. WG14
[N1570 §7.21.6.6 ¶2–3, printed p. 326](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
specifies `sprintf` output and its extra terminator; §7.24.6.3 ¶2–3, printed
p. 372 specifies `strlen` excluding that terminator. Native tests use libc's
formatter; the target build links the actual SAROO formatter. No Sega hardware
rule defines this application filename limit.

Original correction/fixtures: `paps-g`, `be30cf116cc733a809b9cbfe2bed38b78772d505`,
2026-09-05. The public port separates this required consumer from discovery and
retains the already-added controller suspension code. Fixture callbacks assert
if execution reaches suspension; its file reader deliberately returns before
payload memory access. No controller helper or execution behavior is added.

All nine actual-function scenarios pass ASan/UBSan and O2; baseline fails six.
They cover empty/stale catalog, negative/equal-to-count/large indices, names of
116/117/255 bytes, ordinary names and address-suffixed names. The three valid
controls reach the file reader with their exact path/address. Omitting index
or length checks produces four/two failures; rejecting the final valid byte
produces one. Existing three menu-text methods pass, and the new target is part
of the ordinary public check:

```sh
make -C tests check-menu-binary check-menu-text
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-menu-binary
```

`SAROO_MENU_BINARY_SOURCE` selects an independently extracted baseline `main.c`.
Unique ignored `tests/build/menu-binary` cohorts retain inputs/hashes/results.
The complete pinned SH-2 build passes entry/font/ELF/body bounds. Code/data adds
**48 bytes** to **78,544**; BSS/stack remain **9,232/16,384**, body margin becomes
**26,912**. The direct `run_binary` frame remains **144 bytes**. Image length
stays **488,751**, SHA-256
`afa0ff5c6b43d71bd2fd87fd658fac7eea7c862681f54331899134da5127f68e`.
Only `main.c` changes among menu build inputs. MCU production inputs remain
unchanged from the preceding full App/RTX link. No new allocation or I/O is
added; valid names incur one length scan before the existing formatting scan.

The guard consumes the producer's offsets and terminated names; it does not
authenticate arbitrary corrupted catalog memory or concurrent generations.
The paired producer's capacity/publication checks follow. Payload transfer,
load-address authorization, binary execution, game/controller and physical
memory/bus timing are outside this native path-selection qualification.

## MCU-DISC-002: checked discovery, paths and catalog publication

The previous directory helpers pass a missing extension to `strcasecmp`, fail
to check allocation and accept partial scans after directory errors. Unbounded
copies can overrun path, offset and shared string storage. The new shared
catalog scanner publishes zero before scanning and the complete count only
after successful reading, closing and sorting. Failure leaves both counts zero.
It checks allocation and directory open/read/close results and retains the first
error. The existing catalog clear is intentionally retained in this correctness
checkpoint; removing that work is evaluated separately.

Discovery skips dotless/hidden/directory entries, keeps case-insensitive CUE
preference and the last ISO when no CUE is found, and clears candidate output
on error. Complete paths must fit the original 256 bytes including NUL; they
are rejected rather than truncated. Shared loader/preview path construction
checks the selected index/category, 32-byte category record and joined capacity
before copying. Sorting modes for game directories and original BIN order are
preserved. Existing catalog listing log text remains; no logger is added.

The project layout remains exactly 2,559 four-byte offsets after the four-byte
count and 133,120 terminated-name bytes beginning at `0x2800`, within `0x23000`
bytes. Both capacities are checked before writing. Exhaustion reports
`FR_NOT_ENOUGH_CORE`. Shorter/failed scans confer no ownership of old entries.
The bounded BIN consumer at `9c3342e` is already installed in public source.

The [baseline layout, producer and helpers](https://github.com/papsg/SAROO/blob/9c3342e/Firm_MCU/Saturn/cdimg.c)
define those application capacities and selection rules; no Sega manual expands
them. Public FatFs R0.15's
[`f_opendir`](https://github.com/papsg/SAROO/blob/9c3342e/Firm_MCU/FatFS/ff.c#L4597),
[`f_closedir`](https://github.com/papsg/SAROO/blob/9c3342e/Firm_MCU/FatFS/ff.c#L4663)
and [`f_readdir`](https://github.com/papsg/SAROO/blob/9c3342e/Firm_MCU/FatFS/ff.c#L4693)
are the actual checked result/ownership contracts. `f_readdir` distinguishes
failure from end-of-directory through its result and filename output.

Original correction/fixtures: `paps-g`, `be30cf116cc733a809b9cbfe2bed38b78772d505`,
2026-09-05. This public adaptation separates the consumer prerequisite and
clear-removal optimization, preserves catalog listing text and retains the
existing clear. Private history contains no later modification of this specific
patch. Public actual-source fixtures replace only physical addresses and fault
seams; no private preparation, framework or runtime diagnostic is imported.

Six methods / **43 scenarios** pass ASan/UBSan and O2. Baseline fails **22**:
the clear-removal-only difference from historical counts is deliberately absent.
Cases cover preference/filtering, path/category limits, allocation/open/read/
close failures, three sort modes, shorter scans, full 2,559-entry ordering in
both directions, one entry beyond capacity and the final string byte. The
close-error seam actually closes first; it does not prove recovery from a
latched physical-card error. Eight isolated controls expose 17 failed cases by
omitting extension/allocation/error/path/capacity/publication checks. All **38
coupled MCU methods / 512 scenarios** pass both profiles, including the actual
disc/save-session integration after the shared path helper changes.

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_discovery.py -v
make -C tests check-mcu-save
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-mcu-save
```

The complete GNU App/RTX link and 166-vector check pass. Load bytes decrease
**212** to **59,604**, with **5,932 ITCM free**. BSS/data/span and stack/heap gap
remain 60,656/504/61,160/69,912. The shared scanner's direct frame is 176 bytes,
versus 120 for old BIN/168 for old directory scans; wrappers tail-call it with
zero direct frame. New path/game helpers use 32/24. Loader/preview frames remain
312/280 and the discovery scanner remains 112. Transitive FatFs/sort/IRQ peak
stack is not measured. Only `cdimg.c` differs among qualified MCU inputs.
Menu inputs remain unchanged from the preceding complete SH-2 link.

Native rescan counters still measure **143,356 cleared bytes per scan**, plus
existing filesystem, name-copy and sort work. No clear-removal speedup is claimed
here. The shared scanner adds no static buffer or heap allocation. It does not
establish concurrent catalog generations, arbitrary-folder discovery, CUE content
parsing, card identity/recovery, physical timing or a named game's compatibility.

### Separate evaluation: omit unused catalog clearing

After `ebb232f`, every live offset/name is rewritten before publication. The
source consumer audit checks `find_disc`'s count-bounded loop, `game_dir`'s
loader/preview bounds, the menu's count-bounded list construction and the newly
bounded `run_binary`. This is sequential producer/consumer ownership, not a
concurrent generation protocol. The otherwise unused catalog tail is therefore
unspecified rather than required to contain zeros. MCU and menu built from this
series belong together; the older unbounded BIN consumer does not meet this
contract.

The public test now connects actual MCU list production to the actual menu BIN
consumer in one synthetic catalog. It checks a valid retained selection, a
shorter scan whose old second entry remains, an I/O-failed scan and an empty
scan. Every invalid selection must stop before file I/O. The retained-clear
version passes all **44 functional scenarios**, providing the control before
the single clear call is removed. The optimized version passes those same 44
scenarios with the operation counter updated to require zero clearing. Source
hashes and both results are preserved separately.

Both profiles pass. Restoring the clear is detected by its byte counter; removing
the consumer bound or publishing partial counts is independently detected by
the joined producer/consumer case. All **39 coupled MCU methods / 513 cases**
pass ASan/UBSan and O2. The test's explicit little-endian count decoder and
serialized callbacks do not simulate physical bus/cache/endian or concurrency.

Measured catalog clearing falls **143,356 bytes→0 per scan**, without changing
directory I/O, live entry capacity/order or name-copy work. This is eliminated
memory work, not a Saturn latency measurement. Complete GNU App/RTX/166-vector
qualification saves another **12 load bytes**, reaching **59,592** with **5,944
ITCM free**. BSS/data/span, the stack/heap gap and individual frames remain
unchanged; no allocation is added. The only changed MCU input is `cdimg.c`.
Menu inputs remain byte-identical to the complete SH-2 build at `9c3342e`.

This is the separately verified optimization from `paps-g`'s original
`be30cf11`, with the preceding source contracts, citations and limitations.
`SAROO_MENU_BINARY_SOURCE` can independently select the joined consumer source
for the deliberate missing-bound control. No diagnostic, runtime negotiation,
new catalog format, device operation or hardware/game acceptance is added.

## MCU-CUE-001: bounded CUE/raw loading and reader cleanup

The old parser trusts whole-file sizes, missing tokens and unbounded track/index
counts. It opens FILE readers before associating them with a track, leaks them
on partial failures and accepts incomplete previews. Raw ISO/PCM setup accepts
sub-sector extents, narrows wide sizes into 32-bit reader metadata and can
overwrite an already-live handle. Unload neither retires all ownership nor
reliably reports close errors.

The parser now checks exact CUE reads/close results, requires a nonempty file
of at most **466,303 bytes** plus NUL and rejects embedded NUL. It validates
required FILE/TRACK/INDEX fields, decimal/time bounds, supported sector modes,
track/index ordering and file extents. Its local tokenizer handles quoted names,
spaces/tabs, CRLF and a UTF-8 BOM without changing the configuration tokenizer.
Unknown metadata remains ignored; this is not a complete CUE-language validator.
Both existing 512-byte path workspaces are checked before copying. Relative
CUE filenames, CUEs at root and quoted leaf names with spaces remain usable.

Files open only after a valid TRACK declaration. Adjacent tracks belonging to
one FILE share its reader, and partial loads close the readers they own before
leaving zero published tracks/subindices. The complete count is published only
after successful parsing/TOC construction. Unload closes each adjacent shared
reader once, handles standalone subcode even without tracks, retires track/
index/playback ownership and reports the first close error. A failed unload
stops a replacement load without overwriting its discovery-format selector.

Raw loaders share checked setup and refuse an existing live disc. They reject
less than one sector or more than `UINT32_MAX` bytes before narrowing into the
existing reader fields. Trailing partial-sector floor behavior is unchanged.
FatFs retains its configured 64-bit size/seek support; it is not narrowed.
Preview checks complete I/O and leaves live metadata untouched. Normalized
header offset/framing and reader pause acknowledgment are later series members.

### Contracts and attribution

The baseline [200-block allocation and 100 track records](https://github.com/papsg/SAROO/blob/33b7b01/Firm_MCU/Saturn/saturn_main.c)
and [32-bit reader offsets, TOC construction and parser](https://github.com/papsg/SAROO/blob/33b7b01/Firm_MCU/Saturn/cdimg.c)
establish the storage contracts. CUE text starts at `0x2400b000` and must end
before block metadata at `0x2400a000 + 200*2352 = 0x2407cd80`. The TOC has track
slots 0–98, followed by three metadata words; `track_fp[99]` is preview scratch.
Thus the parser preserves **99 tracks** and **2,048 extra subindices**. The
unused `MAX_TRACKS 32` declaration is not the allocation bound and is not used
to introduce a 32-track regression.

Sega's [CD Communication Interface User's Manual (CD Part), §7.2.3 and
§7.2.4(1)(a)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p07_11.htm#no4_0)
describes TOC/subcode data; the latter table identifies track numbers 1–99.
This is unpaginated HTML, inspected from the preserved source snapshot. It does
not specify CUE text grammar, SAROO's scratch capacity or filesystem policy.
Those follow the project sources. Public FatFs
[`FIL`/`FSIZE_t`/`f_size`](https://github.com/papsg/SAROO/blob/33b7b01/Firm_MCU/FatFS/ff.h)
and the already cited exact-read/error/close contracts define the I/O boundary.

Original correction and fixtures: `paps-g`,
`60d5df01d827c9f257ac1e2478337a1fa5c41c65`, 2026-09-06. This public port separates
the filesystem shift/seek work, preserves the checked discovery and catalog
policy, and deduplicates the shell signature already fixed in `f23a7d4`.
Original notices and source comment bytes outside the changed functions remain.
No later mixed-width/gap reader implementation or title integration is imported.

### Qualification and costs

Ten public methods / **85 scenarios** pass ASan/UBSan and O2; baseline fails
**78**. These are scenarios, including explicit cleanup contracts, not 78
distinct upstream bugs. Actual parser/raw loader/TOC/unloader/dispatch/FatFs
functions cover valid and malformed input, all retained modes, 99 tracks,
2,048/2,049 indices, exact text/path boundaries, quoted names, short/failed/
partially failed I/O, preserved preview state, repeated unload, subcode and
partial-handle cleanup. Eleven isolated regressions are detected, including
wrong format dispatch, ignored unload error and a one-byte path-limit error.

All **49 coupled MCU methods / 598 cases** pass both profiles. The first full
run exposed a fixture extraction that included the new cleanup helper inside
the discovery-only snippet. That setup failure is preserved and excluded;
the corrected complete runs pass. Register/scheduling/configuration/discovery
seams and synthetic FAT32 files remain test-only. Wide extent boundary tests
inject `FIL` size metadata; they do not claim real >4GiB image reads. Close-error
seams close the real fixture handle first and do not qualify latched-card recovery.

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_cue.py -v
make -C tests check-mcu-save
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-mcu-save
```

Complete GNU App/RTX/166-vector qualification adds **848 load bytes**, reaching
**60,440**, with **5,096 ITCM free**. BSS/data/span and stack/heap address gap
remain 60,656/504/61,160/69,912. Direct CUE frame grows **136→160**; shared raw
setup uses 32 versus old PCM 24/ISO 16, with zero-frame wrappers. Cleanup uses
40, unload stays 16 and the loader stays 312. These individual figures do not
measure transitive/IRQ peaks. Only `cdimg.c` changes among qualified MCU inputs;
menu inputs remain unchanged. No static buffer or heap allocation is added.
The existing CUE workspace now incurs a bounded NUL scan and per-track clearing.
The wide filesystem division helper is still linked; its optimization is separate.

This does not complete mixed sector widths, generated gaps, normalized header
reads, shared CD-workspace concurrency, physical media/error recovery or game
qualification. CUE scratch still aliases CD-block storage. No native/model test
here establishes bus/cache/endian, DMA/audio/RTOS timing or SD durability.

## MCU-FS-001: reject unsafe exFAT geometry shifts

Two mount-time expressions shift a signed `1` by an untrusted boot-sector
byte. Values of 31 or greater can cause undefined behavior before the existing
sector-size/cluster-size checks reject the volume. Bound each exponent before
evaluating its shift and use `1U`. The bound follows this port's 16-bit
`FATFS.csize` representation (1–32,768 sectors) and fixed 512-byte sector
configuration; it is not a statement of all geometry allowed by exFAT.
Neither file offsets nor filesystem configuration are narrowed.

[C11 N1570, §6.5.7 paragraphs 3–4, printed pages 94–95](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
defines the shift-width and signed-result requirements. The local primary
copy was inspected. The existing
[`mount_volume` checks](https://github.com/papsg/SAROO/blob/7fcd2c6/Firm_MCU/FatFS/ff.c#L3458),
[`WORD`, `FSIZE_t` and `csize` declarations](https://github.com/papsg/SAROO/blob/7fcd2c6/Firm_MCU/FatFS/ff.h#L51)
and [sector configuration](https://github.com/papsg/SAROO/blob/7fcd2c6/Firm_MCU/FatFS/ffconf.h#L196)
establish the project limits. Sega does not specify the SD filesystem policy.
This is the geometry portion of `paps-g`'s original
`60d5df01d827c9f257ac1e2478337a1fa5c41c65` (2026-09-06), separated from its seek
optimization. FatFs copyright and permission notices are preserved.

The public fixture compiles actual FatFs, its Unicode support and production
configuration, enabling the formatter only in its isolated test copy. Four
methods cover **19 scenarios**: fifteen mount-header inputs, **327,200**
64-bit comparison controls, an actual **5 GiB + 129-byte** exFAT file with eleven
marker positions, and fragmented FAT32/exFAT files read after remount in three
orders with neighboring contents checked. Baseline produces **ten UBSan
failures**; ASan/UBSan and O2 candidates pass. Removing either guard while
retaining unsigned shifts produces four independent shift-width failures.
The unmodified/512-byte valid headers and file workloads remain valid controls.

All **49 coupled MCU methods / 598 scenarios** pass both profiles. The sparse
8 GiB host image occupies 917,504 physical bytes in the wide-file run. It is an
ordinary generated file under `tests/build`, never a device. The seek formula
is still unchanged at this checkpoint; its comparison tests are controls for
the separately evaluated optimization.

```sh
make -C tests check-fatfs
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-fatfs
make -C tests check-mcu-save
```

The complete GNU App/RTX/166-vector build saves **16 load bytes**, reaching
**60,424** with **5,112 ITCM free**. BSS/data/span and stack/heap address gap
stay 60,656/504/61,160/69,912. Mount/seek direct frames remain 40/56 bytes.
Menu sources are unchanged. There is no added buffer, heap allocation or
runtime diagnostic. This is not complete malformed-volume validation, original
MDK equivalence, physical SD recovery, concurrency or game qualification.

### Separate evaluation: equivalent wide seek comparison

The original `f_lseek` uses two 64-bit divisions to decide whether traversal
can start from the current cluster. Mounted cluster-byte sizes are nonzero
powers of two: the FAT mount path checks the power-of-two sector count and
the guarded exFAT path derives it by shifting. For positive positions,
`floor((ofs-1)/bcs) >= floor((ifptr-1)/bcs)` is equivalent to comparing `ofs-1`
against the current cluster's aligned start. Replace only that condition with:

```c
(ofs - 1) >= ((ifptr - 1) & ~(FSIZE_t)(bcs - 1))
```

The outer `ofs > 0` and inner `ifptr > 0` checks remain. Widening the mask
before complement preserves the upper file-offset bits. The next statement
already calculates this same cluster start. This changes the expression's
cost, not the seek policy, file-size limit, allocation or error behavior.

The proof uses the inspected
[`f_lseek`, mounted geometry and existing alignment expression](https://github.com/papsg/SAROO/blob/2eb30d0/Firm_MCU/FatFS/ff.c#L4516),
with [64-bit `FSIZE_t`](https://github.com/papsg/SAROO/blob/2eb30d0/Firm_MCU/FatFS/ff.h#L72).
This ports the other independently evaluated portion of `paps-g`'s
`60d5df01d827c9f257ac1e2478337a1fa5c41c65`; original FatFs notices remain.
It does not claim a filesystem defect in the preceding division expression.

All **19 filesystem scenarios** pass both profiles before and after the
optimization, including the independent 327,200-comparison division oracle,
actual wide exFAT file, fragmented files and unchanged neighbors. Disk-call
counts are identical: wide exFAT **65 reads / 80 writes**, fragmented exFAT
**124 / 77**, FAT32 **1,326 / 2,165** for the complete fixture workloads.
Three separate controls (narrow mask, missing destination decrement and missing
positive-origin guard) each fail the independent comparison check. The test
extractor accepts these changed conditions so a setup error cannot count as
detection. All **49 coupled MCU methods / 598 scenarios** pass both profiles.

The complete GNU App/RTX/166-vector build saves **848 more load bytes**, reaching
**59,576** with **5,960 ITCM free**. `f_lseek` shrinks 482→454 instruction bytes;
its two wide-division calls and the linked wide-division support disappear.
BSS/data/span, address gap and the 56-byte direct seek frame stay unchanged.
Menu sources stay unchanged. Public commands are the same as above. No physical
seek latency or end-to-end throughput improvement is inferred from code size,
host runtimes, synthetic media or instruction inspection.

## MCU-DISC-003: read the declared track's complete user header

The old ID getter reads from the file's current cursor and assumes every
non-2048 sector has a 16-byte prefix. CUE preview returns the first FILE's
prefix before interpreting its TRACK/INDEX. A stored pregap, displaced cursor
or XA framing can therefore select incorrect header/ID bytes. ISO preview
also accepts short reads and ignores close failures; its positive allocation/
open errors conflict with the cover caller's negative-error convention.

A shared reader now seeks to the declared track byte offset plus its framing
prefix, checks the resulting cursor and requires exactly 256 user bytes. It
skips a seek when already positioned correctly. The getter copies the 16-byte
ID at user-header offset `0x20` only after success; all 17 output bytes remain
unchanged on an error. Failed full-load header reads unload the candidate and
do not select its save session.

| Retained CUE preset | Prefix before user bytes |
| --- | ---: |
| MODE1/2048 | 0 |
| MODE1/2352 | 16 |
| MODE2/2048, MODE2/2324 | 0 |
| MODE2/2336 | 8 |
| MODE2/2352 | 24 |

The Mode1 layout follows [ECMA-130, second edition, §14.1–14.2,
printed page 15](https://www.ecma-international.org/wp-content/uploads/ECMA-130_2nd_edition_june_1996.pdf).
The XA layouts follow [ECMA-168, second edition, Annex C.2.1,
Figures C.1–C.2, printed pages 113–114](https://www.ecma-international.org/wp-content/uploads/ECMA-168_2nd_edition_december_1994.pdf):
12 sync + 4 header + 8 subheader bytes precede XA user data. Both local primary
copies were inspected, including the XA figure pages. These describe sector
layouts; the retained CUE preset meanings follow SAROO's existing parser.
Plain non-XA Mode2 is not automatically detected.

Preview waits for the first valid INDEX 01 and uses stack-local track metadata
and the separate existing `track_fp[99]` reader. It does not publish track/index
state. A union reuses the already-closed CUE reader's storage for that metadata.
Both CUE/ISO preview check complete I/O, close their owned handles and return
negative errors. Callers must discard the preview buffer on error: it is not a
transactional output buffer. These checks follow inspected FatFs
[`f_lseek`, Description](https://elm-chan.org/fsw/ff/doc/lseek.html) and
[`f_read`, Description](https://elm-chan.org/fsw/ff/doc/read.html): a successful
seek can be clipped and a successful read can reach EOF with a short count.
The cover caller and first-track ID convention are
[existing source contracts](https://github.com/papsg/SAROO/blob/ace316a/Firm_MCU/Saturn/saturn_utils.c).

This port credits `paps-g`'s `cb3abf06` implementation and original fixtures.
It separates the reader-pause change and adapts the tests to public FatFs's
**64-bit** `FSIZE_t`. Its final-complete-sector control follows `paps-g`'s
later `8a98f5f6` verification refinement, without importing the later media
classifier or its FAT32-only configuration. Offset addition is wide in this
public configuration. Valid CUE offsets additionally come from a complete
indexed sector within the checked 32-bit reader extent. This is not a general
corrupted-metadata validator or a filesystem-width change.

Six actual parser/TOC/loader/FatFs methods cover **67 scenarios**; the preceding
source fails **46**, including the added final-sector control. ASan/UBSan and O2
pass. Tests compare all 256 preview bytes for each retained preset with/without
INDEX displacement and exercise short/failed/partial reads, seek failure/
clipping, corrupt/beyond-EOF offsets, allocation/open/close errors, unchanged
live state and candidate cleanup. Twelve isolated regressions are detected.
All **55 coupled MCU methods / 665 cases** pass both profiles. The initial
complete runs retained one obsolete positive-preview-allocation expectation;
those failures are preserved and excluded. Corrected complete runs pass.
Runtime fixtures now supply the actual track-offset fields and helper source;
discovery-only extraction stops at the path helper rather than including
unrelated header code. Faults/scheduling/registers remain test-only.

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_header.py -v
make -C tests check-mcu-save
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-mcu-save
```

The complete GNU App/RTX/166-vector build adds **96 load bytes**, reaching
**59,672** with **5,864 ITCM free**. Static RAM/address gap remain unchanged.
Direct CUE frame falls 160→152; shared read/track helper frames are 32/16,
getter stays 24, loader/preview stay 312/280. These are individual compiler
frames, not transitive/IRQ peaks. Menu sources remain unchanged.

First-track selection remains unchanged. No later-data-track search, Sega
signature authentication, fingerprint, save migration, mixed-width CUE geometry,
generated gap support or media-corruption repair is claimed. Physical SD/
reader concurrency, endian/cache, RTOS/audio timing and games remain unqualified.
