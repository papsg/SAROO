# CD audio startup, ownership and recovery safety

Starts at public `c890af9`. Selected **paps-g** `e019e57c`, `104b25fb`,
`e8a47c86` and `247c1dbd` address self-reported MCU-AUDIO-001–005. Port direct
source changes with independently reproduced behavior and valid controls.
The warning/replay corrections repair private intermediate regressions; they
are dependencies of the recovery implementation, not separate claims against
the original public firmware. USB reload/handoff changes bundled into the
private continuation are excluded. Runtime diagnostics are excluded.

## MCU-AUDIO-001: retained startup status

The current public initializer and DMA-start helper disable/re-enable SPI2
without acknowledging retained I2S status. A retained SUSP blocks CSTART, and
underrun acknowledgment is separate from peripheral enable. Add one IFCR write
at each startup site, with SPE zero, before enabling and starting the interface.
Only the four valid I2S clear bits are used. No format, rate, wiring, queue,
DMA-bank selection, interrupt/callback or ordinary payload behavior changes.

The authority is ST's [RM0433 Rev8](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf):
§50.9.3/Table417 p.2194 lists the I2S-usable clear bits; §50.9.7 p.2203 gives the
startup sequence and SUSP/CSTART restriction; §50.11.1 p.2218 distinguishes SPE
disable from status acknowledgment and explains FIFO clearing; §50.11.7 p.2227
defines UDR acknowledgment. The cached ST-authored PDF is SHA256
`9ba54135736a47a302cdc28fdc7ca8f23c6eed5a646fa0a639af6ed2ad71336e`.
SPI-only transfer-length/end/underrun-pattern features are not I2S controls.

Adapted from paps-g `e019e57c`. The public fixture compiles the actual source
through fill_audio_buffer, removing only unrelated setter/polling helpers and
widening the five MMIO address casts plus fixed ring-address conversion for a
64-bit host. Its independent write-one-to-clear register contract retains flags
across SPE toggles, rejects unsupported clear bits and checks acknowledgment
before enable. It is not a physical clock, receiver or audio-timing model.

At that checkpoint, five methods pass ASan/UBSan and O2: all16 retained-flag combinations at both
startup sites,50 successive restarts, all32 payload sectors/backpressure, exact
DMA addresses/count/format and an ordinary no-fault preservation control.
The preceding source fails four new checks and passes that preservation control.
Six compiled behavioral controls detect preceding source, either omitted
acknowledgment, missing SUSP clear, unsupported clear bits and acknowledgment
after enabling. An initial fixture incorrectly counted only three address casts;
the source has five. That setup failure is preserved and excluded from defect
reproduction.

At startup checkpoint `7984fe2`, complete GNU MCU App/RTX/166-vector load
remains60,888 bytes with4,648 ITCM free.
Static DTCM remains60,312 and the main-stack/heap address gap70,760. Direct
initializer/DMA-start/producer/IRQ frames are0/12/24/0 bytes, unchanged from
the preceding build. All56 public inputs bind to the current source;85 SDK
inputs match baseline. No retained field/buffer/allocation/task/poll/wait/copy
or SD I/O is added. The cost is two cold-path peripheral stores; no hardware
speed or complete IRQ/stack peak is measured. GNU output is not original-MDK
or physical Saturn/game qualification. Nothing is installed.

```sh
make -C tests check-audio
SAROO_AUDIO_PROFILE=release make -C tests check-audio
```

The maintained recovery fixture below replaces the startup-only fixture and
retains its flag/queue/format coverage. Reproduce the historical startup-only
failure alongside its ordinary control in a detached public checkout:

```sh
git worktree add --detach tests/build/audio-startup-check 7984fe2
mkdir -p tests/build/audio-startup-check/tests/build
git show c890af9:Firm_MCU/Saturn/cdda_i2s.c > tests/build/audio-startup-check/tests/build/audio-before.c
SAROO_AUDIO_SOURCE="$PWD/tests/build/audio-startup-check/tests/build/audio-before.c" \
  make -C tests/build/audio-startup-check/tests check-audio
```

The independent DMA bank-switch counterexample still failed after that startup
correction: automatic hardware selection could reach a sector before publication,
before its completion ISR can intervene. That is evidence for MCU-AUDIO-002,
not a passing startup test or a claimed audible observation. The following
coherent port addresses ownership, ongoing underrun and FIFO/residual recovery.

## MCU-AUDIO-002–005: coherent ownership and recovery

Adapt paps-g `104b25fb`, with its required `e8a47c86` warning and `247c1dbd`
residual corrections folded into the introducing change. Publishing the older
intermediate recovery engine would introduce already known regressions.

The original DMA ISR could program an unpublished sector, and automatic bank
selection could read it before the next ISR. The source counterexample selects
stale slot2 with only slots0/1 published. The preceding running path also leaves
UDR latched through eight completions/publications; ordinary controls pass.
These are source/register-contract reproductions, not audible observations.

Use monotonic read/write positions and publish only after the payload stores.
Every DMA address names a published sector or immutable silence. Late inactive-
bank publication checks the remaining count and pending completion/error; a
hardware switch can still race CPU masking, so active-bank write-error recovery
remains necessary. Retain all32 payload sectors and one2352-byte producer copy.
Callbacks remain registered through the existing API and notify real completed
payloads, including low-water completion; silent banks do not generate callbacks.
The internal descriptor-start helper is replaced; declared public APIs remain,
with one new spi2_flush entry used by the actual worker for an accepted final
audio sector with no remaining repeats. It never flushes failed admission,
data/file/directory delivery or a repeat that still needs more audio.

DMA EN clearing is asynchronous. A one-shot TIM7 retry defers rather than spins
until ownership is released. Startup prefills the serial FIFO before CSTART;
running underrun masks its interrupt until a full FIFO and acknowledged UDR
permit channel-correct progress. Recovery retains valid FIFO words and resumes
the exact stable NDTR tail in normal mode, then restores full DBM. Quiet repair
preserves stereo parity. TEIF enters stopped-stream recovery; nonfatal FEIF
backpressure retains normal completion accounting. Persistent faults retry at
bounded intervals instead of immediately interrupting again.

ST [RM0433 Rev8](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
§15.3.11 pp.644–645, §15.3.14 pp.647–649 and §15.3.19–20 pp.653–655 cover DBM,
asynchronous disable, active-bank protection and error distinctions. Table415
p.2159 gives SPI2's16-byte FIFO; §50.9.12 pp.2209–2210 describes I2S underrun
and phase; §50.9.16 p.2214 supplies programming sequences. The startup
and stop/status sections cited above remain applicable. These requirements do
not prescribe this queue representation, retry intervals or publication guard.

[Arm DUI0646C §3.12.4, printed3-175](https://developer.arm.com/documentation/dui0646/c)
defines DMB ordering; the cached CMSIS __DMB implementation supplies the compiler
memory clobber and system barrier. Payload visibility is ordered before queue/
bank publication. DMB does not clean a write-back cache. The ordinary public
cold boot does not enable D-cache; another boot/cache policy needs its own
coherence qualification. The model's visibility fence is not a cache simulation.

### Necessary frame and interrupt adaptations

The historical33-method recovery fixture passed while missing a stop-contract
violation. Adapt the relevant quiet-stop part of paps-g `2617a839`: request
suspension before clearing SPE, and let only real TIM7 update flags consume the
two grace intervals. Stale SPI/TIM7 entries neither rearm nor shorten them.
CSUSP reads zero; use readable CSTART and, additionally in this public port,
require SUSP acknowledgment before the normal stop. After both timer expiries,
a stalled clock can take the DMA-first forced-abort path. That exceptional path
does not claim an unfinished frame completed. Full call/IRQ timing is unmeasured.

A second public adaptation fixes a reproduced IRQ snapshot race. A completion
or error can arrive after reading LISR but before writing LIFCR. Clearing every
stream flag erased the unseen event: the model reproduced one extra1176-word
sector or stalled playback. Clear only observed stream flags so the later IRQ
can retire/recover its event. RM0433 §15.5.3 p.658 defines the write-one-to-clear
contract. Initialization/rearm may still clear stale flags with DMA stopped.

The late-error fixture initially masked all earlier interrupts, unintentionally
coalescing two completions. Its corrected schedule services prior completions
and delays only the final beat; exact payload assertions remain. That fixture
error and the earlier weak-frame/failed runs are preserved separately. They
are not additional hardware findings. Arbitrarily coalesced completions and
physical interrupt latency are not qualified by the model.

### Validation, dependencies and costs

Thirty-seven actual-source methods pass ASan/UBSan and O2: startup flags,
32-sector capacity/wrap, autonomous preload during copying, exact sample order,
late/pending bank publication, both stereo phases/banks, every1176 residual
count, startup/ongoing underrun, warning/completion combinations, repeated and
permanent faults, delayed EN/TC, single-sector worker flush, quiet tails,
protected normal-mode addresses, real/stale timer events and both IRQ snapshot
races. Hardware underrun repetitions are tracked separately from accepted
payload; exact consumed PCM does not promise inaudible fault output.

Twenty-six compiled recovery mutants are detected in addition to the six startup
controls. The original33-case suite's passing result is historical; the stronger
frame/snapshot counterexamples reject its earlier candidates. One old fixture
expected immediate AUDIO_STOPPING; it now waits for the healthy frame and checks
the delayed DMA acknowledgment without removing payload/ownership checks.
Worker flush is counted directly, independently of playback state. All156 CD
and68 MCU save methods also pass; they retain the earlier public ownership work.

The existing boot config gives TIM7 the same200MHz timer source as TIM6.
PSC1999 gives10us ticks; nominal suspend grace is60us then1ms. These are configured
intervals, not physical response-time bounds. TIM7 is otherwise dormant in
healthy playback. The original vector table is retained: actual ELF entries for
DMA1_Stream1, SPI2 and TIM7 resolve to their strong handlers. No task, game/menu
patch, private control hook or USB integration is needed.

Complete GNU MCU load changes60,888→61,960(+1,072), leaving3,576 ITCM bytes.
Static DTCM falls60,312→60,184(-128); the stack/heap address gap grows to70,888.
The32 payload sectors retain75,264 fixed SRAM1 bytes; one immutable zero sector
adds2,352 bytes, for77,616. It ends at0x30014f30, below SRAM1's0x30020000 boundary
and outside the existing4KiB UART prefix at0x30000000. No other fixed workspace
or buffer reservation changes. All56 public inputs and85 unchanged SDK inputs
are verified. Direct init/producer/DMA IRQ/TIM7 frames are8/24/24/8; service16,
arm12, start8 and flush8 are nested, not a measured whole-path stack bound.
No additional payload copy, SD I/O or dynamic allocation is introduced. Silent
DMA traffic is temporary for FIFO drain/recovery; no CPU-cycle saving is claimed.

### Later policy and hardware limits

The later ordinary-session cancellation and tested continuous-silence default
were reviewed. This port does not introduce that cancellation operation: healthy
stops follow a complete zero tail and stereo frame, checked before serializer
reset in the independent model. Adding indefinite idle transmission/session
retirement is separate policy, with separate DMA/IRQ costs; neither the retired
workaround nor private profile/diagnostic machinery is imported.

[Sega ST-077-R2, printed p.80/PDF92](https://antime.kapsi.fi/sega/files/ST-077-R2-052594.pdf#page=92)
describes external digital audio mixing separately from internally generated
sound. It does not establish the receiver's behavior with missing clocks.
Game mixer state is not altered. Related historical upstream reports
[#182](https://github.com/tpunix/SAROO/issues/182),
[#199](https://github.com/tpunix/SAROO/issues/199) and
[#357](https://github.com/tpunix/SAROO/issues/357) supply symptom context, not
proof of these root causes or resolution. Historical private builds/game results
belong to their exact cohorts; they do not qualify this public GNU port.
Nothing is installed, and physical timing/audio/game/MDK acceptance remains open.
