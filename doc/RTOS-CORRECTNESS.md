# Selected RTX 5.5.1 API corrections

## SSR-007: report refused waits

`osDelay(0)` now returns `osErrorParameter`. Relative and absolute waits
return `osError` if the kernel refuses admission, rather than claiming that
the thread waited. Admitted waits and the existing wrap-aware deadline
arithmetic are unchanged. Normal firmware calls positive relative waits;
this is an API contract repair, not evidence of a particular game failure.

Arm CMSIS-RTOS2 API 2.1.3, **Generic Wait Functions**, `osDelay` and
`osDelayUntil`, **Return values**, requires these outcomes. The same
reference documents tick wrap and the maximum absolute delay of 2^31−1:
[Arm Generic Wait Functions](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__Wait.html).
The locally retained matching reference was used; no new online research.
No Sega rule specifies these MCU-internal kernel API results.

The correction adapts paps-g's `692f9716`/SSR-007. Original RTX code and
unchanged private support headers remain attributed to Arm Limited, under
Apache-2.0. See [provider provenance](../Firm_MCU/RTE/CMSIS/RTX/provenance.json)
and its adjacent complete license. Public integration and checks are by papsg.
There is no original tpunix issue/PR identified for this independent finding.

## Provider selection

The original project selects CMSIS 5.6.0 / RTX 5.5.1's Library variant.
Both App and BootLoader explicitly compile the replacement `rtx_delay.c`;
its public symbols resolve before the library's delay member is needed.
Three source-private headers supply the compilation interface; SSR-069 below
adds exactly one prototype to `rtx_lib.h`. The other two remain byte-identical.
Public RTX/API headers still come from the selected pack. The override checks
the kernel version at compile time. The GNU builder selects the same source
and excludes the stock delay translation unit, retaining all other SDK units.
It rejects changed base-provider/header hashes, never patches the installed
SDK, and records both original SDK and owned inputs. No compiler ABI, mutex,
dispatcher or other historical private-kernel change is imported.

## Reproduce offline

Build `firmware-build` with the locked Cargo workspace, then run:

```sh
firmware-build rtos-check --repo . --out tests/build/UNUSED \
  --toolchain ARM-GNU-ROOT --cmsis CMSIS-ROOT \
  --stm32 STM32-INCLUDE --rtx CMSIS-RTOS2 --case delay
```

Use a fresh output directory for each run. `--baseline` selects only the
original delay provider and must fail after reporting all five false-success
results; successful/expired/current/masked-IRQ controls pass before that failure.
The selected provider passes 23 assertions, including positive waits, zero,
locked-kernel relative/absolute waits, real idle-thread empty-ready-list
relative/absolute waits, public IRQ-mask refusal and an admitted deadline
across tick wrap. `--lto` repeats the actual API/SVC/exception check with LTO.
`--archive` retains all original kernel units in a GNU archive and links the
owned source first; the same checks prove that the original delay member
does not override the correction. It tests library-member resolution, not
the unavailable proprietary Keil linker or its compiled library.

The maintained Rust command stages pinned inputs, builds and executes the
real kernel and Cortex-M7 exception assembly on local QEMU. Startup, virtual
RAM and semihosting belong solely to the test executable. The standard MCU
contains no test threads, test startup or added runtime diagnostics; software
timers remain disabled. Failed runs and input/command receipts are preserved.
These checks do not qualify physical time, stack high-water, Keil App/BootLoader,
Saturn scheduling or game behavior.

## Public build cost and limits

The same compiler, epoch and SDK inputs rebuild the parent and candidate.
The LTO App grows 8 load bytes to 59,444 (6,092 ITCM bytes free); non-LTO
grows 16 to 63,720 (1,816 free). Data/BSS remain 200/50,696 with LTO and
204/50,688 without; both static DTCM spans remain 50,896 bytes. The relative
service direct frame stays 8 bytes; absolute is unreferenced with LTO and
its non-LTO direct frame changes 16→8. These are compiler direct frames,
not maximum task stack measurements. All 166 linked vector checks pass.
The menu/FPGA and configured software-timer exclusion are unchanged.

## SSR-008: validate the queue block before rounding

A very large message size can wrap during 4-byte alignment/header addition,
leaving a tiny backing block for a huge advertised payload. Reject sizes above
`UINT32_MAX - sizeof(os_message_t) - 3` before either addition, allocation
or publication. The existing product-admission check then sees the actual,
nonzero block size. Ordinary capacities, attributes and priority behavior stay
unchanged. This adapts paps-g `b5760f73`/SSR-008; Arm's original provider remains
under its existing Apache-2.0 notice, with papsg's integration identified.

Arm CMSIS-RTOS2 API 2.1.3, **Message Queue**, `osMessageQueueNew`, **Parameters**,
**Returns** and the following storage/alignment description, requires storage
for the requested capacity and message size and NULL on constructor error:
[Arm Message Queue](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__Message.html#ga24e895a00f9d484db33aaf784c57bfed).
This is the locally cached matching reference, not a Sega-specific API rule.
No original tpunix issue/PR was identified for this independent finding.

Use `rtos-check --case queue` with the same arguments above. The genuine
32-bit Cortex-M7 API/SVC/kernel parent admits eleven capacity-one queues
for sizes `0xfffffff5` through `0xffffffff`; its complete failed witness
reports their 4/8/12-byte backing blocks, then deletes them without copying
a giant payload. The selected, LTO and stock-kernel archive variants reject
all top33 sizes and pass37 assertions including ordinary two-message/7-byte
priority/content delivery, zero dimensions, overflowing capacity, deletion,
subsequent allocation and the complete SSR-007 wait controls. `--baseline`
reverts only this case's queue provider, retaining the delay correction.
Receipts pin the staged provenance, provider/header inputs, compiler and
builder. SDK/header mismatches refuse without modifying the SDK.

App and BootLoader explicitly select this owned source in the original project;
the GNU builder excludes its stock duplicate. Standard firmware has no public
queue-constructor caller. These API checks establish the conditional defect;
they do not establish an ordinary game, audio or physical memory-corruption
failure. No oversized payload is copied and no software timer is enabled.

Fresh standard LTO/non-LTO App links retain every loaded segment byte,
59,444/63,720 load bytes, 6,092/1,816 ITCM margin, data/BSS/static span and
166 vectors from SSR-007. `osMessageQueueNew` is absent from both links;
its code and direct frame are conditional test costs, not added standard
firmware costs. LTO debug metadata changes with source selection; whole ELF
identity is not claimed. Menu and FPGA sources remain unchanged.

## SSR-009: restart a periodic timer with the new period

A running timer restarted from10 ticks to3 previously changed only its next
expiration; later callbacks incorrectly reverted to10 ticks. Publish the new
reload for both admitted running and stopped starts, immediately before the
unchanged list insertion. Parameter and unavailable-service refusals still
return first. List ordering, expiration, callbacks and stop/delete ownership
are unchanged. This adapts paps-g `f5da5ea0`/SSR-009, retains Arm's original
Apache-2.0 source/notice and adds papsg's explicit build integration. No original
tpunix issue/PR is identified for this independent finding.

Arm CMSIS-RTOS2 API 2.1.3, **Timer Management**, `osTimerStart`, the **ticks**
parameter and start/restart description specify the supplied period:
[Arm osTimerStart](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__TimerMgmt.html#gab6ee2859ea657641b7adfac599b8121d).
The matching locally cached reference and actual provider were reviewed.
No Sega-specific software-timer rule is inferred.

`rtos-check --case timer` enables an explicitly test-only512-byte virtual
timer-thread stack and8 callback queue entries. The original provider reports
10/10 callback intervals after a running restart to3, then fails. Selected, LTO
and stock-kernel archive variants complete six phases: running10→3, stopped10→3,
running3→3, running3→7, restarted one-shot and simultaneous periodic deadlines.
Measured intervals are3/3,3/3,3/3 and7/7 kernel ticks. NULL/zero refusal, state,
one-shot nonrepetition, exact common-deadline callbacks and deletion pass.
Both timer starts must occur in one tick; the fixture retries that setup at
most8 times if the calls cross a boundary. `--baseline` reverts only the timer
provider, retaining the prior delay and queue corrections.

The standard timer configuration remains zero stack/zero callback queue.
No timer task, constructor call, test executable, diagnostics or loader enters
standard firmware. The kernel API, exception assembly and timer implementation
are real; virtual startup and semihosting belong only to the offline test.
This does not qualify wall-clock periods, task stack high-water, physical
interrupt latency, proprietary Keil linkage or games.

Fresh LTO/non-LTO App/166-vector links retain every loaded byte,59,444/63,720
load bytes,6,092/1,816 ITCM margin and unchanged data/BSS/static span. Neither
`osTimerNew`, `osTimerStart` nor `osMessageQueueNew` is linked. These are
standard-image costs; the optional virtual application's allocations and
periods are separate. Menu and FPGA sources are unchanged.

## SSR-067: publish pool ownership and occupancy together

The original exclusive-access pool implementation publishes a free-list change
and updates its used-block count in separate operations. A higher-priority IRQ
can allocate a just-returned block between those operations. The real public
API then reports33 used blocks in a32-block pool and `0xffffffff` available
blocks. Allocation has the inverse gap: the last block is owned while the
used count still reports one available. These examples use valid live objects,
owned blocks and timeout-zero ISR calls; no double free supplies the trigger.

Both primitives now use the provider's existing PRIMASK policy: save the incoming
mask, update links and count inside a bounded exclusion, and restore IRQs only
when enabled on entry. Validation and event callbacks remain outside it. There
is no loop, wait, allocation or callback in the exclusion; pool capacity, public
dispatch, statistics getters and deferred-processing policy stay unchanged.
Other RTX atomics retain their configured policy.

This adapts paps-g `093a51b6` with papsg's public integration and test adaptation.
The complete selected5.5.1 source and Apache-2.0 notice are retained; its pinned
original SHA-256 is `1a9f4af3ca4ffbd2cdf8e34b137a9bdb04ae294d8413daa004bc92203b8d4bf8`.
App and BootLoader explicitly compile the override, while GNU excludes the
stock duplicate. No later pool-constructor or admission correction is imported.
No original tpunix issue/PR is identified for this independent finding.

Primary requirements, read from locally preserved references:

- [Arm CMSIS-RTOS2 API2.1.3, Memory Pool](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__PoolMgmt.html),
  the **Note** on ISR-callable functions and `osMemoryPoolAlloc` timeout,
  `osMemoryPoolGetCount` and `osMemoryPoolGetSpace` descriptions. Counts describe
  used and available blocks. Separate getter calls are not one atomic snapshot
  across intervening operations.
- [Cortex-M7 Generic User Guide, ARM DUI0646C](https://developer.arm.com/documentation/dui0646/c),
  **§2.1.3, Priority Mask Register**, printed p.2-8, and **§§2.3.5–2.3.6**,
  printed p.2-23: configurable-priority preemption, grouping and PRIMASK.
  These concern the MCU's kernel; no Sega-specific pool API rule is inferred.

Use the existing `rtos-check` command above with `--case pool`. Run each of
`--phase=release`, `--phase=allocate`, `--phase=masked` and `--phase=after`
under both `--atomic=0` and `--atomic=1`, with a fresh output each time.
Default mode1/release can also run with `--lto` and `--archive`.
`--baseline` selects only the pinned original pool provider, retaining the
previous wait/queue/timer corrections.

The public parent reproduces256 release and256 allocation observations with
wrong statistics. Its already-masked, after-return and mode0 release controls
pass768 histories. Each fixture varies capacities1/2/7/32 and block sizes
4/8/12/64 over16 cycles. It checks unique ownership, the other owned blocks'
payload words, nonzero ISR timeout refusal, mask restoration, exact interrupt
delivery and recovery of every pool. All ten selected images pass2,560
histories across both atomic modes, four phases, LTO and archive selection.
The existing real-kernel delay, queue-constructor and timer checks pass, as does
the common-allocator startup/directory-depth/hole-reuse test. Rust14 unit and
two RTL tests, formatting and strict Clippy pass.

Only a copied test provider receives pending-IRQ hooks; the normal source has
none. Receipts preserve original and instrumented hashes. The after-return
variants compile uninstrumented providers. M7 disassembly places both stores
between CPSID and conditional CPSIE, with callbacks outside; isolated selected
allocation/free functions are52/62 bytes with direct frames16/8, versus72/94
and16/24 in the original exclusive-mode fixture. These are static compiler
observations, not physical cycles or maximum interrupt-stack use.

Both fresh standard166-vector App builds pass. LTO load changes59,536→59,492
bytes (6,044 free); non-LTO changes63,792→63,748 (1,788 free). Data/BSS are
200/50,696 and204/50,688; static DTCM span stays50,896. All68 public MCU input
pins match their staged build cohort. Unlike the historical private profile,
the public links retain the internal pool primitives, so whole-image parity
is not claimed. No ordinary public pool-API caller is found; retained optional
and internal code is not evidence of a shipping-game failure.

Actual RTX APIs, SVC, external IRQ and PendSV execute in a one-core QEMU M7.
Startup, memory map, IRQ source, clock and semihosting are test infrastructure.
No STM32/Saturn timing, cache/bus/IRQ latency, arbitrary priorities/schedules,
NMI/fault caller, stale/deleted object, whole stack or game qualification is
claimed. Menu, FPGA, feature selections and device state remain unchanged.

## SSR-068: retire ISR reads and count reserved backing slots

The public parent reproduces432 rejected replacement puts per atomic mode:
an ISR has completed a successful queue read, but the backing block is still
held until PendSV. Thread Get/Put and after-IRQ replacement controls pass.
PendSV cannot finish that deferred release while the higher-priority IRQ runs.
This is a retained API defect; no ordinary application queue caller or linked
queue API is found in the standard public firmware.

A read now detaches its ready node and decrements the count under saved
PRIMASK, then copies the payload and releases the backing block before success
returns. A blocked sender receives a queue-owned deferred callback; nothing
retains a pointer to the released message. The existing message callback slot
handles queue objects as well as pending ISR-put message objects. Its object
identifier is checked before conversion. The shared dispatcher adds that queue
identifier; no callback-table entry or persistent buffer is added.

Immediate reuse can invalidate a ready-list pointer saved by an interrupted
priority search. A spare private queue flag records a detachment. The inserter
reads each traversal step under PRIMASK and, when enabled on entry, restores
IRQs with ISB between steps. It tests the flag before dereferencing a saved
pointer again and restarts from the current tail after removal. Priority and
equal-priority FIFO behavior are preserved; an already-masked caller stays
masked. Each step has bounded structural work, but the search remains dependent
on capacity and may repeat after removals. This is not a measured IRQ-latency
or wall-clock bound. Payload copies remain outside that list exclusion.

The final correction also computes available slots from backing-pool occupancy,
not just ready-message count. A successful ISR put owns storage before PendSV
publishes it to the ready list. Reverting only this expression gives816 wrong
space observations per atomic mode, even with no competing producer and ample
deferred-ring capacity. The selected expression counts ready, private and
pending reservations. SSR-067's consistent pool publication is a prerequisite.
GetCount retains its existing deferred visibility; Count+Space need not equal
capacity while ISR puts are pending. Space is not a reservation or a guarantee
against another producer or exhaustion of the separate deferred ring.

This ports paps-g `8b5cbe9b` and the final `03fa6494` amendment together.
Papsg's public adaptation preserves the previous SSR-008 constructor guard,
original Arm Limited notices and Apache-2.0 license. The new owned system source
is the pinned5.5.1 original plus the version guard and queue-dispatch case
(with one inherited whitespace-only line normalized);
its original hash is `f2abcf9eb25ab57f680ee01d8c1e7dc0dbbb20f4a69b9ec99c9f3c6923361569`.
App/BootLoader explicitly select it and GNU excludes its stock duplicate.
Later private ring-admission changes are outside this contribution. No original
tpunix issue/PR is identified for this independent finding.

The locally preserved [Arm CMSIS-RTOS2 API2.1.3, Message Queue](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__Message.html)
documents ISR-callable timeout-zero Put/Get, higher-numbered message priorities
and **osMessageQueueGetSpace**, **Returns** and description: available message
slots. The **ISR-callable functions** note includes the getter. There is no
cached exception treating occupied backing storage as available. The [Cortex-M7
Generic User Guide, ARM DUI0646C](https://developer.arm.com/documentation/dui0646/c),
**§2.1.3**, printed p.2-8, and **§§2.3.5–2.3.7**, printed pp.2-23–2-24,
describe PRIMASK, group priority and exception preemption/return. These support
the tested IRQ/PendSV interaction, not a Sega-specific kernel requirement.

Run `rtos-check --case message` with `--atomic=0` and `--atomic=1` for each
`--scenario=reuse`, `waiters`, `walk` and `space`, using fresh output paths.
The two-mode selected checks cover:

- 128 reuse cases/992 ISR reads, optional priority outputs, guarded complete
 payloads and preserved caller masks; no rejected replacement or retired-block
 callback remains.128 full-queue resets and subsequent reuse pass as controls.
- 128 real blocked-thread cases: receivers, senders, replacement before sender
 admission and three senders with priority/FIFO wake order. Pools and lists empty.
- 400 SVC/PendSV priority walks, interrupted at each selected traversal step
 across capacities2/3/8/16 and four message sizes. Omitting only the invalidation
 store fails an exact payload assertion.
- 512 public ownership histories/4,160 space observations, capacities1/3/5/8,
 sizes1/4/12/64, masked/unmasked ISR, pending puts, replacement, get-only and
 thread controls. The old-expression controls finish all histories with816
 wrong observations each; selected code has zero and exact payload recovery.

All13 selected message images pass, including LTO reuse/walk/space and
stock-kernel archive waiter/walk variants. The initial actual public parent and
the final pinned-original controls reproduce the same432 failures per mode;
64 thread and64 after-IRQ controls per image remain valid. Final fixtures also
retain64 reset controls per mode. Existing delay, constructor, timer and both
pool-publication regression modes pass. Rust15 unit/two RTL tests, formatting
and strict Clippy pass. Test-only IRQ hooks change no list, payload or count;
copied provider/mutation hashes and failed-control logs remain preserved.

Both fresh standard166-vector App links add4 load bytes:59,496 LTO/free6,040
and63,752 non-LTO/free1,784. Data/BSS stay200/50,696 and204/50,688, with50,896
static DTCM span. All69 public MCU input pins match their actual staged cohorts.
The queue APIs are absent from both final links. Non-LTO object insertion text
grows74→110 bytes/direct frame12→16; removal stays66 bytes/frame8, the space
getter42/frame8 and dispatcher144→148/frame16. These are conditional object
and direct-frame costs, excluding transitive/interrupt stack peaks. No layout
fields, queue capacity, payload allocation or callback-table allocation grows.

Actual RTX APIs, real blocked threads, SVC, IRQ and PendSV run in QEMU M7.
Startup, memory map, clock, scheduling hooks and semihosting are test fixtures.
No STM32/Saturn timing, physical IRQ/cache/bus, arbitrary schedule/priority,
deleted/stale-object lifetime, deferred-ring-overflow or game qualification is
claimed. No intermediate design was published; menu, FPGA, feature selections
and device state remain unchanged.

## SSR-069: admit an event wakeup before publishing its flags

The parent sets an event's bits before attempting to queue its deferred wakeup.
With a full ISR ring, Set reports success but a real waiting thread remains
blocked. The public-parent fixture reproduces 32 such losses per atomic mode,
with 32 last-slot controls passing. Retry after draining recovers the waiter.
The selected ISR/masked-call frontend first admits its callback, then publishes
the flags. Failed admission returns `osFlagsErrorResource`, preserves all flags
and emits no overflow notification. Parameter validation still precedes both
operations. Thread-context Set retains the original SVC path.

The shared `osRtxPostProcessTry` helper returns admission status and retains
the existing PendSV/blocked-kernel scheduling. Its caller must prevent PendSV
until object publication completes; ISR execution or the existing caller mask
does so here. The old void wrapper retains its overflow notification for other
callers. No object layout, ring capacity, callback table or buffer grows. The
support-header extension is exactly its one prototype; the GNU tool still
requires the original SDK hashes and rejects every other header change.

There is one necessary prerequisite from paps-g `6e96f528`: in exclusive mode,
PendSV must read a ring object's pointer before releasing its slot. The public
parent decrements occupancy first, allowing a higher IRQ to overwrite the old
slot. Across 256 real event histories at 16 offsets and occupancies1–16, reversing
only this ordering loses 16 callbacks and leaves 16 actual waiters blocked;
240 controls pass. The selected order loses none. The already IRQ-masked mode0
also passes all 256 old-order controls. PendSV remains the sole consumer; it
reads the object and advances its private output index before atomically
releasing capacity. This is required for the new admission policy to rely on
a reusable slot. No broader private-kernel work is imported.

The event correction adapts paps-g `d3b1f4d5`/SSR-069; the dependency above and
papsg's public integration/tests are identified separately. Original Arm Limited
notices and Apache-2.0 license remain. The complete pinned 5.5.1 event provider's
original SHA-256 is `be526e1766fb47fa6c578b8c5e0bc7ded931f514a27c0e4b0b4aacffad1299fe`.
App and BootLoader explicitly select it; GNU excludes the stock duplicate.
No original tpunix issue/PR was identified for this independent finding.

The locally preserved [CMSIS-RTOS2 API2.1.3 header](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/Include/cmsis_os2.h#L538-L561),
`osEventFlagsSet` and `osEventFlagsWait` return descriptions, specifies flags
or an error result; its flag constants define `osFlagsErrorResource`, WaitAny,
WaitAll and NoClear. The explicit refusal policy is the chosen remedy, not a
claim that this header mandates one implementation. The [Cortex-M7 Generic User
Guide, ARM DUI0646C](https://developer.arm.com/documentation/dui0646/c), **§2.1.3**,
printed p.2-8, and **§§2.3.5–2.3.7**, printed pp.2-23–2-24, describes PRIMASK,
preemption and exception return. Together with the observed interleavings these
support the ownership argument. No Sega-specific MCU event rule is inferred.

Use `rtos-check --case event --atomic=0|1` for each
`--scenario=single|waiters|nested|legacy`. Single covers 64 cases/mode; waiters
covers 32 with three priority/FIFO waiters; legacy checks three ring capacities
and the retained single overflow notification. Nested covers 17,856 cases/mode:
all 16 ring offsets, 31 legal bits, four wait modes, three IRQ/masked contexts and
occupancies 14–16, including 5,952 refusals/retries and 11,904 higher IRQs. Exact
results, unrelated bits, one wakeup, caller masks and empty recovery all pass.
`--ignore-event-refusal --scenario=nested` intentionally fails the higher-IRQ
refusal assertion. `--baseline --scenario=single` reproduces 32 lost wakeups.

Use `--case retire` for the prerequisite; `--baseline --atomic=1` reverses only
its retirement order, retains the new helper and reproduces 16 lost callbacks.
Mode0 is a valid old-order control. Hooks in copied providers only select IRQ
timing; source and mutated hashes remain in receipts. All 15 selected images
pass, including LTO single/nested/retire and archive waiters/retire. Thirteen
existing wait, constructor, timer, pool and message regression images pass.
Rust 17 unit/two RTL tests, formatting and strict Clippy pass. Initial Rust-borrow
and mode0 fixture compile failures were setup errors, repaired and preserved;
they are not defect witnesses.

Both fresh standard 166-vector App builds pass, with all 70 public MCU input
pins matching their staged cohorts. LTO load adds 12 bytes to 59,508 (6,028 free);
non-LTO adds 16 to 63,768 (1,768 free). Data/BSS remain 200/50,696 and 204/50,688;
static DTCM span remains 50,896. Event APIs are absent from these standard links,
so this is a retained optional-API repair, not a demonstrated game bug.
Uninstrumented M7 output tests admission before flag OR/store and loads the
ring pointer before releasing occupancy. Non-LTO Set text changes 108→128
bytes, direct frame 16 unchanged; the old 100-byte/frame 16 wrapper becomes 24/
frame 8 plus a 108-byte/frame 16 helper. PendSV changes 148→160 bytes, frame 16
unchanged. These are object/direct-frame observations, excluding callees and
exception peaks, not measured cycles or whole-stack bounds.

Actual RTX APIs, blocked threads, SVC, IRQ and PendSV execute on one QEMU M7.
Startup, memory map, clock, IRQ scheduling hooks and semihosting are synthetic.
No STM32/Saturn IRQ/cache/bus timing, arbitrary schedules, NMI/fault behavior,
stale/deleted lifetime, other APIs' overflow behavior or game qualification is
claimed. Menu, FPGA, feature selections and device state remain unchanged.
