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
