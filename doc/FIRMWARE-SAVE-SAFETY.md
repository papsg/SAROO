# Firmware save-safety ports

This series starts at public `7d1d461`. It independently ports the original
v0.8 firmware save corrections, preserving layouts/capacities. The desktop
[save-tool series](SAVETOOL-SAFETY.md) is separate. Source changes are direct,
with no private patch framework, USB integration, added runtime diagnostics,
native-backup migration or experimental journal/transaction system.

## SATURN-SAVE-002: unsigned dirty-window shifts

`mbuf_mark` tracks modified blocks in two 32-bit words for the 64-block MEMS
window. At positions 31 and 63 it evaluates signed `1 << 31`, outside the
representable positive range. The two shift operands are now unsigned. No
bitmap layout, buffer, command or normal allocation is added.

The primary C requirement is
[WG14 N1570 §6.5.7 paragraphs 3–4, printed pp. 94–95](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).
The operand's promoted type determines shift arithmetic; signed left shift has
a representability condition, while unsigned arithmetic is reduced modulo its
range. This is a C source defect rather than a Sega register requirement.
The project-specific window is established by
[`get_block_addr` and `mbuf_mark` at the public starting point](https://github.com/papsg/SAROO/blob/7d1d461/Firm_Saturn/game_save.c).

Our original correction was authored by `paps-g` in `d8fe6875` on 2026-09-05,
alongside save resizing. This public port separates the two findings. The dirty
mask is an explicit prerequisite for sanitizer qualification of large resize
cases, so it is ported first. Original source notices remain unchanged.

Three public methods include the actual firmware C with test-only MMIO stubs.
They exercise every bit at three window starts (192 cases), repeated marks,
combined words and a slot-device control that leaves MEMS tracking unchanged.
The baseline has seven failed subcases under UBSan: six single-bit cases at
positions 31/63 and one combined-window case. Other bit positions and the
slot-device control pass there. All 194 invocations now pass ASan/UBSan and
normal O2. The fixture issues no storage command.

The preceding and corrected public trees both fully compile/link all sixteen
menu units with the pinned GCC 13.2.0 / Yaul environment. The actual
`game_save.o`, ELF and complete menu image are byte-identical between them.
Code/data remain 75,056 bytes, BSS 7,180, reserved stack 16,384 and body margin
32,452. The image remains 488,751 bytes with the same 357,679-byte font. This
compiler already emitted the intended shift instructions; source correction
removes undefined arithmetic without adding instructions or storage in this
qualified build. Existing unrelated target warnings remain.

From the repository root:

```sh
make -C tests check-firmware-save
SAROO_FIRMWARE_SAVE_PROFILE=release make -C tests check-firmware-save
cargo build --offline --locked --manifest-path tools/Cargo.toml -p firmware-build
tools/target/debug/firmware-build menu --repo "$PWD" --out tests/build/menu-dirty
```

`SAROO_GAME_SAVE_SOURCE` selects an independently extracted `game_save.c`, for
example from `7d1d461`, to reproduce the old failures. The test stubs/model are
not linked into firmware. The public MIT host builder is a verification
prerequisite for repeated isolated builds; it uses the existing Makefile and
cached toolchain, records inputs/resources, and refuses existing output paths.

These are host C/sanitizer and target compile/link checks. They do not establish
physical endian encoding, interrupt/reentrancy/cache/bus behavior, storage
durability, game compatibility or a historical crash cause. In particular,
[issue #363](https://github.com/tpunix/SAROO/issues/363) is related investigation
context, not a report resolved by this two-line correction. No device, private
source/cache/branch or installed firmware is changed.

Save resizing, metadata validation, successful dirty-flush bookkeeping, separately
evaluated header batching, checked MCU startup/runtime, incomplete-tail admission
and Saturn error/retry propagation remain following series members.

## SATURN-SAVE-001: resize saves and flush changed allocation

At public `1239a7d`, overwrite stores a new date but uses the old header size and
allocation for copying. Growth is truncated; shrinking can read beyond the
caller's smaller buffer. MEMS allocation changes also need the global header
flushed, so correct RAM behavior alone does not establish correct reloads.

The replacement path plans allocation in private scratch, reuses the selected
save's old data blocks, checks the selected old layout/capacity and retains its
header/list position. Only after the plan succeeds does it publish size, data
allocation and actual free count, then write the complete supplied payload.
MEMS final flush includes changed global allocation metadata. The 960-byte
boundary stays inline; new saves above table capacity and a full MEMS directory
are refused before allocation. Existing same-size overwrite semantics remain.

The public API references are Sega's
[Saturn System Library User's Guide, ST-162-R1-092994](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf),
Backup Library §2.1 `BupDir` (printed p. 42, PDF p. 47: byte data size), and
§2.3 function No. 5 `BUP_Write` (printed p. 45, PDF p. 50: directory/data inputs,
overwrite-check mode and failure/success results). The original unchanged PDF's
text and rendered write specification were checked. Those API rules do not
define SAROO's bitmap/table layout, which comes from the starting
[`BUPHEADER`, `MEMSHEADER`, `access_data`, `find_save` and writer](https://github.com/papsg/SAROO/blob/1239a7d/Firm_Saturn/game_save.c).
Maximum payloads remain 510 × 128 bytes for an otherwise empty slot and
479 × 1,024 bytes for MEMS; neighboring saves can reduce actual capacity.

Our correction was originally authored by `paps-g` in `d8fe6875` on 2026-09-05.
This public source port independently revalidates it after the separate
SATURN-SAVE-002 prerequisite; no private preparer is used. Source notices and
the prior language/controller/host-tool contributions remain intact.

Two actual-C model methods execute 32 cases: 21 successful growth/shrink/zero/
inline/table/maximum/same-size round trips and eleven refusals covering capacity,
oversize, selected old chains, overwrite protection and full directory. They
check payload/read/verify, metadata, output sentinels, neighbors, allocation/free
counts and complete refused disk images. Discarding model RAM and reloading
checks the MCU flush contract. The preceding public source fails 28 cases;
all 32 pass ASan/UBSan and normal O2. Removing only the changed-allocation flush
flag makes eight cases fail after reload. This control distinguishes updated
RAM from allocation metadata actually sent through the modeled storage commands.

The model includes the real firmware module and synchronously implements the
existing MCU slot/MEMS load/flush behavior, including overlapping header/data
windows. Stored typed fields use host byte order. It is not a physical reset,
SD/controller, bus, cache, interrupt or concurrency simulation. Checks establish
the logical API/allocation/reported-command behavior. They do not authenticate
arbitrary input archives or establish power-loss atomicity. The MCU still needs
the later series members to check actual I/O results and propagate them.

The complete pinned SH-2 build qualifies all sixteen menu C units. Code/data
grow 960 bytes to 76,016, and BSS grows 2,028 to 9,208 for the private bitmap and
block list. The fixed 16,384-byte menu stack and 488,751-byte image/font layout
remain, with 29,464 body bytes free. The two large arrays live outside the
calling game's stack, under the service's existing synchronous ownership.
GCC inlines the planner: the write function's direct frame grows from 52 to
104 bytes (eight four-byte saves plus 20/72 local bytes). This is partial
compiled stack accounting, excluding callees/interrupts; the menu stack reserve
does not prove a calling game's stack headroom. Same-size writes skip planning;
different-size writes scan at most 512 slot or 8,064 MEMS blocks. No new I/O
command type or whole-archive copy is added. No physical latency is claimed.

Run `make -C tests check-firmware-save` for coupled dirty/resize checks; the
source/profile overrides and isolated target-build command above also apply.
Full metadata/ownership admission, dirty-flush bookkeeping, separately evaluated
batching and MCU/error/retry changes remain following scope. This correction
does not establish that a named game's historical save report is resolved.
