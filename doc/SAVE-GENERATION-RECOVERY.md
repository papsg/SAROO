# Save-generation recovery

## Journal record core (staged; not enabled)

This checkpoint adds the record encoder, bounded publisher and validator/replayer
as direct menu source. `save_journal.c` is **not in the menu Makefile's object
list**. It changes no installed firmware or current save behavior. The later
planner, menu ownership and activation are required before an interrupted save
can be recovered. In particular, this checkpoint is not a completed durability
fix.

The original implementation was authored by **paps-g** in private commits
`3c773b0d` (record core), `6ff1e686` (bounded streamed publication), and
`d4847f56` (bounded recovery); the selected core was refined alongside
`5e318867`. This port retains the selected SJR1 format and source logic. Its
only C adaptation casts both operands of one compile-time size assertion to
`uint32_t`, so the public strict target compile does not issue an enum-type
warning. The copied test cases retain the original authorship. No private
preparer, patch framework, USB source or diagnostic command is imported.

The record contains a 512-byte header, big-endian offset/length descriptors,
and opaque backup bytes. It describes one 64 KiB internal-save slot, or an
8 KiB expansion-save root plus a selected 1 KiB header and at most 479 payload
blocks. A claims bitmap rejects duplicate expansion blocks. Header and body
CRCs, geometry, reserved bytes and exact EOF are checked before replay. A
published header commits the otherwise inactive body; clearing happens only
after the caller confirms primary writes. The publisher limits each file
request to 65,536 bytes. Recovery keeps its table, CRC checkpoints and transfer
window inside one 65,536-byte workspace, validating the whole record before
any primary write and rechecking each chunk before applying it.

These are SAROO's recovery rules, not a journal format specified by Sega. Sega's
[Backup Library §2.1, printed p. 42](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=47)
defines the backup directory and data size; [§2.3 function No. 5, printed p. 45](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=50)
defines the write API and its results. The eventual file adapter must confirm
the underlying [FatFs `f_sync`](https://elm-chan.org/fsw/ff/doc/sync.html) and
close outcome before reporting a durable write. The existing public
[archive-owner transport and command acknowledgments](SAVE-OWNERSHIP-RAM.md)
are prerequisites, not duplicated by this module.

### Reproduce the checks

```sh
python3 -B -m unittest tests.test_save_journal_core -v
SAROO_SAVE_JOURNAL_PROFILE=release python3 -B -m unittest tests.test_save_journal_core -v
tools/target/debug/firmware-build menu --repo "$PWD" --out tests/build/save-journal-menu
```

The two host profiles each pass three methods: 2,960 record/bounds/corruption/
publication checks, 67,388 split-publication cases and 317 bounded replay cases.
Python independently decodes all four emitted wire variants and checks their
CRCs. The original baseline has no SJR1 API, so these are new component
contracts, not a claim that the unmodified menu previously attempted journal
replay. The in-place interrupted-save defect must be reproduced and corrected
at the later activation checkpoint.

The pinned offline Yaul SuperH compiler also builds `save_journal.c` as a
freestanding C99 object with `-m2 -mb -Os -fomit-frame-pointer -Wall -Wextra
-Werror -fstack-usage`. It reports **3,537 text/constant bytes**, no data or
BSS, and an 80-byte largest direct function frame. These are object and direct
frame measurements, not linked or whole-program stack costs. The normal menu
still links 88,040 code/data and 9,508 BSS bytes, reserves a 16,384-byte stack,
has 17,140 bytes of body margin, and produces a 488,770-byte image. Its object
list and resulting behavior are unchanged.

The host callbacks model confirmed, ordered file operations and selected
corruption/failure schedules. They do not model torn sectors, SD cache
reordering, power rails, the cartridge bus, concurrent external writers,
Saturn execution or game behavior. A CRC detects modeled corruption but does
not authenticate a record. The core allocates no new task, heap object or
resident payload; integrating it must still prove exclusive workspace and
save-owner lifetimes.

## Allocator projection (staged; not enabled)

This second checkpoint adds the selected planner as direct source. It is still
unlinked, and the normal menu image is byte-identical to the preceding public
checkpoint. The planner uses the existing backup allocator's directory, header,
bitmap and pending-save state to describe the bytes a transaction will publish.
For an internal-save change, the allocator first updates its RAM shadow; a
root-only journal record then references the complete 64 KiB shadow. For an
expansion-cart write, the planner copies only the 8 KiB root and 1 KiB selected
header into a bounded prefix and references the caller's payload. This avoids a
second resident 479 KiB payload. It performs no file I/O and must run while the
allocator state, work buffers and caller payload are exclusively owned.

**paps-g** authored the original planner in private `3c773b0d` and the split
representation in `6ff1e686`, later selected by `5e318867`. This public port
keeps the two production planner paths. It excludes the older experimental
contiguous internal-save projection: that path assumed a different internal
layout and is not called by the selected runtime. The public tests and target
compile fixture are new adaptations of that work; the existing public save
storage model and allocator source supply their behavior oracle. The planner
does not change the game's backup API. Sega's [Backup Library §2.1, printed
p. 42](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=47)
defines the directory and payload size, while [§2.3 function No. 5, printed
p. 45](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=50)
defines writes and their results. SJR1 and the planner are SAROO mechanisms,
not Sega-defined formats.

Run `python3 -B -m unittest tests.test_save_journal_plan -v`, then repeat with
`SAROO_SAVE_JOURNAL_PROFILE=release`. Each profile compares the planned record
to the **complete** persisted archive in 44 cases: new writes, overwrites,
inline and block boundary sizes, the maximum 479 KiB expansion payload, and
deletion for both devices. It checks neighboring saves after a modeled RAM
discard/reload, rejects misaligned work/header pointers, a mismatched size and
an invalid expansion block, checks workspace guards, and confirms planning
adds no storage commands or changes live root/header bytes. The target compile
fixture `tests/target_save_journal_plan.c` includes the actual menu allocator
and planner; it passes with the pinned offline Yaul SuperH compiler using C99,
`-Os -Wall -Wextra -Werror -Wno-sign-compare`. The signedness exception covers
an unrelated pre-existing date comparison in `game_save.c`.

```sh
PYTHONPATH=tests python3 -B -m unittest test_firmware_save_resize -v
docker run --rm --pull=never --platform linux/amd64 --network none \
  -v "$PWD":/work:ro \
  'ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6' \
  /opt/tool-chains/sh2eb-elf/bin/sh2eb-elf-gcc \
  -m2 -mb -ffreestanding -fno-builtin -fomit-frame-pointer \
  -std=c99 -Os -Wall -Wextra -Werror -Wno-sign-compare -fstack-usage \
  -I /work/Firm_Saturn -c /work/tests/target_save_journal_plan.c \
  -o /tmp/save-journal-plan.o
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/save-journal-plan-menu
```

The normal menu build still links 88,040 code/data and 9,508 BSS bytes with
17,140 bytes of body margin and the same image SHA-256
`d3ec0d5a6b96f14d00c7d502cdd4a9d518747f4b9c3bc8cc81a7409fdae61fc2`.
This proves no release behavior change at this stage. The modeled store does
not prove FatFs durability, power-loss ordering, actual Saturn behavior, or
recovery from an interrupted transaction. Those require the later runtime,
command ownership, launch binding and activation checkpoints.

## Transaction runtime (staged; not enabled)

The third checkpoint adds `save_runtime.c` and `save_runtime.h` as unlinked menu
source. **paps-g** authored the original binding in private `5e318867`, with
later session and input-admission corrections retained in the selected source.
This public adaptation tests it against the current public allocator and
archive-owner interface, without importing the private patch framework. The
test injects the prospective call sites into a temporary copy of `game_save.c`;
the standard menu still links neither the runtime nor the SJR1 core.

The runtime writes an inactive journal body to `/SAROO/SAVEJRN.BIN`, then its
checked header. After publication, it applies the selected bytes through the
existing primary archive owner, synchronizes, clears the journal header and
reloads the backup shadows. A failed reply can mean that the journal is already
durable, so admission must replay or refuse it before another operation. The
record's 16-byte game key is retained separately from the MCU's selected key.
The implementation reuses the existing 65,536-byte file workspace beginning at
`0x220c7100`, immediately after the 64 KiB MEMS data window, and the allocator's
1,008-byte claims bitmap. The owner must exclude concurrent use of both for the
whole transaction. [FatFs `f_sync` documentation](https://elm-chan.org/fsw/ff/doc/sync.html)
describes the required file synchronization; the public
[archive-owner contract](SAVE-OWNERSHIP-RAM.md#access-the-existing-save-archive-handles)
describes the selected primary handles and exact command counts. Sega's
[Backup Library §2.3 function No. 5, printed p. 45](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=50)
defines the game-facing write result, not SAROO's journal protocol.

The public host fixture runs the real allocator, planner, core and runtime with
synchronous modeled file and archive owners. Each of its ASan/UBSan and O2 runs
passes 44 cases: normal create/overwrite/delete/format, inline and block sizes,
the 479 KiB expansion limit, body-write failure, lost publication reply, failed
and partially applied primary writes followed by recovery, and corruption that
blocks replay. The normal-operation oracle is the same allocator's complete
legacy persisted image, including its neighboring and unused bytes. The public
owner's directory lookup, FatFs implementation and command register timing are
covered separately; this fixture models only their synchronous contract.
There is no file/MCU command timeout or physical power-cut qualification here.

An isolated SuperH menu **cost link** uses the test integration and the selected
core with `-ffreestanding` on the two affected objects. It links 93,064
code/data bytes, 9,532 BSS bytes and the existing 16,384-byte stack reserve,
leaving 12,092 body bytes. Against this checkpoint's ordinary menu, that is
+5,024 linked code/data and +24 BSS bytes. The largest direct runtime frames
in the cost link are 80 bytes for the MEMS write planner and 52 for the root
commit helper; these are not whole-call-chain stack peaks. The standard menu
still produces the preceding byte-identical image with 17,140 body bytes free.
The isolated cost link is **not an installable or production-qualified image**:
it has no command/workspace exclusion, launch binding or startup recovery.

Reproduce the host checks with `python3 -B -m unittest
tests.test_save_journal_runtime -v` and the same command under
`SAROO_SAVE_JOURNAL_PROFILE=release`. The test writes its target integration
source to `tests/build/save-journal-runtime/target-source/game_save.c`. That
translation unit compiles with the pinned offline SuperH compiler using
`-m2 -mb -ffreestanding -std=c99 -Os -Wall -Wextra -Werror
-Wno-sign-compare -Wno-unused-function`; the two exceptions cover the older
date comparison and API guards not wired until activation. The host fixture
does not yet exercise those caller-span and initialization guards;
they remain part of ownership and activation acceptance. The selected public
source has no new heap, task, handle or resident payload. BIOS delegated
internal saves remain on their existing path; the excluded native-backup
migration is not implied by this checkpoint.

## Shared command gate (staged; not enabled)

The fourth checkpoint stages `command_gate.c` and its current public API. The
ordinary menu has no lease around its direct shared command register writes;
this source is **not yet included in `main.c` or linked**. Callers, startup
drain, launch/return transitions and the menu/MCU protocol still require their
own checkpoints before the journal can be enabled.

**paps-g** authored the gate in private `e4fe5cff`, the sticky reply fault in
`bc592d82`, the reset-owner fence in `dfa2f1d1` and `90ff71c0`, and the
selected integration in `607267b9`. This public port keeps the gate logic,
adapts `file_cache_purge` to the current three-argument menu helper, and limits
the header to implemented core functions. The host and target compile fixtures
are public adaptations. Diagnostic shell functions and their declarations are
excluded.

The SH-2 `TAS.B` instruction acquires a byte lease through the uncached address
alias. The [SH7604 Hardware Manual §7.10, printed p. 203](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=218)
specifies that bus ownership is retained across the read/write portions of
`TAS`; [§8.4.4, printed p. 221](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=237)
describes its memory and cache behavior. The gate checks that the previous
FPGA command has retired before admitting a new owner. It records an inherited
command on first entry after a Saturn CPU reset and quarantines the lease
after a malformed tagged reply. The [manual's cache-coherency discussion,
§8.5.3, printed p. 228](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=244)
requires software to coordinate stale data between CPUs; `backup_enter` uses
the existing bounded menu cache purge before touching shared allocator state.
The exact purge cost and cartridge-bus timing need physical measurement.

Reproduce the checks with `python3 -B -m unittest
tests.test_command_gate_core -v` and the same command under
`SAROO_COMMAND_GATE_PROFILE=release`. Each ASan/UBSan and O2 profile checks
boot ownership, the sticky fault, 655,360 signed/tagged reply combinations and
40,000 two-thread admission attempts. The test includes the production source
but replaces the target `TAS` and address alias with host atomics. A strict
freestanding C99 SuperH fixture compiles that source at `-m2 -mb -Os -Wall
-Wextra -Werror -fstack-usage`; its disassembly contains `tas.b` on the
`0x20000000` alias. The fixture object has 288 text bytes, two BSS bytes and
at most a four-byte direct function frame. Its purge stub cannot measure the
actual target helper or whole-call-chain stack.

```sh
docker run --rm --pull=never --platform linux/amd64 --network none \
  -v "$PWD":/work \
  'ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6' \
  /opt/tool-chains/sh2eb-elf/bin/sh2eb-elf-gcc \
  -m2 -mb -ffreestanding -fno-builtin -fomit-frame-pointer \
  -std=c99 -Os -Wall -Wextra -Werror -fstack-usage \
  -I /work/Firm_Saturn -c /work/tests/target_command_gate.c \
  -o /work/tests/build/command-gate-core/target_command_gate.o
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/command-gate-stage-menu
```

The standard menu build still links 88,040 code/data and 9,508 BSS bytes with
17,140 body bytes free. The menu image SHA-256 remains
`d3ec0d5a6b96f14d00c7d502cdd4a9d518747f4b9c3bc8cc81a7409fdae61fc2`.
No installed firmware or release behavior changes at this checkpoint. The
host atomic model and target instruction compile do not qualify inter-CPU
arbitration, resets with an outstanding MCU request, actual Saturn execution
or game behavior.

## Menu command channel (staged; not enabled)

The fifth checkpoint stages the selected four-byte menu channel and its public
interface. The channel coalesces pending previews, keeps an active `SELECT`
request under the shared command lease until exact tagged completion, and holds
its metadata lock while an explicit action drains that preview and issues its
own request. A timeout during the drain retains the active request and lease
for later polling. The optical `LSAVE` helper copies the 16-byte identity only
after it owns both locks. It accepts only the negotiated menu-completion version
from the existing [public wire constants](../Firm_Saturn/main.h); the existing
[MCU completion tests](../tests/test_menu_command_completion.py) separately
cover tagged `STARTUP`/`SELECT` completion and legacy behavior.

**paps-g** authored the original channel in private `139fd48f`; `afb17879`
bounded the preview drain, `bc592d82` retained faulted ownership, `dfa2f1d1`
and `90ff71c0` refined hot-reset admission, and `5b7729cc` added the optional
launch hook. This
port copies its production state machine, limits the public header to functions
implemented in that file, and adds host and SuperH compile fixtures. The BIN
catalog/console helper, menu loop conversion, launch binding and diagnostic
shell remain separate integration work. The channel is **not included in the
ordinary menu**; no standard behavior or firmware installation changes here.
The SH-2 metadata and command locks use the atomic/cache rules cited in the
[command gate section](#shared-command-gate-staged-not-enabled), including the
[SH7604 Hardware Manual §7.10, printed p. 203](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=218).

The host fixture includes the production channel and command gate, substitutes
host atomics and a small command-completion peer, and extracts every tested
opcode/tag constant from the public menu header. Both ASan/UBSan and O2 profiles
pass capability refusal, foreign-owner deferral, queued-preview retirement,
sticky fault propagation, ownership of a full optical key, and a stalled drain
that later completes. They also check 393,216 raw reply cases: all 65,536 values
for asynchronous `SELECT` and each of five explicit command families. Existing
public MCU tests pass 13 menu-completion and five save-ack methods. These are
separate host models; they do not establish a complete simultaneous MCU/menu
runtime or Saturn timing.

```sh
python3 -B -m unittest tests.test_menu_channel_core -v
SAROO_MENU_CHANNEL_PROFILE=release python3 -B -m unittest tests.test_menu_channel_core -v
PYTHONPATH=tests python3 -B -m unittest test_menu_command_completion test_save_ack -v
docker run --rm --pull=never --platform linux/amd64 --network none \
  -v "$PWD":/work \
  'ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6' \
  /opt/tool-chains/sh2eb-elf/bin/sh2eb-elf-gcc \
  -m2 -mb -ffreestanding -fno-builtin -fomit-frame-pointer \
  -std=gnu99 -Os -Wall -Wextra -Werror -fstack-usage \
  -I /work/Firm_Saturn -c /work/tests/target_menu_channel.c \
  -o /work/tests/build/menu-channel-core/target_menu_channel.o
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/menu-channel-stage-menu
```

The strict SuperH fixture emits a combined channel-plus-gate object with 1,260
text bytes, six BSS bytes and a largest direct function frame of 20 bytes. It
uses a no-op cache-purge stub and is neither a linked size delta nor a complete
stack bound. The standard menu remains byte-identical at 88,040 code/data,
9,508 BSS, 17,140 body bytes free and image SHA-256
`d3ec0d5a6b96f14d00c7d502cdd4a9d518747f4b9c3bc8cc81a7409fdae61fc2`.
Caller conversion, a compatible production MCU/menu pair, normal-game behavior,
actual reset handoff and physical bus timing remain unqualified.

## Launch save binding (staged; not enabled)

The sixth checkpoint stages the selected save-session transition and menu
callbacks. Before `STARTUP`, the callback checks the save/file/archive
capabilities and admits journal recovery under the command lease. Before a new
`LOADDISC` or physical `LSAVE`, it settles an earlier journal and both devices'
pending writes, then detaches the old internal slot. A successful game load
publishes a full 16-byte canonical key in the existing
[MCU output field](../Firm_MCU/Saturn/saturn_utils.c); the callback resolves its
slot through the primary archive owner. A successful audio/stop result leaves
the internal slot unselected. A failed lookup retains the new key and requires
recovery. The physical request key is copied after prior save work, because that
work can reuse the same request workspace. These rules preserve the distinct
game-facing [Sega Backup Library §2.3 write API](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=50)
and SAROO's own journal/owner protocol.

**paps-g** authored the selected binding in private `5b7729cc`; the later
production selection was `607267b9`. This public port adds `save_launch.c/.h`
and `menu_launch.c` as direct unlinked source. It adapts the cache-purge call to
the current public three-argument helper. The runtime test's temporary
`game_save.c` integration now includes the selected launch source, while the
shipping `game_save.c`, menu command callers and MCU image remain unchanged.
At production integration, the allocator's archive lookup must resolve to the
explicit **owned** transport variant while this lease is held; the isolated
test currently supplies a synchronous archive owner and does not prove that
caller wiring.

Both ASan/UBSan and O2 profiles pass three focused launch methods: startup,
new game, audio/stop and legacy controls; missing capability, failed admission,
pending write, failed lookup, completed MCU error and malformed reply; and
physical key preparation after simulated workspace reuse. The two existing
transaction profiles still pass their 44 allocator/store scenarios each with
`save_launch.c` compiled into that test integration. The host launch fixture
models owner results and MCU key publication; it is not the actual MCU or a
physical-disc test. The public [MCU completion suite](../tests/test_menu_command_completion.py)
separately verifies the compatible menu command response protocol.

```sh
python3 -B -m unittest tests.test_save_launch_core tests.test_save_journal_runtime -v
SAROO_SAVE_LAUNCH_PROFILE=release python3 -B -m unittest tests.test_save_launch_core -v
SAROO_SAVE_JOURNAL_PROFILE=release python3 -B -m unittest tests.test_save_journal_runtime -v
docker run --rm --pull=never --platform linux/amd64 --network none \
  -v "$PWD":/work \
  'ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6' \
  /opt/tool-chains/sh2eb-elf/bin/sh2eb-elf-gcc \
  -m2 -mb -ffreestanding -fno-builtin -fomit-frame-pointer \
  -std=gnu99 -Os -Wall -Wextra -Werror -fstack-usage \
  -I /work/Firm_Saturn -c /work/tests/target_menu_launch.c \
  -o /work/tests/build/save-launch-core/target_menu_launch.o
docker run --rm --pull=never --platform linux/amd64 --network none \
  -v "$PWD":/work \
  'ijacquez/yaul@sha256:b6d272204767cd0a734139dd3a2ab298ef5912fb7b7224b78e6d9fac1a7825a6' \
  /opt/tool-chains/sh2eb-elf/bin/sh2eb-elf-gcc \
  -m2 -mb -ffreestanding -fno-builtin -fomit-frame-pointer \
  -std=c99 -Os -Wall -Wextra -Werror -Wno-sign-compare \
  -Wno-unused-function -fstack-usage -I /work/Firm_Saturn \
  -c /work/tests/build/save-journal-runtime/target-source/game_save.c \
  -o /work/tests/build/save-launch-core/target_game_save.o
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/save-launch-stage-menu
```

The strict SuperH menu-glue fixture compiles to 1,344 text/eight BSS bytes;
the temporary allocator integration containing the launch source compiles to
10,364 text/2,144 BSS bytes. Those are separate object costs with stubs, not a
combined linked-image delta. The new allocator launch helpers have eight-byte
direct frames; the glued physical-key channel has a 16-byte direct frame.
Whole-call-chain stack remains unmeasured. The standard menu is byte-identical
at 88,040 linked code/data, 9,508 BSS and 17,140 body bytes free, so this
checkpoint adds no shipping payload or resident workspace. Activation must
reserve the existing 64 KiB journal window exclusively. The current companion
ROM staging range overlaps that window during physical save binding; it must
be moved and verified before the binding is linked. Actual Saturn reset,
physical media, electrical power loss and game behavior remain unqualified.
