# CD initialization and selector-reset safety

Starts at public `0010a39`. Selected **paps-g** `3772326a`, self-reported
**MCU-CDC-017**, is adapted in separate handoff, initialization-policy and
selector-reset changes. Preserve original notices and the newer public buffer,
directory, request, playback-position and cache ownership corrections. No
upstream issue or PR is claimed as the origin of this finding.

## Bounded initialization handoff

Initialization command `0x04` previously changed the decode policy before its
reader acknowledged pause. Repeated two-tick semaphore waits had no retry bound,
so an unresponsive worker could hold the command indefinitely. Reuse the public
pause_drive helper before changing settings or reclaiming buffers. It wakes the
worker once and permits at most 500 waits; failure returns WAIT with the request
still pending. Settings, buffers, FIFO, interrupt state and reset completion are
not prematurely changed. A late acknowledgement permits retry without pretending
to undo producer progress. This is a wait-count bound, not a physical deadline.

Sega [ST-162-062094 §8.2.1, function 1.7, printed pp.78–79](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=66)
defines initialization parameters, defaults and reset completion.
The [later CDC_CdInit §8.2.1, function 1.7](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p08_11.htm)
explicitly requires pausing at the current position; the cached Japanese original
agrees. Its speed/standby ranges differ from the older PDF. This correction does
not implement those controls or infer hardware timing from them. The retry bound
and internal producer handoff are SAROO implementation policy, not Sega-mandated
semaphore behavior.

The actual command, report, helper and both worker acknowledgement branches run
in the public drive fixture. Across sector, file and directory owners and both
branches, it compares complete command state and FIFO/IRQ registers at every
wait. Cases include timeout, delayed retry, acknowledgement at the final allowed
wait, acknowledgement between commands, directory retirement, block ownership,
retained position/range/endpoint/repeat state and unchanged same-reader cache.
Physical scheduling, bus/cache coherence and wall time are not modeled.

Twenty-one drive methods and 52 related connection/copy/PUT/bounds/routing
methods pass ASan/UBSan and O2. Six compiled behavioral controls are detected:
preceding source, unbounded initialization, early policy change, premature block
release, missing wakeup and false timeout success. The preceding source retains
its exact valid report/log control. An initial fixture incorrectly expected
WAIT to erase a retained low report byte; the corrected oracle preserves it.
Compiler-only failures are not counted as defect reproduction.

Complete GNU MCU App/RTX/166-vector load changes 60,888 → 60,872 bytes, leaving
4,664 ITCM bytes. Static DTCM remains 60,312 bytes and the main-stack/heap address
gap remains 70,760 bytes. Direct initialization and disk frames remain 24/72
bytes; the shared pause helper adds a nested 16-byte frame where the previous
wait wrapper had no direct frame. Full call-chain/interrupt peaks are unmeasured.
No new field, allocation, task, payload copy, per-sector work or SD I/O is added.
All repository/SDK inputs are hashed; GNU accounting is not original-MDK or
Saturn/game qualification. Nothing is installed.

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
```

For the original handoff failure with its valid control, save the preceding
source without changing branches:

```sh
mkdir -p tests/build/reset-before
git show 0010a39:Firm_MCU/Saturn/saturn_cdc.c > tests/build/reset-before/saturn_cdc.c
git show 0010a39:Firm_MCU/Saturn/saturn_main.c > tests/build/reset-before/saturn_main.c
cd tests
SAROO_CDC_DRIVE_DIR="$PWD/build/reset-before" python3 -B -m unittest \
  test_cdc_drive.CDDriveTests.test_initialization_handoff_is_bounded_and_defers_all_mutation \
  test_cdc_drive.CDDriveTests.test_baseline_valid_position_metadata_and_exact_logs
```

The copy-only fixture asserts that its reset controls are idle. Active handoff
uses the real shared helper and acknowledgement branches in the drive fixture;
the connection fixture also extracts the shared helper. No private preparer,
runtime diagnostic or device is required.

## Initialization flags, reports and reset defaults

The next separate adaptation of paps-g `3772326a` removes three reachable
initialization defects: bit7 skipped pausing and falsely returned reset-completion
flags; ordinary nonreset initialization left the previous report in the registers;
and software reset retained the request's nondefault decode flags. The same Sega
§8.2.1/function1.7 references above specify current-position pause, keep-settings,
zero-valued initialization defaults and reset-only ESEL completion. The later
English and Japanese copies agree on these points. Bit7 retains its existing
precedence over other flag bits; unsupported standby/ECC/retry controls are not
introduced by this port.

After successful handoff, bit7 retains the supported settings, refreshes the
report and returns no reset-completion flags. Ordinary initialization applies its
flags and refreshes all four report words. Software reset sets the decode policy
to zero before the existing FIFO/filter/buffer cleanup, retaining the current
position, play range, endpoint types and repeat budget. Inactive OPEN/NODISC
states remain unchanged. Failure still returns WAIT before policy or cleanup.
No extra cache invalidation is added; the per-sector mode snapshot remains.

Twenty-three actual-source drive methods and the same52 related methods pass
ASan/UBSan and O2. Every flag byte is checked with active/idle readers through
both real worker branches (1,024 combinations), plus512 OPEN/NODISC flag cases.
Independent exact reports, reset-only completion, FIFO/IRQ state, all selector
defaults and retained ownership/playback/cache state are checked. Keep-settings
now also participates in the timeout/late-retry matrix. The preceding source
fails both new flag groups while retaining the exact valid report/log control.
Eight additional compiled behavioral controls detect skipped keep-settings pause,
stale/partial reports, retained reset flags, changed keep-settings policy, false
keep-settings completion and missing reset completion, as well as preceding
behavior. Two older fixtures explicitly expected the false completion; those
oracles are corrected while preserving their unrelated ownership checks.

GNU MCU load changes60,872→60,888(+16), leaving4,648 ITCM bytes. Static DTCM,
stack/heap address gap and direct command/disk/helper frames remain unchanged;
all56 public inputs and85 unchanged SDK inputs are verified. No field, allocation,
task, payload copy, per-sector work or SD I/O is added. Ordinary/keep-settings
initialization now writes its required four-word report. Nothing is installed;
host/GNU evidence is not physical Saturn/game or original-MDK qualification.

## Bounded selector reset and worker wakeup

The third adaptation of paps-g `3772326a` replaces selector reset's open-ended
wait with the same bounded helper. Command `0x48` previously set the pause request
without waking the worker. When its existing predicate requires a pause, it now
wakes once and waits before deleting partition data or changing connectors.
Failure returns WAIT with no premature deletion, output disconnection or ESEL
completion; a late acknowledgement/retry can finish normally.

Sega [ST-162-062094 §8.2.5, function5.9, printed p.91](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=79)
and the [later CDC_ResetSelector §8.2.5, function5.9](https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/prgg/sysg/cdif/hon/p08_15.htm)
define selective data/connector/condition reset and ESEL completion. The cached
Japanese original agrees. These define the interface; they do not prescribe
SAROO's worker wakeup or retry count. The existing wait_pause_ok wrapper calls
osSemaphoreAcquire with a two-tick timeout; the cached RTX svcRtxSemaphoreAcquire
uses that timeout when entering its semaphore wait. Repeating finite waits
indefinitely was still unbounded. Arbitrary scheduler/storage/logging failure
can exceed any inferred wall-clock deadline even with the new retry bound.

The existing pause predicate, mode interpretation, invalid selected-buffer
refusal, nonzero-mode ignored buffer number, audio/idle behavior and output
ownership corrections remain intact. In contrast to the original combined
patch, keep both original selector log formats/masks and check them exactly.
Same-reader cache bounds and playback position/range/repeat metadata are retained.

Twenty-six drive methods and the same52 related methods pass ASan/UBSan and O2.
All256 mode bytes run across own/other/disconnected inputs and audio/data/idle
cases (1,280 combinations), checking all selective conditions/connectors and
partition ownership. Timeout/retry checks cover all3 worker types and both actual
acknowledgement branches, complete state during every wait, output transfer
termination, IRQ/FIFO state, unfinished-directory retirement and cache retention.
Eight more compiled behavioral controls detect preceding selector code,
unbounded waits, early deletion, ignored timeout, broadened pause scope, missing
log/wakeup and cache loss. The preceding exact valid report/log control passes.

Complete GNU MCU load remains60,888 bytes with4,648 ITCM free. Static DTCM,
main-stack/heap gap and direct init/disk frames remain unchanged; selector32 and
shared pause16 are nested, with full call-chain/interrupt peaks unmeasured.
All56 public source inputs and85 unchanged SDK inputs are verified. No new
field/allocation/task/SD I/O/payload copy/per-sector work is added. A qualifying
selector reset adds one worker wakeup. Existing deletion scans remain bounded
by200 blocks and24 selectors; logging can still wait.

The same saved preceding source can reproduce selector failure by selecting
the test_selector_reset_handoff_preserves_ownership_and_retries and
test_all_selector_modes_preserve_existing_scope_and_pause_decisions methods.
No private preparer or device is needed. Full-suite and final build/preservation
results are maintained in the public progress ledger.

Broader software-reset state, standby/ECC/retry, tray/scan/session, all selector
output-reset modes and routing-wide pause decisions remain separate. The
original source/host work was not installed or hardware-qualified; neither is
this public port. No audio or named-game defect is claimed resolved.
