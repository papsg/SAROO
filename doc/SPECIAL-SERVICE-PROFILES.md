# Special-service source profiles

These optional profiles extend the [overlay system](SD-SECTOR-OVERLAYS.md).
Normal callbacks, explicit configuration, original media and saves remain
available. No proprietary game/ROM, USB stack or runtime diagnostic is supplied.
Host/source/link checks are separate from physical or whole-game qualification.

## Command & Conquer USA GDI and NOD

Recipes: [GDI](../recipes/command-conquer-us-gdi-sector.json) and
[NOD](../recipes/command-conquer-us-nod-sector.json). Adapted from paps-g
`a5ac9179`, preserving the already-public `6bcc069b` reset correction
(SATURN-MENU-CNC-001 / GAME-REG-004). This changes delivery of the existing
[guarded fix](CD-EVENTS-AND-GAME-LAUNCH.md#avoid-the-prohibited-dma-force-stop-write-in-command--conquer).
It does not establish a general DMA defect or resolve the separate random-exit
report. The original callback and its complete-file guard are retained.

The game writes `2212` at `06076344` to the removed SCU force-stop register
`25fe0060` during soft reset. Change only that word to NOP `0009`, retaining
subsequent SCU enables, SH-2 DMAOR clear and return trap.
[Sega SCU Final Specifications ST-210 precaution 14, printed p.5](https://antime.kapsi.fi/sega/files/ST-210-110194.pdf#page=9)
prohibits the store. [Hitachi SH7604 Table 2.12, pp.33–34](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)
and [Table 2.17, p.40](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=56)
define the original store and NOP.

Both discs identify as `T-7028H   V1.000`. File ID2, `0_CNCSAT.BIN;1`, starts
at LBA27/FAD177 and loads 950,320 bytes at `06003800`. Complete original SHA-256:
`ab4e7cc5f3b1b122e747fb5f5c6d10d279c28716887ae5f4285c5069303e7a23`.
Original CRC is `d606ee5b`; selected postimage CRC is `a851cea7`, SHA-256
`f709be784d7acd1df1729848134afd9d32b9813dd7a123b35c2f6606783e5b6f`.

| Disc | Recorded original data-track SHA-256 |
| --- | --- |
| GDI | `87eed9c65a827ae930b386cbd0ffda3de053a9c9ca9ac75688a265fe9ba77178` |
| NOD | `45b260d02a01f6b76ec94f3299a3939b9e99569d9e3b3ef54e9b246697af4805` |

The two sources produce the identical 720-byte `C5195D9E.SOV`, SHA-256
`a082202b738235207a7bbfc87f2105e508eafe58af1313ceeabea94e416ad604`.
There are two provenance recipes and one pack, not competing alternatives at
the same key. One record edits FAD406/raw852; the other authenticates FAD641
and ends source editing. A damaged completion sector retains the edit for
retry. A valid completion leaves later reads original. It does not prove RAM
loading or AIP execution has completed.

## Complete loaded-file qualification

A sector match alone is weaker than the old callback's complete loaded-file
CRC and instruction check. Optional `first_read_guard` therefore requests a
distinct ABI4 pack and checked initial launch. The compiler derives load,
length, logical edit offset, complete postimage CRC and completion sector from
the original IP/ISO. It requires one two-byte edit, complete source SHA,
canonical raw sectors and a later completion boundary. Alternatives, extra
guards and unknown metadata refuse.

[Sega Disc Format ST-040 §4.3, pp.28–29 and §4.7](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf)
defines declared IP and first-load fields. ISO mapping includes extended
attributes and interleaved units, with endian/volume/extent/directory checks.
Malformed, cross-volume and multi-extent inputs refuse.
[ECMA-119 second edition §§6.4.3.4–6.4.3.5, pp.5–6](https://ecma-international.org/wp-content/uploads/ECMA-119_2nd_edition_december_1987.pdf)
defines the attribute/unit/gap interpretation. Metadata and
file sectors used by inspection must match bytes captured during complete FILE
hashing; later reads cannot silently change the inspected layout.

ABI4 retains the 24-byte header and 348-byte records. Bytes24–47 of its first
record hold six big-endian descriptor words:

| Descriptor offset | Meaning |
| --- | --- |
| 0 /4 /8 | Load / complete length / postimage CRC32 |
| 12 | Logical instruction offset |
| 16 /18 | Original / selected instruction, two bytes each |
| 20 | CRC32 of preceding 20 bytes |

The second record has no edit and marker byte9=1. Both processors validate
checksum, alignment, contained word and Work-RAM arithmetic before derived
accesses. The decoder also binds source/selected words, raw/logical offset and
minimum completion distance. Interleaved gaps may increase that distance; the
compiler derives the exact boundary. No title address/CRC enters generic code.

The MCU publishes the descriptor at SYSINFO+40..57 before action `534f5780`
(optionally OR1/4 for existing RAM actions). Before callback/configuration,
the menu checks the IP load field and complete postimage through P2:

- Complete CRC and target match: normalize to the existing replacement action;
  explicit user edits follow and retain priority.
- Complete CRC fails with selected word present: restore only the original
  word through P1, then use normal guarded callback/configuration handling.
- Unexpected target: leave it alone and retain callback fallback.
- Explicit skip: restore the known source word and skip compatibility/configuration
  work. The selector also avoids overlay requests when skip is known.
- Corrupt metadata, invalid guarded action or header mismatch: refuse before
  derived game-memory access and stop before game entry. Unrecognized ordinary
  actions retain their existing fallback behavior.

[Hitachi SH7604 §8.4.2, p.219](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=235)
defines the P1 write-through instruction update; [§8.4.3, pp.220–221](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=236)
defines P2 observation. Physical cache/concurrent-bus timing stays unqualified.

## Capability and lifetime

Overlay capability3 advertises `FIRSTDISC=000f`. It preserves the full 16-bit
disc argument; `ffff` remains unload. Existing commands/acknowledgments keep
their assignments. A newer menu uses this command only with a recognized
capable MCU, otherwise retaining legacy load. Older menus do not request
overlays from an unrecognized newer capability and keep normal callbacks.
The baseline decoder rejects ABI4. There is no global menu-protocol replacement
or private USB CRC dependency.

Only checked initial launch permits ABI4 admission. Ordinary, unrequested,
player and automatic disc-change paths cannot arm it. Both sectors must pass
preflight and save selection must succeed before publication. Unload/tray
retirement clears action, descriptor and engine. ABI1–3 packs retain their
format and normal delivery. Physical-disc callbacks remain preserved.

## Checks and remaining limits

Public compiler/decoder, real FatFs mount and actual menu launch checks cover
synthetic ISO/postimages, descriptor/preimage/bounds failures, mapped attributes/
interleaves, marker retry, later original reads, save/preflight faults, stale
cleanup, capability fallback, all 65,536 opcode words, full-width indices, skip
and configuration order. Separate guard checks include 392 independent bound
combinations and invalid actions before RAM access. BIOS copies and bus access
are modeled; these checks do not execute a physical game.

Both preserved original files and complete postimages rehash. Current Rust/C
checks reproduce the archived packs over 468 sectors per source, including the
original 6,144-byte IP. Final 2,000-byte zero padding is reconstructed: its
canonical sector matches the archived marker CRC, but it is not freshly
SHA-authenticated original raw padding. Complete raw discs are unavailable now;
no fresh whole-track compilation/authentication is claimed. The full-CUE
compiler path is separately exercised on authored ISO layouts.

The original 1,298-pin receipt recheck finds 983 matches, 276 missing and 39
later-changed shared inputs. Available saved sources/outputs remain preserved;
missing/changed pins are recorded instead of rewritten or counted as freshly
verified historical runtime. Historical 16-case linked SH-2 comparisons cover
both discs and original/overlay/missing/wrong/configured/bypass routes with
complete Work RAM, VDP1, SP/PR and saved state. Slave was parked and cache
disabled; BIOS/AIP/full-game/concurrent/physical startup were excluded.

Fresh target builds use 59,068 MCU load bytes with LTO /63,252 without,
leaving 6,468 /2,284; static DTCM remains 50,896. Code adds 440 /432 bytes.
Menu body is 86,312 (+448), with 18,888 margin, unchanged 9,488 BSS and 16 KiB
reserved stack. Its guard has a 48-byte compiler-direct frame, not a measured
full call/interrupt stack. Both existing linked callback suites pass against
the new menu. FPGA source is unchanged. No installation, game qualification,
callback retirement or issue closure occurs. Keep profiles opt-in.

```sh
make -C tests check-sector-overlay
SAROO_OVERLAY_PROFILE=release make -C tests check-sector-overlay
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
python3 -B -m unittest discover -s tests -p test_menu_command_completion.py -v
python3 -B -m unittest discover -s tests -p test_menu_launch_errors.py -v
```

## In the Hunt USA V1.000

[Recipe](../recipes/in-the-hunt-us-v1000-sector.json) and
[reviewed assembly](../recipes/assembly/in-the-hunt-us-preload.S). Adapted from
paps-g `e2029f04`, preserving the
[original upstream music service, 8928b7da](https://github.com/tpunix/SAROO/commit/8928b7da7311edb65b7b2d0e9fd1989cc25c4202)
and the already-public complete-read correction. This is an optional delivery
of that workaround, not proof of a native-game defect or an audio-equivalent
replacement. Original menu callbacks remain available.

`T-10001G  V1.000` loads `0;1`, LBA23, 605,536 bytes at `06004000`.
Complete original SHA-256 is
`8be9ff04a0f116742ef5cdcbf22a836083d91ac2959a564ab90d55e9de9cecb8`;
postimage is `5d3e8dfe70bae47cbaa87e798ee9a57a4b23fc5abe13799e829a1a94ad763aac`.
The recipe separately pins the complete original data track. Original first
caller is JSR at `060072EA`, using the pointer literal at `06007324`.
That literal remains unchanged. The second pointer at `060044E0` selects the
copy entry at `0600A3F4`.

The 108-byte body begins at `0600A3B8`; it includes a P2 flag at `2600A420`.
Before every preload it clears the flag, reads the existing `02000F04` function
pointer, and requests FAD2612 /`54F00` bytes into `22400000`. Only an exact
complete count enables copying the same `54F00` bytes to `002AA000`. Short,
failed and oversized returns leave copying disabled, including after a previous
success. Repeated successful copies remain permitted. PR, SP and saved
registers are preserved. This is a functional preload/copy routine; it is not
a resident compatibility callback that writes other game instructions.

The source uses literal loads before JSR and a NOP delay slot. The rejected
104-byte draft placed a PC-relative load in that slot.
[Hitachi SH7604 §2.4, Table 2.12 pp.33–34 and Table 2.17 p.40](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)
defines these loads, delayed transfers and register operations;
[Renesas SH software manual §6.1.34, pp.202–203](https://www.renesas.com/en/document/mas/sh-1sh-2sh-dsp-software-manual)
specifies the PC-relative delay-slot address rule. The 108-byte version retains
its resolved literal addresses. The file mapping uses
[Sega ST-040 §4.3, pp.28–29 and §4.7](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf).

The ABI3 pack has four records over two sectors, 1,416 bytes, SHA-256
`8f50e5f6bd567acdb5b05ccb32a39fecc3a3f497391f88a4a322a30d9ef2f4b4`.
It uses the existing long-span decoder and exported reader; no firmware runtime
or resource change is required. Current Rust/C checks reproduce the complete
preserved IP/first-file postimages. Synthetic source/decoder checks cover
312 streamed sectors; 672 unedited final padding bytes are generated, not
newly authenticated original raw padding. The full raw disc is unavailable,
so its complete track hash remains historical. Additional checks cover
atomic fragments, source/version/hash refusal and retries. Instruction checks
execute the actual recipe bytes across absent, complete, short, failed,
oversized and repeated reads/copies, comparing the complete modeled memory and
saved registers. Reader transport and bus aliases are explicit models. Actual
menu launch checks preserve admitted source pointers, retained missing/rejected/
physical callbacks, explicit skip and later user configuration priority.

Historical 38-operation ABI checks and four natural US/JP startup runs remain
separate evidence. Each natural route completes two real-reader preloads and
retains the body/flag/callers at four snapshots. US video matches all121
checkpoints; audio matches17/121 and complete PCM differs. That natural route
never reaches the copy entry; bounded instruction checks cover it. Do not infer
full-game, audio, concurrent-cache or Saturn qualification. The intermittent
SMPC startup freeze remains unresolved. The original shared receipt recheck
records956 matches,2 unavailable pins and3 later-changed shared inputs; the
final frozen inventory records6,107 matches,111 missing and zero changed pins.
Missing evidence is preserved as a limit, never rewritten into a passing result.

```sh
python3 -B -m unittest discover -s tests -p test_sector_overlay_special.py -v
python3 -B -m unittest discover -s tests -p test_menu_vdp1_order.py -v
```

## Fenrir loader 2020-07-22

[Recipe](../recipes/fenrir-loader-20200722-sector.json), adapted from paps-g
`e2029f04` and the
[original upstream Fenrir adapter](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).
The [official ISO](https://www.fenrir-ode.fr/release/dev/20200722/Fenrir.iso)
is 1,548,288 bytes, SHA-256
`0b30d1475c080c31cf2e4396ae37c33291daf50df6ce17ae3e6620c5cba6c66f`.
Its pinned Mode1/2352 conversion is
`5418143f104428ca8231194c906ef8a01f0f64501f5013c6726b7c4dd0ad6532`.
Every original 2,048-byte payload remains unchanged. This recipe applies to
that exact geometry and source, not all ISO layouts sharing its product ID.

`CD0000001 V1.000` loads `FENRIR.BIN;1`, LBA24, 141,376 bytes at `06004000`.
Change only the `0600DD18` literal, `0600029C→02000F30`. The original
`MOV.L @R1,R0; JSR @R0; NOP` uses the existing published launch-service slot.
The retained adapter instead omits the dereference and changes the call
register. After return, native code replaces R1/R0 with the next BIOS pointer.
Bounded execution checks match both complete modeled states there, including
saved registers, PR, call arguments and untouched RAM. External launch and
cartridge accesses remain models. The relevant instruction rules are
[Hitachi SH7604 §2.4, Table2.12 pp.33–34 and §4.5](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49);
file/IP mapping follows [Sega ST-040 §4.3, pp.28–29 and §4.7](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf).

The one-record372-byte `20F1AA4B.SOV` has SHA-256
`83df58154d878fb693d218b1a5d2f115fc0d51c62fcae3b5d850f37d57051238`.
Current full-CUE compilation reproduces the archived pack; original ISO/raw
payload equality and complete logical original/postimages revalidate.
The logical stream covers86 sectors, with1,984 unedited final padding bytes
generated independently; complete raw-track authentication is provided by
the separate compiler run. Native
C/synthetic-sector checks retain malformed-sector refusal/retry. Actual menu
dispatch preserves the original adapter on missing/rejected/physical routes,
explicit skip and later user configuration. No firmware runtime/resource change
is needed. Tests use the same commands as the music profile above.

Physical Fenrir-owned reads do not pass through this sector overlay. Their
route/lifecycle design, full loader execution and actual device acceptance
remain open. The original Fenrir callback stays available; no CD-block
coexistence, physical Fenrir support or issue closure is claimed. The separately
missing exact RMENU2 revision is not substituted by this loader.

## Kaitei Daisensou Japan V1.004

[Recipe](../recipes/kaitei-daisensou-jp-v1004-sector.json), adapted from paps-g
`3fafb587` and the same
[original upstream music workaround](https://github.com/tpunix/SAROO/commit/8928b7da7311edb65b7b2d0e9fd1989cc25c4202).
It uses the [shared reviewed assembly](../recipes/assembly/in-the-hunt-us-preload.S)
at Japanese addresses. The music/read ABI, instruction rules and exact primary
citations are the same as [the US profile](#in-the-hunt-usa-v1000), with no new
runtime helper, buffer or firmware resource growth. Original callbacks remain.

`T-15006G  V1.004` loads `0;1`, LBA23, 606,172 bytes at `06004000`.
Complete original SHA-256:
`44055be8a187b5344a1e3d038a95b272ea860dffee98799c37485d2161689f56`;
postimage `f3582a7762f2868b0fe8a0352c617ad55a9961483f5e6dfc1f884d2419fe367f`.
The pinned original data track is
`0dc03b26e011a2da87cba696b93103d7c5fe32a4c42caad6a5f4a557f212d575`.
Full current-CUE compilation reproduces the archived four-record, 1,416-byte
pack, SHA-256
`8580e92e2245c573d9fdec6a42a72b496f143a7419ee2fd51ed2eec9c7193411`.

The body is at `0600A2D4`, copy entry `0600A310`, P2 flag `2600A33C`.
The original first pointer at `06007324` stays; the second at `060044E0`
changes from `0600A374` to the copy entry. Both flag literals are independently
checked. The preload still requests FAD2612 /`54F00` bytes into `22400000`;
only an exact complete return permits the same copy to `002AA000`.
The existing reader export is unchanged.

Current Rust/C checks reproduce complete IP/first-file postimages over312
sectors;36 generated unedited padding bytes remain explicit. Synthetic source
checks refuse US layouts even with a matching header, and retain atomic
fragments/refusal/retry controls. The actual recipe instructions preserve
complete modeled RAM/stack and saved registers across absent/full/short/failed/
oversized/repeated operations. Actual menu checks use each region's real body/
copy addresses, preserve fallback/physical routes, skip and user priority.

Historical regional38-operation checks and natural startup remain separate:
all121 video checkpoints match,38 audio checkpoints match and complete PCM
differs. The natural route does not call the copy entry. The intermittent
SMPC startup freeze, audio/timing differences, later routes and physical
qualification remain open. Current source/host verification does not close
them. Run the same public tests as the US profile; profiles stay opt-in.

## Checked generic ROM startup

Adapted from paps-g `3fafb587`, with a public pairing/ownership adaptation.
Original ROM callbacks, the ROM-free KOF95 path and existing transport/copy/
clock/reset fixes remain. The private build's blanket callback retirement and
global protocol replacement are excluded.

ABI5 retains ABI3 record encoding. A distinct capability4, `ROMDISC=0010`
and action `534f5880` admit this service. The full16-bit argument is preserved,
including the existing `ffff` unload value. Tagged command normalization covers
1–16; file/archive/BIN assignments remain separate. Ordinary, older checked,
player and automatic disc-change launches cannot arm ABI5. Preflight/source,
save-selection or transport failure cannot publish it. Unload clears ownership.

| Pair | Overlay behavior |
| --- | --- |
| New menu with capability1/2 | Existing legacy load and ABI1–3 behavior |
| New menu with capability3 | Existing `FIRSTDISC`, including ABI4; no ROM admission |
| Capability4 menu/MCU | `ROMDISC` admits ABI1–5 with explicit ROM action |
| Earlier menu with capability4 MCU | Unrecognized capability keeps ordinary load and original callbacks |
| Ordinary or `FIRSTDISC` command sent to new MCU | Cannot admit ABI5 |

The previous real ABI4 decoder rejects current ABI5 packs. No new title ID,
resume/movie address or ROM-specific gameplay command enters MCU code.

SOR1 occupies the last64 bytes at `06009FC0` of SAROO's existing32KiB initial
transfer. The compiler verifies original raw geometry, declared IP length/
alignment, zero source padding and complete FILE identity. Inspected header/
tail bytes must match the same hashed source read. It refuses mixed complete-
file/ROM guards, overlapping writes, unsupported actions and noncanonical paths.
[Sega Disc Format ST-040 §4.3, pp.28–29 and §4.7](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf)
defines declared IP/AIP and first-file fields; the fixed32KiB transfer is
SAROO's existing implementation property.

| Descriptor offsets | Big-endian content |
| --- | --- |
| 0–3 /4 | `SOR1` /phase1 game entry or2 first-read handoff |
| 8–11 | Even resume in `06002000..060037FE` |
| 12–15 /16–19 | Exact2MiB length /complete asset CRC32 |
| 24–55 | Bounded `/SAROO/ISO/[A-Z0-9_-]+.BIN`, NUL and zero padding |
| 60–63 | CRC32 of preceding60 bytes |

Bytes5–7,20–23 and56–59 are zero. Runtime validation occurs before I/O; malformed
metadata, short/failed/oversized reads or wrong complete CRC refuse launch.
Each attempt clears generic and old ROM ownership. Missing/ordinary/physical
profiles retain original admission. An accepted profile performs one asset
read; it does not repeat the old title lookup/read. Skip suppresses overlay
requests before source admission. An inconsistent late ROM state refuses game
entry instead of executing an unstaged handoff.

The public workspace remains `TMPBUFF_ADDR+100 = 220C7100`, not the private
build's different staging address. It fits before artwork and uses the existing
file transport. The source service captures the resume address, calls the
original installer and transfers to the original forward copy in low Work RAM.
It adds no2MiB buffer. Phase1 retains normal initialization/configuration before
the later native call; phase2 transfers before VDP1 clear/configuration, matching
the existing direct-ROM behavior. No artificial unwinding or delay is added.
Existing clock/reset behavior and forward-overlap direction remain unchanged;
[Hitachi SH7604 Table2.12, pp.33–34, §4.5 and §8.4](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)
define the instruction, delay and alias semantics used here.

Current source checks cover complete synthetic assets, full4MiB cart and2MiB
Work-RAM/stack state, invalid fields/paths, every descriptor byte's checksum,
header bounds, stale ownership, skip/physical scope and reader failures.
The linked-menu check executes actual launch/installer instructions and the
relocated36-byte native forward copy in both phases. It uses the bounded
instruction model with explicit `memcpy` and memory/cart boundaries. It checks complete cart/Work-RAM and exact stack
writes, saved registers, PR, copy endpoints/last word and publication after all
three fragments. It stops at the requested resume; it does not execute the
original game continuation or natural BIOS startup. Separate existing checks
execute the linked clock fragments and validate ROM transport/installation.

The complete-memory oracle exposed a fixture alias error: SYSINFO was modeled
as separate storage even though the ROM copy overwrites that cart location.
The corrected fixture shares its cart bytes. Another rejected fixture changed
an even resume to a different valid even address while expecting rejection;
the corrected negative changes alignment. Those failures remain preserved;
they are not firmware defects or physical reproductions.

Fresh builds: MCU59,220 LTO /63,420 non-LTO load bytes, leaving6,316 /2,116;
static DTCM span stays50,896. Menu body87,336 (+1,024 from C&C), BSS9,496 (+8),
margin17,856 and reserved stack16KiB. Compiler-direct service/launch/after-read
frames are24/8/4 bytes, not full interrupt-stack qualification. The native copy
stays36 bytes within its existing96-byte installed reservation; clock/reset
fragments stay96/36. FPGA source remains unchanged.

```sh
make -C tests check-sector-overlay check-menu-safety check-file-transfer
SAROO_OVERLAY_MENU_ELF="$PWD/tests/build/rom-menu/source/ssfirm.elf" \
  python3 -B -m unittest discover -s tests -p test_menu_overlay_rom.py -v
```

## KOF95 native startup

[Europe](../recipes/kof95-eu-v1001-rom-sector.json) and Japan
[1M](../recipes/kof95-jp-1M-rom-sector.json) /[2M](../recipes/kof95-jp-2M-rom-sector.json)
/[3M](../recipes/kof95-jp-3M-rom-sector.json). paps-g `3fafb587` supplied the
generic ROM migration; `8704963f` and `d42a84a6` replace its overwritten IP
helper with native source edits. Original compatibility behavior comes from
[upstream `kof95_patch` /`kof95_handle`](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c#L269).
The three omitted MM instructions retain that workaround; no general native
game/spec violation is asserted.

The former32-byte helper at `06003780` did not survive the native MM load at
`06002040`. Current original files reproduce that overwrite. The replacement
shares an existing BIOS pointer literal, relocates the unchanged IMS-address
literal, preserves native file loading, then dereferences the existing
`02000F18` service slot. No replacement IP helper is emitted.
MM changes `2802→0009` at `060020C2` and `400B4F22→00090009` at
`0600217E/80`. The BIOS return delay slot remains NOP. The European original
`BSR` at `0607CDAA` and its downstream DCHG rejection remain unchanged.
[Sega SCU ST-097 §3.5/Figure3.21, p.57 and §3.6/Figure3.24, pp.62–67](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=73)
identify IMS/ASR0; [CD-block ST-162 §3.1, HIRQREQ, p.28](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf)
defines DCHG bit5. [Hitachi SH7604 §2.4, pp.33–40 and §4.5](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=49)
defines the shared literal/branch/NOP behavior.

| Source | First file `0;1` | `MM.BIN;1` | Pack |
| --- | --- | --- | --- |
| Europe `MK-81088  V1.001` | LBA23,40,360B at `0607CD80` | LBA43,360,172B at `06002040` | `2B0CAF87.SOV` |
| Japan `T-3101G   V1.000`,1M/2M/3M | LBA25,40,784B, same load | LBA45,380,044B, same load | `675EF097.SOV` |

Complete first-file SHA-256: Europe
`d301002c65ff34a02b0dc7a6d550add892ede9d71078c12c160d63ea0f4ef2ad`;
Japan `25cf3d668ca28b766ce984049225c2558bf697a64c93d3cf60d29866b51c007a`.
MM source SHA-256: Europe
`a195bf8e5aa4c1f6f4b28eb66e6c2104ef303fa038f24cf17c3f9477a1a997e5`;
Japan `ab938524fd6f3d2012db8e99096e3da66cd51a8dbb59a57c8859a48168d3076f`.
Both packs are2,112 bytes/six records and change21 native source bytes plus
the descriptor. Pack SHA-256: Europe
`4e58215e9c404a538c31eaa5744a0ca61a9acdab9b0f79e03bf35f8ed8e0aa4b`;
Japan `f6f9db8946629b8520cc739f1e45ebc02543e5dc482c99bc12cbfba2f3b7330d`.

The2MiB companion `KOF95.BIN` is not supplied. Its preserved original rehashes
to `1dd154a9fcee1816749502b5b2497bf4b1f2d973df38d0984905ee83b12d5e81`,
CRC32 `90412e10`. Current Rust/C checks reproduce complete original IP/first/MM
postimages and archived packs. All three Japanese whole data tracks recompile
identically; these aliases are not independent gameplay qualification.
European complete raw media are unavailable; their track hash stays historical.
Current source/decoder proofs are separate from that missing whole-track check.

Public tests exercise atomic descriptor/native fragments, earlier-decoder
refusal, correct/malformed sources and independent IMS/first-call/tail register
semantics. The European DCHG instruction and complete unchanged surrounding
file remain source controls. Historical native-startup/ROM-service/MM-stack
joins remain separate, with explicit BIOS/file/MMIO/cache models. Current receipt
audits record JP19 matched/7 changed; Europe20 matched/2 missing/5 changed;
joined320 matched/6 changed. Later-mutated shared code/recipe pins are not
counted as freshly verified historical runtime. Full natural startup, installed
clock/reset execution under gameplay, later routes/input/audio/return and
physical timing remain open. No issue closure or Saturn qualification is made.

## Ultraman Japan V1.001

[Recipe](../recipes/ultraman-jp-v1001-rom-sector.json), adapted from paps-g
`3fafb587` and the
[original upstream `ULTRAMAN_patch`](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).
The [generic service and its exact primary citations](#checked-generic-rom-startup)
preserve the existing2MiB copy, clock/reset installation and immediate takeover.
This group adds no firmware runtime; it uses the already-verified phase2 path.
The original callback remains available.

`T-13308G  V1.001` loads `1ST_READ.PRG;1`, LBA28,66,952 bytes at `06004000`.
Its complete original and unchanged postimage SHA-256 is
`cde4f3c7c2a16a8966352ebd3422adf46f8c8484044a167f2ba4c16046bcdb82`.
The complete original6,144-byte declared IP rehashes to
`e29e12f2dd9bedb8ad04370c6baf8087c9927769d6f9e2dfba9564ad3a0915d6`,
matching all independently retained native-IP windows. The preserved32KiB
initial transfer is `9215624ae15e5604b68ade76f367dd82b3ac0900eff00394d0e5c8a6fb7bcd80`.

The recipe has no game-code writes. It authenticates the stock IP predicate,
then inserts only SOR1 in the reserved transfer tail. Phase2 resumes at
`06002222` before ordinary VDP1 initialization/configuration. The admitted
initial-transfer postimage is
`5036419016b46cdd6197f453454c423e38ecbdf6c4609fee1c157f1afa9a8ad2`.
The pack `183B8EAF.SOV` has three records/1,068 bytes, SHA-256
`e3b4223eef6890c8c83a8d7a5815400c784c4b3e66e2a37b291a3b4e2c5a4949`.

Current Rust/C verification reproduces that final archived pack and both
complete logical postimages over49 sectors. The632 unedited first-file padding
bytes are generated; the original full data track remains unavailable and
its recipe hash stays historical. The earlier different predecessor pack was
not substituted for the retained final ABI5 artifact. The independently supplied
`ULTRAMAN.BIN` is exactly2MiB, SHA-256
`6e3b35af6e45d9005460cfe44e3907563bde7a4636926ebd316dc84913efdd1b`,
CRC32 `b9644b52`; its preserved original rehashes. No companion asset is supplied.

Public source tests compare the complete initial stream, showing unchanged
original payload outside the64-byte descriptor, and exercise the production
decoder and prior-decoder refusal. The existing actual caller test transfers
before VDP1/configuration, with retained ordinary/physical/missing callbacks and
explicit skip. The current linked phase2 service/copy check compares complete
cart/Work-RAM/stack state and the exact retained frame at resume. It stops there.

Historical four native IP/menu/service/AIP joins remain separate. They reached
the first program's native SP/SR initialization with explicit BIOS/file/MMIO/
cache models. The current inventory recheck records317 matching and4 later-
changed shared pins; no historical frame contract is claimed for a different
menu build. Natural BIOS entry, actual bus/interrupt timing, later gameplay,
audio/input/return and physical acceptance remain open. No issue is closed.
Use the public ROM/overlay/menu tests above; profiles remain opt-in.
