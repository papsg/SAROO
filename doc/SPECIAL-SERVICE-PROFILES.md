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
