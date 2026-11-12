# Save archives, CD workers, and reset ownership

## Protect live primary save archives

Adapted from paps-g `6b728c6d` (MCU-SAVE-003). The primary handles for
`SS_SAVE.BIN` and `SS_MEMS.BIN` stay open. An ordinary named write could
open the same file again, truncate it during `f_open`, and leave the
owner with stale file metadata.

FatFs now consults the save owner after resolving an existing file
and before any writable open can change it. The guard matches the
mounted filesystem, mount generation and directory entry recorded
by the primary handle. This covers path aliases and leaves unrelated
files, owner-based archive I/O, and the existing read-only path
available. A successful close releases the reservation. A failed
handle remains protected until it closes.

The public port takes the existing application filesystem mutex from
named open through close. Its FatFs build has volume reentrancy
disabled. The archive command continues to use the mutex inside its
existing owner path.
[FatFs R0.15 application note, Duplicated File Open](https://elm-chan.org/fsw/ff/doc/appnote.html#dup),
[configuration, FF_FS_LOCK](https://elm-chan.org/fsw/ff/doc/config.html#fs_lock),
[v0.8 MCU named file and save paths](https://github.com/tpunix/SAROO/tree/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn).

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_menu_owner.py -v
SAROO_MCU_SAVE_PROFILE=release python3 -B -m unittest discover -s tests -p test_mcu_save_menu_owner.py -v
make -C tests check-mcu-save check-file-transfer
```

Run the updated owner fixture with the parent MCU and FatFs sources to
reproduce conflicting writable opens. The paired fixture tests both
archives on FAT32 and exFAT, with 72 named-write cases covering aliases,
truncation, offsets and directory placement. It also checks all writable
open modes, read-only access, failed owner state, reopen and unrelated
files. The parent fails all 12 filesystem/device subcases of the three
new methods and passes the six existing methods. The reopen fixture
compares archive contents through read-only handles. The paired owner
and reopen groups each pass nine methods in both profiles. The GNU MCU
App links 64,572 load bytes with 964 bytes of ITCM space. The generated
filesystems and faults are host evidence.

## Report CD input failure before retiring the reader

Adapted from paps-g `c3bfa9ef` (MCU-CDC-035). A failed track lookup,
seek, or sector read leaves the active reader in place while the
worker reports the error. Both lookup at startup and sector input
have a terminal error exit. Retirement updates the old reader's
state under the IRQ guard. File and directory reads signal EFLS.
Sector and audio failures leave PEND clear and skip repeat handling.
Existing buffered sectors, ordinary file metadata, and a pending
command handoff survive the failure. A directory failure clears
its incomplete directory state.
[Sega CD Communication Interface ST-162-062094, §3.1 HIRQREQ,
printed p. 28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=16),
[§6.2.2–6.2.3 file access, printed pp. 52–53](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=40).

```sh
make -C tests check-cdc-command-buffer
SAROO_CDC_PROFILE=release make -C tests check-cdc-command-buffer
```

Run the current sector-input and PLAY-continuity fixtures with the
parent sources. Five input methods reproduce early release and
one worker method reproduces missing terminal reporting under the
old owner. The other 12 methods pass. The candidate exercises
startup and active failures for sector, file and directory work,
and a new PLAY waiting for a failed worker to retire. The
directory fixture checks preemption after publication. Both CD
profiles pass 208 methods. The GNU MCU App links 64,572 load
bytes with 964 bytes of ITCM space. Physical event timing and
game behavior remain unqualified.

## Keep settled drive state after reader retirement

Adapted from paps-g `218dcb45` (MCU-CDC-036). A queued worker wake
can outlive a pause and a subsequent SEEK. The idle worker returns
to waiting before it can change the settled status or position.
At normal completion the worker updates the old index before
publishing idle. A compiler memory barrier preserves the store
order for the MCU's higher-priority command task.
[Sega CD Communication Interface ST-162-062094, §4.2 drive
control, printed pp. 37–38](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=25),
[v0.8 disk worker](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn/saturn_main.c).

The actual-source PLAY-continuity fixture reproduces both parent
failures and passes the other 11 methods. The port checks a queued
idle wake in three settled states and simulates a new command
publishing its index immediately after reader retirement. Both CD
profiles pass 210 methods. The GNU MCU App links 64,572 load bytes
with 964 bytes of ITCM space. The preemption boundary is modeled
on the host.

## Acknowledge retained PLAY as one operation

Adapted from paps-g `c6dc9847` (MCU-CDC-037). A higher-priority
command can replace a request after its wait times out. The CD
worker now checks whether it can keep the sector reader and clears
the current request under the same IRQ guard. Ordinary pauses use
that guard to retire the reader and its directory state. The worker
signals completion after restoring the interrupt mask.
[Sega CD Communication Interface ST-162-062094, §4.2 drive
control, printed pp. 37–38](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=25),
[CMSIS-Core CPU interrupt mask functions](https://arm-software.github.io/CMSIS_5/Core/html/group__Core__Register__gr.html).

The actual-source drive fixture schedules a replacement pause at
the old request's clear. The parent loses the new request and
passes the other 46 methods. The port retains the replacement,
then services it at the next checkpoint. Both CD profiles pass
211 methods, including ordinary pause and directory controls.
The GNU MCU App links 64,556 load bytes with 980 bytes of ITCM
space. The preemption sequence is a host model.

## Wake CD pacing for drive handoff

Adapted from paps-g `39a45215` (MCU-CDC-038). Drive commands used
to wake the worker on a different semaphore from the one it used
for TIM2 pacing. At the largest supported delay, the worker could
stay asleep past the command's handoff budget.

Timer completion and handoff now wake the worker through the same
semaphore. An ordinary wake still requires the one-pulse counter
to have stopped. A pending drive request cancels pacing. An
absolute RTX deadline bounds a missing completion, including when
unrelated wakes arrive or the tick counter wraps. On timeout the
worker stops TIM2 and clears pending completion, preserving any
queued worker wake for its next wait.

Both startup and end-of-play waits re-enter command admission on
cancellation. A retained reader can resume in PLAY, even after the
second end delay had published BUSY. The worker still finishes
an admitted sector before its next checkpoint. Buffer-full data
can retry from the same FAD, and queued audio advances once.
[STM32H750 reference manual RM0433 Rev 8, §§39.3.1–39.3.2,
printed pp. 1633–1635](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=1633),
[§39.4.1, CR1, printed pp. 1676–1677](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=1676),
[CMSIS-RTOS2 kernel tick API](https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS__KernelCtrl.html).

Both host profiles pass 217 CD methods. Against the parent, the
timer fixture reproduces two waits that miss handoff, and the
actual worker fixture reproduces missed startup and PEND checkpoints.
The other 24 methods in those two fixtures pass. The port checks
late interrupts, missing completion, tick wrap, both audio wait
lengths, and a full-buffer retry. These are host scheduling and
register models.

The GNU MCU App used 64,556 load bytes before this change and
64,636 with the wait change alone. Moving the MCU-private CD
control fields ahead of the fixed tables reduces the final link
to 63,628 bytes, leaving 1,908 bytes of ITCM space. The rearrangement
keeps the 8,168-byte structure, table capacities, and static RAM
totals. Of 737 matched compiler-reported direct frames, three
shrink and the rest are unchanged against the timer-only link.
The delay routine itself uses 24 bytes of direct stack, up from
16 before the timer change. This measures link and compiler
resources. Physical timing and game behavior remain unqualified.

## Preserve an in-flight mailbox across Saturn Reset

Adapted from paps-g `90ff71c0` (MCU-SAVE-001), following the reset
investigation in `dfa2f1d1`. The front-panel Reset requests an NMI
on the master SH-2. SAROO's FPGA command and argument registers
have a separate reset driven by the MCU. When the menu starts again,
a retained command can still own the argument and shared buffer.
Clearing the FPGA's interrupt request happens before the handler
finishes. The MCU's final command clear is the point at which the
menu can reuse the mailbox.
[Sega SMPC User's Manual ST-169-R1-072694, §1.1, printed p. 2](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=12),
[Sega Boot ROM User's Manual ST-079B-R3-011895, §1.0, printed p. 3](https://antime.kapsi.fi/sega/files/ST-079B-R3-011895.pdf#page=7),
[v0.8 FPGA command and argument registers](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v).

The stock menu waits up to ten seconds for an existing command
before publishing startup. It uses the public MCU's existing menu
v1 capability to request completion for startup and preview. Startup
waits for the new catalog and services to finish. Preview still runs
asynchronously after submission. The next stock publisher waits
before modifying the argument or shared request. A timeout leaves
the existing owner intact. The menu uses the retained command
register as the fence, following the later private design.

An older MCU with no menu v1 capability keeps its original untagged
startup and preview behavior. Those two opcodes can remain present
after completion. The compatibility path accepts the legacy sticky
values, so those two operations still have their original reset
limitation on that pairing. Ordinary legacy requests that clear
their command still use the bounded admission wait.

```sh
make -C tests check-file-transfer check-menu-safety check-firmware-save
SAROO_FILE_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_command_completion.py -v
```

Against the parent menu, the command fixture reproduces startup
overwrite, timeout overwrite and preview reuse, with seven other
methods passing. The parent file fixture admits four read/write
subcases while command admission is busy. The candidate preserves
the old argument and payload, waits for final MCU completion,
handles clock wrap and retries, and keeps legacy cold/sticky
controls. The FPGA fixture models console reset and MCU reset
separately. These checks establish the source and register model
behavior. Physical reset timing remains unqualified.
