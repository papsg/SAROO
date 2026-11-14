# Later-load game profiles

These profiles move existing SAROO compatibility behavior into reviewed,
opt-in SD sector recipes. A missing or rejected pack keeps the built-in
callback. Admission and explicit cartridge configuration use the existing
[overlay contract](SD-SECTOR-OVERLAYS.md#recipe-cartridge-setup).
Original images and saves are not modified.

## Cyber Bots Japan V1.002

[Recipe](../recipes/cyberbots-jp-v1002-sector.json). Adapted from paps-g
`e342a022`, `b86f9f7c` and the `21ca2e9b` source guard, with the later
`b1420c3f` startup review. Original behavior comes from
[tpunix's SAROO contribution](https://github.com/tpunix/SAROO/commit/ca1d4caebdd132e9f2aa17ca3406c6849d2c11fa)
and its retained `CYBER_BOTS_patch`/`CYBER_BOTS_handle1` functions.
[Original issue #50](https://github.com/tpunix/SAROO/issues/50) reports this
revision's boot and graphics problems. This port does not establish a new
defect in that callback or claim to resolve every reported symptom.

The source is `T-1217G   V1.002`. The original Track 1 identity recorded in
the preserved comparison is 103,001,136 bytes, SHA-256
`758fa7956aa6139460c391ec5225e69cc9cde13dfe2281e8c544e594d201dbd1`.
The compiler checks that whole FILE hash when an owner supplies the original
CUE/BIN. The complete track was unavailable during this public port; its
historical identity is not reported as a fresh source-track rehash.

| Sector | Behavior | Archived original raw-sector CRC-32 |
| --- | --- | --- |
| FAD 303 | Guard the unmodified dispatch literal `060dd058=0600d000`. | `5ea4c6ba` |
| FAD 333, raw offset 362 | Replace `2122` at `0601695a` with `0009`. | `3b49c487` |

The later program begins at LBA 164 and loads at `0600d000`; offset
`(333 - 150 - 164) * 2048 + 362 - 16 = 995ah` identifies the same instruction.
The recipe selects 1 MB expansion RAM. The 720-byte, two-record ABI 3 pack
requests callback replacement only after normal admission. The scoped
sixteen-byte product/version key is a public-port adaptation. It does not
relax the original full-track or raw-sector checks.

The retained menu callback selects 1 MB and redirects the dispatch literal
to its handler. The handler writes the two-byte NOP and jumps to `0600d000`.
The recipe delivers the NOP through the original disc read and leaves the
dispatch literal unchanged. These routes intentionally use different call
targets. The preserved natural-startup review traces their incoming R1/R2
differences until the original startup overwrites them; all 22 recorded CPU
fields then converge. Its two complete 2 MiB memory witnesses check the
declared CRT stores. This is a bounded path argument, not a claim that every
possible caller is equivalent.

The rehashed 63-input historical receipt records 201 matching video/audio,
sample-count and visible-pixel checkpoints over 12,000 frames. The full
35,379,664-byte PCM streams match SHA-256
`f287ad460eef207a3c44f42a7f10b0f65b2c54fee31afe1b7e92d1a47c83bd23`.
The comparison includes an observer-disabled control and uses no synthetic
delay or register substitution. These are preserved emulator observations;
they do not qualify current public firmware on a Saturn.

### Verification and references

```sh
make -C tests check-sector-overlay
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
tools/target/debug/firmware-build menu --repo "$PWD" --out tests/build/later-menu
python3 -B tests/check_later_callbacks_sh2.py tests/build/later-menu/source/ssfirm.elf
```

The generated-sector checks compile this recipe with only its source identity
substituted. They compare complete raw output with independent EDC/ECC,
preserve the dispatch guard, reject changed source/versions/instructions, and
check repeated runtime refusal followed by a valid retry. The actual launch
caller and actual built-in patch check absent/rejected versus admitted packs,
the physical-disc branch and user overrides. The linked SH-2 check executes
the current launch callback and handler under bounded instruction semantics;
the existing cartridge accessor is modeled at its call boundary. Wrong write
width, changed delay slot, wrong target and an incorrect return are detected.
The current 24 handler bytes also match the retained comparison menu's ELF.

The menu links at 85,864 code/data bytes, leaving 19,336 body bytes. This
recipe adds no MCU/menu code, static allocation or FPGA change. It adds two
preflight source checks and no extra playback-time SD access. Physical RAM
mapping, concurrent transfer/cache behavior, other game modes and full paired
SAROO boot remain unqualified. Keep the recipe opt-in and the fallback installed.

Sega [ST-040-R4, §§4.3 and 4.7, printed pp. 28–29 and 31](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf)
describes IP and first-read loading; it does not establish a game's later
program mapping. That mapping comes from the retained exact-source review.
The [SCU manual ST-097-R5, §3.6, Figure 3.24, printed p. 62](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=78)
documents A-bus access timing. SAROO's 1 MB personality action and this
game's workaround come from the original implementation, rather than a Sega
requirement to apply these particular edits.

## Groove on Fight Japan

[Recipe](../recipes/groove-on-fight-jp-sector.json). Adapted from paps-g
`b86f9f7c` and `b40bc0c5`, using the later `9eb2cb2b` native-source review.
The original two launch writes and later handler are attributed to
[tpunix's retained SAROO implementation](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).
The exact revision is `T-14411G  V1.001`.

| Original program | Loaded address | FAD / raw offset | Original → replacement |
| --- | --- | --- | --- |
| `1ST.BIN;1` | `00200a44` | 175 / 596 | `2012` → `0009` |
| `1ST.BIN;1` | `00202eb8` | 179 / 1736 | `2322` → `0009` |
| `G3_PROG.BIN;1` | `06008338` | 310 / 840 | `2012` → `0009` |
| `G3_PROG.BIN;1` | `06099a84` | 601 / 660 | `2322` → `0009` |

These are the four original ASR0 stores targeted by the inherited callbacks.
Adjacent refresh-register writes and the original call target are preserved.
FAD 174 guards that call path. The loader reads `06004000` from `00200460`,
calls it, and stores the same target to `00280008` in its delay slot. The old
callback redirects that target and therefore repairs the delay-slot value
in its handler. With the original target intact, the game already stores
`06004000`; no additional pointer repair or copied helper is needed.

The 1,764-byte pack has four edited sectors and one read-only guard. It
requests callback replacement and adds no cartridge action, matching the
old callback's policy. Both retained comparisons used 1 MB expansion RAM;
an explicit user cartridge setting remains authoritative. This port does not
qualify every cartridge personality.

The preserved complete files were freshly rehashed:

| File | LBA / load address | Bytes | Original SHA-256 |
| --- | --- | --- | --- |
| `1ST.BIN;1` | 24 / `00200000` | 262,141 | `c6d93e695e5e6e52a3f4dd094f5d56ad081211b05a88478ac9db68701035888a` |
| `G3_PROG.BIN;1` | 152 / `06004000` | 942,086 | `e53ac6491b12df26a29622682fad147e0c7df231860f8f61c21a4122f3a50014` |

All five reconstructed target-sector CRCs match the archived pack. Compiling
those exact fragments with the public compiler reproduces its bytes and
SHA-256 `76cf14f986cc22c35fac1c61f4cd0e079a82cdd9d8fc5c53870078114773d441`.
The current production decoder then passes both complete program streams:
128 and 461 sectors, respectively. Only the eight intended logical bytes
change. The final unedited partial sectors use generated zero padding;
their original raw padding is not claimed recovered. The complete source
track was unavailable, so this is not a fresh whole-track hash check.

Two rehashed historical 12,000-frame comparisons use the original-address
and relocated inherited controls. Each matches all 201 sampled video/audio,
sample-count and visible-pixel checkpoints and the complete 35,378,032-byte
PCM stream, SHA-256
`e4cf13c5d96c56182dee5e74908b221eac2fbc4772036191e69d91a8618d44f0`.
The earlier four-NOP comparison diverged under different test conditions;
that difference remains unexplained and preserved. The later controlled
evidence supports this opt-in native candidate without synthetic timing
compensation. It does not erase the earlier result or qualify other paths.

The commands above also check Groove's generated source, complete sector
output, original loader delay slot, linked launch/late callbacks, absent or
rejected-pack fallback, physical-disc branch and explicit user overrides.
The stream fixture includes unrelated sectors so a comparison cannot count
only changed bytes. Runtime firmware inputs and resource sizes are unchanged.
Physical A-bus/cache behavior, audio, other loads and reset remain open.

Sega [ST-097-R5, §3.6, printed pp. 62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=78)
defines ASR0 and its timing fields. [ST-210, precautions 09 and 11, printed
p. 5](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf#page=9) restricts
preread bits and requires a completed A-bus read before changing ASR.
Those requirements do not establish a violation in these game routines
without their preceding accesses. The choice to retain SAROO's existing
NOP workaround comes from the original compatibility implementation, not
an invented Sega mandate or a new diagnosis of the game's failure.

## Habitat II Japan V1.004

[Recipe](../recipes/habitat-ii-jp-v1004-sector.json). This is an opt-in native
source candidate with an **unresolved modeled audio difference**. Adapted
from paps-g `b86f9f7c`, `21ca2e9b` and the later `95bcc4d4` source review.
The original shared `HABITAT2_patch`/`HABITAT2_handle` modem workaround is
attributed to [tpunix's SAROO implementation](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).
The built-in callback remains the ordinary fallback. This port does not
close the audio hold or claim modem/game qualification.

The exact source is `GS-7105   V1.004`, original Track 1 SHA-256
`6b71ebb83c666ade734b89058d433ff433143b9da701b32af0884a84d377df70`.
FAD 214/raw offset 1342 supplies `8b02` → `0009` at `0602b52e` in numeric
program `1;1`. The original function reads modem byte `25885029`, compares
it with `11`, and branches to return zero if different. The NOP preserves
the read, comparison, T and existing return-one path, exactly as the old
handler's instruction write does for this loaded program. No copied handler
or new RAM writer is supplied, and no cartridge action is added.

The shared handler checks the Pad predicate first, then the Habitat predicate.
In the preserved `1;1` source those encoded bytes are `f4` and `11`, so only
the Habitat write applies. FAD 212 guards that first predicate's source;
FAD 173 guards the unmodified initial JSR/NOP and `060105b4=06020000` target.
The later `2;1` reuses the same RAM addresses with bytes `dc` and `26`.
Neither old predicate matches, and its separate source sectors remain
entirely unedited. A matching RAM address alone never authorizes an edit.

Both complete preserved files were freshly rehashed and passed through the
current decoder: 115 sectors / 234,120 logical bytes for `1;1` at LBA 42,
and 165 sectors / 337,056 bytes for `2;1` at LBA 157. Only the intended two
bytes change in the first; the second is byte-identical. Original SHA-256s:
`5a561a55e2681ed57362acbeda0eb0800e4eee27398f52bd51fa5d8e2a1307b3`
and `e716d7a3e865ef7de5dfe618baf911e293c67532a4a8f27fbd7481209e1d956a`.
The first postimage is
`899d765e631ab5f7850b630a948f3037a6d8cbed9fe96b0db442b428dc172a18`.
Reconstructed FADs 212/214 match the archived original raw-sector CRCs.
FAD 173's preserved source window rehashes, but its complete original raw
sector and the whole track were unavailable. Last unedited partial-sector
padding is generated; no recovery or fresh hash of the original track is claimed.

The archived 1,068-byte pack has one edit and two read-only guards. Public
checks compile generated source for all three, verify full sector output and
repeated source rejection/retry, execute the actual launch caller and check
the current linked shared handler's guard priority, writes and T effects.
The handler's 48 bytes match the retained comparison ELF. The isolated
native check runs both original and patched instructions for all 256 modeled
modem bytes and both incoming T values. It preserves the comparison flag
and reads the modeled port in both versions; it does not operate a modem.
The commands above run these checks without game images or device access.

The rehashed historical 18,000-frame comparison matches all 301 video and
visible-pixel checkpoints, but only 156 audio CRCs and 299 sample counts.
Both 53,071,856-byte complete PCM streams remain preserved and have different
hashes. The first CRC difference follows the second handoff. Its cause is
unresolved: no latency-only explanation, compensating delay or physical sound
diagnosis follows from these records. Keep the candidate opt-in; audio,
other modem paths and physical acceptance remain open. Runtime firmware
inputs, image sizes and FPGA are unchanged.

[Hitachi SH7604 Hardware Manual, §2.4.1, Tables 2.16–2.17, printed
pp. 39–40](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=55) specifies
the BF and NOP encodings and unchanged T effects. The game's modem workaround
comes from the original SAROO code, rather than a Sega requirement to fake
modem presence. The exact program extents and later loading are established
by source and retained execution evidence, rather than the general boot format.

## Marvel vs. Street Fighter Japan V1.000

[Recipe](../recipes/marvel-vs-street-fighter-jp-v1000-sector.json). Adapted
from paps-g `63c89aee` and `f77be0af`, with the later `c6063922` native-source
review. The inherited independent conditional writes and launch redirection
come from [tpunix's original implementation](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L536).
The exact identity is `T-1238G   V1.000`, recorded data-track SHA-256
`8274307f9562d2da7fb7be0f5f306846edb74afde5913324bef04938ed3214f9`.
The full track was unavailable for a fresh hash during this port.

| Program | Native target | FAD / raw offset | Original → replacement |
| --- | --- | --- | --- |
| `CPK.BIN;1` | `0600286e` | 1256 / 126 | `2012` → `0009` |
| `SYS.BIN;1` | `0600205a` | 184 / 106 | `2102` → `0009` |

`00LOAD.BIN` loads CPK at `06002000` and calls it, then loads SYS at the
same address and jumps to it. Both native load instructions, JSR/JMP, NOP
delay slots, names and shared `060f008c=06002000` literal stay intact.
FAD 947 guards that loader and FAD 1255 guards CPK's original entry. The
two edits belong to different files; applying only the second loses the
first stage's correction. CPK has `2fc6` at SYS's target, while SYS has
`4610` at CPK's target. The old callback's two tests are independent, not
an if/else pair.

Both edited instructions store `23301ff0` to SCU ASR0. Their replacements
preserve register setup, T and adjacent refresh-register writes. CPK's AREF
store remains in the original RTS delay slot; SYS retains its dummy reads
and AREF store. This is the inherited cartridge-bus workaround. Neither
these stores nor this port establish a CDON operation or a violation in
the original game routines.

The public source verifier rehashes the complete preserved files and runs
every generated sector through the current C decoder:

| File | LBA | Bytes | Edits |
| --- | --- | --- | --- |
| `00LOAD.BIN;1` | 797 | 39,296 | none |
| `SYS.BIN;1` | 34 | 61,536 | one instruction |
| `MM.BIN;1` | 65 | 848,396 | none |
| `CPK.BIN;1` | 1105 | 349,680 | one instruction |

All 637 sectors / 1,298,908 logical bytes pass, including the complete
unaltered loader and MM control. All four target/guard records reproduce
the archived 1,416-byte pack, SHA-256
`15cceecceb4ae7882472eb2eb648e5c88645729c1fb83f8d0379b2238e1f8a98`.
The two complete edited postimages match their preserved reference hashes.
Only four logical bytes change. Padding after unedited partial last sectors
is generated, not claimed recovered from the original raw image.

The new [logical-source command](../tools/README.md#logical-source-verification)
replaces copied source/decoder orchestration for this and the earlier ports.
It pins its recipe, pack, source files, own Rust inputs and supplied C binary;
missing edited extents, overlap, bad pins, unrelated corruption and mismatched
policy/records refuse. Unknown read-only guards stay explicitly unverified.
A failed child records an incomplete receipt, and an existing output is
never overwritten. It is a host authoring tool, with no firmware growth.

The linked checks execute the current launch callback and 60-byte handler,
including both independent predicates, register/T effects and continuation.
The handler bytes match the retained comparison ELF. Actual launch checks
preserve both native handoffs for an admitted pack and retain the old
redirection otherwise. Source words edited by byte are modeled explicitly
in SH-2 byte order. The commands above include all these fixtures.
All 42 overlay methods and 16 launch methods pass with both the sanitizer
and optimized C builds. Rust workspace tests, formatting and lint checks pass.

The rehashed 18,000-frame original-control comparison matches all 301
video/audio/sample-count/visible-pixel checkpoints and the complete
53,071,712-byte PCM stream, SHA-256
`825b1fc590dfac8967aa2cfbe9cf3ee0c3c12fa845f6ecaf3db08394efe933f6`.
Both native edits were observed at their respective loads. This modeled
comparison used 4 MB; the recipe adds no cartridge action and keeps explicit
user configuration authoritative. Earlier copied-helper timing/audio
differences remain preserved. Physical bus/audio, other loads, reset and
full-game behavior remain unqualified; keep the recipe opt-in.

Sega [ST-097-R5, §3.6, printed pp. 62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=78)
defines ASR0. [Hitachi SH7604, §2.4.1, Table 2.12, printed pp. 32–33](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=48)
and [Table 2.17, printed p. 40](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=56)
describe the MOV.L and NOP encodings and T behavior. These references support
the instruction interpretation; the game-specific workaround comes from
the original SAROO contribution.
