# CD read safety and ownership

Starts at public `f9ed8fe`. Port selected **paps-g** contributions `89e644f6`
and `3d2c405e` (2026-09-06), with `ac0aeac1`'s corrected read-count semantics,
as separately verified source changes. Keep original notices and logging.
These self-reported source defects are not attributed to a named game's failure.
Measurements below describe individual checkpoints; models/builds do not qualify
physical Saturn timing, scheduling, concurrency, games or original-MDK firmware.

## MCU-CDC-008: command selectors and sector-information positions

Original: selected paps-g `89e644f6`. Fourteen command guard sites cover fifteen
entry points: set/get range, set/get subheader, get mode/connection, get sector
count, calculate size, sector information, GET/GETDEL, delete, change directory,
read directory and read file. GET/GETDEL share admission. The preceding source
indexes24-entry arrays with a byte that can encode256 values; file reads may
wait for playback before validating that byte. Other selectors already guarded
by the earlier ports remain unchanged.

Sega's [ST-162 CD Communication Interface §8.2.6 functions6.1/6.2, printed p.92](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=80)
specifies24 partitions and equal numbers of selectors/apertures. The same
source's [§8.2.5 functions5.1–5.8](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_15.htm)
defines filter arguments, while [§8.2.8 functions8.1/8.2/8.5](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_18.htm)
defines working selectors for directory/file commands. This is ordinary host CD
command admission, independent of USB and the filesystem worker's later derived
routes. Reject an invalid array selector before state access or playback waits;
an already-active GET/GETDEL retains its existing WAIT precedence.

[§8.2.6 function6.5, printed p.93](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=81)
defines the sector position and the last-sector sentinel, and returns stored FAD
and subheaders. Keep the full16-bit position; the old mask aliases position256
to0 and changes0xffff into255. Resolve the sentinel, validate extent and check
the actual node before reading it. Preserve the cached logical metadata rather
than reparsing cooked data's absent header. No selected transfer length or
stored payload changes in this checkpoint.

Invalid selectors, invalid sector-information positions and inconsistent missing
nodes return existing REJECT status without state mutation. This is a defensive
response policy from the original contribution, not a measured malformed-command
reply on original hardware. The [general range rule, §5.3.4(4)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
specifies WAIT for empty/invalid buffer ranges; existing GET/delete range behavior
is retained here. Exact sector-query error-response differences remain unmeasured.
Valid disconnected outputs, ignored reset arguments, register layouts, returned
interrupt masks and every existing log format/mask remain.

Eight new actual-source methods pass ASan/UBSan and O2. They include3,480 invalid
selector requests across all fifteen commands; all65,536 position words; each
partition's last/empty queries and a short-list check; all8 transfer types' busy
GET precedence;408 valid read/delete tails; all24 filter-register, directory/file
selector and ordinary calculated-size controls. The original source fails three
groups and passes five controls. No fake command implementation or private
preparer supplies those results. FIFO/IRQ/track/wakeup boundaries are host models.

Eighteen compiled mutants are rejected by behavioral assertions or sanitizers,
including removal of each of the14 guard sites, truncation, missing last/null
handling and changed busy precedence. A nineteenth removal of explicit position
extent checking survives on consistent lists because the remaining null guard
also refuses out-of-list positions. That redundant mutant/log is preserved and
is not counted as a detected defect or claimed test success.

The earlier subheader fixture now includes the actual status helper and a small
IRQ model, retaining all byte/state/control oracles. Its three methods also pass
against the preceding public source. All68 current CD methods pass both profiles;
initial fixture-dependency and incomplete SDK-path attempts remain archived.

The full GNU App/RTX/166-vector MCU link adds240 load bytes:58,228→58,468,
leaving7,068 ITCM bytes. Static DTCM remains60,296, with the same required stacks,
pool and workspace. All cached SDK inputs match the preceding receipt. Direct
get-range/get-connection frames grow0→8; sector-info8, common GET48, size24,
file-read32, FIFO24 and IRQ/dispatch24/16 stay unchanged. New invalid-command
status writes use the existing short status critical section; valid-command
payload work and masks are unchanged. No buffer, heap allocation, payload copy,
extra I/O or wait is added. Compiler frames exclude other callees/exception state
and do not measure physical stack peaks or latency.

```sh
python3 tests/test_cdc_bounds.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_bounds.py
make -C tests check-cdc-command-buffer
# Baseline/mutant override: SAROO_CDC_BOUNDS_SOURCE=/path/to/saturn_cdc.c
```

Size semantics009 and output/private-read ownership010/count012 follow separately.
Directory/ISO/XA filesystem011/013 and broader asynchronous copy/move/MPEG
ownership are separate work. This checkpoint includes no device operation,
installed-firmware change, USB integration or added runtime diagnostic.

## MCU-CDC-009: selected read sizes and calculation lifetime

Original: selected paps-g `89e644f6`, requiring008's selector admission.
The preceding calculator sums stored block lengths, independent of the host's
selected format, and accepts incomplete or invalid ranges as successful partial
calculations. A last-sector start is not resolved and reset retains the result.

Sega [ST-162 §8.2.6 functions6.3/6.4, printed p.93](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=81)
defines independent last-position/through-end sentinels, the selected-length
word calculation, retained query result and initial zero.
[§8.2.7 function7.1, printed p.95](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=83)
defines2048/2336/2340/2352-byte selections and2324 bytes for Mode2 Form2 under
the2048 selection. [§5.3.4(4)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
requires WAIT on empty/invalid ranges; [§5.5 Table5.2](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
and [§8.2.1 function1.7](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_11.htm)
distinguish software reset from retained/no-change initialization.

One helper resolves and validates ranges for calculation, GET, delete and
copy/move after selector admission. Refused calculations retain the last result.
Fixed-length formats use a single multiplication. The2048 format walks only the
selected range and uses the same existing header predicate as both actual FIFO
producers. Incomplete variable-length lists yield the24-bit word error, never a
partial success. The cached [Sega header's CDC_ACTSIZ_ERR=0xffffff](https://github.com/johannes-fetz/joengine/blob/master/Compiler/COMMON/SGL_302j/INC/SEGA_CDC.H)
defines that result; this URL identifies the cached header, not a new online
observation or imported dependency. Software reset clears the calculation;
ordinary/no-change initialization preserves it. Startup already initializes it.

The public adaptation keeps the old calculation trace format/mask and argument
values, which the private contribution removed. All other logs/masks remain.
This is size arithmetic, not complete output ownership: calculation's required
disconnection of an existing partition output is the separate010 continuation.
The new helper factors the preceding raw-header Form2 predicate unchanged.
Later raw-Mode1 layout and image-audio type tagging are separate corrections;
audio samples that mimic an XA header are not claimed fixed by this port.

Three added actual-source groups pass ASan/UBSan and O2:405 range/sentinel edges
with state-preserving refusals;800 complete FIFO transfers (counts1–200, all four
lengths, rotating Mode1/Mode2 Form1/Form2); and short-list error plus software/
nonreset/no-change lifetime. Independently derived payload words and byte/word
totals must match actual production functions. Stored lengths and cached tags
deliberately differ from selected format, and Mode1 carries the Form2 bit.
First and later windows rotate through all three formats. The preceding008
source fails all three new groups while passing the eight existing controls.
Existing GET/delete/copy/move and PUT tests remain active; all71 CD methods and
three prior setup/differential methods pass in both final profiles. FIFO draining
is modeled; early/pending completion timing remains012 work.

Fourteen compiled behavioral mutants reject missing sentinels, zero/overlong
ranges, wrong formats/Mode1/Form2 decisions, first/later FIFO length mismatches,
partial-list success, lost retained results and wrong reset lifetime. Original
failed mutation/fixture attempts remain preserved, with compilation failures
excluded from the detected count. Helper dependencies are extracted into the
existing public fixtures; no production function is replaced by a fake.

The complete MCU link saves16 load bytes:58,468→58,452, leaving7,084 ITCM bytes.
Static DTCM remains60,296 with no new state, pool, buffer, allocation, payload
copy or I/O. Direct calculation frame grows24→32; range helper has a12-byte leaf
frame and length helper0. IRQ/dispatch24/16, first FIFO16, FIFO service24,
common GET48, copy/move64 and delete24 stay unchanged. Frame sums exclude other
callees/exceptions. Fixed calculations avoid list traversal; variable format
still traverses the starting offset and selection, and FIFO production now
shares a small helper instead of duplicate predicates. No physical cycle,
latency, stack-peak or game-speed gain is claimed. Masking remains unchanged
apart from existing status handling on newly refused invalid calculations.

```sh
python3 tests/test_cdc_bounds.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_bounds.py
make -C tests check-cdc-command-buffer
```

Output/private-read ownership010 and corrected completion counts012 remain
pending. The wider filesystem, asynchronous ownership and physical qualification
limits above remain; no installed firmware or hardware was accessed.
