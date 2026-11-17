# Save-generation recovery

The standard menu now includes save-generation recovery with the matching
public MCU protocol. The sections below retain the chronological staged
evidence; see [Standard-firmware activation](#standard-firmware-activation)
for the current build and remaining qualification limits. No device image was
installed during this port.

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

## Companion ROM workspace separation (enabled)

This checkpoint moves the existing 2 MiB companion-ROM staging area from
`0x220c7100..0x222c7100` to `0x220d7100..0x222d7100`. The 256-byte request
and following 64 KiB file/journal window end exactly at the new start. The
late copy still moves forward to `0x22000000..0x22200000`, and the staged end
remains below the live artwork workspace at `0x22400000`. The source keeps the
original game predicates and load/callback behavior. The live address union
grows by 65,536 bytes; no new BSS object, heap buffer, request, copy or
companion file is added. This is a change in the **standard** menu source,
required before the staged physical-save binding can safely use that window.

**paps-g** identified and corrected this private integration regression in
`b57a578a`. This public adaptation changes the direct
[ROM source](../Firm_Saturn/game_patch.c), adds an actual-source save-window
collision control, and updates the linked-ROM fixture to read the current
staging constant. The parent source's normal complete-ROM and failed-read
controls pass, but a modeled 64 KiB save-window write between ROM preflight
and late copy corrupts all three KOF95/Ultraman predicates. The moved stage
passes all three. Compile-time assertions reject both a return to the old
overlap and an extent reaching beyond artwork.

Reproduce the focused checks with:

```sh
PYTHONPATH=tests python3 -B -m unittest test_menu_rom -v
SAROO_MENU_SAFETY_PROFILE=release PYTHONPATH=tests python3 -B -m unittest test_menu_rom -v
PYTHONPATH=tests python3 -B -m unittest test_menu_overlay_rom test_menu_cd_boot_handoff -v
PYTHONPATH=tests python3 -B -m unittest \
  test_menu_file_transfer.MenuFileTransferTests.test_one_mib_binary_and_two_mib_companion_loads -v
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/save-rom-separation-menu
SAROO_OVERLAY_MENU_ELF=tests/build/save-rom-separation-menu/source/ssfirm.elf \
  PYTHONPATH=tests python3 -B -m unittest \
  test_menu_overlay_rom.OverlayROMTests.test_linked_launch_installs_fragments_then_executes_complete_native_forward_copy -v
```

Seven actual-source ROM methods pass in both ASan/UBSan and O2 profiles,
including full 2 MiB copies and failed/stale callback controls. Six generic
overlay-ROM host methods, five CD boot handoff methods, the focused 2 MiB file
transfer method and a linked SuperH instruction simulation pass. The linked
copy's literal is `0x220d7100`, and its two launch phases preserve the complete
modeled cart/WRAM/stack poststate. The standard menu still links 88,040
code/data and 9,508 BSS bytes with 17,140 body bytes free; its padded image is
488,770 bytes with SHA-256
`d95fd722bb9fb9634939c8003c4de9d7560739a03596bafee9067035b49982f4`.
The changed bytes reflect the staging address, not save-generation activation.
The host and instruction models do not establish physical cartridge mapping,
live game behavior, electrical reset or SD durability.

## Shared transport caller ownership (staged, not enabled)

This checkpoint adapts **paps-g**'s private command-gate integration
`e4fe5cff` (later refined in `bc592d82` and `607267b9`) to the current public
menu transport. When `SAROO_SAVE_GENERATION` is defined, public file and primary
archive entry points acquire the existing atomic lease. Their explicit
`*_owned` counterparts let one journal transaction retain that lease across its
file and archive calls. Invalid reply tags or impossible transfer counts poison
the lease; a caller cannot report success after that fault. The diagnostic
formatter and byte comparison have owned entry points, so a backup mismatch
can log without trying to reacquire its own lease. The alternative build also
tags MCU messages with the latched menu completion bit.

The unmodified public transport has no cross-CPU admission lock: an idle
`SS_CMD` register alone cannot exclude another Saturn CPU while it prepares a
request. The host tests model that foreign owner and show the staged wrappers
refuse reads, writes and archive calls before invoking their internal endpoint
or changing request memory. They also prove normal lease release, nested public
call refusal, owned mismatch logging and sticky quarantine. The actual menu/MCU
file transfer fixture passes all 13 methods under ASan/UBSan and O2 with the
staged flag, including reset-cleared/malformed replies, short counts, full 2 MiB
loads and foreign-owner refusal. The same ordinary fixture passes 13 methods
with one staged-only method skipped. The three command-gate core methods and
source-extracted public-wrapper method pass; the latter passes both profiles.

Reproduce the host checks with:

```sh
SAROO_TEST_OWNER_TRANSPORT=1 PYTHONPATH=tests python3 -B -m unittest \
  test_menu_file_transfer -q
SAROO_TEST_OWNER_TRANSPORT=1 SAROO_FILE_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest test_menu_file_transfer -q
PYTHONPATH=tests python3 -B -m unittest test_command_transport_binding \
  test_command_gate_core -q
SAROO_COMMAND_TRANSPORT_PROFILE=release PYTHONPATH=tests python3 -B \
  -m unittest test_command_transport_binding -q
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/save-owner-stage-default-final
```

The ordinary menu build is byte-identical to the preceding ROM-separation
checkpoint: 88,040 code/data, 9,508 BSS and 17,140 body-margin bytes, with
image SHA-256 `d95fd722bb9fb9634939c8003c4de9d7560739a03596bafee9067035b49982f4`.
An isolated copy of the same menu sources with only
`-DSAROO_SAVE_GENERATION` added to its Makefile links at 88,968 code/data,
9,512 BSS and 16,208 body-margin bytes. This is a **candidate transport cost**,
not the final journal-enabled cost. No new workspace buffer or save capacity is
introduced here. The [Hitachi SH7604 Hardware Manual](https://antime.kapsi.fi/sega/files/sh7604.pdf)
§7.10 (printed p. 203), §8.4.4 (p. 221) and §8.5.3 (p. 228) describes the
TAS, cache and bus operations used by the existing gate.

The gate remains disabled in the standard menu because its backup and menu
callers are not all linked yet. The current asynchronous folder browser is an
additional menu-command producer that must join the same ownership scheme.
Host TAS and file-bus models, plus the alternative SuperH link, do not qualify
physical dual-CPU arbitration, SRAM aliases, SD durability, Saturn startup or
game execution. The installed firmware is unchanged.

## Backup API and journal binding (staged, not enabled)

This checkpoint binds the selected **paps-g** allocator/runtime from private
`5e318867`, the command ownership from `e4fe5cff`, and the selected recovery
integration from `607267b9` to the public menu's actual `game_save.c` and
Makefile. The alternate `SAROO_SAVE_GENERATION=1` build links the journal core
and groups the existing allocator fields with its recovery state. Public backup
API entries take one backup/command lease; internal allocator, file and archive
calls use owned variants until that entry returns. A busy owner refuses the
call, and an invalid command reply cannot be reported as a successful save.
Journal start/selection and the legacy `save_command` entry also have public
lease wrappers. The normal menu build remains byte-identical, so this change
does not activate recovery in standard firmware.

The binding validates caller input/output extents before using the journal's
transfer workspace or backup shadows. It preserves zero-length payloads and
the existing 64 KiB/8 MiB backup capacities. The only target portability
adaptation is a freestanding SuperH definition of the exact-width journal
types: the pinned Yaul compiler has no libc `stdint.h`. The host model now
compiles the actual opt-in `game_save.c` rather than inserting runtime logic
into a transformed copy. It exercises both backup devices, public API calls,
owner contention, protected-input refusals, normal writes and interrupted
body/publication/apply paths, replay, and corrupt-record refusal. The older
transformed-source test remains a separate comparison to the public allocator.

Reproduce this checkpoint with:

```sh
python3 -B -m unittest tests.test_save_journal_binding -q
SAROO_SAVE_JOURNAL_PROFILE=release python3 -B -m unittest \
  tests.test_save_journal_binding -q
python3 -B -m unittest tests.test_save_journal_runtime \
  tests.test_save_journal_plan tests.test_save_journal_core \
  tests.test_save_launch_core tests.test_command_gate_core \
  tests.test_command_transport_binding -q
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/save-binding-default
python3 -B tests/build_save_generation_candidate.py
```

Both binding profiles pass 38 process cases, including invalid output/input
and name extents plus an occupied command owner before every operation. The
six existing focused suites pass 12 methods under ASan/UBSan. An isolated copy
of the menu with only `SAROO_SAVE_GENERATION=1` added to its Makefile compiles
and links with the pinned SuperH toolchain: 95,608 code/data bytes, 9,536 BSS
bytes, 9,544 bytes of body margin and a 16,384-byte reserved stack. The normal
menu remains 88,040 code/data, 9,508 BSS, 17,140 bytes of body margin and
SHA-256 `d95fd722bb9fb9634939c8003c4de9d7560739a03596bafee9067035b49982f4`.
The alternate's direct compiler frames include 92 bytes in `sro_bup_write`,
80 in `journal_mems_write`, 80 in the record publisher and 72 in replay; these
are not a whole-call-chain or interrupt stack bound. The existing 65,536-byte
transfer workspace is reused rather than duplicated.

Sega's [Backup Library §2.1, printed p. 42](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=47)
defines the directory and data size, and [§2.3 function No. 5, printed p. 45](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=50)
defines write results. The [SH7604 Hardware Manual](https://antime.kapsi.fi/sega/files/sh7604.pdf)
§7.10 (printed p. 203) and §8.4.4 (p. 221) cover the TAS and cache assumptions
of the existing command gate. SJR1 and command leasing remain SAROO mechanisms.

The alternate image is **not** ready to install: raw menu command producers,
including the asynchronous browser and launch/return paths, have not all joined
the lease. Host execution models completed file operations and selected
interruption points; it does not establish physical power-loss atomicity,
dual-CPU arbitration, Saturn behavior or SD durability. The standard image
and installed firmware remain unchanged.

## Synchronous menu caller ownership (staged, not enabled)

This checkpoint adapts **paps-g**'s private menu-channel routing `139fd48f`
and selected `607267b9` integration to the public menu's current synchronous
callers. With `SAROO_SAVE_GENERATION=1`, game preview, game load (including
overlay preparation), category and legacy BIN listing, physical-disc stop,
firmware check/update, STARTUP, the serial console's existing BIN listing, and
optical `LSAVE` pass through the command channel. The normal branch of every
route remains compiled exactly as before. Capability words are latched before
the staged STARTUP request, so the channel can refuse an incomplete peer.
The selected channel and gate are included in the alternate menu image.

The parent menu has 15 direct command-register writers after preprocessing.
The alternate now has six: two owned transport requests, two channel requests,
and **two remaining asynchronous folder-browser requests**. Its serial console
listing goes from one direct writer to zero. A source-level route test checks
these counts and the actual call sites; the host command-channel model covers
all eight synchronous command families plus SELECT, including tagged replies,
busy ownership, queued preview retirement and timeout. The 13 existing
menu/MCU completion methods still pass under ASan/UBSan and O2, as do the
backup binding and launch-model suites. A failed legacy BIN catalog restore
keeps its selection state, and the game selector retries restoration before
using that shared directory.

Reproduce with:

```sh
python3 -B -m unittest tests.test_menu_caller_routes \
  tests.test_menu_channel_core -q
SAROO_MENU_CHANNEL_PROFILE=release python3 -B -m unittest \
  tests.test_menu_channel_core -q
PYTHONPATH=tests python3 -B -m unittest test_menu_command_completion -q
PYTHONPATH=tests SAROO_FILE_PROFILE=release python3 -B -m unittest \
  test_menu_command_completion -q
python3 -B tests/build_save_generation_candidate.py
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/menu-channel-sync-default-final
```

The pinned SuperH opt-in link uses 96,568 code/data bytes, 9,540 BSS, a
16,384-byte reserved stack and leaves 8,580 body bytes. The standard menu
remains byte-identical at 88,040 code/data, 9,508 BSS and 17,140 body-margin
bytes, with SHA-256
`d95fd722bb9fb9634939c8003c4de9d7560739a03596bafee9067035b49982f4`.
Both paths reuse the same 65,536-byte transfer workspace. Compiler stack
reports show unchanged largest direct frames in main/conio/game-load/game-save
(268/88/84/92 bytes), plus 80 bytes for the journal core; these do not bound
nested calls or interrupts.
The existing [SH7604 Hardware Manual](https://antime.kapsi.fi/sega/files/sh7604.pdf)
§7.10 (printed p. 203) provides the command gate's TAS basis; the tagged
commands are SAROO's protocol, documented by the public MCU dispatcher and
menu wire constants. These host and target results are not physical
dual-CPU, reset, SD, Saturn or game qualification.

The alternate build remains **uninstallable**. The asynchronous folder
browser still writes commands without a lease, and launch callbacks have not
yet been enabled for startup/game/return. No save-generation recovery occurs
in the standard menu or this incomplete alternate.

## Asynchronous folder-browser ownership (staged, not enabled)

This checkpoint brings the remaining folder-browser command producer under
the opt-in menu channel. **paps-g** authored the selected private integration
in `607267b9`, building on the channel from `139fd48f`. The public adaptation
admits one channel step per UI poll instead of blocking while an earlier
preview retires. It holds the metadata and command leases across each `BINDIR`
or `LISTDISC` request, including cancel and failed-child restore, and consumes
the returned path before releasing the command lease. A foreign owner defers
publication without touching the shared input. An invalid reply poisons the
command lease and leaves the browser in its reset-required fault state. The
standard menu path remains compiled exactly as before.

The source-extracted browser fixture runs the actual menu function alongside
the actual gate and channel source, with a modeled mailbox. In the staged
ASan/UBSan and O2 profiles, eight active methods cover prior-preview
retirement, folder traversal, cancel/restore, retry, bounds/UTF-8, foreign
ownership, malformed-reply quarantine and reply-buffer replacement immediately
on lease release. The standard branch passes seven active browser methods;
its thirteen paired menu/MCU completion methods pass in both profiles. The
source-route check confirms that the alternate's six direct command writes
are now two owned transport, two channel and two **leased** browser writes.
Those two browser writes are intentionally still direct because they span UI
ticks. Reproduce with:

```sh
PYTHONPATH=tests SAROO_MENU_BIN_OWNER=1 python3 -B -m unittest test_menu_bin_browser -q
PYTHONPATH=tests SAROO_MENU_BIN_OWNER=1 SAROO_MENU_BIN_PROFILE=release \
  python3 -B -m unittest test_menu_bin_browser -q
PYTHONPATH=tests python3 -B -m unittest \
  test_menu_bin_browser test_menu_command_completion -q
PYTHONPATH=tests SAROO_MENU_BIN_PROFILE=release SAROO_FILE_PROFILE=release \
  python3 -B -m unittest test_menu_bin_browser test_menu_command_completion -q
python3 -B -m unittest tests.test_menu_caller_routes -q
python3 -B tests/build_save_generation_candidate.py
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/menu-bin-owner-default-final
```

The pinned SuperH alternate links 96,680 code/data bytes, 9,540 BSS bytes,
reserves 16,384 stack bytes and leaves 8,468 body bytes. Largest reported
direct frames remain 268/88/84/92 bytes in main/conio/game-load/game-save and
80 in the journal core; these are not nested-call or interrupt stack maxima.
The existing 65,536-byte transfer workspace is reused. The ordinary menu
remains byte-identical at 88,040 code/data, 9,508 BSS and 17,140 body-margin
bytes, image SHA-256
`d95fd722bb9fb9634939c8003c4de9d7560739a03596bafee9067035b49982f4`.

The gate's SH-2 atomic lease is based on the [SH7604 Hardware Manual §7.10,
printed p. 203](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=218);
the tagged acknowledgment is SAROO's menu/MCU protocol, not a Sega command.
The host mailbox and target link do not qualify Saturn CPU interleaving, a
physical reset, SD durability or game behavior. Launch/return callbacks are
still unbound, so neither the alternate nor the standard image performs
save-generation recovery yet.

## Menu launch and return ownership (opt-in candidate)

This checkpoint links the already staged launch callbacks into the isolated
`SAROO_SAVE_GENERATION=1` menu build. The standard menu still does not include
the journal. With a complete menu/file/archive-acknowledgment peer, the opt-in
menu replays a pending journal before its tagged `STARTUP` command, and settles
the previous save session before a game-load or physical-save request. After
a successful load, it reads the MCU's canonical 16-byte game key and resolves
its archive slot while the command lease is still held. Returning from a game
reenters the menu startup path and repeats admission before another launch.

**paps-g** authored the original callbacks in private `5b7729cc` and selected
them in `607267b9`. The public adaptation also includes `FIRSTDISC` and
`ROMDISC` in the game-load callback set. The current public selector sends
these for checked overlay and companion-ROM launches, and the MCU dispatches
both to `load_disc(..., start_save)` with a save load and result key. The
staged callback previously skipped both: a source-extracted host control
reproduced that gap with `admission_calls == 0` for the first checked load.
The corrected callback covers all three load commands, ordinary stop/audio
loads with an empty result key, legacy sessions and physical `LSAVE`. It
clears the output key before every request, never treats the MCU's selected
save state as its output, and refuses absent capabilities or an uncertain
reply.

Reproduce with:

```sh
python3 -B -m unittest tests.test_save_launch_core -q
SAROO_SAVE_LAUNCH_PROFILE=release python3 -B -m unittest \
  tests.test_save_launch_core -q
python3 -B -m unittest tests.test_menu_caller_routes -q
PYTHONPATH=tests python3 -B -m unittest test_menu_launch_errors -q
PYTHONPATH=tests python3 -B -m unittest \
  test_mcu_save_disc.DiscSaveSessionTests.test_explicit_menu_load_still_selects_requested_save \
  test_mcu_save_disc.DiscSaveSessionTests.test_optical_boot_stop_unloads_without_save_io_and_is_idempotent -q
PYTHONPATH=tests SAROO_MENU_BIN_OWNER=1 python3 -B -m unittest \
  test_menu_bin_browser -q
python3 -B tests/build_save_generation_candidate.py
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/menu-launch-owner-default
```

All three launch-model methods pass under ASan/UBSan and O2 after the new
checked-load assertions. The source-route, six ordinary selector and staged
browser checks pass, as do two MCU disc-save controls for explicit load and
optical stop. The selector fixture now extracts the current public
button helper rather than retaining an obsolete implicit declaration.
The paired SuperH candidate links 97,000 code/data and 9,540 BSS bytes, with
a 16,384-byte reserved stack and 8,148 bytes of body margin. The direct frame
maxima remain 268/88/84/92 bytes in main/conio/game-load/game-save and 80 in
the journal core; whole-call-chain and interrupt stack use are not measured.
The 65,536-byte transfer workspace remains shared, not replicated. The normal
menu is byte-identical to its predecessor: 88,040 code/data, 9,508 BSS,
17,140 body margin, image SHA-256
`d95fd722bb9fb9634939c8003c4de9d7560739a03596bafee9067035b49982f4`.

Sega's [Backup Library §2.1, printed p. 42](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=47)
defines the archive-facing directory and data sizes. [FatFs `f_sync`](https://elm-chan.org/fsw/ff/doc/sync.html)
describes the file synchronization needed by the selected owner; the SJR1
record, command tags and launch leases are SAROO mechanisms. The alternate
build and host model do not prove physical power-loss ordering, SD durability,
Saturn startup timing, overlay-game execution or return from a real game.
The opt-in image remains a candidate and was not installed. Production
activation still requires a final source/peer audit and validation gate.

## Standard-firmware activation

The final checkpoint selects `SAROO_SAVE_GENERATION=1` as the normal menu
Makefile profile. This follows **paps-g**'s selected private activation in
`607267b9`, adapted to the public direct-source layout and its checked-overlay
launch variants. The prior menu remains selectable with
`SAROO_SAVE_GENERATION=0` for controlled comparisons. The MCU source and wire
constants are unchanged: its published save, file, archive and menu capability
words match the menu headers; its dispatcher completes the tagged startup,
preview, three game-load, physical-save, file and archive requests. Unknown or
incomplete peers are refused before a journal operation. The selected SJR1
path reads and validates a pending record before ordinary menu startup, uses
the existing primary archive owner for replay, and retains the command lease
through each backup and launch transaction. It does not migrate BIOS-delegated
internal backup or add any CDON recipe, USB feature or diagnostic command.

The source/peer audit accounts for all six direct writes in active `main.c`:
two owned file/archive transport requests, two menu-channel requests, and
two asynchronous folder-browser requests under its retained channel lease.
The backup allocator's separate direct writer runs under its backup lease.
The standard SuperH link contains `journal_start_owned`, both launch callbacks,
`save_journal_load` and `save_journal_replay`. The standard menu and paired
MCU GNU App builds pass with the pinned offline tools. Focused host checks
pass for the actual backup API and allocator, SJR1 publication/replay cut
points, input and journal corruption refusal, command ownership, launch/return
selection, folder browse, MCU save/disc behavior and tagged command replies.
The legacy save-ack fixture now restores the normal source's top-level
`SAVE_COMMAND` alias when extracting its old control path; its five methods
pass. That fixture edit changes no firmware source.

Reproduce the acceptance with:

```sh
python3 -B -m unittest tests.test_save_journal_binding \
  tests.test_save_journal_runtime tests.test_save_journal_plan \
  tests.test_save_journal_core -q
SAROO_SAVE_JOURNAL_PROFILE=release python3 -B -m unittest \
  tests.test_save_journal_binding tests.test_save_journal_runtime \
  tests.test_save_journal_plan tests.test_save_journal_core -q
python3 -B -m unittest tests.test_menu_channel_core \
  tests.test_command_transport_binding tests.test_command_gate_core \
  tests.test_menu_caller_routes tests.test_save_launch_core -q
PYTHONPATH=tests python3 -B -m unittest \
  test_menu_command_completion test_save_ack test_mcu_save_disc -q
PYTHONPATH=tests SAROO_MENU_BIN_OWNER=1 python3 -B -m unittest \
  test_menu_bin_browser -q
tools/target/debug/firmware-build menu --repo "$PWD" \
  --out tests/build/save-generation-standard-final
```

For the paired MCU, use `firmware-build mcu` with the pinned local Arm GNU,
CMSIS Core, STM32 and RTX SDK roots shown in the build receipt. It is an
offline App compile/link, not a device install. Both builds report success.
The menu links 97,000 code/data bytes and 9,540 BSS bytes, retains the
16,384-byte stack reservation and 65,536-byte transfer workspace, and leaves
8,148 body-margin bytes. Relative to the previous normal menu, linked
code/data grows 8,960 bytes, BSS grows 32 bytes and body margin falls
8,992 bytes. The padded image remains 488,770 bytes, SHA-256
`8e4fa6798d649665b97ca3176c7fa9823cefa7f561176eff46d260ad545984e4`.
Compiler-reported largest direct frames are 268/88/84/92 bytes in
main/conio/game-load/game-save and 80 in the journal core; these do not
establish a whole-call-chain or interrupt stack maximum. The matching MCU
GNU App has 59,344 image-load bytes, 50,904 DTCM data/BSS bytes, 6,192 free
ITCM bytes and 166 vectors. Its source and image are unchanged by menu
activation.

Sega's [Backup Library §2.1, printed p. 42](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=47)
and [§2.3 function No. 5, printed p. 45](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf#page=50)
define the backup directory, data size and write-result contract. The
[SH7604 Hardware Manual §7.10, printed p. 203](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=218)
supports the TAS lease operation; [FatFs `f_sync`](https://elm-chan.org/fsw/ff/doc/sync.html)
describes file synchronization. SJR1, tagged command ownership and archive
replay are SAROO-defined behavior. Host fault injection and successful target
links do not prove physical power-loss ordering, SD controller persistence,
dual-CPU timing, Saturn startup, save behavior in games or a safe field
upgrade. The firmware has not been installed or tested on a Saturn.
