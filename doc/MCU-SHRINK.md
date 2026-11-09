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

Queued after shared CD setup. Preserve recognized spellings, bounded UTF-8
filenames, CUE preference and discovery failure behavior; remeasure savings.
