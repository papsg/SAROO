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
are host checks and a firmware build. Physical media behavior remains
unqualified.

## Browse and launch files from the menu

Adapted from paps-g `a09e734b` after the MCU folder protocol. Open
**Load Binary** and use A or Z to enter a folder. C goes up one level
or exits from the root. B exits from any folder. The menu remembers
the last folder for the current session. Files retain their existing
A and Z launch behavior. The optional `@address` belongs to the file
name, so an at sign in a parent directory has no effect on loading.
[Upstream issue #84](https://github.com/tpunix/SAROO/issues/84),
[v0.8 binary selector and launcher](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/main.c).

The stock menu negotiates both directory and menu completion
capabilities. Older MCU firmware keeps the flat list. The browser
continues controller and animation processing while a folder command
is pending. It waits for the command register to clear and checks
its tagged reply before reusing the request field. Leaving during
a read queues a return and restores the game catalog after the
request retires. A failed folder change retries the previous folder
once. An invalid completion leaves the command workspace unavailable
until reset.
[Public mailbox completion contract](SAVE-CD-RESET-OWNERSHIP.md#preserve-an-in-flight-mailbox-across-saturn-reset).

```sh
python3 -B -m unittest discover -s tests -p test_menu_bin_browser.py -v
SAROO_MENU_BIN_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_bin_browser.py -v
make -C tests check-menu-safety check-menu-binary check-file-transfer
```

With `SAROO_MENU_BIN_SOURCE` pointing at the parent menu directory,
the actual stock selector passes the flat fallback control and
fails the negotiated browsing check. The port checks the real
menu's nested listing, launch path and address, parent navigation,
late replies, queued exit, failed restore retry and malformed
catalog bounds. The linked menu has 84,164 code/data bytes, up
2,112, and a 240-byte saved path in BSS. It retains 21,036 bytes
of body space. The mailbox and input checks are host models.
Physical menu and game behavior remain unqualified.
