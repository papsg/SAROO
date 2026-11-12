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
