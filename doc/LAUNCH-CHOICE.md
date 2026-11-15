# Launch one session without game patches

Tap A in the game list to launch normally on release. Hold A for 650 ms to
offer **A: Without game patches  B: Cancel**. Release A, then press it again
to confirm. B cancels. The original hold and repeated button events cannot
confirm the offer or retry a failed launch. A short tap has no 650 ms wait.

This adapts paps-g's private checkpoint `cda33cfa` (4 October 2026).
The state machine and interaction are the original contribution; the public
transport and ROM-service integration are papsg's adaptation. No upstream
issue is claimed resolved by this authored menu feature. The original menu
and dependency notices remain in place.

## Scope and session ownership

The choice bypasses SD sector overlays and recipe actions, inherited game
callbacks, companion-ROM startup helpers and configured game-memory writes.
Standard CD emulation, save handling, video initialization and explicit user
RAM-cartridge personality remain active. The existing global `skip_patch`
setting retains its previous priority and behavior. Games requiring a ROM
helper or compatibility patch can fail to boot in this mode. This is the
corrected SAROO core with game patches disabled, not stock firmware emulation.

A bypassed session retains that choice through automatic disc changes.
The next ordinary menu launch restores normal patching. No BIN/CUE, recipe
or configuration file is renamed or rewritten; the choice adds no SD writes.
Z enters the ordinary CD-player path and clears the per-launch menu flag.

The public MCU already implements the needed opt-out: ordinary `LOADDISC`
with an unflagged catalog index clears overlay admission and replaces its
session policy. Automatic loads retain that policy; a later requested launch
can select overlays again. The new menu uses this existing command instead
of the private checkpoint's additional high-bit bypass encoding. On public
legacy commands that bit already requests overlays. No opcode, capability,
pack ABI, MCU source or MCU allocation changes are needed.

The menu publishes the ordinary request for a bypass at every tested
capability value 0–5, while ordinary launches retain their existing
legacy/FIRSTDISC/ROMDISC selection. Unsupported capabilities retain the
existing ordinary fallback. This checks the known public negotiation
contract; it does not qualify arbitrary third-party firmware.

The menu also gates callback/configuration writes and both ROM preparation
paths directly. Stale recipe/ROM actions cannot make a bypassed launch copy
a ROM over the menu. Configuration values are consumed before skipping
their memory write, so a value resembling a cartridge key cannot be
misinterpreted as the next key. Explicit cartridge configuration is retained.

## Timing and preserved hardware setup

The nonblocking state machine uses the existing changed/held controller
events and 1 MHz FPGA timer. Only a pending hold needs idle polling; there
is no allocation, new task, busy wait or persisted setting. Zero denotes
an idle poll and cannot manufacture a release. Unsigned elapsed arithmetic
preserves the hold boundary across timer wrap. The bare SH-2 toolchain has
no libc `stdint.h`; native integer fields retain the original 32/16-bit
widths, enforced by a compile-time assertion.

The timer rate comes from the existing FPGA 100 MHz clock divided by 100
in [`SSMaster.v`](../FPGA/SSMaster.v), not a newly measured physical delay.
[C11 draft N1570 §6.2.5 paragraph 9](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
defines unsigned arithmetic modulo its representable range. The 650 ms
interaction is authored policy, not a Sega requirement.

Normal VDP1 initialization still precedes game/configuration operations.
It retains the complete existing clear and command markers, including the
`8000h` Draw End value specified by
[Sega VDP1 User's Manual ST-013-R3 §7.10, printed p. 132/PDF p. 147](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf#page=147).
The BIOS clock tails and original ROM copies are unchanged. These references
explain preserved arithmetic/hardware contracts; they do not certify this
new user interaction on a Saturn.

## Reproduce verification

From the repository root with the documented local dependencies:

```sh
python3 -B -m unittest discover -s tests -p test_menu_launch_errors.py -v
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
python3 -B -m unittest discover -s tests -p test_sector_overlay_mount.py -v
SAROO_MENU_LAUNCH_PROFILE=release SAROO_MENU_VDP1_PROFILE=release \
  PYTHONPATH=tests python3 -B -m unittest -v test_menu_launch_errors test_menu_vdp1_order
cargo run --offline --locked --manifest-path tools/Cargo.toml -p firmware-build -- \
  menu --repo "$PWD" --out tests/build/launch-choice-menu
SAROO_OVERLAY_MENU_ELF="$PWD/tests/build/launch-choice-menu/source/ssfirm.elf" \
  python3 -B -m unittest discover -s tests -p 'test_menu_*.py' -v
python3 -B tests/check_menu_clock_sh2.py tests/build/launch-choice-menu/source/ssfirm.elf
```

The public-parent selector fails the new hold control because it launches
on the first A press; all four pre-existing selector methods still pass
against that parent. The candidate checks short taps, the exact hold
boundary, wrap, idle polls, held repeats, fresh confirmation, cancel, failed
launches, every 16-bit mailbox reply and normal/player/configuration paths.
The real FatFs loader verifies bypass through automatic disc changes and
restoration on the next requested launch, without extra SD writes.

Actual startup/dispatcher checks retain complete VDP1 output, byte/word/long
write suppression, user cartridge priority and the next ordinary launch.
Linked SH-2 checks retain the complete ROM copy and private call frames;
bypass refuses stale ownership with no writes outside those exact frames.
The ordinary clock tail and 48 linked ROM clock cases pass. These are
source/host/instruction/link checks, not physical controller timing,
natural return/relaunch or full-game acceptance.

The first target build exposed the missing freestanding `stdint.h` and
is preserved alongside the corrected build. Two host-fixture assumptions
were corrected: binary extraction now selects complete functions instead
of unrelated adjacent declarations; the ROM control includes its actual
ABI stack saves and the linked image's initialized data. Earlier failures
are retained. No product workaround was added to hide a failed test.

## Measured cost

| Resource | Public parent | Candidate | Change |
| --- | ---: | ---: | ---: |
| Menu code/data | 87,336 bytes | 87,880 bytes | +544 bytes |
| Menu BSS excluding reserved stack | 9,496 bytes | 9,508 bytes | +12 bytes |
| Body margin | 17,856 bytes | 17,300 bytes | −556 bytes |
| Reserved stack | 16,384 bytes | 16,384 bytes | unchanged |
| Selector direct compiler frame | 32 bytes | 32 bytes | unchanged |

The state-machine direct frame is 0 and the new idle caller's is 12 bytes.
Direct frames exclude callees/interrupts and do not prove full stack
headroom. Menu font and fixed padded image sizes are unchanged. All source
inputs match the successful linked candidate; MCU/FPGA source and the
previously verified MCU build cohort are unchanged. Firmware size is not
a measured gameplay-speed or physical-stack result. No Saturn operation
or hardware qualification was performed.
