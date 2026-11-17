# Native internal backup candidate

Adapted from paps-g private commit `24a4b069` (NATIVE-BUP-001). This
checkpoint stages only the missing device-0 allocation behavior. The
extended-cartridge accounting from public `f40702b` and the game-facing
Hitachi SH-2 register boundary from public `b52b40d` remain in place.
Their earlier [attribution and source citations](FIRMWARE-SAVE-SAFETY.md)
apply; neither implementation is duplicated here.

[Sega ST-162, Backup Library §§2.1–2.3, printed pp. 39, 41–48](https://antime.kapsi.fi/sega/files/ST-162-R1-092994.pdf)
identifies internal backup as device 0 and specifies the public API,
geometry fields and operation results. The private native-BIOS analysis
retained with `24a4b069` establishes the allocation cost: two reserved
blocks, one header block per save, its first 28 data bytes in the header,
then 58 data bytes per continuation block. This is a virtual implementation
of the native API, not a physical SRAM image.

The candidate continues to transport one 65,536-byte `SaroSave` slot per
selected game. Its private blocks remain 128 bytes apart. The candidate's
header identifies the new format with total size 32,768 and block size 64;
the previous format identifies itself with 65,536 and 128. A valid
old-format header is rejected as broken by the native path before reading
or mutating its records. Initialization only autoformats a wholly blank
slot with an absent signature. An explicit format clears the entire
65,536-byte transport slot. The [source](../Firm_Saturn/game_save.c) uses
one allocation rule for Stat, Dir, new write, protected overwrite, resize,
read, verify and delete. The existing [save-generation journal](../Firm_Saturn/save_journal_plan.c)
still publishes the complete device-0 transport slot; its command and
recovery ownership remain unchanged.

## Admission and current status

`SAROO_NATIVE_INTERNAL_BACKUP=0` is the [standard menu build](../Firm_Saturn/Makefile).
The old expanded-format service therefore remains active until the archive
converter and both host editors can safely handle existing saves. The
opt-in build is source and host qualified here; it has not been installed.
Do not use an old archive with the opt-in image. Conversion and final
standard activation are separate required checkpoints.

The public parent is `d955fe7`. Its source fails the native geometry
fixture at the initial Stat call; the candidate passes. The
[actual-module host fixture](../tests/firmware_save_native.c) checks payload
sizes 0/1/27/28/29/85/86/87/992/1024/1276/29,550, nine resize paths,
metadata/payload round trips, protected and oversized refusal, 510 tiny
files, fragmentation/reuse, three 1,276-byte saves and cartridge
0/960/961/1,024/1,025/479-KiB controls. A legacy 65,536/128 slot is
refused without a flush. The [public-entry journal fixture](../tests/test_save_journal_binding.py)
checks both devices, normal writes, format/delete, interrupted body/header
publication, apply, partial write and corrupt recovery. Its native format
case seeds the upper transport half and checks that format clears it.

```sh
python3 -m unittest discover -s tests -p test_firmware_save_native.py -v
SAROO_NATIVE_INTERNAL_BACKUP_TEST=1 \
  python3 -m unittest discover -s tests -p test_save_journal_binding.py -v
python3 -B tests/build_save_generation_candidate.py --native-internal
```

Both native host methods pass under ASan/UBSan and `-O2`. The default
resize (six methods), validation (six) and public-entry journal (one)
controls pass. The linked backup-entry check passes on both menu profiles:
all 11 entries, 64 adversarial register-clobber cases and three broken
restore controls. The standard menu ELF and complete 488,770-byte image
are byte-identical to the parent: image SHA-256
`8e4fa6798d649665b97ca3176c7fa9823cefa7f561176eff46d260ad545984e4`,
97,000 code/data bytes, 9,540 BSS bytes and 8,148 bytes of body margin.
The opt-in SuperH link has 96,984 code/data bytes, unchanged BSS, 8,164
bytes of body margin and image SHA-256
`e67f3ec93c8b795d118b6935afd289be782424262b4cc63cb9201985e2369f39`.
The shared 65,536-byte workspace and 16,384-byte stack reservation do
not change. Direct function frames are not a whole-call-chain stack bound.

These are host models and target links. They do not qualify SD durability,
physical power loss, Saturn timing, retail games, or use of old archives
with the opt-in firmware. Standard activation must wait for explicit
conversion and verified host-tool support.

## Staged host migration support

The [Rust archive converter](../tools/README.md#native-internal-backup-archive-conversion)
and existing [C CLI](../tools/savetool/sr_bup.c) and
[browser editor](../tools/savetool/web/src/save-codec.mjs) now understand the
native layout. The converter is an adapted subset of private `24a4b069`'s
`saroo-save-native`; the C layout handling adapts its
`savetool-native-backup.patch`, and the browser layout handling adapts the
same commit's `src/host/web/save-codec.mjs`. Public adaptations keep new-slot
creation in the old format until the standard firmware switches. Existing
old and native slots can be read and edited without implicit conversion.

Independent Python encoders produce old/native archives for the Rust,
sanitized C and browser checks. A mixed archive converts byte for byte to
the independent native encoder while an unnamed slot stays untouched.
Boundary payloads, 510 empty native saves, resize, round trips, and
overcapacity/corruption/pending-journal refusal are covered. The source file
is hashed before and after conversion and never overwritten. These checks do
not establish physical save compatibility or power-loss behavior.
