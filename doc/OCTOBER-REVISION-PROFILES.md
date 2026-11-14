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
