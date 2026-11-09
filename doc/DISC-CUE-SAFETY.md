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
