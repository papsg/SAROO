# Configuration safety

Direct source ports start at public `f7d7299`. Original findings are independently
revalidated here; historical host/build results are not reused as qualification.
The matcher is the first prerequisite; bounded whole-file loading and actual
startup/disc error handling have a separate checkpoint below. USB and runtime diagnostics are
excluded from both ports.

## MCU-CONFIG-001: bounded ID matching with preserved version rules

The [public baseline matcher and caller](https://github.com/papsg/SAROO/blob/f7d7299/Firm_MCU/Saturn/saturn_utils.c)
still copy unchecked section/game IDs into two 24-byte arrays. A 24-byte name
also needs its terminator, overflowing that array. A versioned section compared
with an unversioned ID takes the mistaken `ver1 && ver1` branch and passes a null
second version pointer to `strcmp`. The legacy 16-byte joining exception uses
`strcpy` between overlapping regions of the same local array.

Check that each terminator occurs within its first 24 bytes before copying
anything. Overlong or unterminated fields return non-match, without truncation.
Check both version pointers before comparing versions. The joining case uses a
bounded forward copy: the source follows the destination, so each write precedes
all unread source bytes. Its combined ID length is 16, and at most 17 bytes,
including the terminator, are copied into the existing 24-byte array.

Preserve case-sensitive byte equality, ASCII-space splitting, skipped extra
spaces, the first version token, ignored trailing tokens, exact version presence
and equality, the 16-byte join and existing empty-token behavior. A missing
version remains distinct. Caller inputs remain byte-for-byte unchanged. This
is the existing project configuration policy, not a new Sega ID grammar.

WG14 [N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
§7.24.2.3 p2 (printed363) specifies `strcpy`'s terminator copy and makes overlap
undefined; §7.1.4 p1 (183) covers invalid library arguments; §7.24.4.2 (365)
defines string comparison. The [POSIX.1-2024 strlen/strnlen DESCRIPTION and
RETURN VALUE](https://pubs.opengroup.org/onlinepubs/9799919799/functions/strlen.html)
defines the bounded length check. Both references were inspected from preserved
local primary snapshots. No Sega manual defines this local matching exception,
and no individual upstream issue is claimed as its source.

Original implementation/fixtures: **paps-g**, commit
`3e0725733d1233dada85737a1bef978ae87fd254` (2026-09-06). Public adaptation extracts
the current original-layout functions directly and removes the private build/
patch-framework dependency. Attribution uses public contributor handles only;
no private contact details or raw reference logs are published.

### Verification and resource costs

Five public methods run **33 processes / 5,362 pairs / 10,724 calls**, checking
both argument directions and exact input preservation. Immutable byte-token
expectations cover versions, case/high-bit bytes, tabs/empty fields, six joining
cases, 23/24-byte and long-prefix boundaries, unterminated arrays and a 5,329-pair
corpus. Actual baseline C fails **14** processes; sanitizer and O2 candidates
pass all. Reintroducing unbounded copies, the wrong version condition or the
overlapping copy fails **4/3/5** focused cases respectively. These are scenarios,
not separately counted defects.

```sh
make -C tests check-config
SAROO_CONFIG_PROFILE=release make -C tests check-config
# Source-only comparisons can use:
# SAROO_CONFIG_MATCH_SOURCE=/path/to/saturn_utils.c
```

The complete original-project GNU MCU/RTX build passes with 166 vector entries.
Load size grows **59,812→59,844 bytes**, leaving **5,692 ITCM bytes free**. Static
DTCM remains **61,112 bytes** (60,608 BSS + 504 data); its configured stack/heap
address gap is unchanged at 69,960 bytes. Compiler direct frames remain 16 for
`gameid_split`, 64 for `gameid_match` and 128 for `parse_config`. Frames exclude
callees/interrupt nesting. No heap, cache, persistent state or I/O is added.
The two bounded scans examine at most 48 bytes per comparison; this is extra
validation work, not a measured speedup. Menu inputs are unchanged.

Native tests do not emulate MCU scheduling, buses, boot or games. The linked GNU
artifact is not original MDK equivalence or hardware qualification. Whole-file
parsing, snapshot/command/string bounds, publication and caller cleanup are
addressed by the separate MCU-CONFIG-002 checkpoint below.

## MCU-CONFIG-002: validate owned snapshots before applying settings

The baseline loader reads an unchecked file size into the 32 KiB window at
`0x24002000`, then appends a terminator. A 32,768-byte file already reaches the
CD block pool at `0x2400a000`. Quoted category/multi-disc values and the generated
command list are also unbounded. It accepts short reads/failed closes, partial
numeric conversions and malformed widths, and changes live state before later
errors. Startup and disc loading ignore the result; some delay values can
survive from a previous selection. These source-confirmed defects are not tied
to an identified upstream game report.

Read at most **32,767 bytes**, leaving room for the terminator, and check exact
read count and close status. Reject embedded NULs; normalize a UTF-8 BOM and
CR/LF/CRLF in the owned snapshot. Validate all applicable selected operations,
then apply the same bytes without another file read or compiled-config cache.
Temporary section delimiters are restored. Runtime calls hold the existing
filesystem-workspace mutex through loading, validation and application; startup
runs before that mutex and the Saturn workers exist. Existing quiescent-reader
callers remain a prerequisite; this is not a general concurrency protocol.

On failure, configuration scalars, strings, categories and command bytes retain
their previous contents. On success, per-game delays and multi-disc marker start
at zero/empty, and the command list starts empty. Startup rebuilds categories;
unspecified global-only scalars retain their initialized values. Missing
`FR_NO_FILE`, empty and comment-only inputs keep default behavior. Other file
errors remain errors. Unknown keys and nonselected options stay ignored, while
the first section must be `[global]` and the first exact matching game section
wins, using the preceding matcher semantics.

The actual startup caller now returns before firmware/archive/audio/worker
startup on error. The disc caller unloads candidate readers and returns before
new-save admission; automatic changes retain their existing reinsertion retry
policy. A public refinement removes the caller's premature multi-disc-string
clear: accepted-default reset now belongs to the validated parser. The wider
disc replacement can already have stopped/unloaded the previous disc, so this
is not a rollback transaction for all drive metadata.

### Bounds and compatibility policy

Known scalar/string keys require a complete key and `=`. Decimal values are
nonnegative and fit `INT32_MAX`; hexadecimal masks retain all 32 bits. Optional
`+` and hexadecimal prefixes are accepted. Empty/partial/overflowed conversions
and noncomment suffixes fail. These are explicit local input policies; they do
not add semantic restrictions such as limiting `auto_update` to only 0 or 1.
Quoted category/multi-disc values fit 31/15 bytes plus NUL, respectively. Category
count is at most 12; a thirteenth rejects the selection instead of disappearing.

Command storage spans `SYSINFO+0x100` through the byte before categories at
`+0xe80`: 864 words, of which one must be the terminator. The selected global
and game sections therefore share **863 payload words**, including complete
pairs. Memory values have exactly 2/4/8 hexadecimal digits, excluding prefixes,
whitespace/comments, for 1/2/4-byte writes; addresses must be aligned. Expansion
commands keep their existing one-word encoding, and intentional address/value
patching remains supported without a new address allowlist. These are the
[existing SH-2 consumer's encodings and typed writes](https://github.com/papsg/SAROO/blob/c7a4f3d/Firm_Saturn/game_patch.c#L1218).

Spaces/tabs and trailing `#`/`;` comments are supported. The public port uses the
first closing section bracket, so a later `]` inside a trailing comment cannot
change the section identity; this is a tested refinement of the historical
correction. The 32 KiB ceiling follows the existing workspace allocation, not
SD capacity. Supporting larger files needs a separate streaming/ownership
representation. No later configuration-shrink optimization is imported.

### Contracts and provenance

Original implementation/fixtures: **paps-g**,
`0dc69582448a549c8eeb31012da3802312e0b59c` (2026-09-06). This direct public port
omits its integration include, USB error trace, byte-offset reporting and SD
boot checkpoint. It retains the existing configuration-error LED and normal
return/control paths, without adding a diagnostic subsystem. Fixtures own their
captured source/input cohorts, avoiding private source-preparer lifetimes. The
coupled public fixture now uses the actual parser instead of a success stub.

FatFs R0.15 [`f_read`, Description](https://elm-chan.org/fsw/ff/doc/read.html)
requires checking the returned count even after success;
[`f_open`, Description](https://elm-chan.org/fsw/ff/doc/open.html) and
[`f_close`, Description](https://elm-chan.org/fsw/ff/doc/close.html) define file
object lifetime. This configuration file is read-only and public
[`FF_FS_LOCK` remains zero](https://github.com/papsg/SAROO/blob/c7a4f3d/Firm_MCU/FatFS/ffconf.h#L266);
FatFs permits discarding such a read-only object even without closure. Errors
are still reported; changing that file-lock configuration requires a fresh
lifetime review. These unpaginated contracts were inspected from local primary
snapshots. N1570 §6.3.2.3 p7 (printed56) covers alignment of converted pointers;
the string/array contracts are cited above.

The RAM boundaries and output layout come from SAROO's
[workspace/pool initialization](https://github.com/papsg/SAROO/blob/c7a4f3d/Firm_MCU/Saturn/saturn_main.c#L934),
[configuration and save scratch use](https://github.com/papsg/SAROO/blob/c7a4f3d/Firm_MCU/Saturn/saturn_utils.c)
and the SH-2 consumer. No Sega manual defines this local text format, capacity,
selection policy or LED behavior. No unrelated issue is attributed to this fix.

### Verification, costs and limits

Twelve actual-parser/startup methods pass **177 invocations** under ASan/UBSan
and O2. They include complete before/after settings/system bytes, full strings,
32/64-bit reported file sizes, short/error/close failures, malformed fields,
exact category/command limits, shared global/game capacity, comments/line
endings/BOM, scopes/defaults and runtime read ownership. The shipped public
example passes global and all 30 game selections with independent delay
expectations. Selected eleven-method baseline comparisons make 134 invocations
and fail 90 assertions, including modeled ownership/I/O guards and 17 native
process failures. These counts are scenarios, not separate bugs.

The actual disc/menu/automatic-change/save/FatFs fixture passes **11 methods /
55 scenarios** in both profiles. Six explicit configuration failures cover
syntax, short read, reported close failure, oversize file, open error and a
pending save retry. They preserve save bytes/slots/retry state, verify reader
cleanup, exact 16-bit command error transport and a successful later attempt.
Automatic-change failure/reinsertion and successive default reset are included.
Selected baseline comparisons fail eight scenarios. The injected close-error
fixture releases the real object before reporting failure; it is not a model
of every media-removal or failed-close ownership state.

Removing validation-first behavior, command bounds, short-read checking,
startup admission, disc cleanup, workspace locking or first-header-bracket
selection fails **2/3/4/1/6/1/3** targeted checks. The integrated public suite passes **231 Python methods**,
authored C checks, **14 Rust tests**, formatting and clippy. Its actual MCU
fixtures cover **68 methods / 744 scenarios**. Tests model scheduling/registers,
not physical RTOS ordering, DMA, buses, card durability, boot or games.

```sh
make -C tests check-config check-mcu-save
SAROO_CONFIG_PROFILE=release make -C tests check-config
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -m unittest discover -s tests -p test_mcu_save_disc.py -v
# Bounded loader comparisons use SAROO_CONFIG_LOADER_DIR; coupled source
# comparisons use SAROO_MCU_SAVE_DIR, each containing the captured Saturn files.
```

Complete original-project GNU MCU/RTX link, compared with the matcher checkpoint:

| Resource | Before | After |
| --- | ---: | ---: |
| Load / ITCM occupancy | 59,844 B | 59,684 B |
| ITCM free | 5,692 B | 5,852 B |
| Static DTCM data+BSS | 61,112 B | 60,792 B |
| Configured stack/heap address gap | 69,960 B | 70,280 B |
| Parser direct frame | 128 B | 16 B |
| Separate loader / walk / numeric direct frames | — | 112 / 88 / 24 B |

The closed file object's frame is separate from the two walks. Direct frames
exclude callees, interrupts and whole runtime peaks. Disc/startup direct frames
remain 312/136 bytes. No heap, second snapshot, cache or SD write is added. The
new 144-byte read-only argument table replaces writable dispatch metadata;
complete measured static savings are reported above. The full menu build is
byte-identical: 78,836 code/data, 9,232 BSS, 16,384 reserved stack, 26,620 body
margin and 488,751 image bytes. These are source-bound GNU/menu qualifications,
not original MDK equivalence or hardware acceptance.

Both walks add CPU work. Three alternating native `-O2` rounds, 2,000 parses per
sample, use synthetic resident file/mutex stubs with complete final-state equality:

| Warm host workload | Baseline median | Port median |
| --- | ---: | ---: |
| 32 unmatched sections then selection | 1.992 µs | 4.153 µs |
| Full 32,767-byte snapshot | 14.308 µs | 12.661 µs |
| Full 863-word command list | 55.023 µs | 89.837 µs |

Each sample makes one open/read/close per parse in both versions. The ordinary
and full-command cases cost more CPU; there is no blanket speedup claim. These
measurements exclude actual storage, RTOS contention, bus traffic and logging;
the benchmark's old-reader ownership assertion is disabled in its file stub,
while production parser bodies remain unchanged. The public test harness has
an optional repetition argument for resident-input comparisons. No Saturn
latency/frame-pacing inference is made.

The caller's existing quiescence and shared-workspace assumptions remain.
Concurrent file edits/media replacement, general command ownership, streamed
large configs and later configuration shrink are separate work. Raw evidence
stays local/ignored; public code/docs/metadata are reviewed for secrets/private
PII and use public handles, portable paths and synthetic or already-public inputs.
