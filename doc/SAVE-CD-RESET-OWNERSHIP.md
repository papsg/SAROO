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
