# Menu GIF safety and scheduling

Direct source ports start at public `24c5c73`. These are menu-background
corrections, independently revalidated on public source. Later display clipping,
composition/resolution/disposal features and unrelated firmware changes remain
separate. Existing source notices and prior public ports are preserved.

## MCU-GIF-001: bound decoding and release the disc workspace

The baseline `Firm_MCU/Saturn/tinygif.c` is byte-identical to the original v0.8
file (`27bd2fd31ad9adfddfec715751a5eb4fc483ace34ed1c646f3640bd08c88ff86`).
It reads an unchecked whole-file size into a shared buffer, ignores read/close
and parser errors, and advances through headers, sub-blocks and LZW data without
complete extent checks. Invalid geometry, dictionary references, code sizes or
pixel counts can escape their buffers. The baseline also stops GIF decoding
only after disc operations can already have reused the compressed-file memory.

Bound every file/parser advance and enforce the complete image rectangle's
pixel count. Validate headers, color tables, 2–8 initial LZW code sizes and
palette indices; cap dictionary codes at12 bits. Full tables remain readable
until an explicit clear. A constructed prefix always points backward, so a
string is finite and fits the 4,096-byte reversal stack. Read codes directly
from sub-blocks rather than copying/speculatively reading their ends. Preserve
small-height interlace passes, transparency/control scope and unsigned elapsed
delays. Saturating frame state distinguishes single-frame images from loops.

The compressed data starts at `0x24020000`. The current project's 200 blocks
start at `0x2400a000` and occupy2,352 bytes each; live metadata starts at
`0x2407cd80`. Thus the accepted file range is13–380,288 bytes. These limits
come from `MAX_BLOCKS`, block initialization and the existing graphics layout,
not a GIF-format limit or a display-resolution recommendation. Logical canvases
remain1–512 in each dimension, with the existing 512-byte pixel stride.
The Saturn menu retains its existing320×240 presentation.

Stop a prior reader before initialization and publish initialized state only
when complete. Stop/cancel uses atomic state and reader ownership; transitions
cannot revive a cancellation. Wait for both pixel and palette acknowledgments
before advancing. `SSCMD_LOADDISC` now stops the reader before load/pause/unload,
preserving the existing disc/save results. Its host fixture was corrected to
extract the complete command body rather than stop at the moved GIF call.

Primary format requirements are in CompuServe's
[GIF89a specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt):
§§15–16 (sub-blocks/terminators),17–21 (headers, canvases, rectangles and tables),
22 (image data),23 (control/transparency/delay), AppendixE (interlace), AppendixF
(variable-width LZW), and the cover-sheet deferred-clear clarification. The
locally preserved specification has SHA-256
`c0e48c12e3e91b1e78090caffb1d1ffe8e7d0face4350da675044a9a4be02aa3`.
WG14 [N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf),
§6.5.6 p8 (printed93) defines array-pointer bounds; §6.2.5 p9 (40) defines
unsigned modulo arithmetic. The file/canvas capacities, deterministic blank
fallback, buffer ownership and acknowledgment protocol are project policies.
They are not requirements prescribed by Sega for this decoder.
The toolchain's bundled [GCC atomic builtins documentation](https://gcc.gnu.org/onlinedocs/gcc-14.3.0/gcc/_005f_005fatomic-Builtins.html)
defines acquire/release, sequential consistency and compare/exchange semantics;
it was read locally and checked against linked ARM code.

Original implementation and fixtures: **paps-g**,
`3e62998cf908797a2e4bdb5881d7e347a02b8017` (2026-09-06). The public adaptation
uses current disc/save callers, removes private preparation and all added
integration/error logging, and adds first-frame preservation, cancellation,
reinitialization and stronger output/timer controls. No USB support or runtime
instrumentation is included. Read/open/close/header failures retain the existing
blank-background fallback; parse failures stop without publishing a bad frame.

Ten public actual-C methods cover independently generated input and expected
RGB, minimum code sizes2–8,256 colors, code-width growth, full dictionaries,
deferred clears, KwKwK, clear codes, one-byte sub-block splits, both palettes,
small-height interlace, maximum canvas, subrectangles, transparency/control
scope, wrapping delays, every truncation of a representative frame, malformed
input, load failures and all three disc handoffs. Explicit GIF87a/GIF89a first
frames and missing-file fallback are preservation controls. A deterministic
cancellation at the actual publication CAS checks that no frame is published
and that reinitialization succeeds. This does not model every preemption.

The public native fixture replaces physical address backing, file/clock service
and one atomic-intrinsic boundary; production expressions remain intact. It
checks bytes below the logical canvas independently, including rejected frames.
Sanitizers and uninstrumented O2 pass. The retained baseline has126 failing
cases and three passing controls; an early failure prevents one later case in
the same method from running. The accepted source executes the complete set.
ImageIO independently agrees with first-frame RGB for39 generated GIFs;
this checks a single frame per file, not complete animation compositing.

Eight targeted mutations are rejected: omitted capacity bound (three cases),
ignored I/O errors (six), omitted input bound (36), omitted output bound (one),
omitted palette bound (one), pixel-only acknowledgment (one), absolute wrapped
delay (one), and late disc handoff (three). The output/delay mutations exposed
fixture gaps: independent canvas-tail guards and an observation before unsigned
wrap were added before final validation.

```sh
make -C tests check-gif
SAROO_GIF_PROFILE=release make -C tests check-gif
# Optional comparison: SAROO_GIF_SOURCE=/path/to/tinygif.c
#                      SAROO_GIF_MAIN=/path/to/saturn_main.c
# Each run preserves a unique tests/build/gif-safety/case-* cohort.
swift tests/gif_reference.swift /path/to/passed/cohort/corpus
make -C tests check
```

The complete GNU MCU App/RTX/166-vector link grows112 load bytes:
59,684→59,796, leaving5,740 ITCM bytes. Static DTCM falls60,792→60,752 bytes.
Reserved dictionary/scratch falls28,672→20,480 bytes; that8 KiB change concerns
fixed workspace, separate from the40-byte static decoder-state reduction.
Direct initializer/timer/exit frames remain112/24/8 bytes; frame decoding
falls72→64. Direct frames exclude callees/interrupts. Menu inputs are unchanged;
the full menu build is checked separately. No new heap/thread/stack allocation
is introduced. Bounds validation adds work; removed copies, per-pixel checks and
function pointers are source changes, not measured physical speed gains.

State/publication order is also inspected in the linked ARM code: initialization
has a barrier, the timer retains exclusive compare/exchange and barriers, and
stop waits for reader release. This establishes compiler output, not physical
bus/cache synchronization or complete concurrency qualification. It assumes
one timer caller and serialized init/stop callers. General CD ownership and
initial graphics-consumer handshakes remain outside this correction.

Disposal2/3, logical background color, palette changes affecting retained pixels,
plaintext/user-input control and full compositing are not implemented here.
The decoder is not claimed fully GIF89a compliant. These source defects do not
establish the cause of an untraced physical freeze or any game's behavior.
Exact assets and physical animation/audio behavior from
[issue345](https://github.com/tpunix/SAROO/issues/345) remain unqualified; its
cached comments motivate the separate scheduling work below. No hardware,
USB, SD-card or installed-firmware operation is performed.

The integrated public check passes253 Python methods,14 Rust checks and native
C controls. All56 current MCU inputs match the complete build receipt. The
complete menu link retains identical code/data/BSS/stack/image/font values and
bytes; all36 source inputs match its unchanged receipt. Compiler load/stack
accounting remains GNU qualification, not original MDK or device execution.

## MCU-GIF-002: service animation from the existing low-priority task

The baseline at `dbafcd8` invokes the GIF timer only when the disk worker is
idle. Its menu PCM loop repeatedly services music and waits on TIM6's semaphore,
so it can miss the idle path indefinitely. The source explains why removing
menu music can restore GIF progress, as reported in the preserved comments on
[issue345](https://github.com/tpunix/SAROO/issues/345). This is a menu scheduling
defect; exact report assets and physical behavior remain unqualified.

Public `main_task` runs `simple_shell` after startup. Replacing that call with
the private build's background loop would discard the original UART shell.
Instead, retain the full shell and line editor. While App input is unavailable,
use the existing `_getc_tmout(0)` readiness probe, service GIF work, and sleep
for two ticks. Zero timeout performs one status probe and never busy-spins.
Ready input is consumed normally; echo, editing, history and command dispatch
remain unchanged. The BOOT branch retains its original blocking UART reader.
Startup and UART driver sources are unchanged. There is exactly one timer
caller, in the existing low-priority task; the disk worker no longer decodes.

No new task, stack, heap object or RTOS synchronization object is allocated.
Effective priorities remain startup8, disk24, command worker31 and LED40.
The preexisting unused High attribute at disk creation is deliberately not
corrected here: passing NULL retains the actual Normal priority. At the
configured100 Hz, two idle ticks request50 wakeups/second, compared with the
old UART poll's five. This is up to45 extra idle wakeups/second before execution
cost, including when GIF service has stopped. No physical CPU/pacing saving is
claimed. Long-running interactive shell commands can postpone GIF service until
their command/input loop resumes; they are not a separate worker in this design.

The original **paps-g** change is
`f285ede892f2e76189b570626f169ab42ba20d29` (2026-09-06). This port adapts its
scheduling to public main's shell, omits all private USB reload gates and
acknowledgment changes, and imports its bounded decoder simplifications:
precompute the transparency sentinel once per frame and track remaining pixels
rather than total plus emitted counts. Valid palette indices are0–255, so the
unsigned -1 sentinel cannot hide a pixel when transparency is disabled. Check
string length before subtracting; accept the terminator only at zero remainder.
Pixel bounds, cancellation and output remain covered by the decoder tests.

The pinned [Arm RTX source](https://github.com/ARM-software/CMSIS_5/blob/b5f0603d6a584d1724d952fd8b0737458b90d62b/CMSIS/RTOS2/RTX/Source/rtx_thread.c),
`osRtxThreadDispatch` (lines430–459), switches to a ready higher-priority thread.
Its default thread creation priority is Normal. The original configured
`RTX_Config.h` supplies100 Hz, disabled round robin and4,096-byte default stacks.
These source requirements support preemption, not a physical audio deadline.
`tests/vendor/rtx` contains the exact dispatcher extraction, original Arm
copyright header, Apache2.0 license and pinned full-file/function hashes.
It matches the SDK provenance used by the complete public MCU build. This is
an unchanged host-test dependency, not a replacement RTOS or new firmware code.

Six public scheduling methods compile actual startup/task creation, UART reads,
line editing and that dispatcher. App progress is checked after initialization,
including modeled music waits with dispatcher preemption. Separate controls
retain BOOT timing/no GIF calls, App/BOOT backspace and history, and all four
original task priorities. Eighty dispatcher cases cover explicit/ready-list
paths at priorities1–40. Twelve modeled idle waits require exactly18 UART
register references and twelve GIF services; a static caller check guards the
single-owner placement. The baseline fails three methods; BOOT, editing and
priority controls pass. Corrected sanitizer/O2 runs pass all six methods,
six native invocations and the separate caller check. Hardware registers,
semaphore waits, task operations and GIF execution are modeled boundaries;
complete decoder code is exercised by the ten safety methods, not this stub.

Five additional mutations are rejected: omitted background service (three
failures), slow idle cadence (two), a positive timeout that busy-polls (two),
raised background priority (one), and lost backspace editing (two). The source
outside the shell polling block, startup and UART driver is byte-preserved.
Linked App code independently contains the GIF/probe/yield calls in the line
editor and no GIF call in the disk worker. Ordinary UART command code is not
claimed generally hardened by these focused controls.

The complete MCU link saves16 load bytes versus decoder safety:
59,796→59,780, with5,756 ITCM bytes free. Static DTCM60,752 and fixed
scratch/dictionary20,480 remain unchanged. Direct main/shell/editor/disk frames
remain8/248/32/80; initializer/timer/exit remain112/24/8 and decode remains64.
These are individual frames, not complete task/IRQ peaks. The whole series
adds96 load bytes versus `24c5c73`, saves40 static DTCM bytes and8 KiB fixed
scratch, and adds no allocations. No installed firmware is changed.


The final complete public check passes259 Python methods,14 Rust checks and
native C controls. All16 GIF safety/scheduling methods pass both sanitizers and
O2. Both final build receipts match current inputs; menu source and complete
menu bytes remain unchanged. The BOOT line-editor preprocessed C tokens match
the accepted baseline exactly. Original MDK/BOOT-image execution, real-time
pacing, hardware UART/audio/video behavior and exact issue345 assets remain
unqualified. No physical cartridge or installed firmware is touched.

Each public source/ledger checkpoint is reviewed in full and scanned for
secrets/private PII, including commit text and approved noreply identities.
Only synthetic/public inputs, public contributor handles, primary citations
and sanitized summaries are published. Raw paths/configuration/logs and
source/evidence receipts remain local and ignored.
