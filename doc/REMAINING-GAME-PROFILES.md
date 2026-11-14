# Remaining stock game profiles

This series continues the [later-load ports](LATER-LOAD-PROFILES.md). Profiles
are opt-in; missing or rejected packs retain the built-in callback. Use the
normal [compiler and admission contract](SD-SECTOR-OVERLAYS.md). Original
images, configuration priority and firmware fallback remain preserved.

## Batman Forever Arcade: USA, Japan and Europe V1.000

Recipes: [USA](../recipes/batman-forever-arcade-us-v1000-sector.json),
[Japan](../recipes/batman-forever-arcade-jp-v1000-sector.json),
[Europe](../recipes/batman-forever-arcade-eu-50-v1000-sector.json).
Adapted from paps-g's September 30 candidates (`84f83f5d`, `67718cd7`,
`905d5309`, `2540a90e`) and later corrections, selecting `687bd259`'s
native task-ordering design. The original conditional instruction reorder
and fixed entry come from [tpunix's retained SAROO callback](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L959).
Earlier copied-helper and wrapper-preserving trials remain evidence; they
are not the selected recipes.

The frontend loads WING8 into `00210000`, publishes decompression work to
the slave through cache-through mailbox `260d2810`, then loads WHEEL8 into
that same buffer. The original code waits for completion after the second
load, permitting reuse while the worker still owns the input. The inherited
sixteen-byte reorder moves that existing wait before the second load:

```text
original: d439 490b 6583 d239 6122 2118 8ffc eb00
selected: d23a 6122 2118 8bfc d437 490b 6583 eb00
```

This is the original task-ordering workaround. It adds no rate-dependent
delay. Both PC-relative loads retain their original literal values. The new
BF has no delay slot; JSR retains MOV R8,R5 in its delay slot. The original
slave function clears the mailbox after decompression returns. Bounded
instruction checks show the original sequence can call the texture loader
before the mailbox drains; the selected sequence polls to zero first.
This host boundary does not model real cache, interrupts or scheduling.

The other edit changes `060043c4=0600af9c` to `0600d000`. The old menu replaces
that pointer with its handler, which conditionally writes the reorder and
tail-jumps to `0600d000`. The source recipe enters that existing target
directly, preserving the original caller PR. All three original JSR sites
and their delay slots stay intact, including the third call's longword store.
The original wrapper's twenty bytes remain unchanged. The third route is
source-checked, not qualified by the startup/play comparison.

| Region / exact identity | First file FAD | Wrapper guard FAD | Recorded data-track SHA-256 |
| --- | --- | --- | --- |
| USA `T-8140H   V1.000` | 181 | 194 | `22a06bae32d6e0e180ceb642e44c709bc03e62215173f7f61be2058b494d62ea` |
| Japan `T-8118G   V1.000` | 181 | 194 | `d64d662add5849f11cff8157b4f3b1b97cf4fc4b579da0b8c2ee0e7026b0b245` |
| Europe `T-8140H-50V1.000` | 178 | 191 | `5da6ea9354f2025a54c67c5ffed9a73d85137c0d5fd4d11043a3dbfec485d226` |

The entry edit is at raw offset 724 in the first sector. FRONTEND's edit is
at `06023e44`, FAD664/raw 1620, in all three regions. Guard FAD150 preserves
the native `0f0c` IP declaration. No IP extension, copied helper, runtime
writer or new cartridge action is delivered. Exact revision lookup keys
are a public-port adaptation; European identity has no space before V1.000.

Complete original `0;1` is 28,336 bytes, loaded at `06004100`, SHA-256
`9bfc6f59f7f543d5c03f1468e5f3538ff0eff33be2f043886c383236ccc7a469`.
FRONTEND is 809,792 bytes at LBA 469/load `0600d000`; US/JP SHA-256 is
`76ca9d883da4d6b22bb44f97d25b1daeb69f787d0ea31fa6e013281605fde1da`,
Europe is `7cd5f176ab7491febcc897c46fdb964f0c4e9063e6286314586d03e400607da3`.
Exactly 16 of the 838,128 edited-program bytes change per region. Unchanged
GAME files and full original IP are separate controls. GAME has `000b0416`
at the old handler's guard, so that handler does not rewrite it.

Fresh complete-disc compilation rehashes the Japanese and European source
images and reproduces their entire archived packs. The US complete image is
unavailable; no fresh whole-US-track claim is made. The logical-source check
verifies every region's four records through the production decoder. It uses
the complete preserved first file plus 336 bytes of original sector padding
from the independently pinned raw-read witness, including its final wrapper
guard. All 2,550 streamed sectors cover IP, first file, GAME and FRONTEND.
Padding after other unedited partial final files is generated explicitly.
Each pack is 1,416 bytes. Archived SHA-256 values:

| USA | Japan | Europe |
| --- | --- | --- |
| `43d4846563b6dffd076b414f2779a674446ea39ca32bad3774a6d17e9ac4c90b` | `d9fc8ab9faec20a44c161b9477aa19ae8ebe017dfded4ed65eb05e08ac1aacc3` | `b2e1e1b0701d14c1473ccfed95cec78f3fe67e19f26e6bc1f5233338d689ff58` |

## Batman verification and limits

```sh
make -C tests check-sector-overlay
SAROO_OVERLAY_PROFILE=release make -C tests check-sector-overlay
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
SAROO_MENU_VDP1_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
tools/target/debug/firmware-build menu --repo "$PWD" --out tests/build/remaining-menu
python3 -B tests/check_remaining_callbacks_sh2.py tests/build/remaining-menu/source/ssfirm.elf
python3 -B tests/check_later_callbacks_sh2.py tests/build/remaining-menu/source/ssfirm.elf
```

All 55 overlay methods and 19 actual-launch methods pass in sanitizer and
optimized C profiles. Checks cover original IP/first-file guards, damaged
reads/retries, exact whole-sector postimages, all regional lookup routes,
missing/rejected handoffs and conditional callback fallback. The synthetic
fixture now preserves the IP and declares its source hash when FAD150 itself
is guarded. The linked 32-byte launch callback and 64-byte handler retain
every conditional memory write, register/T effect and continuation. The
handler bytes match the preserved comparison ELF. Three caller models check
original PR and delay-slot writes for native and inherited handler entry.
The fresh menu reproduces the preceding firmware inputs and resource sizes.
There is no MCU/menu/FPGA runtime source change or firmware growth.

The rehashed 9,000-frame comparisons each contain 151 checkpoints. Video and
visible-pixel counts match all 151 in every region. Japan also matches all
audio/sample-count checkpoints and the complete 26,532,188-byte PCM, SHA-256
`61b447fe3cd89c2fd1473e09e20a1c018431506df2bc642aab51e6ca5de906d8`.
USA matches 95 audio CRCs and 148 sample counts, with the first audio difference
after unchanged GAME loads. Europe matches 19 audio CRCs and 143 sample counts,
first differing during frontend startup. Full PCM differs in both regions.
Both receipts observe the requested frontend region, including US mapped to
the actual `na` option. Earlier incorrect-region and wrapper-only comparisons
remain preserved. Neither an audible defect nor a complete timing explanation
is established. Keep US/EU audio holds explicit; no compensating delay is added.

All profiles stay opt-in. Physical bus/cache/audio/reset, frontend return and
other load paths remain unqualified; source checks and emulator observations
do not certify a Saturn game. Built-in callbacks remain available.

[Hitachi SH7604 §2.4.1, Table 2.12, printed pp.33–34](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49),
[Table 2.16, p.39](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=55) and
[Table 2.17, p.40](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=56)
support the load/store, branch/delay-slot and PR/NOP instruction checks.
[§8.4.3, pp.220–221](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=236)
defines cache-through access. These references support instruction and address
interpretation; the specific compatibility policy comes from the original
SAROO contribution and the preserved game source.
