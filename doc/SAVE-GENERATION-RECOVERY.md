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
