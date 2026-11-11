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

Five methods pass ASan/UBSan and O2: all16 retained-flag combinations at both
startup sites,50 successive restarts, all32 payload sectors/backpressure, exact
DMA addresses/count/format and an ordinary no-fault preservation control.
The preceding source fails four new checks and passes that preservation control.
Six compiled behavioral controls detect preceding source, either omitted
acknowledgment, missing SUSP clear, unsupported clear bits and acknowledgment
after enabling. An initial fixture incorrectly counted only three address casts;
the source has five. That setup failure is preserved and excluded from defect
reproduction.

Complete GNU MCU App/RTX/166-vector load remains60,888 bytes with4,648 ITCM free.
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

Reproduce the startup failure alongside its passing ordinary control:

```sh
mkdir -p tests/build
git show c890af9:Firm_MCU/Saturn/cdda_i2s.c > tests/build/audio-before.c
SAROO_AUDIO_SOURCE="$PWD/tests/build/audio-before.c" make -C tests check-audio
```

The independent DMA bank-switch counterexample still fails after this startup
correction: automatic hardware selection can reach a sector before publication,
before its completion ISR can intervene. That is evidence for MCU-AUDIO-002,
not a passing startup test or a claimed audible observation. Ongoing underrun,
FIFO/recovery ordering and residual replay remain for the next coherent port.

The later frame-safe stop, session cancellation and tested continuity policy are
being reviewed as dependencies before that port. Historical private builds and
game observations do not qualify this public implementation. No named-game or
upstream issue is closed by this source/host result.
