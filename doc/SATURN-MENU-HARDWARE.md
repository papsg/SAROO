# Saturn menu hardware correctness

## Correct level-1 SCU DMA status

SATURN-SCU-001, adapted from paps-g `4b80469e`. The stock menu helper
derived every level's DMA status mask by shifting the level-0 fields.
Level 1 then waited on reserved bit 9 and missed D1WT at bit 19. A
queued level-1 transfer could be mistaken for an idle channel before
the helper rewrote its registers. Public menu callers currently use
levels 0 and 2, making this a latent helper defect.

Use the individual operation, wait and interruption masks defined in
[Sega SCU User's Manual ST-097-R5-072694, DMA Status Register, Figure 3.13,
printed pp. 48–49](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=64).
Level 0 remains `0x00010030`, level 1 uses `0x000a0100`, and level 2 remains
`0x00003000`. Transfer setup and callers are unchanged.

```sh
python3 -B -m unittest discover -s tests -p test_menu_scu_dma.py -v
SAROO_MENU_SCU_DMA_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_scu_dma.py -v
```

The fixture extracts the active helper from the stock source. The parent
fails the reserved-bit and level-1 wait cases. The other 28 register scenarios
pass. Both ASan/UBSan and optimized builds pass all 30 on the port, and the
adjacent VDP1, word-access and palette fixtures pass 13 methods. The complete
[Yaul/GCC menu build](ARABIC.md#build-qualification) retains 84,164 bytes of
code/data and a 488,770-byte image. Linked disassembly contains the corrected
status mask, and all 48 relocated ROM clock checks pass. Physical SCU
arbitration and any future menu use of level 1 remain unqualified.

## Bound the outer waits for physical-disc authentication

SATURN-CD-003 continuation, adapted from paps-g `25e04166`. Each outer
polling phase gets ten seconds from its first nonterminal report. If the
drive is still pending at the deadline, the menu returns the existing
timeout error. Expiry before authentication leaves sector size alone.
Expiry after it attempts to restore the normal sector size and preserves
the timeout even when restoration also fails. A changing nonterminal
status consumes the same phase budget.

[Sega CD Communication Interface ST-162-062094, §4.1, printed pp. 33–35](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=21)
distinguishes ongoing drive states from terminal states. The ten-second
window is a firmware policy measured on returning polls. The existing
relocated operation, command sequence and terminal handling are retained.

```sh
python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
SAROO_MENU_CD_PROFILE=release python3 -B -m unittest discover -s tests -p 'test_menu_cd*.py' -v
```

Set `SAROO_MENU_CD_SOURCE` to the parent `Firm_Saturn` directory to reproduce
six stall failures. Its seven ordinary query/authentication methods pass.
The port passes all 64 CD methods in both profiles and three launch-error
controls. Cases cover counter wrap, failed size restoration, and two valid
slow phases with independent budgets. The SH-2 build links 84,292 code/data
bytes. The 504-byte relocated routine has identical object bytes and only
its resolved print and string addresses change at link time. Physical drive
timing, recovery and waits inside that routine remain unqualified.
