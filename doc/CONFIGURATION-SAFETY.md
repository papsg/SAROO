# Configuration safety

Direct source ports start at public `f7d7299`. Original findings are independently
revalidated here; historical host/build results are not reused as qualification.
The matcher is the first prerequisite; bounded whole-file loading and actual
startup/disc error handling follow separately. USB and runtime diagnostics are
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
parsing, snapshot/command/string bounds, publication and caller cleanup remain
the separate MCU-CONFIG-002 candidate.
