# October revision profiles

These optional profiles continue the [stock-source migrations](REMAINING-GAME-PROFILES.md).
Use the existing [compiler and admission contract](SD-SECTOR-OVERLAYS.md).
The complete original CUE FILE hash and instruction preimages are required;
the menu prefers the full sixteen-byte product/version key. No pack is
installed or enabled automatically. Missing/rejected packs retain the original
callback. Physical discs retain the callback, and explicit user configuration
keeps priority. Firmware runtime sources and resource use are unchanged.

## Die Hard Trilogy USA V1.000

[Recipe](../recipes/die-hard-trilogy-us-v1000-sector.json), adapted from
paps-g's October 2 review `362e6508`. The inherited `FF` workaround and its
rationale come from [tpunix's callback](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L867);
[original issue #139](https://github.com/tpunix/SAROO/issues/139). Our adaptation
replaces the identified source waits instead of writing into cartridge space.
This title is Die Hard Trilogy, distinct from Die Hard Arcade.

Exact identity: `T-16103H  V1.000`; recorded original data-track SHA-256:
`3bcf41e701bd9a2040d5a8d000aa55b5f7e8816d61c0c0531de9de111d6f03d8`.
Six original executables contain eighteen literal-backed loops:

```text
MOV.W @R1,R0 ; R1 = 025e7ffe
AND #4,R0
TST R0,R0
BT loop
```

The old callback stores `FF` at `025e7fff`, forcing bit 2 ready. The recipe
changes only `6011` to `e004` (`MOV #4,R0`), preserving the subsequent mask,
test and branch. It preserves their successful CPU result and removes the
cartridge read. It does not identify or reproduce intended synchronization,
bus transactions, cache behavior or timing. `025e7ffe` is cartridge CS0,
distinct from VDP2 VRAM at `25e00000`–`25e7ffff`.

| Executable | LBA | Original bytes | Three file offsets (hex) |
| --- | ---: | ---: | --- |
| 0.BIN | 25 | 458752 | 4c7ea, 4cb72, 4cca8 |
| SLIDE.EXE | 249 | 294912 | c2f2, c67a, c7b0 |
| LOADER.EXE | 12875 | 327680 | c2f2, c67a, c7b0 |
| DH1.EXE | 13675 | 524288 | ad36, b0be, b1f4 |
| DH2.EXE | 29849 | 589824 | c2f2, c67a, c7b0 |
| DH3.EXE | 34052 | 637172 | 50106, 50482, 505b4 |

All eighteen source loops, their PC-relative readers and `025e7ffe` literals
are rechecked. The first-file-only three-site proposal is superseded. Static
coverage does not rule out computed/generated code or qualify every route.
The complete 2,832,628 supplied executable bytes produce eight records,
2,808 pack bytes and exactly 36 changed payload bytes. Pack SHA-256:
`9ebc5839a5dc06d86fc6a0e639c6ef8d9aebd856cab6364444ee6e71e8cd7455`.
The current Rust compiler and sanitizer-enabled production C decoder reproduce
the archived pack and complete postimages over 1,384 sectors. The final 1,804
bytes after DH3's unedited partial sector are generated padding, not proof of
original raw padding. The complete data payload is preserved and rehashed;
the original complete raw CUE FILE is unavailable, so its whole-track hash is
historical rather than freshly authenticated.

Public synthetic checks compare every complete raw-sector postimage, damaged
read refusal and valid retry, wrong revision/hash/instruction, complete modeled
GPR/T/PR/memory outcomes, and the real C launch callback. All 256 high bytes
with low byte FF and both incoming T states produce R0=4/T=0; the original
reads once, the replacement reads zero times. A never-ready source still loops
in the original control. The actual launch retains callbacks for missing,
malformed and physical-disc cases. These are bounded host checks without
interrupt, cache or hardware timing qualification.

Historical 9,601-frame native-emulator comparisons reproduce an injected
bad-read failure and show the complete edit removes those observed bad reads.
All 161 checkpoints and complete PCM match between the edited normal and
injected-read controls. The inherited workaround comparison remains different:
the later same-route on-foot New Game check matches only 112/161 video and
44/161 audio checkpoints, and complete PCM differs. Earlier fixed-frame input
entered different modes and is not an equal-route verdict. No compensating
delay is added. Keep the profile opt-in with audio/timing, reset/return,
other-route and physical qualification open. No original issue is closed.

[Hitachi SH7604 §2.4.1 Tables 2.12–2.14, printed pp.33–37](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)
defines MOV and logical/test effects;
[Table 2.16, p.39](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=55)
defines BT. [Sega SCU §1.2, Figures 1.3 and 1.5, printed pp.4 and 6](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf)
documents the A-bus map. These references interpret instructions/addresses;
they do not endorse the game's original synchronization or certify this policy.

## Croc USA V1.000

[Recipe](../recipes/croc-us-v1000-sector.json), adapted from paps-g `5fcb1555`.
The inherited final-VRAM-word workaround comes from
[issue #181](https://github.com/tpunix/SAROO/issues/181#issuecomment-2295618018),
[its reported success](https://github.com/tpunix/SAROO/issues/181#issuecomment-2296596138)
and tpunix's retained `Croc_LOG_patch`. The source-mode repair is our
independent adaptation; it does not resolve all reports in
[issue #359](https://github.com/tpunix/SAROO/issues/359).

The verified USA image has identity `T-5029H-50V1.000`, also used by Europe.
The exact original CUE FILE SHA-256 in the recipe distinguishes the reviewed
source: `8fca6a9e78d07fb1260305572cba7df60c4c1b44f177b4f1767c347064ecd7c1`.
Do not select a recipe by region label or title alone. Original `0;1` is
330,384 bytes at LBA22/load `06004000`, SHA-256
`d6c1ce9c681b60b829ded4fa1d737a2cfbf43a42fb2b75be9904a9ac1a20eb5f`.

Seven literals in five nontextured command-builder branches and one initialized
polygon template contain PMOD `00a8`/`00ac`: SPD is clear and color mode5 is
selected. Change them to `00c0`/`00c4`, preserving the Gouraud bit. Both
textured branches and all reader/store instructions remain original. The
seven PC-relative readers, complete reader-to-store contexts and command-type
branches are freshly checked against the complete preserved file. The eighth
value is in the original fullscreen polygon template, not a copied helper.
The corrected address `060110e4` is the fourth reader; an earlier prose note
omitted one digit. There is no callback writer or new RAM action.

[Sega VDP1 §6.3, printed pp.86–88](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf)
requires ECD=SPD=1 for polygons, polylines and lines; its color-mode definition
and [§7.7, pp.126–127](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf)
require mode000 for nontextured polygons. The observed native emulator reads
the final VRAM word for the invalid mode, explaining why the inherited `FFFF`
write changes opacity. That undocumented read is not claimed as guaranteed
physical VDP1 behavior. The original programmer's broader register-initialization
account does not independently identify these precise PMOD errors.

Eight payload bytes change in six guarded sectors. The current compiler/C
source check reproduces all 162 sectors of the complete file and the archived
2,112-byte pack, SHA-256
`fe7a4047f5360fdf4f9b908531590c40626d5fb0dd8f0948bf8163d3dd43c6a6`.
The unedited final 1,392 padding bytes are generated explicitly. Whole raw-track
identity remains historical because the complete original media is unavailable.
Public synthetic tests check exact byte positions, ECD/SPD/color/Gouraud,
complete raw postimages, refusal/retry and retained callback/configuration
priority, including physical discs. No firmware runtime or resource changes.

The rehashed historical 9,601-frame comparison reaches first-level gameplay:
corrected source with cleared final VRAM matches the inherited `FFFF` control
at all 161 video/audio/sample-count/nonblack checkpoints and across complete
PCM. Corrected observed commands use `00c0`/`00c4`. A full-data-track scan of
201,504,768 payload bytes found each 64-byte source window once; this does not
exclude other, compressed or generated code. Earlier capped observers and
invalid fixtures remain evidence and are not absence proofs. Keep opt-in;
full-game paths, reset/return, paired compiled-menu launch and physical VDP1
qualification remain open. No callback retirement or issue closure.

## Five Japanese revisions

Adapted from paps-g `af511e24`. Inherited operations and attribution are the
original tpunix callbacks `FIGHTERS_MEGAMIX_JAP_patch`,
`Fighting_Vipers_JAP_patch`, `Metal_Slug_patch`, `MANX_TT_SUPER_BIKE_patch`
and `WHIZZ_patch` in the
[original game-patch source](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).
These preserve existing compatibility policy; an undocumented symptom is not
newly established as fixed merely by moving its edit.

| Exact identity / recipe | Reviewed change | Records |
| --- | --- | ---: |
| `GS-9126   V1.001` [Fighters Megamix](../recipes/fighters-megamix-jp-v1001-sector.json) | NOP JSR `060203f4`; retain following NOP | 1 |
| `GS-9101   V1.013` [Fighting Vipers Rev C](../recipes/fighting-vipers-jp-revc-sector.json) | NOP matching JSR `06022a60`; retain following NOP | 1 |
| `T-3111G   V1.002` [Metal Slug](../recipes/metal-slug-jp-v1002-sector.json) | NOP ASR0 store `06079dec`; select 1 MiB RAM | 1 |
| `GS-9102   V1.001` [Manx TT](../recipes/manx-tt-superbike-jp-v1001-sector.json) | Six EXTS.W instructions become full-width MOV | 2 |
| `T-36102G  V1.000` [Whizz](../recipes/whizz-jp-sector.json) | IP `e109` becomes `e108` at `06003192` | 1 |

The Megamix/Vipers helpers write `02201220` to ASR0 and zero to AREF;
Metal Slug's removed store writes `23301ff0` to ASR0. The independent literal
readers, callees and source preimages remain preserved. The
[Sega SCU §3.6, printed pp.62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf)
defines ASR0 `25fe00b0`, AREF `25fe00b8` and device-dependent bus settings.
This corrects an earlier §3.2 citation. The manual does not prohibit every
game-side ASR write; the exact removals are SAROO compatibility policy.

[Hitachi SH7604 Tables 2.12–2.13, printed pp.33–36](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)
confirm MOV and EXTS.W, while [Table 2.16, p.39](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=55)
defines delayed JSR. Manx preserves register fields and T but intentionally
keeps all 32 source bits. Public instruction checks cover all 256 register
pairs, eight boundary values and both T states for original and selected forms.
Whizz stages command8 at `06003408`, later copied to VDP1 command memory;
it changes system clipping9 to user clipping8. The original comment's claim
of complete VDP1 initialization is broader than this edit.
[Sega VDP1 §§7.1–7.2, pp.110–113](https://antime.kapsi.fi/sega/files/ST-013-R3-061694.pdf)
defines those commands. Bounded checks preserve every other register, T and
staged-memory byte for both command values.

Fresh full-CUE compilation reproduces all five complete archived packs and
rehashes their exact original data tracks. Current production-C source checks
stream each complete declared IP and complete first executable. All six
records and full logical postimages match. Partial final padding is generated
and reported, separate from full raw-track compilation. All five packs total
2,208 bytes; no MCU/menu/FPGA runtime code, RAM allocation or helper is added.

Vipers Rev A `GS-9101   V1.010` has `d13b` at the guard; the retained callback
performs no edit. Actual source compilation refuses the Rev C recipe on Rev A,
and actual C launch checks preserve that no-edit control. No Rev A patch is
invented. Megamix plain/1M/2M complete data tracks rehash identically, including
their first files/IP; this does not establish audio-track/whole-disc identity.
Metal Slug Rev A V1.005 is already public with the same replacement/1 MiB policy
and remains a separate exact-revision control. Full sixteen-byte keys keep the
two recipes separate; no competing product-only file or new bundle is needed.

The real C launch checks compare complete 1 MiB high Work RAM and VDP1 for
admitted, missing/malformed and physical routes, with explicit user RAM-mode
priority. They include the menu's unchanged BIOS helper writes. Earlier fixture
failure omitted those shared writes and remains preserved as a test failure,
not a firmware defect. Synthetic sector tests cover exact edits and retries.
Historical compiled callers additionally compare 2 MiB Work RAM, 512 KiB VDP1
and 128 KiB menu memory; Metal Slug leaves only the known four-byte saved-PR
slot in dead menu stack. Returned state and other expected bytes match.

Historical paired 7,201-frame native-disc runs retain these limits:

| Profile | Equal video / audio checkpoints (121) | Complete PCM equal |
| --- | --- | --- |
| Megamix | 121 / 121 | Yes |
| Vipers Rev C | 121 / 121 | Yes |
| Manx Japan | 121 / 121 | Yes |
| Metal Slug V1.002 | 61 / 49 | No |
| Whizz | 102 / 121 | Yes |

All preserved available comparison inputs rehash. Different video/audio remains
recorded, without added delays or a claim of all-site execution. The original
native-disc runs exclude SAROO's bus/service; the caller models park the slave,
disable cache and model BIOS copying. Keep all profiles opt-in; full-game,
concurrency, reset/return and physical qualification remain open.

## Six direct revisions

Adapted from paps-g `a9c15a7e`, with the later Hissatsu status interpretation.
Attribution is [UMK3 issue #206](https://github.com/tpunix/SAROO/issues/206)
and the retained tpunix callbacks `Ultimate_Mortal_Kombat_3_patch`,
`Hebereke_Popoitto_patch`, `srmp7_patch`,
`Hissatsu_Pachinko_Collection_patch` and `Heart_of_Darkness_patch` in the
[original source](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).
Our adaptation uses exact-disc source edits and revision-specific filenames;
it does not newly prove the need for every inherited workaround.

| Identity / recipe | Source operation |
| --- | --- |
| `T-25403H  V1.007` [UMK3 Europe](../recipes/umk3-eu-v1007-sector.json) | NOP `480b` at `0601875a`, `060187ae` |
| `T-25403H  V2.000` [UMK3 Europe Rev A](../recipes/umk3-eu-reva-v2000-sector.json) | NOP `480b` at `0601876a`, `060187be` |
| `T-1504G   V1.002` [Popoitto Hebereke](../recipes/hebereke-popoitto-jp-sector.json) | Compare immediates `1→56` at `0601d378`, `0601d2f4` |
| `T-16510G  V1.003` [P7 Special](../recipes/super-real-mahjong-p7-special-jp-sector.json) | NOP `2232` ASR0 store at `06011208` |
| `T-1503G   V1.003` [Hissatsu](../recipes/hissatsu-pachinko-jp-v1003-sector.json) | First guard false; compare `1→78` at `0601088c` |
| `999999999 V1.000` [Heart prototype](../recipes/heart-of-darkness-jp-proto-sector.json) | Literal `04000000→02400000` at `060108c0`; NOP ASR0 store `2102` at `060106c4` |

UMK3's helpers at `06017a3c`/`06017a4c` write argument8 to COMREG
`2010001f`, with SF at `20100063`.
[Sega SMPC command list, printed p.27](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf)
identifies command08 as CDON. The recipes preserve both original `e408`
argument instructions formerly in JSR delay slots. Bounded instruction checks
show they execute after NOP with unchanged other GPR/T/PR state and no helper
call; the original JSR control observes argument8. No separate detector or
new runtime suppression is included.

Hissatsu V1.003's first revision guard is false (`0601085d=03`). Its original
status getter copies cached CD status while preserving the interrupt mask;
`06010888` reads the byte, `0601088c` compares with1 and the following BT
returns to `0601087e`. The inherited change is `78`, without moving that
branch or changing the getter. **PAUSE is CD-drive status here.**
[Sega CD communication interface, §7.2.4 data specification 4.0, Status and Report (1/2), printed p.59](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf)
defines 01 as PAUSE; 78 is an inherited compatibility value, not a specified
status. The later source review corrects the earlier ambiguous pause wording.
It does not establish why the workaround was originally necessary or demonstrate
hardware execution of that loop. Public instruction controls exhaust all256
status bytes, both incoming T states and each inherited compare value.

P7's literal-backed store writes `23301ff0` to ASR0. Heart's source preserves
the following `e000 1102` AREF clear and the literal's PC-relative reader
`d08c` at `0601068c`.
[Sega SCU §3.6, pp.62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf)
supports those register interpretations;
[Hitachi Tables 2.12–2.14, pp.33–37](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)
supports the memory and compare effects. These references do not prove every
ASR write is prohibited or explain Heart's broader literal dependency.
The prototype's generic nine-digit product ID requires the exact original
track hash and complete sector/preimage guards, not a title/ID guess.

Fresh full-CUE compilation rehashes all six complete data tracks and reproduces
the archived packs byte for byte: seven records / 2,580 bytes. Current Rust/C
source checks stream all complete IPs and first files, including Heart's full
983,040-byte first file through `06100000`; none is shortened for a fixture.
Hissatsu's complete postimage SHA-256 is
`3e5f5f1e0eace80148923354fc05a3a3a776c9f10b42765ee8e0745c23456ddc`,
matching the later independent full-file review. Generated unedited final-sector
padding remains explicit. Original media is unchanged.

Synthetic sector checks cover every exact edit, original delay slots and
untouched AREF clear, and complete raw postimages/refusal/retry. The real C
launch checks cover all six profiles and the already-public Hissatsu V1.007 /
Metal Slug V1.005 controls, complete high Work RAM/VDP1, rejected/missing/physical
routes and user RAM priority. The host fixture explicitly maps Heart's
big-endian high-half store relative to its full scalar guard; separate byte
oracles and SH-2 checks establish the source edit. A changed prototype literal
retains the callback's no-write branch.

Historical compiled callers use the full 32 KiB initial transfer and complete
first files. The first Heart fixture put its artificial stack/return stub
inside the game's load extent; the corrected fixture uses the linked menu's
`0201d8d8` stack, checks all36 caller-stack bytes and moves its return stub to
low Work RAM. That is a fixture correction, not a game fix. BIOS copies are
modeled, cache disabled and slave parked; concurrency remains unqualified.

Historical 7,201-frame native-disc pairs match all121 video checkpoints for
each source. Popoitto, P7 Special, Hissatsu and Heart match all121 audio
checkpoints and complete PCM. UMK3 Europe V1.007 / V2.000 match only47 /48
audio checkpoints and full PCM differs. Those timing/audio holds remain open;
no artificial delay is added. Later private Hissatsu trials report launch,
purchase/firing and BIOS/menu return on a separate test pair, not qualification
of this public firmware or proof that the edited loop executed. Test/input
helpers from those trials are excluded.

Hissatsu Rev A and Metal Slug Rev A replacement settings are already public and
are deduplicated. Full sixteen-byte lookup keys permit each revision's pack to
coexist without a new product-only alternative bundle. No firmware runtime or
resource change, callback retirement, issue closure or new physical game
qualification. Keep all six opt-in with the recorded route/timing limits.

## Reproducible checks

```sh
make -C tests check-sector-overlay
SAROO_OVERLAY_PROFILE=release make -C tests check-sector-overlay
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
SAROO_MENU_VDP1_PROFILE=release python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
```

The fixtures contain synthetic sectors and minimal instruction sequences;
no proprietary game image is distributed. With independently owned sources,
`sector-overlay compile` verifies the complete CUE FILE, identity and preimages.
`sector-overlay verify-source` accepts pinned complete logical files and a
pinned production-C test decoder, reports generated padding and missing guards,
and verifies complete postimages without treating them as whole-disc or Saturn
qualification. Keep original media, saves and callbacks available.
