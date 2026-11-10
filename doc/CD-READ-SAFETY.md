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

Output/private-read ownership010 and corrected completion counts012 follow
separately. The wider filesystem, asynchronous ownership and physical qualification
limits above remain; no installed firmware or hardware was accessed.

## MCU-CDC-010: stable read ownership and partition-output cancellation

Original: selected paps-g `3d2c405e`, requiring008/009. GET borrows a public
partition's linked blocks while delete/reset may free them. GETDEL formerly
removed saved positions at DataEnd even if intervening mutations changed which
sectors occupied them. Partition release also published an old head while
freeing blocks and fetched each next pointer only after making the block reusable.

Sega's [CD interface §8.2.7 introduction and functions7.2–7.4](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm)
defines connector takeover/error and GET/erase/GETDEL. [ST-162 printed p.96](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=84)
requires deleting the complete selected range even if the host fetches less.
[§8.2.6 function6.3, printed p.93](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=81)
also takes the partition output for size calculation. [§5.3.4(1)](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p05_10.htm)
defines stream errors after disconnection. Exact original-drive event timing is
not inferred from these command requirements or host callback models.

Before readiness, GETDEL validates and detaches the complete selection into a
private list using existing next pointers. Its blocks stay allocated until
DataEnd or whole-CD reset, independently of later public appends/deletions.
The existing head byte stores a pool index and read progress becomes count-
relative; pointer/count fields hold the selection with no extra state/table.
Header comments now describe these meanings without changing layout or capacity.
Ordinary GET keeps its public chain until a conflicting valid delete, removal,
clear/reset or calculation cancels its output. Both read types stop further
FIFO service and return an error after takeover; GETDEL retains its private
blocks until cleanup. Unrelated partitions and condition resets preserve reads.
Invalid requests do not steal output ownership.

Partition clear detaches its public head and clears metadata under a short mask,
then releases the old list outside it. Release captures next before making the
current block reusable. New appends after detachment survive. Delete similarly
detaches its exact range once before release. Detached GETDEL cleanup uses its
original private chain, including untransferred sectors, rather than positions
in a potentially changed public list. DataEnd clears pointer/progress ownership;
software reset still owns and resets the entire pool.

The public adaptation retains all original log formats/masks and valid-command
text/order, instead of importing the private consolidated trace. Move the old
transfer-start trace before acquiring the selection. Re-admit the original
request under the mask after logs, since UART capacity waits may let disk work
clear or shrink the partition. [Existing printk](https://github.com/papsg/SAROO/blob/f9ed8fe/Firm_MCU/Main/printk.c#L258)
calls [UART output with delay-capable capacity handling](https://github.com/papsg/SAROO/blob/f9ed8fe/Firm_MCU/Main/uart4.c#L191).
After publishing a borrowed pointer, transfer start/service perform no logging,
blocking lock or delay before finishing their FIFO writes.

Command/FIFO work stays priority31 and disk work24. The old unused High attribute
was not passed to disk creation; the [pinned RTX null-attribute path](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_thread.c#L636)
uses Normal. Pass the zeroed attribute with explicit Normal to preserve that
effective ordering. The actual creation fixture and pinned dispatcher enforce
it; raising disk above command is rejected. This preserves the nonblocking
borrowed-pointer contract, not a measured priority/timing improvement. Other
interrupts can run; source review finds partition release on disk/command paths,
not those interrupt handlers. New blocking calls or priorities need re-review.

Ten new actual-source groups pass sanitizers/O2. Coverage includes720 GETDEL
head/middle/tail/full-pool selections plus a scrambled200-block chain;10
conflicting and8 preservation cases; actual incoming appends; release-time reuse
that overwrites freed next pointers;8 clear/shrink log-boundary cases;10,368
read admissions; reset ownership; incomplete GETDEL rejection and baseline-valid
exact log/payload controls. The preceding source fails nine groups and passes
that valid control; failures share causes and are not nine hardware findings.
Eighteen compiled ownership/scheduling mutants fail behavioral/sanitizer checks.

The old transfer-state differential used fabricated partition counts and a fake
trans_start. Its saved field positions cannot qualify private ownership. It is
retired in favor of these actual list/FIFO/admission/log controls, with its source
and results preserved in prior public commits. Both directory differentials
remain unchanged in purpose. All81 CD methods and2 directory methods pass both
profiles;6 scheduling/startup methods also pass both profiles. Final strengthened
log-boundary tests pass separately after the full focused runs. Model callbacks
are explicit host simulations, not captured preemption or physical IRQ timing.

Complete MCU load grows224 bytes:58,452→58,676, leaving6,860 ITCM bytes. Static
DTCM stays60,296, with no retained table, payload buffer, allocation or copy.
Direct common-GET frame48→64, delete24→32, remove8→16; disconnect/detach/release
helpers use8/20/8. Clear16, calculation32, first FIFO16, FIFO service24, DataEnd40,
disk80 and IRQ/dispatch24/16 remain. Compiler frames exclude other callees and
exceptions. Valid GETDEL/delete masks at most200 public nodes while selecting/
detaching metadata; payload writes and list release remain outside that mask.
Preserving logs requires bounded range revalidation after their possible waits.
One detachment replaces repeated same-position removal searches. No measured
physical cycle, IRQ-latency, peak-stack or game-speed claim follows.

```sh
python3 tests/test_cdc_read_ownership.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_read_ownership.py
python3 tests/test_gif_scheduling.py
make -C tests check-cdc-command-buffer
# Baseline/mutant override: SAROO_CDC_READ_SOURCE=/path/to/saturn_cdc.c
```

This checkpoint deliberately leaves the preceding early-read count arithmetic
for the separate012 correction. It does not import the old, superseded
host-consumed count policy. Wider asynchronous copy/move/MPEG ownership,
directory/ISO/XA workers and physical/game qualification remain separate.

## MCU-CDC-012: CD-produced read counts and pending windows

Original: selected paps-g `3d2c405e`, corrected by `ac0aeac1` (2026-09-23),
requiring010. The original handler guesses an unserviced window as2048 bytes
and subtracts FIFO remainder. The earlier private repair tracked the correct
window but still subtracted unconsumed words. That superseded host-consumed
interpretation is deliberately not imported here.

Sega [ST-162 §8.2.1 function1.10, printed p.81](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=69)
explicitly distinguishes host words from effective CD-block words. For an
interrupted read, CD output can exceed host consumption; writes report consumed
input. Complete/excess transfers are capped at the normal request. Error/no
active transfer is distinct from valid zero. This is also described in
[the cached function reference](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_11.htm).

Read service already accounts for each previously queued FIFO window before
emitting another. DataEnd adds the actual current window exactly once if its
service has not been processed; it does not subtract host-unread data or enqueue
future requested sectors. A serviced final window is not added again. Preserve
the impossible-occupancy error guard, cancelled/no-active errors and existing
host PUT consumed counts/reservation/routing. A legitimate empty fixed reply
returns zero rather than the error sentinel. All legacy log formats/masks remain.

For example, an untouched user-data read has already produced1024 words even
when the host consumed none. A first XA Form2 window plus an emitted user-data
window reports1162+1024 words independently of how much remains in the FIFO.
No total can grow merely because DataEnd is issued repeatedly. The public menu
[decodes the same24-bit field](https://github.com/papsg/SAROO/blob/f7137f6/Firm_Saturn/cdblock.c#L158);
its inspected callers either discard the count or do not reject read-ahead.
No menu patch is required for this source contract.

Four new actual-source groups pass ASan/UBSan and O2, with ten ownership groups
retained. There are10,452 first-window prefix cases across both data-read types,
all four sizes, XA and TOC;12 serviced/mixed/completed-window controls; five
fixed-reply formats including254-file output; repeated/idle completion, valid
zero and impossible occupancies. Expected counts come independently from actual
FIFO writes, while modeled host consumption varies. Assert no extra FIFO writes
at DataEnd. Full bit11 is tested separately from used-word bits; synthetic upper
bits test the preserved mask, not claimed FPGA05 physical counter states. The
preceding010 source fails all four new groups and passes its ten existing groups.
Eleven compiled behavioral mutants fail, including the superseded subtraction,
wrong pending length, double counting, zero/idle errors, full-bit/occupancy errors,
lost cancellation/PUT theft and manufacturing extra read windows.

All85 CD methods pass both profiles, including13 actual PUT groups and every
earlier bounds/size/routing/metadata/control fixture. The host FIFO model does
not simulate RTL clock crossings, bus timing or simultaneous physical service/
DataEnd. The first valid reply is at most1524 words, a later sector at most1176
plus the256-word threshold; both fit the existing2048-word FIFO. The200-sector
pool remains below the24-bit result limit. These are scoped bounds, not complete
physical interleaving or game qualification.

The full MCU adds32 load bytes:58,676→58,708, leaving6,828 ITCM bytes. Static
DTCM remains60,296 and DataEnd's direct frame remains40. The success flag is a
local within the existing compiled frame. No retained state, buffer, allocation,
payload copy, FIFO output or mask operation is added. Reading occupancy uses the
preceding interface; existing logs can also read status when enabled. The
pending-window arithmetic replaces a fixed-size guess/subtraction, with no
measured physical cycle, latency or stack-peak improvement claimed.

```sh
python3 tests/test_cdc_read_ownership.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_read_ownership.py
make -C tests check-cdc-command-buffer
```

The selected source-level012 contract is complete; exact physical service/end
timing and games remain unqualified. Later layout/filesystem/asynchronous
ownership and initialization findings remain outside this four-port series.

## Final verification and limits

The full project check passes375 Python methods in37 groups and14 Rust tests.
All85 final CD methods also pass ASan/UBSan and O2. Nine cohorts bind exact
current source, headers, fixtures, dependencies and generated fragments in both
profiles; all93 native cases/modes per profile return success. Failing baselines,
valid controls, compiled mutants and the explicitly uncounted redundant mutant
remain preserved with the original contributions and later correction evidence.

Each complete MCU receipt binds56 repository inputs to its respective source
checkpoint. The final menu binds36 current inputs and exactly matches the prior
resource/image/ELF cohort:79,572 code/data,9,232 BSS,16,384 reserved stack,
357,679 font and488,751 total image bytes. Across these four ports MCU load
grows480 bytes:58,228→58,708, leaving6,828 ITCM bytes. Static DTCM remains60,296;
the200-block pool, required stacks and persistent workspace remain unchanged.
Per-port stack, masking and traversal costs are retained above, with no claim
of measured physical peaks, deadlines or speed gains.

Selected008/009/010/012 source contracts are complete at the model/build level.
This does not complete broader asynchronous copy/move/MPEG ownership, directory/
ISO/XA filesystem011/013, later raw-Mode1/audio representation or initialization
work. Exact malformed-command replies and physical service/interrupt/timing/game
qualification remain separate. No Saturn operation or installed-firmware change
was performed. Public source, citations, attribution and reproducible tests are
published separately from ignored raw evidence; no USB integration or new
runtime diagnostic is included.
