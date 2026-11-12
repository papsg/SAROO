# CD power and timing

## Bound physical CD power and cartridge takeover

Adapted from paps-g `ce4de2ff` (SATURN-CD-003) for the stock menu and
BIOS callbacks. CDON and CDOFF share a one-second budget for waiting
for SMPC admission and completion. CDON releases the emulated CD
interface after admission. CDOFF gives SAROO control after SF clears
and OREG31 confirms the command. CDON can separately wait up to ten
seconds for a non-BUSY drive reply, reading all four response registers.
The deadlines are firmware policy. A timed-out command can finish later.
[Sega SMPC User's Manual ST-169-R1-072694, §2.2, printed pp. 13–14](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=23),
[OREG31, printed p. 21](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=31),
[CDON and CDOFF, printed pp. 27–28](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=37).

The menu checks CD power before entering the physical player, starting
disc authentication or selecting a binary or image player. A virtual
BIOS boot checks takeover before initializing the CD interface or changing
boot state. Physical BIOS callbacks keep a power retry pending through
another initialization callback, then retry OFF and ON before reading
the drive status.

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
python3 -B -m unittest discover -s tests -p test_menu_launch_errors.py -v
```

Use the tests from this checkpoint and point `SAROO_MENU_CD_SOURCE` and
`SAROO_MENU_LAUNCH_SOURCE` at the parent's `Firm_Saturn` directory. Twelve
methods reproduce stalled commands, ignored replies or caller failures.
The two ordinary power and physical menu controls pass. The port checks
counter wrap, late completion, a wrong completion code, the ready timeout
and retry before stale status can be consumed. The pinned SH-2 menu links
81,172 code/data bytes with 24,272 bytes of body space. Physical SMPC timing
and disc recovery remain unqualified.

## Bound a stalled game sector read

Adapted from the game-facing portion of paps-g `63e0e2b4` (SATURN-CD-003).
After the first empty buffer reply, the reader has ten seconds to observe
another sector. A newly empty buffer starts a fresh budget. Already buffered
sectors take the normal path without reading the timer. Terminal states and
command errors return their existing errors. A timeout returns `-3` and
retains any bytes the reader has already copied. The timeout is a firmware
policy for an empty buffer and depends on the timer and commands returning.
[Sega CD Communication Interface ST-162-062094, §4.1, printed pp. 33–35](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21),
[§8.2.6 No. 6.2, printed p. 92](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=80).

Run `test_menu_cd_transfer.py` with the parent `Firm_Saturn` path in
`SAROO_MENU_CD_SOURCE`. The parent issues another count query after the
deadline and passes the other 15 methods. The port checks stalls with
and without copied output, terminal status, wrap, and a read that makes
progress across 27 seconds of modeled time. Both CD profiles pass 54
methods. The SH-2 menu links 81,236 code/data bytes with 24,208 bytes of
body space. Physical drive timing and game behavior remain unqualified.
