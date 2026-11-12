# Executable folders

## List executable folders on the MCU

Adapted from paps-g `6f1ab00f` (BIN-DIRS-001). The MCU can list files
and child directories for a single path within `/SAROO/BIN`. The
existing flat list remains available for older menus.
[Upstream issue #84](https://github.com/tpunix/SAROO/issues/84),
[v0.8 catalog and command handling](https://github.com/tpunix/SAROO/tree/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_MCU/Saturn).

Version 1 is advertised at `SYSINFO+0x34`. Command `0x0038` takes
a NUL-terminated path in the existing 240-byte path field at
`TMPBUFF+0x10`. The path must use the exact BIN root and ordinary
child components. Validation rejects alternate separators, dot
components, FAT filename aliases and overlong paths before FatFs I/O.
The scan takes the public filesystem mutex from open through close,
sharing the non-reentrant volume with the disc worker.
[FatFs path rules](https://elm-chan.org/fsw/ff/doc/filename.html),
[`f_opendir`](https://elm-chan.org/fsw/ff/doc/opendir.html),
[`f_readdir`](https://elm-chan.org/fsw/ff/doc/readdir.html).

The catalog keeps its 2,559 offset slots and existing string area.
The top bit of an entry's offset marks a directory. Game lookup
and loading reject those entries. The scan publishes the count
after all entries and directory close succeed. Failed scans leave
an empty catalog for a retry. The tagged reply and final command
clear follow the public mailbox protocol.

```sh
python3 -B -m unittest discover -s tests -p test_mcu_save_discovery.py -v
make -C tests check-file-transfer
```

The exact parent retains the flat list and lacks the tagged folder
command. It fails the six folder subcases and the command check.
The port covers nested directories, UTF-8 names, the 239-byte path
limit, rejected aliases and four injected scan failures with retry.
It verifies the capability word and all signed FatFs results through
the real MCU dispatcher. The GNU MCU App links 63,864 load bytes,
leaving 1,672 bytes of ITCM space with unchanged static RAM. These
are host checks and a firmware build. Physical media and the paired
menu remain separate qualification steps.
