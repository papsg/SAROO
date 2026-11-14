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
The [SCU manual ST-097-R5, §3.2, Figure 3.24, printed p. 62](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf)
documents A-bus access timing. SAROO's 1 MB personality action and this
game's workaround come from the original implementation, rather than a Sega
requirement to apply these particular edits.
