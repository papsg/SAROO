# Save ownership and expansion RAM follow-up

## Compact the configuration parser

The loader derives the 12 setting names, their byte offsets and lengths
from one definition. Each MCU table entry falls from 12 to 8 bytes. A
compile-time check bounds the name pool, and the parser uses the stored
lengths. Numeric parsing keeps the carry in a wide temporary before
checking the 32-bit limit. The linked Arm digit loop uses UMLAL in
place of UDIV. Setting order, case, accepted values, error handling and
the existing two-pass validation and publication are preserved.
[Existing configuration contract](CONFIGURATION-SAFETY.md) ·
[Shipped configuration](saroocfg.txt) ·
[Preceding public parser](https://github.com/papsg/SAROO/blob/f04106d7ed9dc25c800df97728ba8f6c3cf0e8a9/Firm_MCU/Saturn/saturn_utils.c#L216-L269).

Adapted from paps-g's `3ec942a4`. The preceding code and the port both
pass all 20 configuration methods, including 328 loader cases. The
added controls cover case-sensitive names, repeated assignments,
leading zeroes and 32-bit overflow. Run `make -C tests check-config`
and repeat with `SAROO_CONFIG_PROFILE=release`. The port passes
ASan/UBSan and O2. To test the preceding sources, set
`SAROO_CONFIG_LOADER_DIR` and `SAROO_CONFIG_MATCH_SOURCE` as documented
in the respective test modules.

The public GNU App load drops from 62,812 to 62,752 bytes. Static DTCM
stays at 60,216 bytes. This comparison measures linked bytes and target
instructions. Configuration load latency on hardware remains unmeasured.

## Access the existing save archive handles

The MCU advertises the archive protocol at system info offset 0x18 and
publishes the full 16-byte selected key at 0x1c after successful
selection. Startup, failed selection and a slot-archive write clear the
key. Two tagged commands read or write through the primary FatFs handle
under the existing filesystem lock. They require exact transfers of at
most 64 KiB within the existing archive. The slot directory is read-only
through this interface. A zero-length write at offset zero requests
sync. The reply publishes the count and signed result before clearing the
command. [Original command and result registers, lines 526–548](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/FPGA/SSMaster.v#L526-L548) ·
[FatFs f_sync, Description](https://elm-chan.org/fsw/ff/doc/sync.html) ·
[Existing archive recovery and extent rules](FIRMWARE-SAVE-SAFETY.md).

Adapted from paps-g's `b7720d00`. The public adaptation uses the
checked reopen path and keeps the existing first-8-MiB MEMS limit on
archives with opaque trailers. An attempted write revokes stale legacy
writeback before it can modify a prefix. For MEMS, an exact read of the
root into its resident buffer restores eligibility. The protocol returns
zero bytes on an error, even if a partial read or write changed data.
Ordinary BIOS saves keep using the existing requests. Physical media
recovery and transactional save behavior remain unqualified.

Run `make -C tests check-file-transfer` and
`python3 -m unittest discover -s tests -p test_mcu_save_owner.py -v`.
The preceding MCU leaves the new archive commands pending. A control
that omits revocation on a failed root read permits stale writeback on
both FAT32 and exFAT. The corrected code passes 44 real FatFs image
cases, 85 command-boundary cases and the existing file/save controls.
The 20 focused methods pass ASan/UBSan and O2, with 46 methods passing
in the broader O2 run. Use `SAROO_FILE_PROFILE=release` and
`SAROO_FIRMWARE_SAVE_PROFILE=release` for the optimized fixtures.
The GNU App adds 432 bytes to 63,184. Static DTCM stays at 60,216 bytes.

## Pair the menu with the primary save owners

The menu latches the MCU archive capability and uses direct FPGA buffers
for archive transfers. A transfer requires the exact tagged reply and
byte count. If a read fails, the caller invalidates the full possible
written extent. The existing named file API and BIOS save callers use
their established request paths. [Shared command register contract](IO-CACHE-FOLLOWUP.md#confirm-file-transfers-explicitly) ·
[Original public save archive layout](FIRMWARE-SAVE-SAFETY.md).

The slot helper scans the 64 KiB directory through the same primary
owner. A caller-provided window can be 512 bytes through 64 KiB in
512-byte units. Lookup copies the 16-byte input key before the first
read, accepts a single matching entry, and refuses a missing or
duplicate key. It returns the payload offset. Archive I/O checks the
actual extent when a caller uses that offset.

Adapted from paps-g's `3f6e0121` after the bounded MCU owner port.
The public lookup validates the full 16-byte directory signature,
including its terminator, consistent with the public MCU. Run
`python3 -m unittest discover -s tests -p test_mcu_save_menu_owner.py -v`
and repeat with `SAROO_FIRMWARE_SAVE_PROFILE=release`. Sixteen paired
FAT32 and exFAT cases cover both archives, reset replies, bounded reads,
key aliasing and directory faults. Omitting the exact-count requirement
falsely accepts short replies in four cases. The paired and ordinary
transfer methods pass under ASan/UBSan, and all 32 focused methods pass
under O2. Three legacy-menu controls also pass against the new MCU.
The menu adds 528 linked code/data bytes to 80,244. BSS stays at 9,240
bytes and the padded image remains 488,770 bytes. These tests exercise
the real handlers with modeled cartridge registers. Physical timing
and save recovery remain unqualified.
