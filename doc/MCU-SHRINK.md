# MCU size and resource efficiency

Three independent source checkpoints start at public `d38e06a`. These port
selected parts of MCU-SIZE-001, independently measured on current public code.
Command-name packing and diagnostic changes, USB, constant packing, broad
compiler/linker experiments and unrelated optimizations remain excluded.
Original code/dependency notices and prior public ports are preserved.

## Unused software-timer callback service

Original implementation/evidence: **paps-g**, `1b49c143` (2026-09-06).
The public adaptation changes the standard shared `RTE/CMSIS/RTX_Config.h`:
set `OS_TIMER_THREAD_STACK_SIZE` and `OS_TIMER_CB_QUEUE` to zero. These are the
supported settings when software-timer callbacks are unused, explicitly stated
in that original configuration template. Application sources contain no timer
API references. Ordinary delays, wait timeouts, SysTick and audio's separate
TIM6/semaphore remain. No required task, stack, allocator pool or feature shrinks.

The original project selects the same configured header/source for App and
BootLoader. The owned GNU builder consumes it unchanged. Its SDK source list
and all vendor kernel code remain unchanged: the supported disabled configuration
removes the callback worker and queue, while the linker drops unreachable
provider sections. The private build's explicit provider-source exclusion is
unnecessary here and is not ported. This avoids a build-only setting that would
leave the standard project with a different profile.

The pinned [Arm CMSIS/RTX source](https://github.com/ARM-software/CMSIS_5/tree/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX)
supplies the primary behavior: `rtx_lib.c` conditionally defines timer stack,
thread/queue control blocks and queue data, and publishes null attributes/count
when either setting is zero. `osRtxThreadStartup` in `rtx_thread.c` creates the
worker only for a nonzero queue count. `rtx_system.c` keeps the ordinary kernel
tick and thread-delay processing, with a null timer callback guard. The exact
local sources and [RTX configuration reference](https://arm-software.github.io/CMSIS_5/RTOS2/html/config_rtx5.html)
are preserved in the original evidence. The public tests vendor only unchanged
required extracts under the existing Arm copyright/Apache2.0 notices and license.
Their complete-source and extracted-body hashes identify the precise inputs.

The actual TIM6 programming, semaphore acquire and IRQ release in
`saturn_main.c` are unchanged and compiled by the fixture. This is a project
resource policy supported by RTX configuration, not a new Saturn compatibility
fix or a Sega-prescribed clock change.

Five public methods check selected-profile startup, tick/timeout progress across
unsigned wrap, TIM6/semaphore behavior, restoration of the original timer
configuration, and absence of application timer dependencies. The baseline's
single-worker policy check fails as expected; its required timing and enabled
worker controls pass. This is an optimization-policy failure, not evidence that
the original enabled callback service was incorrect. Corrected sanitizer/O2
runs pass. Restoring the unused service, changing the100 Hz tick rate, or
leaving half the selected profile enabled each fails its targeted check. The
restored control creates both idle and timer workers. Delay and
semaphore waits expire together, a mutex wait expires later, kernel tick wraps,
and ordinary processing continues. Task/list/hardware operations are modeled;
these tests do not qualify physical timing or complete RTOS concurrency.

A source dependency check conservatively requires review of any new `osTimer`
API reference. Future callback users must restore both settings and rerun the
profile/resource checks. Providers remain part of the SDK; this port does not
promise that unsupported future calls fail to link. Ordinary message queues
remain available independently of software-timer callbacks. This limitation
and restoration policy replace the private build's negative timer-API link test.

```sh
make -C tests check-mcu-shrink
SAROO_SHRINK_PROFILE=release make -C tests check-mcu-shrink
# Baseline/configuration comparisons: SAROO_RTX_CONFIG=/path/to/RTX_Config.h
```

The complete original-project GNU App/RTX/166-vector link drops **1,884 load
bytes**, 59,780→57,896, leaving7,640 ITCM bytes. Static DTCM falls
60,752→60,296 (**456 bytes**). The removed dedicated objects are the256-byte
timer stack,68-byte thread control block,52-byte queue control block and80-byte
queue storage. Required4 KiB task stacks, the32 KiB allocator pool and all other
configuration bytes are unchanged. No new allocation or firmware code is added.
The callback worker is not created and its tick handler remains unset; no
physical cycle or latency gain is inferred.

Full source/ELF/map/cost receipts, controls and privacy review stay local and
ignored. Later checkpoints retain their own baselines and measurements. This
is compile/link and source/model qualification, not original MDK, device stack,
audio pacing, game compatibility or installed-firmware acceptance. Private repo
and Saturn remain untouched.

## Shared CD setup

Original implementation: **paps-g**, `96f9329a` (2026-09-06), selected part of
MCU-SIZE-001. Starting at `2eb80d0`, share read/read-delete setup and the three
identical directory-start paths. Keep the four named handlers and their APIs.
The busy return still precedes partition access, sentinels normalize in the
same order, read/delete modes remain distinct, and rejected spans return the
same status. Preserve complete state, response masks, existing diagnostic text,
masking and callee order. The public adaptation uses the existing `SSLOG` macro
and portable `noinline`; the private GCC-only `noclone` annotation is unnecessary.
There is no command-name packing or new diagnostic/logging behavior.

Sega's [CD buffer-I/O specification, §8.2.7](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_17.htm),
function7.2 `CDC_GetSctData` and7.4 `CDC_GetdelSctData`, defines partition/position/
count selection and end sentinels; read-delete additionally deletes its selected
range. [CD filesystem §8.2.8](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_18.htm),
function8.1 `CDC_ChgDir` and8.2 `CDC_ReadDir`, defines selector-backed directory
work, root/current directory handling and retained file information. The local
HTML/text snapshots and source manifest were read. Their sections supply command
meaning; precise emulated status/masks/order here are preservation contracts
from current public code. No complete Sega-compliance or DataEnd/FIFO/deletion-
completion repair is claimed by this refactor.

A preserved public-source fixture identifies its commit, complete source hash
and exact four baseline function bodies. Three actual-C methods compare41,474
transfer and55,296 directory cases, then check seven independent contracts.
They cover valid partition/selector objects0–23; empty/single/full200-block
buffers; start/count boundaries and sentinels; both modes; busy transfers;
root discovery/current directory, page/index and size boundaries; logging on/off;
complete state and return values; and formatted logs plus ordered callee events.
Busy requests with partition255 return before table access. Nonbusy invalid
objects are outside this preservation domain; existing input validation,
stream ownership and completion behavior remain separate work.

Independent contracts check last-sector/end selection, rejected/busy transfer
state, mode/result masks, publication before transfer start, and filter setup
before directory wakeup. The event record includes filter contents at the
callee boundary, so reordering a wakeup cannot hide behind identical final
state. Baseline self-comparison and independent controls pass; the candidate
passes all96,777 checks under sanitizers and O2. Five mutations each fail two
methods: wrong delete mode, last-sector off-by-one, missing host-end result bit,
early transfer publication and directory wakeup before filter publication.
These are deterministic source/callee/register models, not concurrent hardware
execution or a full CD-block emulator.

The complete App/RTX/166-vector link saves **288 load bytes**:
57,896→57,608, leaving7,928 ITCM bytes. Static DTCM remains60,296, with no new
allocation, lock, task, buffer or storage operation. Linked helpers occupy268
and108 code bytes; there is one shared transfer helper. Existing command/IRQ
frames remain16/24 bytes. Read/read-delete frames change32/40→0/0 via tail calls
to the48-byte shared helper. Change/read-directory frames change24/24→24/16,
with a16-byte helper while active. The selected setup paths therefore add8–16
stack bytes before other callees/interrupts; required4 KiB task stacks remain.
Extra calls/type selection buy code space. No faster service or physical stack,
command/audio latency, original MDK or game qualification is claimed.

```sh
make -C tests check-mcu-shrink
SAROO_SHRINK_PROFILE=release make -C tests check-mcu-shrink
# Differential/mutation override: SAROO_CDC_SETUP_SOURCE=/path/to/saturn_cdc.c
```

## Exact ASCII image suffixes

Original implementation and independent spelling fixture: **paps-g**,
`96f9329a` (2026-09-06), selected part of MCU-SIZE-001. Starting at `fed7c6f`,
replace the only two application `strcasecmp` calls with one exact ASCII
helper for the lowercase patterns `.cue` and `.iso`. The public adaptation
isolates its actual-source spelling test and retains the existing real-FatFs
discovery/cover controls. Original source notices remain.

The [current public scanner](https://github.com/papsg/SAROO/blob/fed7c6f/Firm_MCU/Saturn/cdimg.c)
defines the supported suffixes, final-dot selection, CUE preference and bounded
256-byte joined path. These are application rules, not Sega requirements.
Fold only ASCII `A`–`Z`, compare unsigned bytes and require the pattern and input
to end together. A short input's terminator mismatches the remaining nonzero
pattern before another input byte is read. C11 [N1570 §6.5.6 paragraph8,
printed page93](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
defines array-pointer bounds and forbids dereferencing a one-past pointer.
The test's exact-sized allocations enforce that boundary. Full filenames remain
UTF-8; no filesystem, Unicode/CP437 table, locale or storage setting changes.
The application does not select another C locale.

One actual-C method performs **41,130 checks** against an independent enumeration
of all eight case spellings of each pattern. It tries all byte values at all
five positions, exact-sized truncated allocations, Unicode substitutions and
extra characters. ASan/UBSan and O2 runs pass. Four mutations fail: removing case
folding, accepting a prefix, folding unrelated byte values, and reading after
a short terminator. The seven existing actual-FatFs discovery methods pass on
both preserved baseline and candidate; seven coupled cover methods also pass.
These exercise CUE/ISO preference, final-dot selection, dotless names, UTF-8,
path limits, allocation/directory failures and publication. Baseline discovery
already passes: this is size specialization, not a newly claimed correctness
repair. The helper's token rejection does not reject a whole filename such as
`movie.cue.iso`; the unchanged scanner selects its final `.iso` extension.

The complete App/RTX/166-vector link saves **260 load bytes**:
57,608→57,348, leaving8,188 ITCM bytes. The linked baseline has a56-byte
`strcasecmp` and257-byte `_ctype_` table; the candidate has neither. The new
helper occupies44 code bytes, and scanner/layout changes determine the net
saving. Static DTCM stays60,296. All293 common application direct frames are
unchanged; the helper adds an8-byte frame while active. The scanner, loader
and disk-task direct frames remain112/312/80 bytes; the old library comparison's
nested frame is not included in those figures. No complete call-chain peak or
speed improvement is claimed. The two fixed patterns bound each helper call
to at most five input-byte reads, with range checks instead of case-table reads.
There is no new allocation, task, buffer, directory pass or I/O.

```sh
make -C tests check-mcu-shrink
SAROO_SHRINK_PROFILE=release make -C tests check-mcu-shrink
python3 -B -m unittest discover -s tests -p test_mcu_save_discovery.py -v
python3 -B -m unittest discover -s tests -p test_mcu_save_cover.py -v
# Spelling/mutation override: SAROO_IMAGE_SUFFIX_SOURCE=/path/to/cdimg.c
```

Across all three measured source checkpoints, MCU load falls59,780→57,348
(**2,432 bytes saved**) and static DTCM60,752→60,296 (**456 bytes saved**).
This does not shrink required task stacks, the allocator pool, disc workspace,
filesystem capabilities or menu assets. The complete menu rebuild retains
identical code/data78,884, BSS9,232, reserved stack16,384, font357,679 and
image488,751 bytes, including identical final image/ELF hashes. Current56 MCU
and36 menu input hashes match their complete build receipts. The selected
helpers' stack/call costs above remain explicit. Original MDK builds, physical
stack/timing/audio behavior and game compatibility are not qualified.

## Shared CD filter selector decoding

Adapted from paps-g `7c92d827`. Seven filter handlers now decode the
upper CR3 byte in their existing checked lookup. The serial command
task reads CR1–CR4 before dispatch and holds that RAM snapshot through
the handler. The lookup still accepts all 24 selectors. Each caller
keeps its reply, state updates and interrupt mask. Enabled stock
logging uses the same selector and format strings.
[Public parent command task](https://github.com/papsg/SAROO/blob/e44efab959b4d76ce677c139734f6abf2798fdc7/Firm_MCU/Saturn/saturn_main.c#L914-L943).

Sega documents the filter-number parameter for range, subheader, mode
and connection queries. SAROO's original command handlers show the
CR3 encoding that this refactor preserves.
[Sega ST-162-062094 §8.2.5, Nos. 5.1–5.6, printed pp. 87–89](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=75),
[No. 5.8, printed p. 90](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=78),
[v0.8 command source](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_cdc.c).

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
```

Both revisions pass 217 methods in each run, including the
selector-boundary, nonzero low CR3 byte, complete reply and routing
controls. With the same compiler and SDK,
both GNU App builds link 63,864 load bytes with 1,672 bytes of ITCM
headroom and 60,208 bytes of static DTCM. The range and mode setters'
direct compiler frames each fall from 16 to 8 bytes. The other 729
matched frames keep their sizes. Physical timing and peak stack usage
remain unmeasured.

## Size the permanent LED stack and common RTX pool

Adapted from paps-g `7ec67882` and `e4953e40`
(AUD-RES-008 and AUD-RES-017). App and BootLoader create the LED thread
once before starting the kernel. It now requests 512 bytes instead of
inheriting 4,096. Its priority, semaphore, delays and GPIO writes are
unchanged. Both builds still leave the default stack at 4,096 bytes.

Both thread stacks come from the common pool. The allocator rounds
blocks to eight bytes with an eight-byte header, so the permanent LED
block shrinks from 4,104 to 520 bytes. The pool reserves exactly those
3,584 fewer bytes (32,768 → 29,184). Every hole after the LED stack
retains its size. That preserves admission for later allocations,
including the public `scan_dir` path, which holds a 256-byte filename
and a `FILINFO` for every recursion level. The smaller private pool
target is not used here.

[CMSIS-RTOS2 thread management, `osThreadAttr_t::stack_size`](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__ThreadMgmt.html#aacbc9a219f2d6870e9ce89bb93f975c9)
describes the byte-sized stack request. The pinned RTX
[thread creation](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_thread.c#L652-L700)
and [allocator](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_memory.c#L102-L165)
establish the pool and block accounting. See the public
[recursive directory path](../Firm_MCU/FatFS/file_utils.c).

In both linked GNU targets, the LED task has a 40-byte frame and its
largest wrapper contributes 16 bytes. The wrappers' callees on the
thread stack add no frame. Add 104 bytes for the extended exception
frame, 96 for RTX's saved core and high floating-point registers,
four for alignment and four for the guard word. The result is 264
bytes and 248 bytes of margin within the request. See Arm *Cortex-M7
Devices Generic User Guide*, DUI 0646C, §2.3.7 pp. 2-24–2-25 and
[the pinned RTX context save](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/GCC/irq_cm4f.S).
This is a linked bound for the thread stack; neither physical peak
use nor the separate handler stack is measured.

With a completed [GNU App build](../tools/README.md#original-mcu-app-compilelink-check)
and QEMU installed, run:

```sh
SAROO_RTX_BUILD="$PWD/tests/build/mcu-link-check" \
  SAROO_ARM_CC=/path/to/arm-none-eabi-gcc make -C tests check-mcu-shrink
```

The fixture drives the startup requests through the pinned Arm
allocator, compares nested directory allocations, frees, reused
holes and exhaustion, and catches a 20 KiB pool control losing
admission. It does not start the scheduler. GNU App static DTCM
falls 59,448 → 55,864 bytes and the image grows 16 bytes to 64,728.
The GNU BootLoader comparison of the 13 enabled C units and common
startup/RTX providers also saves 3,584 bytes of static DTCM, adds
eight load bytes (28,704 total), and retains all 166 vectors. Both
are compile/link checks; the MDK image and installed firmware remain
unqualified.
