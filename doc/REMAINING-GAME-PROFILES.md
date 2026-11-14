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

## Marvel Super Heroes: three retail discs and Japanese demo

Recipes: [USA V1.001](../recipes/marvel-super-heroes-us-v1001-sector.json),
[Japan V1.003](../recipes/marvel-super-heroes-jp-v1003-sector.json),
[Europe -50 V1.000](../recipes/marvel-super-heroes-eu-50-v1000-sector.json)
and [Japanese demo V1.000](../recipes/marvel-super-heroes-jp-demo-v1000-sector.json).
Adapted from paps-g `84f83f5d`, `67718cd7` and `2540a90e`, with the later
`f72b02fa` timing review and demo evidence correction. Original behavior comes
from [tpunix's contribution](https://github.com/tpunix/SAROO/commit/dc4b03008cf3c25b39bfe5353e90f3e967ebd01c)
and the retained [MARVEL_SUPER functions](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L470).

| Exact identity | Original later pointer | Source edit FAD / raw offsets | Guard FAD / raw offset |
| --- | --- | --- | --- |
| `T-1214H   V1.001` USA | `060d13f8` | 1250 / 188,484 | 202 / 1032 |
| `T-1215G   V1.003` Japan | `060d13f8` | 1250 / 188,484 | 202 / 1032 |
| `T-7032H-50V1.000` Europe | `060d15ac` | 3469 / 188,484 | 202 / 1468 |
| `6106664   V1.000` Japanese demo | `060d1424` | 260 / 1356,1658 | 191 / 1076 |

All four native pointers contain `060d0ff4`. The first ordinary call through
`060d0e1c` stays unchanged. The retained launch callback replaces only the
first matching later pointer, in retail/demo/Europe priority order. Its
handler calls the original loader once, then selects 1 MB RAM and conditionally
NOPs two stores in the loaded MSH program. A test that intercepts every loader
call overmatches that original behavior and is not an accepted control.

The retail guard pair is `0600b0fc`/`0600b2d8`; the demo pair is
`06005f5c`/`06005d8c`. Both members must contain `23301ff0`. Retail takes priority
if both pairs match. Native source replaces only the original `2212` words
at retail `0600b0ac`/`0600b1d4`, or demo `06005d3c`/`06005e6a`, with `0009`.
These are inherited cartridge-bus initialization edits. Each guard pair is
within its edited source sector and protected by its complete preimage CRC.
Other programs and native loader calls stay unchanged. No copied helper,
entry redirection, runtime patch writer or timing compensation is supplied.

The source recipe requests 1 MB through the existing admission contract,
before explicit user configuration. The retained 24-byte `ssctrl_set` helper
reads the 16-bit SAROO `SS_CTRL` register at `25897004`, clears mask `3000`
and sets `2000`, preserving all other bits. These fields are defined by
SAROO's own source, not a Sega register requirement. Absent/rejected packs
retain that original late callback. For admitted packs, explicit user RAM
configuration stays authoritative under the existing public contract.

The original first files are unchanged: US/JP `00.BIN;1`, 130,536 bytes;
demo `00.BIN;1`, 130,552; Europe `00_E.BIN;1`, 130,936, all at `060d0000`.
Fresh compilation of the Japanese retail, demo and European complete CUE/BIN
inputs reproduces the exact archived 720-byte packs. Current production-C
streams cover all 886 sectors / 1,808,024 logical bytes of their complete
first files and MSH programs. Exactly four logical bytes change per MSH file.
The original guard pointers and all first-file bytes remain intact. Generated
padding after unedited partial final sectors is explicitly recorded.

| Region | MSH LBA / bytes | Complete original MSH SHA-256 |
| --- | --- | --- |
| Japan | 1082 / 475,216 | `6c0539121c56e428a757163072e8eb6b379d2d9c689c641b126266f79697d0e1` |
| Demo | 103 / 465,360 | `f1615961dc26e6324062a91763a90bbeb6b9985d1acbef935235cd7175ca1149` |
| Europe | 3301 / 475,424 | `9d59bd5e314ca448e2aec0ff961b82aaf78516e20a7d9d43f021cf3a047550ba` |

**US source limit:** its complete original image is unavailable now. Its
recorded track hash is
`f515374ec0810379bfa3771f71832e985be27c4f72c2cc62a08e7ffedbc19893`.
The retained 720-byte pack hashes to
`49b055a31761a3f3264cb88a5b9ffad94cda8d86bc1cffa114ab55b50c05cbef`
and matches the historical exact-source production-C receipt for FAD 202/1250,
action 129. The preserved complete first file rehashes, reproduces the FAD 202
source CRC and passes the current C guard/refusal/retry check. Later program source
and full US postimage are not freshly qualified. US and Japanese record
bodies differ; Japanese data is not substituted for missing US source.
The normal compiler still requires the exact full US source hash and before
words from an owner. The US profile remains an opt-in historical-source case.

The public checks execute the actual linked 60-byte launch callback and
128-byte handler with its real 24-byte helper. Eight pointer combinations
check callback priority; sixteen guard combinations, both incoming T values
and four original control values check short-circuit reads, the single loader
call, masked read/modify/write, all conditional stores, complete GPR/T effects,
stack writes, PR restoration and return. The old and current handler bytes
match after adjusting only the linked helper pointer; helper bytes match
exactly. Actual launch checks cover all four IDs, malformed/absent handoffs,
physical-disc bypass and recipe/user RAM-setting order. Both 58-method overlay
and 20-method launch profiles pass sanitizer and optimized builds. Commands
in the Batman verification section include these checks.

All 78 exact-pointer, 27 read-timing and 143 natural caller-state evidence
pins rehash. The source-call comparison records convergence of all 22 observed
CPU fields at 32 paired observations. It records no complete Work RAM witness;
that bounded caller proof does not establish universal CPU or memory equality.
The stronger exact-pointer comparisons match video/nonblack output at all
121 checkpoints through frame 7201 in all four regions. Demo audio CRCs and
sample counts also match all 121; its captured frames 700..2500 PCM interval is
identical, and is not called a complete capture here.

| Retail | Matching audio CRCs /121 | Matching sample counts /121 |
| --- | --- | --- |
| USA | 57 | 116 |
| Japan | 23 | 23 |
| Europe | 34 | 34 |

Complete retail PCM differs in the original menu-address comparisons. A
separate archived control changes only modeled 32-bit cartridge instruction/
data reads in the unchanged handler/helper to Work RAM timing. Its current
rehash and stream comparison reproduce all 121 four-field matches and every
candidate PCM byte in all three regions. The corresponding complete PCM sizes
are 21,229,448 US, 21,230,512 Japan and 25,445,472 Europe bytes. This isolates
the recorded emulator discrepancy to that modeled read cost; it neither
establishes a physical sound defect nor warrants a production delay.

**Qualification limits:** historical comparisons preselect 1 MB RAM. They do
not qualify the earlier availability/physical switching transition, alternate
explicit RAM settings, bus/cache timing or other paths on a Saturn. Retained
source-loaded-handler and overwritten-IP failures remain evidence. Current
ports add no firmware runtime code or growth. Keep all profiles opt-in with
their built-in fallbacks; these tests are not physical game qualification.

[Hitachi SH7604 §2.4.1, Table 2.12, pp.33–34](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49),
[Table 2.14, p.37](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=53),
[Table 2.16, p.39](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=55) and
[Table 2.17, p.40](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=56)
support the load/store, logical, call/return and PR/flag checks.
[Sega SCU ST-097-R5 §3.6, pp.62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=78)
defines the ASR0 bus-control register. The game-specific policy comes from
the original SAROO contribution, rather than a Sega requirement to remove
those game instructions.
## Pia Carrot 2: Japan V1.002

The [exact-disc recipe](../recipes/pia-carrot-2-jp-v1002-sector.json)
adapts paps-g's later `6097f7ee` correction to the original September 30
candidate. Its three edits reproduce
[tpunix's guarded callback](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L902)
directly in the original `A100.BIN;1`. Earlier copied-handler and entry-redirect
experiments are superseded. No helper is inserted, no RAM mode is selected,
and no loader or result code is rewritten.

| Native address | FAD / raw offset | Original → selected | Effect |
| --- | --- | --- | --- |
| `0604673c` | 476 / 1868 | `2122 → 0009` | Omit first ASR0 store. |
| `0604674c` | 476 / 1884 | `490b → 0009` | Omit cartridge-memory test JSR. |
| `060467de` | 476 / 2030 | `2122 → 0009` | Omit second ASR0 store. |

Both ASR0 stores write `23301ff0` to `25fe00b0`; both adjacent AREF=13 writes
remain. The omitted routine at `060468f8` tests cartridge memory using
address-derived values, complementary values and two bank selections. The
following NOP, signed CMP/PZ, saved outer PR and existing return/error paths
remain. The replacement does not invent a success result. Bounded checks with
positive and negative incoming R0 values retain the native result branches;
this does not assert what all real bank-selection paths put in R0.

The register layout is documented in the
[SCU user's manual](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf),
§3.6, printed pp. 62–67. Instruction rules are in the
[SH7604 hardware manual](https://antime.kapsi.fi/sega/files/sh7604.pdf),
§2.4 Tables 2.13, 2.16 and 2.17, printed pp. 35–36, 39–40:
CMP/PZ updates T, JSR updates PR and has a delay slot, and NOP leaves registers
and T unchanged. These define the instruction effects, not a Sega requirement
to omit the cartridge test or reconfigure ASR0 this way.

| Original program | LBA / complete bytes | Original SHA-256 |
| --- | --- | --- |
| `A.BIN;1` | 24 / 150,292 | `b9e09eda512884058092193c19e10070e2fa52b3c98cf1c2ad821b37e2e05653` |
| `A001.BIN;1` | 98 / 38,320 | `b0909993edf5a8d84cb93ecc5c157d56e5bb5e6a89b9fe4aa8db85b2d4fd89f5` |
| `A002.BIN;1` | 117 / 206,400 | `28a78559091669d991987f09c9297e7ad473d34f2e261c35c55186ac04d59989` |
| `A100.BIN;1` | 218 / 241,492 | `cd3884b0d384d802477957b533f729d282d2339ec3bf9d850f1eca6162d76e0e` |

Fresh source correspondence and the current sanitizer-enabled production-C
decoder cover all 312 sectors / 636,504 logical bytes. All guard sectors are
source-verified. Only the six selected bytes in A100 change; its complete
postimage is `32972c5b25dad28c2bba965adba805afd2ad359d75816e6b5689d904ed199a80`.
The four-record, 1,416-byte pack reproduces archived SHA-256
`5fa164eaa824d80e01c22b6a1daf0a7c0fdffdbfcf34e54fa2d6d07e7b82250e`.
The complete disc is unavailable locally now. Its recorded track hash is
`1bc9fb850c43bc8d0f593d7b76926b4139f73997f7a2dd938ab3980c80aadabd`;
the normal owner-side compiler still requires that whole source. Logical-file
verification does not freshly qualify the track hash or original final-sector
padding.

The first entry `06021fe4`, A001's JSR and `002100c0=06010000` literal stay
unchanged. A002 and A100 load at `06010000`; A100 resets its stack and BSS.
Guards at FAD 174/248/477 bind the original entries and later literal context.
The current linked 48/36/124-byte callback and handlers are checked against
all 64 late predicate combinations, both incoming T values, short-circuit
reads, exact writes, complete GPR/T/PR state and both native entry targets.
Actual launch checks cover accepted, absent and malformed handoffs, physical
disc bypass and explicit user RAM-setting priority. Rejected or absent packs
retain the original callback; the source recipe changes no firmware bytes.
Both 62-method overlay and 21-method launch profiles pass sanitizer and
optimized builds. All 37 fresh target-menu input hashes and resource sizes
remain unchanged. The public commands above reproduce these checks without
requiring a game image; full-file checks require the owner's exact source.

The preserved 18,000-frame comparisons are revalidated from their pinned
configuration, original logs and complete PCM. Both no-RAM and 4 MiB callback
comparisons match all 301 sampled video/nonblack outputs, but only 46/301
audio CRCs. The first audio difference at frame 2761 precedes A100's load at
4407; it is not explained by the later six-byte edit. Sample counts match
287/301 without RAM and 299/301 with 4 MiB. Each complete PCM has 53,073,720
bytes and differs; no complete single-shift explanation is established.

Against the unmodified image without RAM, all 301 sampled outputs and every
PCM byte match, SHA-256
`1968cd3cd9430b2f01fb513e18249c258a91d40f5e4600eb1022608f4c40c775`.
With 4 MiB, video matches 275/301 and audio 84/301, with first difference
4921 after A100 loads. The actual bank-test effects and complete cause remain
open. These are historical model results, not a new console run or full-game
qualification. Keep the profile opt-in pending physical bus/audio, bank/reset,
other-program and full-game checks. No delay is added to imitate old helper
overhead.

## X-Men vs Street Fighter: Japan V1.002

The [exact-source recipe](../recipes/xmen-vs-street-fighter-jp-v1002-sector.json)
selects paps-g's `01cc13f5` native design, adapting
[tpunix's original workaround](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L640).
The original handlers skip two CPK calls, write cartridge status `5c/ff`,
NOP a later ASR0 store and redirect two startup calls. The selected recipe
retains the original loaders and calls, using the loaded module's own success
tail and a collateral-free compressed rewrite. It requests 4 MiB RAM before
explicit user configuration, which remains authoritative.

`CPK.EMM;1` changes only its entry pointer `06080008 → 060800e2` and the
existing success immediate `1 → 5c`. Its native tail writes FF at `06080200`,
then 5C at `060801fe`, consumes the return word saved by the wrapper's JSR
delay slot, and returns zero. The wrapper preserves its original context-save/
restore sequence. This bypasses the original cartridge probe, bank test and
ASR0/AREF reconfiguration. It does not replace the shared file loader with a
fabricated success return. The original first file, CPK calls, `CPK.PCA` and
later MMPACK handoff remain unchanged.

The 113-byte rewrite at MMPACK file offset 154 changes 45 stored bytes without
changing its length, destination table, stream termination or later packed
bytes. All 630,460 decoded bytes remain identical except `d82e → a001` at
`06014098`. The native BRA skips the ASR0 store at `0601409c`, retaining both
stack pops and the following AREF=19 write. The original MOV.L remains in
the branch delay slot; its changed PC-relative read is discarded by the
following R0 restoration. T and PR stay unchanged. The
[SH7604 manual](https://antime.kapsi.fi/sega/files/sh7604.pdf), §2.4 Tables
2.12/2.16/2.17, printed pp. 33–34 and 39–40, defines those MOV, delayed-branch,
JSR/PR and system-instruction effects. See also
[SH software manual §§6.1.8/6.1.34](https://www.renesas.com/en/document/mas/sh-1sh-2sh-dsp-software-manual)
for BRA and PC-relative MOV.L. The
[SCU manual §3.6, printed pp. 62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf)
documents ASR0/AREF. These citations describe the hardware; the workaround
and personality choice come from the original SAROO contribution and source
investigation, rather than a Sega mandate to skip the game's tests.

| Source | LBA / bytes | Complete original SHA-256 |
| --- | --- | --- |
| `0;1` | 26 / 48,032 | `85c73748cd5dbedb269c638811ac863c0a693f8dd8a37b79ce780b1c6e517afa` |
| `CPK.PCA;1` | 53 / 74,233 | `8d24cb9515b4de917ad9453635b1e382edc5461268bb0eaa1a794e5987179f20` |
| `CPK.EMM;1` | 1057 / 528 | `f534297e5de222b7c98f89ea5654b0c8af1b6a21148672c58ecc0ab799e6cbb7` |
| `MMPACK.PCA;1` | 45462 / 258,319 | `60dddde37c9f600f2adc1012bf323096faff63659e8b3bda8234e1ae8beab79a` |

Current production-C streams cover 189 sectors containing these 381,112
logical bytes and all guards. EMM is explicitly extended with 1,520 generated
zero bytes to supply its target sector; that canonical sector reproduces the
archived original record CRC. This is not fresh verification of original raw
padding or the unavailable complete track, whose recorded SHA-256 is
`c904a38644964e91e6dc0ee93ea41e921ca673efe708a8e2658207c736bfda70`.
Other unedited partial final sectors also retain their generated-padding limit.
Exactly 47 logical source bytes change. Complete EMM and MMPACK postimages are
`b5b81ba31bd07a8f63a1fabb92605bf380be4a84224f4cad1eb1f508bb7efd57`
and `22154f524c2d50b5404fd2ccaf3f0563593d35e67655b67830f0bfc3e54a3da3`.

The maintained [offline PCA reader](../tools/README.md#offline-pcaprs-check)
is a necessary verification tool, with synthetic packet, overlap, header,
destination, truncation and resource-bound controls. Its full outputs match
the preserved original SH-2 decoder's outputs byte for byte. That retained
native fixture is also rerun freshly on the host with original decoder code:
both cache modes, complete caller context and unrelated 2 MiB Work RAM checks
pass for original MMPACK, edited MMPACK and unchanged CPK. The truncated native
end-marker control refuses. This is host execution, without real IRQ, bus,
loader or game qualification. Complete decoded hashes are:

- Original MMPACK: `1b5b2fd6dc431e0cad2b34c241eb0d5ddea339cddc71374181aadcc7b97883c1`.
- Edited MMPACK: `35e38fa316e0191c46d1879732f8701c97f9e20dd62c2235fcdc397e57573a96`.
- Unchanged CPK: `6eed855a63c8183a29173cbf0cfdf53b54754c76be7085a3f6e73c7cb2dffdad`.

The six-record, 2,112-byte pack reproduces archived SHA-256
`19e15a1627a6274a65e2fc26f2f17e63a7d78623d73e617853b87c51bcfbd58b`.
The [bounded split-record prerequisite](SD-SECTOR-OVERLAYS.md) adds 164 MCU
load bytes, with unchanged static allocation. Older firmware rejects this
pack. Existing packs retain their bytes; no additional per-read SD access is
introduced. Linked checks execute the actual 40/48/24-byte launch/late functions,
including the launch RTS delay-slot store and complete GPR/T/PR effects.
Separate bounded native-tail and branch checks preserve status-write order,
the saved return word, zero result, changed delay-slot read and restored
registers. Actual launch tests cover both original fallback handlers,
accepted/rejected/physical bypass and recipe/user 4 MiB priority. Both
72-method overlay and 22-method launch profiles pass; all 23 Rust tests pass.

The rehashed 18,000-frame historical pair retains 301 checkpoints: 242 video,
291 nonblack, 294 cumulative sample counts and 15 audio CRCs match. Video/audio
first differ at 901, sample counts at 1261. Complete PCM streams each contain
53,069,472 bytes and differ. Native EMM status arrives at frames 799/800 and
the MMPACK branch at 1421; the callback takes a different load/edit route.
These events do not explain all output differences. Earlier private console
entry/return observations do not qualify this public MCU/recipe pair, smooth
gameplay, audio or other routes. Keep this profile opt-in with the timing/audio,
bank/reset, other-path and physical-acceptance limits open. No delay is added
to imitate the callback's overhead.

## Pad Nifty: Japan V1.007

The [exact-revision recipe](../recipes/pad-nifty-jp-v1007-sector.json)
selects paps-g's `6dc60921` correction to the original September 30 candidate.
It adapts [tpunix's shared modem-check callback](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L717),
retaining that callback when no valid replacement pack is admitted. No RAM-mode
action is supplied; explicit user configuration keeps its existing behavior.

The original first program calls its own decoder at `060109d4`, supplies a
917,504-byte capacity and then enters `06020000`. Program `1;1` contains the
native modem routine: read `25885029`, compare with 17, then BF at `0602a15e`
to the false return. The inherited handler NOPs that branch. The source recipe
instead changes only the packed high byte at offset 24,690, FAD 206/raw 130:
`8b → 70`. It decodes `8b02 → 7002`, ADD #2,R0. The following MOV #1,R0
discards the sum, retaining all returned registers and flags from the callback's
NOP path. ADD performs no memory operation or control-register access.

This one-byte choice matters. The rejected packed `8b02 → 0009` attempt also
changes the byte at decoded `0602a18f`, through a back-reference to the shared
low byte at `0602a15f`. A fresh native decode reproduces all three differences;
the selected `8b → 70` changes only `0602a15e` throughout the full 917,504-byte
allocation. The earlier STC candidate also remains unselected. The
[SH7604 manual](https://antime.kapsi.fi/sega/files/sh7604.pdf), §2.4 Table 2.13,
printed p. 35, defines ADD without a T change; Table 2.16, printed p. 39,
defines BF without a delay slot. §4.6 Table 4.10, printed p. 75, explicitly
lists STC among instructions after which an interrupt is not immediately
accepted. ADD avoids that additional inhibition. Those rules establish the
instruction choice, not the intended behavior of the game's modem service.

| Source | LBA / bytes | Complete original SHA-256 |
| --- | --- | --- |
| `0;1` | 24 / 39,470 | `38a0060bccab76442a25a9d9114ca99ce162a624130c5aca32ab8b0a65ed2ddf` |
| `1;1` | 44 / 100,483 | `c37afcfd2e288124a044bdc4fd72b18fe685a5f1359c107bf662600fdb377cf5` |

Fresh current-C/source checks cover all 70 sectors / 139,953 logical bytes
and both original guards. The first program, decoder, original JSR and
`060105b4=06020000` literal stay unchanged. The only changed packed byte yields
complete postimage
`ea667402040b6ae61e044961217f4fd7cb40fd0296e73c590f36e6c800affff7`.
The three-record, 1,068-byte pack reproduces archived SHA-256
`474d3dc9dfbc346fd34c5cdf61a1f9163d00410e8da2842849b9c7b564e5b7ee`.
The unavailable full track remains historically pinned as
`30453920c2e03c9d05ca2a9e29f38ac347936d8aea0b1eb60bc299c9b8f15fe7`;
logical-file proof does not newly authenticate that track or original final
padding. The normal owner-side compiler still requires its whole-file hash.

Fresh host execution of the retained original game-decoder fixture reproduces
all 231,164 decoded bytes and the complete 917,504-byte allocation with both
cache modes. Original/candidate capacity hashes are
`2b5814fcda04983c3e34f422884c60a8628816cdba9de658b01b99b4302889a2`
and `83b503a79dc3e91cf8adf4f9d4ada64571b2f6d5dbfef710036ab3b9b9a55eea`.
Only one byte differs; source and other Work RAM remain unchanged. The native
return fixture runs 262,144 paired cases across both images: all 256 I/O bytes,
all 256 defined SR states and both cache modes. Its 524,288 NOP/ADD calls
retain complete CPU return state, one I/O read and no CPU memory writes.
These are host-interpreter checks, without physical interrupt/timing acceptance.

All ten original numeric streams `1..9/90` are also rerun freshly in both
cache modes and reproduce their complete preserved outputs. Only `1;1` matches
either inherited guard. This preserves other-stream controls without claiming
every real load route executes. Public synthetic tests independently execute
the original BF, callback NOP and selected ADD for all 256 I/O bytes and both
incoming T values, retaining complete GPR/T/PR, exactly one I/O read and zero
writes. Actual launch tests cover the shared fallback, malformed/absent
handoffs, physical bypass and user RAM priority. Both 74-method overlay and
23-method launch profiles pass. Firmware source hashes and build resources
remain identical to the preceding verified checkpoint.

The rehashed 18,000-frame historical pair matches all 301 video/nonblack
checkpoints, 157 audio CRCs and 295 sample counts. Audio first differs at
9421/9601, after a later program replaces the region at 9232. Both complete
PCM streams contain 53,073,632 bytes and differ. The 15,000-frame same-ROM-map
control still differs at those frames: mapping the old handler on both sides
does not explain the mismatch. Both pairs' logs, PCM and all 32 complete
Work-RAM captures rehash. Keep the profile opt-in pending audio/timing,
other-load-route, physical startup and return checks. No compensating delay
or later RAM writer is introduced.

## The King of Fighters '97: exact YZB 4 MiB V1.000 source

The [exact modified-image recipe](../recipes/kof97-yzb-4mb-v1000-sector.json)
selects paps-g's `e2d4dfa0` through `9f558ea4` native-source correction.
YZB's 4 MiB game modification and
[tpunix's original SAROO compatibility callback](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L322)
remain distinct from this adaptation. This profile is for `T-4401G   V1.000`
with the recorded complete source hash; it is not the separate stock 1 MiB
`T-3121G` profile. Recipe RAM setup precedes explicit user configuration.

Original `0.BIN;1` loads at `00200000`. Its loader selects chunk 12 at file
offset `0af988`, whose original `000ef5a1` header declares 490,192 decoded bytes.
The native call goes through BIOS pointer `000005f8` to the checked stock
decoder entry `00001f04`, with output at `06011000`. Those addresses and source
relationships come from the original loader/BIOS evidence, not a guessed
match for an encoded instruction. The selected edit changes only packed
`2122 → 0009` at file offset 929,581, FAD 633/raw 1853. All chunk offsets,
headers, bootstrap, other source bytes and native startup remain unchanged.

Fresh host execution of the retained stock-BIOS fixture in both cache modes
reproduces both complete 490,192-byte outputs:

- Original: `f1ea01e80e7b404bed283b86903718078d394eae1db1aad3a33740fd764f33a0`.
- Selected: `7fdab7b90090946ae20cd776329ebebaa411ffb06f60f944bc2eddd4ef6319ec`.

Only offsets `55f80/81` change, giving the native NOP at `06066f80` in place
of MOV.L R2,@R1. Every other decoded byte matches, without back-reference
collateral. The original writes `23301ff0` to ASR0 at `25fe00b0`; the adjacent
AREF=19 write stays intact. Separate bounded instruction checks retain complete
GPR/T/PR state and that refresh write while omitting only the ASR0 write.
The [SCU user's manual](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf),
§3.6, printed pp. 62–67, documents those bus registers. The
[SH7604 manual](https://antime.kapsi.fi/sega/files/sh7604.pdf), §2.4 Tables
2.12/2.16/2.17, printed pp. 33–34 and 39–40, defines MOV.L, JMP/JSR/PR,
delay slots and NOP. The inherited bus workaround is not proof that the game's
original ASR0 setting violates a Sega requirement.

The full 1,048,576-byte `0.BIN;1` at LBA30 hashes to
`6d7a367567dd9859d22af4027b2085d3dfcf4ce8da5a3b74e5b0d147bcf79baf`;
the complete postimage is
`9a078d7b4513c928fccf15e3615dfdbf75abe550294df5013d787fe54365b382`.
The preserved initial transfer is 32,768 bytes, SHA-256
`e54cd524908bcf049a1045c459bac4fd83810ace08036e99385971cae12a74fb`,
including the declared 6,144-byte IP and unchanged AIP guard at FAD151.
Current production-C/source checks cover all 528 sectors / 1,081,344 supplied
logical bytes, with exactly two changed source bytes and every other byte
unchanged. All target payloads are complete; no target padding is generated.
The two-record, 720-byte pack reproduces archived SHA-256
`a2b2cf97c189bbf7c79f0d64c855334e180741401aeec75925fcfe1aff4f67f5`.
The normal compiler still checks the whole source FILE hash
`3b98e195eeb720d82cfb6995d7ff8df65367b18981d9d0bbad355f5670f6ab56`.
That complete image is unavailable locally now; logical-file evidence does not
freshly qualify its track/CUE geometry or real console behavior.

The source recipe removes the inherited 28-byte writer and startup redirect.
Public checks execute both old writer stages, their JMP/BRA/JSR delay slots,
exact stores and complete register/flag effects. The old writer gives the
native entry PR=`06007066`; the source route retains the original caller's
PR. The native prefix resets SP and jumps to `06011034` while preserving that
incoming PR. This is deliberately not claimed as universal entry equality or
an explanation of audio timing. Actual launch checks preserve both IP guards,
the untouched writer-gap word, the stock-profile path, rejected/absent-pack
fallback, physical-disc callback behavior and explicit user RAM priority.
Both 77-method overlay and 24-method launch profiles pass. Runtime source
hashes and target-build resources stay identical to the preceding checkpoint.

Architectural source/output bounds and unchanged unrelated Work RAM pass in
the fresh host decoder; speculative cache-line refill and physical bus timing
remain separate limits. A shortened chunk refuses without creating output.
The rehashed 1,200-frame unmodified-image control matches all 21 sampled
checkpoints and complete PCM; ten complete RAM captures retain only four differing
raw bytes at frame900, corresponding to the packed/decoded edit investigation.
This is a bounded boot control, not gameplay qualification.

The rehashed 12,000-frame inherited-callback comparison matches all 201 video/
nonblack checkpoints, 17 audio CRCs and 198 sample counts. Complete PCM streams
each contain 35,382,448 bytes and differ, first at frames1021/1681 for sampled
audio/counts. All sixteen complete RAM captures remain pinned. The audio cause,
sound/caller-state analysis, real bus behavior, gameplay, reload and return
remain open. Keep the profile opt-in; no synthetic delay is added.
