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
