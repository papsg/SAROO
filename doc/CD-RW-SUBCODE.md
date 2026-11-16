# R–W subcode correctness

## SSR-064: unread-packet overrun notification

Port of paps-g `329466e4`, independently adapted/verified by papsg against the
public publisher, command and existing reset paths. This is our source finding;
no original upstream issue/PR or named-game failure is asserted.

Sega [ST-162-062094 §4.3, printed p.40/PDF p.28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=28)
defines unread-pack overwrite as overrun.
[Section 8.2.3, function3.2, printed p.85/PDF p.73](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=73)
assigns bit1 to that error. The cached original SBL6.01 header defines
`CDC_SCD_OVER=0x02`. Preserved library disassembly reads response byte7
(CR4 low in SH-2 byte order) and stores the flag before starting FIFO transfer.
The individually rehashed header/library identities are recorded in
[file-information wire evidence](CD-FILE-INFO-WIRE.md). Static SDK evidence
does not authenticate the publisher archive or execute its target code.

The software publisher previously displaced unread packets silently. It now
latches0x02 inside the same saved-IRQ exclusion that advances queue cursors.
An accepted R–W command snapshots/acknowledges that loss burst in CR4 low before
FIFO transfer. Q, invalid, empty-WAIT and busy requests do not acknowledge it.
New loss during an older transfer remains for the next command, including after
DataEnd. Queue retirement also retires the flag; non-reset INIT retains both.
SSR-065 below replaces the earlier cache/track-metadata retirement rule with
actual decode-start retirement, preserving explicit tray/software resets.

The public adaptation places the new byte in existing padding after the two
small playback fields. Both actual target CDB symbols remain5,056 bytes, with
unchanged static allocation. This differs from the private field placement;
no format, payload queue, capacity or new telemetry is added.

```sh
python3 -B -m unittest discover -s tests -p test_cdc_subrw_overrun.py -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_subrw_overrun.py -v
python3 -B -m unittest discover -s tests -p test_cdc_subrw.py -v
python3 -B -m unittest discover -s tests -p test_cdc_drive.py -v
make -C tests check-cdc-command-buffer
```

Two methods join the actual publisher/get-subcode/trans-start/FIFO/DataEnd/reset
bodies through the existing bounds fixture. The parent has128 missing-flag
assertions in45,312 ordinary checks and192 in6,208 lifetime checks across all64
cursor starts. Loss-free15-frame, empty, intermediate-consumption, queue data,
response words and transfer completion remain controls. Corrected sanitizer/O2
profiles pass both methods; all six existing publication/preemption and57 drive
methods pass. Zero packet bytes isolate loss signaling from CRC; existing
publication controls separately check nonzero payload/order. Modeled MMIO,
IRQs, Q track lookup and idle reset handoff are explicit fixture boundaries.
The initial fixture-entry extraction refusals ran zero tests and remain excluded.

Fresh LTO/non-LTO MCU builds pass166-vector checks and all67 public-source pins.
Load grows44 bytes per profile to59,528/63,800, leaving6,008/1,736 ITCM bytes.
Data/BSS remain200/50,696 and204/50,688; static DTCM span stays50,896. The
publisher text grows124→136 bytes while its direct frame stays20 bytes in both
builds; this excludes callees and interrupts. Menu/FPGA inputs do not change.

The63 usable software slots are not a claim of physical23/24-pack capacity.
Physical pointer/error-clear timing, pack CRC/error correction,3.2ms deadline,
actual RTOS scheduling/FPGA transfers and game compatibility stay unqualified.
Source snapshots, failed/passing cohorts, target receipts and local primary
evidence remain preserved. No runtime diagnostic, USB integration, new overlay
recipe, firmware installation or device operation is introduced.


## SSR-065: decode eligibility and session lifetime

Port of paps-g `e50ec388`, independently adapted/verified by papsg. This is our
source investigation; no original upstream issue/PR or named-game failure is
asserted. Sega [ST-162-062094 §4.3, printed p.40/PDF p.28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=28)
limits decoding to CDDA, excludes scan/CD-ROM, clears packs when decoding starts
and retains them across seek/pause. [Function2.3, printed p.84/PDF p.72](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=72)
permits attenuated scan audio separately; that does not enable R–W decoding.
These primary requirements were read from the preserved local cache.

The old reader selected sidecar mode from enable/presence alone, and worker
branches also published during data/file/directory reads and audible scan.
Track metadata lookup retired queued packets before actual decoding began.
The correction selects one mode for all reader/cache stages from ordinary sector
CDDA ownership, enable/presence and non-SCAN state. Existing cache-mode comparison
is retained. It allows lazy startup while the report still says SEEK; requiring
reported PLAY would reject a legitimate path. Only admitted ordinary audio
publishes packets. Scan keeps audio/Q/pickup behavior without sidecar delivery.

A separate worker-owned session byte consumes the remaining public CDB padding.
Pause/INIT, scan, ineligible reads, read failures and completion/repeat boundaries
end decoding without discarding retained packets. The first subsequently accepted
decoded packet clears the old queue/loss under saved-IRQ exclusion. Metadata
queries and uninterrupted retained PLAY updates preserve the session/queue.
Tray/software reset retain their explicit retirement. Continuous track-number
changes are preserved; the manual does not settle every such decode boundary.

The actual reader/publisher/worker fixture adds540 eligibility checks across
ordinary/lazy audio, data/file/directory, absent owner, scan, enable and two
sidecar formats. Its1,920 decode-start checks cover64 cursors, three retained
queue sizes and both initial IRQ masks. Using the final fixture and actual
public-parent main/header reproduces56 and960 semantic failures (exit42).
Current sanitizer/O2 profiles pass all eight publication/retention methods.
The metadata test separately preserves cursor, pending loss and packet bytes.

Six actual-FatFs sidecar methods preserve all18 audio frames/2,352-byte payloads,
planar conversion, EOF/fault/mode-snapshot and deferred-publication controls.
Their former data-track decoding expectation is corrected to legal audio input;
the32KiB reader window consequently holds13 full audio frames. A separate cooked
CD-ROM control checks every2,048-byte payload, zero sidecar reads/publications,
unchanged retained ring/loss and ended decoder. Shared fixture state mirrors the
new bytes; its facade is not a target CDB layout claim. Fifty-eight actual drive
methods include retained-PLAY, pause/INIT and scan-command session controls.
The two overrun methods still pass. IRQ, storage completion, FIFO and scheduling
boundaries remain explicit models; these are not physical CDDA/CD+G results.

```sh
python3 -B -m unittest discover -s tests -p test_cdc_subrw.py -v
SAROO_CDC_PROFILE=release python3 -B -m unittest discover -s tests -p test_cdc_subrw.py -v
python3 -B -m unittest discover -s tests -p test_cue_subcode.py -v
python3 -B -m unittest discover -s tests -p test_cdc_drive.py -v
# Optional exact preimage main/header overrides:
# SAROO_CDC_SUBRW_SOURCE=/path/to/saturn_main.c SAROO_CDC_SUBRW_HEADER=/path/to/cdc.h
```

Fresh166-vector MCU builds retain all67 public-source pins and5,056-byte CDB.
Relative to SSR-064, LTO load adds8 bytes to59,536/free6,000; non-LTO saves8 to
63,792/free1,744. Data/BSS/static DTCM spans stay unchanged. Publication is now
inlined into the sole worker caller; direct disk frames stay96 LTO and88→80
non-LTO, while reader frames remain80/48. These exclude callees/interrupts and
do not establish whole-program stack or runtime speed.

Original and intermediate failed evidence remains distinct: an initial driver
syntax error, unavailable fixture state, an uncompiled scan-helper call, and
old fixture assumptions about cache mode/masked register writes were corrected
before acceptance. The broader CD command run completed103 methods before its
sector-reader setup rejected two now-unused IRQ seams under `-Werror`. Those
seams are explicitly unused for the new reader while retained for parent-source
controls; bodies/assertions and strict warnings stay intact. Only that failed
suite and the unexecuted suffix are resumed. The directory fixture likewise
marks its no-longer-used publication seam explicitly unused. A shared disc-save
handoff facade receives the session byte. Both complete worker scan tests now
require zero R–W publications and retained queue/loss, while preserving all audio,
pickup, Q, pacing and backpressure assertions. The original six-method parent failures and the first two
added-case failures are preserved; only the latter's corrected exact-parent
rerun supplies the semantic counts above. No product gate was weakened to make
tests pass. Native/source/test cohorts and primary evidence remain archived.

Physical pre-roll,3.2ms transfer timing,23/24-pack capacity, pack CRC/correction,
analog scan attenuation, actual RTOS/FPGA transport and game compatibility remain
unqualified. No new timing rule, queue capacity, runtime diagnostic, USB feature,
overlay recipe, installed firmware or device operation is added.


Combined CD verification covers261 distinct passing methods across the completed
prefix and resumed suffix. Every failed scan method has a passing corrected
replay; setup errors are excluded. The MCU save suite passes103 other methods
plus the corrected15-method disc-save class,118 total. Completed passing groups
were not rerun merely to replace their earlier logs. These remain scoped offline
checks, not a claim that every unrelated public target or a physical game ran.
