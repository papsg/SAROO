# CD events and game launch

## Exit CD waits on terminal drive states

Adapted from the sector reader and physical disc authentication portions of
paps-g `80364f8f` (SATURN-CD-003). If a sector query returns zero available
sectors, the reader checks the drive status and returns `-1` for OPEN, NODISC,
ERROR or FATAL. It still consumes sectors already in the buffer and retains any
bytes it copied before a later failure. RETRY can still produce data, so the
reader keeps waiting. The disc authentication path now returns on ERROR after
attempting to restore the original sector size. Its existing FATAL-to-status
path retains the original result and cleanup behavior. Sega describes the
drive states and their recovery conditions in the CD library manual.
[ST-162-062094, §4.1, printed pp. 33–35](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21).

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
```

Run with `-k terminal` and set `SAROO_MENU_CD_SOURCE` to the parent
`Firm_Saturn` directory to reproduce the two failures. Both parent fixtures
observe an extra poll where the caller should return or restore sector size.
All 35 CD methods pass under ASan/UBSan and O2 after the change, including
status and flag combinations, partial output, buffered sectors, and cleanup
failure precedence.

The SH-2 menu links 80,820 code/data bytes (+32) and 9,244 BSS bytes.
Physical disc recovery and elapsed wait time remain unqualified.
