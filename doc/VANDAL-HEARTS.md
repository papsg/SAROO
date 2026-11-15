# Vandal Hearts: native-source overlay

This opt-in recipe converts the inherited Chinese Vandal Hearts workaround
for **DLCT v20240517 only**, whose IP identifies as `T-1219G   V1.002`.
It replaces the existing startup routine and one compressed-source literal.
It adds no copied compatibility stub, unused-IP helper, resident instruction
writer or CDON recipe. The original callbacks remain available when the pack
is absent, incompatible or rejected.

Offline source, instruction, delivery and bounded opening checks pass.
Complete modeled PCM differs from the inherited workaround. Audible
significance, actual BIOS-return/relaunch behavior and physical/full-game
qualification remain open; installing this recipe is not a compatibility
certification. No Saturn was operated for this port.

## Origin and exact scope

The original behavior comes from the
[tpunix addition](https://github.com/tpunix/SAROO/commit/e6854bf651f55b1f413ea8fa700a10bff7562cae)
and [yzb update](https://github.com/tpunix/SAROO/commit/f6a35a4d1d5a32af507f7d6d02edc8b7ab169fbc).
Credit DLCT for the Chinese game source. The native rewrite, finite-delivery
adaptation and independent checks are papsg's work, rather than additional
upstream code. No original issue is claimed resolved by this port.

The retained callback recognizes either `4c2b` at `06002f5e`, or `4c2b`
at `06002f70` when the first predicate does not match. It installs the
corresponding branch to `06002d00` and copies twelve longwords there.
The public callback already declares all 48 bytes, including its historical
RTS/NOP tail. The supported DLCT source has `4601` at the first address
and `4c2b` at the second, so it matches only the second predicate.
[Original callback and both product routes](https://github.com/tpunix/SAROO/blob/c31bf6192eff59533d7268dfdf6c791f11a1f7c9/Firm_Saturn/game_patch.c).

The earlier helper recipe copied that compatibility stub into IP slack.
That design is retained as an execution control only. The selected
[assembly](../recipes/assembly/vandal-hearts-cn-dlct-v20240517.S) instead fits
inside the existing 122-byte AIP. Unknown first-predicate/B1 sources,
modified `T-9526G` images and the unverified Bio Hazard identification are
unsupported. Their preimages have not been invented, and both original
product-table callbacks are unchanged.

| Source identity | SHA-256 |
| --- | --- |
| Complete raw source FILE, 689,263,008 bytes | `0e9361f5b1d130af6b513a8a80ad9f4d872b14669655159e77ff7ee339a21178` |
| Complete 6,144-byte IP | `15c5342a7fdfdbded2d3bb6f7ec3eb688fc5a401e47310d3a25aa71bc5288697` |
| Original 194,560-byte native read | `c9d624acb790fc257f1a6442dc3d0291fbc7e8bd442583b300d95fa2036f8992` |
| Original 4,608-byte packed tail | `e328e1f325f073d8a5843c15bf869bbe31b76de38d994aed9b6532b5d542c0d5` |

The compiler authenticates the complete touched FILE and original edit bytes.
At mount, complete raw-sector CRCs bind all three IP sectors and all three
packed-tail sectors. In particular, FAD150 binds the actual revision/header
and VDP callees; FAD152 binds the native CD callees. The product ID alone
does not select a compatible revision. Owners supply their own untouched
media and BIOS; neither is distributed here.

## Native operations and boundaries

The existing AIP spans `06002f00–06002f79` in FAD151, raw offset1808.
The next native function starts at `06002f7a` and is unchanged. Linking the
assembly produces 124 text bytes; the final two alignment bytes (`0009`)
are excluded from the recipe. No other section allocates storage.

The rewrite preserves the native status wait, filter setup and 95-sector
read beginning at FAD292214, the stock BIOS unpacker, the data adjustment
at `060401b0`, the original VDP initialization/publication routines and
continuation at `06040000`. It calls the original BIOS clock service through
`06000320` with R4=1, retaining the inherited clock/display work and TVMD
value `8001`. It uses the existing caller stack and no persistent helper.

The native read's packed tail starts at logical offset `2e600h`.
Its literal at tail offset `d6h` (FAD292306/raw1766) changes `2202` to `0009`.
Executing the stock BIOS decoder produces 5,440 bytes; only decoded offsets
`c4h–c5h` change. Thus `060400c4` receives NOP directly from its source,
instead of executing a late instruction writer. This is an ASR0 store,
not CDON. The complete 6,144-byte IP and 194,560-byte read postimages are
checked, including every byte outside the two intended edit ranges.

Sega's [Disc Format Standards ST-040-R4](https://antime.kapsi.fi/sega/files/ST-040-R4-051795.pdf#page=29)
§4.2, printed pp. 25–27, defines the product/version metadata. §§4.4, 4.6–4.7,
printed p. 31/PDF p. 35, describe security, AIP and first-read handling;
§4.8, printed p. 32/PDF p. 36, distinguishes the backup IP. These explain
the source/boot boundaries, not this game's proprietary loader geometry.
That geometry is established by its original instructions and complete
source checks. [SCU User's Manual ST-097-R5](https://antime.kapsi.fi/sega/files/ST-097-R5-072694.pdf#page=78),
Figure 3.24, printed p. 62, specifies ASR0 at `25fe00b0` and requires A-bus
timing appropriate to attached devices. The NOP preserves the inherited
SAROO workaround; it is not a new universal register-policy claim.

## Finite delivery, ownership and fallback

The final original sector of the 95-sector native read is FAD292308.
It is the completion witness. **FAD152 cannot be an expiry witness:** it
arrives before the packed edit and would suppress that edit.

This recipe requires the existing ABI6 finite-source support. All edited
fragments must be delivered successfully before a valid completion sector
can retire the edits. Preflight is not delivery; early completion, failed
preimages, corrupt sectors and refused delivery leave required fragments
pending for a valid retry. The three fragments of the AIP are applied
atomically to their accepted sector. Read-only guards do not count as
edited fragments. CRC/EDC/ECC checks still cover complete raw sectors.

Retirement disables future edits but retains action `80h` for the current
checked launch. Otherwise the menu could execute its fallback after the
native AIP was already delivered. Explicit unload/tray retirement clears
that ownership. A new checked launch unloads/remounts even the same game
and rearms the finite state. Automatic disc changes cannot newly arm ABI6.
See the [finite-source contract and compatibility tests](SD-SECTOR-OVERLAYS.md#finite-native-source-edits).

Missing/rejected packs preserve the original callback and physical-disc
branch. The existing skip policy and explicit user settings retain their
priority. Save or transport failure publishes no action. The recipe adds
no expansion-RAM or ROM-cart selection. Old packs keep their old behavior;
old firmware refuses ABI6, and existing MCU/menu capability negotiation
preserves the fallback with an older menu. These are tested SAROO contracts,
not requirements attributed to Sega.

## Reproduce the public checks

From the repository root, with the documented local dependencies installed:

```sh
cargo build --offline --locked --manifest-path tools/Cargo.toml -p sector-overlay -p firmware-build
python3 -B -m unittest discover -s tests -p 'test_sector_overlay*.py' -v
cc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -IFirm_MCU/Saturn tests/sector_overlay.c Firm_MCU/Saturn/sector_overlay.c \
  -o tests/build/vandal-decoder
tools/target/debug/firmware-build vandal-source --repo "$PWD" \
  --out tests/build/vandal-source --recipe recipes/vandal-hearts-cn-dlct-v20240517-sector.json
tools/target/debug/sector-overlay vandal-inputs \
  recipes/vandal-hearts-cn-dlct-v20240517-sector.json \
  /path/to/untouched/DISC.cue /path/to/owned/BIOS.bin \
  tests/build/vandal-decoder tests/build/vandal-inputs
tools/target/debug/firmware-build vandal --repo "$PWD" \
  --out tests/build/vandal-native --upstream /path/to/pinned/yabause \
  --inputs tests/build/vandal-inputs/inputs.json
```

Output directories must be new. `vandal-inputs` reuses the full-track
compiler and pinned actual-C source verifier. It checks all 98 logical
sectors, complete original/postimage hashes and every guard, then emits
the five inputs required by the independent native check. Its per-stream
C checks are not a combined finite-lifetime claim; the finite and mount
fixtures cover that contract separately. Generated media/BIOS bytes remain
local under ignored `tests/build`; do not commit them.

`vandal-source` uses the cached pinned toolchain with networking disabled;
it verifies the linked ELF, entry, allocation boundaries and all 122 recipe
bytes. No silent toolchain download occurs. The independent
[native interpreter check](../tools/README.md#vandal-hearts-native-startup-comparison)
builds clean pinned Yabause source, preserving dependency notices. Ten
cache/entry-state cases execute the actual CD callees, BIOS decoder and
VDP routines, comparing complete 2 MiB work RAM/private stack, VDP registers,
VRAM/CRAM and the complete live registers after their first definitions.
Four deliberately broken controls must fail for their executed effects.
The maximum observed private stack is 156 bytes; it is not a full game's
stack-headroom measurement. Modeled CD replies/clock effects are bounded
fixture inputs, not a complete BIOS/game boot or hardware timing test.
The [Hitachi SH7604 manual](https://antime.kapsi.fi/sega/files/sh7604.pdf),
§2.1.3, Figure2.3, printed p.17, defines the PC as four bytes ahead of
the current instruction. The checker distinguishes instruction addresses
from the core's pipelined trace PC and executes the literal/branch controls;
it does not treat a disassembly listing as proof of runtime behavior.

## Bounded progression and sound investigation

A separate retained full-system model used the exact production C decoder
at accepted CD-buffer delivery. Both variants completed 13,000 frames with
the same fixed Start/C schedule, reaching the opening storybook/movie with
Chinese subtitles. The final image matches. Between variants 213/218
sampled video CRCs and 218/218 nonblack counts match; complete PCM does not.
Each new variant exactly reproduces its own retained 218 sampled records,
final image and all 38,325,016 PCM bytes after adding ABI6 and the source
guards. That establishes output neutrality of those changes, not equality
between the two startup implementations or tactical gameplay acceptance.

The core was independently rebuilt from its retained source with zero
cached objects, reproducing binary SHA-256
`70e075ec857c65b7bf802e302f2a8dfdaf0468cb8d6ba28d3193340c43dcb96b`.
This local provenance check is separate from the public clean-source
native interpreter workflow above. The retained model needs additional
locally available instrumented dependency source/artifacts; it is not
represented as a stock publicly downloadable core or reproducible from
this repository alone. Its bounded host frontend is preserved separately
on the `codex/public-vandal-sound-study` research branch, outside main.

Both the retained core's power-on reset entry and its direct warm-reset
entry reread the original IP/packed sectors after retirement, preserving
the launch action until host unload. A direct model reset does not exercise
the physical Reset button, a game's SYSRES call or the real menu/mount.
Actual-C/FatFs and menu fixtures cover explicit unload, fresh same-game
mount, save/error cleanup, retries and legacy pairs separately.

The sound investigation did not establish a new sound defect. Both paths
issue the same clock command/order and explicit initial sound writes, but
native startup reaches the clock call 740 local SH-2 ticks earlier. The
first 607,811 stereo samples match; the first game onset differs by three
samples (about 68 microseconds), without proving a uniform whole-stream
shift. Three sound-RAM bytes already differ before the clock reset and
retain their respective values through game sound startup. They fall
inside the model's BIOS sound-effect ring range. Passive observation
reproduces each side's prior PCM exactly. It does not trace every internal
sound write/read or prove that those bytes cause the later PCM difference.
No compensating delay, sound-RAM clearing, sample trimming or forced parity
was applied. Audibility and long-game effects remain unqualified.

The [SMPC User's Manual ST-169-R1](https://antime.kapsi.fi/sega/files/ST-169-R1-072694.pdf#page=40)
§2.3 commands 9–10, printed pp. 30–31/PDF pp. 40–41, requires the BIOS clock
routine and describes default LSI state and retained WorkRAM/CD-block
state. Command 8 SYSRES, printed p. 29/PDF p. 39, has a different memory
retention contract. These statements do not specify identical sound phase
or complete sound-RAM erasure; neither inference is used to waive the
observed PCM limit or to qualify the direct emulator reset as SYSRES.

## Pack and firmware costs

The pack is 2,808 bytes: 24-byte header and eight 348-byte records over six
distinct sectors. Versus the earlier unguarded proposal it adds three
read-only context records, 1,044 SD bytes and three mount preflight reads.
It retains the existing 12-record firmware allocation. No title-specific
MCU/menu/FPGA runtime code or persistent RAM allocation is added here.
The existing finite-source prerequisite links at 59,428 LTO / 63,696
non-LTO MCU load bytes, with 6,108 / 1,840 bytes free and 50,896 static DTCM
span. These are source-matched build figures, not hardware stack or
throughput qualification. Physical boot, actual BIOS-return/relaunch,
audible equivalence and full tactical play remain separate acceptance work.
