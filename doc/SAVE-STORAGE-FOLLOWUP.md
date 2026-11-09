# Save and storage follow-up

These independently revalidated ports follow public `9ff69d6`. They preserve
the original repository layout and completed safety work. Host fixtures and
target builds do not establish physical SD, RTOS, BIOS or game qualification.

## SAVETOOL-007: clear only the exported header

Each native exporter clears 32 KiB of scratch, or 1 MiB for MEMS, before
writing a 64-byte header and payload. The existing validated readers overwrite
every emitted payload byte; the Saturn reader rejects incomplete chains/reads
before publishing output. Clear only `BUP_HEADER_SIZE` (64). The entire 1 MiB
scratch allocation remains; this reduces writes, not static storage.

This is an independently discovered optimization by `paps-g`, originally
`99127a6ad2e12128a716d05cbc83a08f913b83eb` (2026-09-06). This public port separates
clearing from its discarded-CRC optimization. Existing source/dependency
notices and format authorship remain intact. The existing BUP contribution by
Damián Parrino / Bucanero in [PR #269](https://github.com/tpunix/SAROO/pull/269),
and SRO extension/CRC contribution in [PR #271](https://github.com/tpunix/SAROO/pull/271),
are retained rather than reimplemented. The inspected cached
[issue #270 format clarification](https://github.com/tpunix/SAROO/issues/270#issuecomment-2615753729)
places the big-endian SRO CRC at `0x0c`, covering bytes `0x10` through EOF.

The actual [three exporters and validated readers](https://github.com/papsg/SAROO/tree/9ff69d6/tools/savetool)
and [`bup_format.h`](https://github.com/papsg/SAROO/blob/9ff69d6/tools/savetool/bup_format.h)
establish the project-defined formats and 64-byte header bound. Sega does not
define SAROO's host scratch-clearing policy. This change preserves complete
SRO/BUP headers, payloads, filenames, reserved bytes, retained CRCs, archive
contents and the already checked output-publication path.

Three public methods exercise **100 poisoned-buffer exports** and **eight BUP
import/re-export scenarios**. Generated MEMS, SAROO slot, plain Saturn and
odd-byte Saturn archives cover 25 layout/size combinations, both formats,
zero length, inline/block/table boundaries and maximum supported payloads.
Every complete file is compared with independently assembled Python bytes;
SRO CRCs use Python zlib. Changed import metadata and both re-exports are
checked, and exports leave the complete archive unchanged.

The fixture stages actual public C. It replaces only each export's scratch
clear with a wrapper that poisons the entire scratch allocation with `0xa5`,
then performs and counts the requested clear. A wrapper counts actual CRC
calls/bytes; the original CRC implementation still runs. These hooks exist
only in test builds. Input/staged source, hashes, compiler logs and results
remain in distinct `tests/build/export-work` cohorts. The shipped CLI gains
no diagnostics or test switches.

The original exports pass the byte/round-trip controls but fail all **50
reduced-clearing assertions**. ASan/UBSan and normal O2 candidates pass all
three methods. This demonstrates unnecessary work, not a baseline output
corruption defect. Deliberate full-clear restoration, incomplete header
clearing and an omitted payload byte are detected independently.
All **50 native save methods** (thirteen MEMS and 37 save-tool methods) pass
with the sanitizer CLI and with the uninstrumented O2 CLI override. Existing
direct fault/reader helpers retain their sanitizer builds in either run.

```sh
python3 -B -m unittest discover -s tests -p test_savetool_export_work.py -v
SAROO_SAVETOOL_PROFILE=release python3 -B -m unittest discover -s tests -p test_savetool_export_work.py -v
make -C tests check-native-save
```

Uninstrumented ARM64 O2 text grows **15,592→15,612 bytes**; zero-fill storage
stays **1,082,580 bytes**. Native executable padding is not code size. Exporter
direct frames stay 224/208/224 bytes for SAROO/MEMS/Saturn; all firmware inputs
remain unchanged. These individual frames exclude callees and stack peaks. There is no new
buffer or allocation. This checkpoint still computes the original CRC for
both formats; skipping the discarded BUP CRC is a separate evaluation.
No timing, physical-device, power-loss or game improvement is inferred.

### Separate evaluation: skip only the discarded BUP CRC

All three exporters build an SRO header and calculate its CRC before the BUP
branch replaces the entire 64-byte header. BUP has no field retaining that
CRC. Skip it only for `exp_type == 1`, keeping the existing SRO calculation
over metadata and payload. Format selection, output bytes, imports, archive
ownership and checked publication remain unchanged. This ports the other
independently evaluated part of `paps-g`'s `99127a6a`; the format/source/request
citations and original notices above apply.

The same 100 poisoned exports and eight import/re-export scenarios pass
ASan/UBSan and O2. The preceding checkpoint passes complete-byte controls but
fails **25 BUP work assertions**, while every SRO work control passes. Across
the workloads, BUP CRC calls/bytes fall **one / N+48 → zero / zero**, and SRO
remains **one / N+48**. At the 490,496-byte MEMS payload limit this avoids
scanning 490,544 bytes. Restoring the redundant CRC is detected by all 25 BUP
work controls; omitting the retained SRO CRC fails 25 independent complete-file
comparisons. This is measured work removal, not a claim of previously corrupt
BUP exports.

The complete 50-method native suite passes with sanitizer and O2 CLIs; existing
direct fault helpers retain their sanitizer builds. Uninstrumented ARM64 O2
text adds **20 more bytes**, reaching **15,632**. Zero-fill storage remains
1,082,580 bytes and all comparable direct frames remain unchanged. Firmware
inputs are unchanged. Public reproduction commands are the same as above;
no physical or end-to-end timing improvement is claimed.

## MCU-SD-001: require ownership before SD transfers

The old disk glue rejects only `osErrorTimeout` from semaphore acquisition.
Other failures enter the SD driver and release a token the caller never
acquired. It also reports successful initialization after semaphore allocation
failure and allocates/replaces its only handle on every initialization call.
An allocation failure makes the invalid-handle path reachable without owning
the semaphore; inspected RTX code returns `osErrorParameter` for a null handle.

Require `osOK` before driver access/release, expose `STA_NOINIT` while the
semaphore is unavailable, and create it only when needed. Allocation can be
retried; successful repeated initialization reuses the same object. The four
transfer-attempt maximum and exactly one release after successful acquisition
remain. No driver, SD retry timing, filesystem configuration or USB feature
is changed.

Original correction and fixture: `paps-g`,
`2ba4e96b2f779770bd4f9100d268f8f8e6db4196` (2026-09-06). This port adds an
uninitialized-I/O control and self-contained native test dependencies. The
baseline [disk glue](https://github.com/papsg/SAROO/blob/b5ae0bd/Firm_MCU/FatFS/diskio.c),
FatFs [`STA_NOINIT`/disk API](https://github.com/papsg/SAROO/blob/b5ae0bd/Firm_MCU/FatFS/diskio.h),
and inspected [Arm CMSIS-RTOS2 Semaphore Management API](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__SemaphoreMgmt.html)
establish the contracts. `osSemaphoreNew` returns an ID or NULL on error;
acquisition success is `osOK`. This is SD/RTOS glue policy, not a Sega CD rule.
Original ChaN notices remain. The unmodified V2.1.3 CMSIS declaration header
and Apache-2.0 license are vendored **for native tests only**, with provenance
and hash in `tests/diskio-stubs/cmsis/README.md`; firmware SDK paths remain
unchanged.

Four public methods / **18 scenarios** compile actual public `diskio.c` with
checked semaphore/SD stubs. Baseline fails **nine**; ASan/UBSan and O2 candidates
pass. Coverage includes status/allocation failure and retry, 1,000 initializations,
uninitialized I/O, all six defined CMSIS error values, unchanged buffers on
refusal, valid reads/writes and zero/one/three/four failed transfer attempts.
The error injection covers the interface defensively; it does not assert every
enum status is reachable from this RTX acquire implementation. Seven isolated
admission/status/reuse/retry/release regressions are detected. The repeated-init
fixture requests **1,000 objects before, one after**; this is a call count,
not a measured RTOS heap saving or speedup.

```sh
make -C tests check-mcu-diskio
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-mcu-diskio
```

The complete GNU App/RTX/166-vector qualification adds **32 load bytes**,
reaching **59,816**, with **5,720 ITCM free**. BSS/data/span/address gap remain
60,656/504/61,160/69,912. Disk status/init/read/write direct frames remain
0/8/32/32 bytes; these exclude transitive/IRQ peaks. Menu production inputs
remain unchanged. Full receipts and source hashes stay in the public build
cohort; no original MDK equivalence or installable-image claim is made.

This checks semaphore availability, not electrical card presence or media
identity. Mount/initialization still require serialization; concurrent remount,
automatic recovery, DMA timing, physical SD durability and game behavior remain
unqualified. It does not establish the cause of historical lost-save reports.

## SATURN-SAVE-004: bounded calendar conversion

The old packer adds an extra day: its decoded epoch, 1980-01-01, packs as
1,440 rather than zero. Its decoder admits a January 32 boundary, disagrees
with the packer on leap years and leaves weekday untouched. Invalid months
can index beyond the table; a sufficiently large packed value triggers signed
overflow. The correction is limited to SAROO's date conversion functions.

Sega's [Saturn System Library User's Guide, ST-162-R1-092994,
§2.1 BupDate, printed page 41, and §2.4 function specifications 10–11,
printed page 48](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf)
defines six byte fields, year relative to 1980, calendar/time field ranges,
Sunday-zero weekday, a 32-bit packed result and no unpacking status return.
The relevant original PDF pages were rendered and inspected; source SHA-256 is
`18978fd7e2e6d2512bd22009fbcb4799bc437b77c85ada53489840063867f966`.
The manual does not specify complete packed arithmetic, Gregorian century
exceptions or invalid-input sentinels. The minute epoch follows the existing
[SAROO source convention](https://github.com/papsg/SAROO/blob/f2b44e7/Firm_Saturn/game_save.c#L1096),
also reflected in the independently inspected historical implementations below.

### Compatibility decisions

- Use a consistent Gregorian calendar for the byte-sized year range,
  **1980–2235**, including non-leap 2100/2200. Packing/unpacking agree and
  weekday is derived; packing ignores weekday without modifying input.
- Valid packed minutes are **0 through `0x08067cbf`**, inclusive. Invalid
  calendar fields or a null pack pointer return `0xffffffff`; out-of-range
  unpacking produces six zero fields, and null output is a no-op. These are
  explicit local policies where the ABI has no specified error channel.
- Keep system-backup delegation unchanged. Do not migrate or rewrite stored
  dates, alter archive I/O or guess which existing dates a game intended.

These policies preserve valid calendar interpretation rather than reproducing
undocumented BIOS quirks. The inspected pinned
[Save-Game-BUP-Scripts / Pseudo Saturn Kai date investigation](https://github.com/slinga-homebrew/Save-Game-BUP-Scripts/blob/b7fb4ea94ff3e58ca0111f88bed0b9d2969069b0/bup_header.h)
records December 32 after a leap year and invalid-input behavior; it explicitly
separates correct dates from bug-for-bug emulation. The inspected
[Yabause HLE implementation](https://github.com/Yabause/yabause/blob/3dc729eee541971987fce4d16a413766b23ac0ed/yabause/src/bios.c)
also has inconsistent calendar handling. Neither was run against a Saturn BIOS,
used as the regression oracle or copied into production. Games depending on
those quirks or consuming invalid sentinels remain a compatibility limit.

Original independent correction/fixtures: `paps-g`,
`f5ed0ae19e29d653d1e9f966a7e2400a9d993cb5` (2026-09-06). This port revalidates
the algorithm and policy, adds a full weekday-input sweep and separates genuine
unchanged controls from the failing native path. Original source notices remain.
The constant 13-entry cumulative-month table replaces mutable month lengths.
Packing indexes that table; unpacking corrects a bounded year estimate once
and finds its month in at most four comparisons. No heap or storage I/O is added.

### Verification and costs

Nine public actual-module methods pass ASan/UBSan and O2. Python's independent
`datetime` calendar generates **311,712 explicit big-endian fixture records**:
three time choices on every one of 93,502 days, every minute on eleven
leap/month/year/century boundaries, 15,360 valid/invalid field-byte combinations
and six out-of-range timestamps. An additional **256 weekday inputs** prove
packing ignores that field without changing input; nulls are checked directly.
Seven previously correct epoch/day/time controls and separate system-delegation
checks pass on both old and new sources. The baseline fails **seven of nine
methods**, including the native-wrapper path, with calendar/weekday mismatches,
out-of-bounds access, null dereference and signed overflow. These are scenarios
and compatibility policies, not seven independent upstream defects.

Eight deliberate arithmetic/boundary/weekday/bounds/null/delegation regressions
are detected. The complete **31-method firmware save suite** passes both
profiles. MMIO, storage commands and system-library calls are test-only seams;
the actual date functions and wrappers are compiled from the public module.
All generated sources, input hashes, records and results remain in separate
`tests/build/save-date` cohorts. Initial seven-method runs remain preserved;
the final nine-method runs explicitly include both baseline valid controls.

```sh
python3 -B -m unittest discover -s tests -p test_firmware_save_date.py -v
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B -m unittest discover -s tests -p test_firmware_save_date.py -v
make -C tests check-firmware-save
```

The complete SH-2 menu build adds **276 code/data bytes**, reaching **78,820**;
BSS stays 9,232, reserved stack stays 16,384, and body margin becomes **26,636**.
The complete padded menu remains 488,751 bytes with identical font bytes.
Decoder/encoder direct frames grow **24→40 / 20→36**; the year helper uses zero
direct frame. These exclude callees/interrupts. MCU production inputs remain
unchanged from `f2b44e7`. Fewer table steps do not establish faster execution,
and native calendar/target-link checks are not physical BIOS/game acceptance.

## Completed follow-up qualification

All three finding IDs are integrated as four focused source changes, separating
the two export optimizations. The complete offline public check passes
**184 Python methods, fourteen Rust unit tests, formatting, strict clippy and
authored C checks**. Native save checks also pass with the release CLI;
31 firmware save and 59 coupled MCU methods pass both profiles, with the
separate 18-case actual-disk-I/O boundary check passing both profiles.

From starting public `9ff69d6`, native save-tool text grows **40 bytes** with
unchanged static storage/direct frames; MCU load grows **32 bytes** with
unchanged static RAM/direct disk frames; menu code/data grows **276 bytes**
with unchanged BSS/stack reservation and the direct date-frame costs above.
Complete menu and GNU App/RTX/166-vector builds have matching current source
hashes. No historical hardware result is reused as qualification.

Public source/tests/commands/citations and attribution remain in this document
and the progress ledger on `codex/pr-progress`. Baseline/candidate inputs,
failed controls/setup evidence, hashes and acceptance records stay under
`.git/porting/evidence/save-storage-followup/`; distinct test/build cohorts stay
under `tests/build/`. No user saves, private files/refs/stashes/caches, installed
firmware, Saturn or other paused work were modified. No PR, USB integration,
runtime diagnostic, private patch framework or unrelated change is included.
