# MBR partition bounds (SSR-012)

A readable FAT boot sector can claim more sectors than its MBR partition owns.
Previously discovery discarded the MBR length, so ordinary file creation could
write into the following partition while staying within the card's address range.
Retain all four lengths with their starts, reject empty/unrepresentable intervals
before reading their VBR, and compare each recognized volume's total with its
partition length before mount publication. A rejected automatic-search candidate
is skipped; a valid later partition remains discoverable.

The guard uses the inclusive last sector, permitting `UINT32_MAX` without wrapping
an exclusive end. A smaller volume in a larger partition remains valid. This adds
a16-byte local array, no persistent filesystem field, task, buffer or extra disk
read. Invalid intervals avoid their former VBR read. The standard App remains
FAT32-only, with reentrancy and64-bit/GPT discovery disabled; retained BootLoader
format policy is unchanged. This is not a general partition-overlap or physical
capacity validator.

## Requirement and attribution

Microsoft **FAT32 File System Specification v1.03,6December2000, printed/PDFp13**,
**total-sector versus disk/partition-size discussion**, permits a BPB total less
than or equal to the hosting extent and identifies a larger total as malformed:
[Microsoft-authored primary specification mirror](https://www.pcjs.org/documents/papers/microsoft/MS_FAT_OVERVIEW_103-2000-12-06.pdf#page=13).
The matching local PDF/text and original evidence were used; no online research.
The selected FatFs search contract is also visible in `find_volume`'s `part=0`
comment and four-entry loop. No Sega-specific rule for this MCU filesystem API
is inferred.

The narrow correction follows paps-g `ba3938b0`/SSR-012. Original FatFs remains
attributed to ChaN under its existing notice. Public integration and regression
adaptations are by papsg. No original tpunix issue/PR is identified for this
independent finding. A later review's early-return warning informs the valid
later-candidate control; this implementation assigns a rejected format inside
the original loop rather than returning before the next candidate. No unrelated
filesystem changes are imported.

## Public reproduction

```sh
python3 -B -m unittest discover -s tests -p test_fat32_app.py -v
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B -m unittest discover -s tests -p test_fat32_app.py -v
```

To reproduce against the parent, save its unmodified `Firm_MCU/FatFS/ff.c`
outside the worktree source and set `SAROO_FATFS_SOURCE` to that file. The tests
compile that actual C with the unchanged public headers/configuration and current
fixture. They preserve generated inputs, hashes, commands/results, sparse images
and per-image before/after records under unique `tests/build/fat32-app/case-*`.
Only test formatting is enabled in the copied configuration; production config
is unchanged. App and retained-format binaries use the selected sanitizer or
optimized profile. All media are synthetic host files.

The public parent admits short, empty and wrapping extents. In the short case
the MBR owns100,000 sectors fromLBA2048 while the BPB claims131,072. An on-disk
FSInfo hint steers a normal create/write/close into cluster97,920. One write
replaces all512 marker bytes at the next partition's start with the0x5a payload;
there is no mutation of a mounted FATFS object's geometry or allocation hint.
Independent MBR/BPB decoding, full-image digests and marker bytes record the
effect. Zero-length and wrapping entries also mount before correction.

Corrected App checks refuse those three with `FR_NO_FILESYSTEM`, `fs_type=0`,
zero writes and unchanged complete images/neighbor markers. Empty/wrapping
entries perform zero reads of their VBR. Valid exact-size and larger fourth-slot
partitions preserve payloads across close/reopen, MBR, unused tail and marker,
with every write inside the selected interval. A malformed FAT-looking early
entry cannot hide a valid later volume.

A read-only virtual mapping mounts an actual FAT32 VBR/FSInfo with the partition's
last sector exactly`0xffffffff`, without allocating a2TiB file or accessing media.
It preserves the full image and proves address arithmetic, not physical capacity.
Existing superfloppy, format refusal,4GiB file-size representation, active-FAT,
FSInfo/mirror retry and retained-media checks remain covered. Six retained-format
FAT12/FAT16/exFAT controls admit smaller volumes or reject oversized totals without
writes; these compile optional host configurations and do not enable them in App.

All14 methods pass ASan/UBSan and optimized builds. An older end-wrap fixture
initially expected two reads; the new interval guard correctly refuses after
only the MBR read. That failed run is preserved and the check now requires this
earlier refusal, still asserting the error, unpublished filesystem and no writes.
The initial witness's tail inspection was also bounded to valid partitions;
original failed cohorts remain preserved.

Fresh GNU App links use the same compiler/epoch/SDK and differ only in `ff.c`
among their firmware/SDK inputs. LTO load grows44 bytes to59,488 (6,048 ITCM
free), non-LTO40 bytes to63,760 (1,776 free). Data/BSS remain200/50,696 and
204/50,688 respectively; both static DTCM spans stay50,896 and the main-stack/heap
address gap stays80,176. The individual mount frame grows48→64 bytes in both
profiles. All166 linked vector checks pass. These direct frames/address gaps
are not peak task/interrupt stack measurements. Menu and FPGA sources are unchanged.

No physical SD/Saturn, installed firmware, real-user-media loss, game failure,
wall-clock performance, proprietary Keil build or complete filesystem qualification
is claimed. Superfloppy physical-capacity admission, disk-adapter sector-count
reporting, arbitrary conflicting partitions, GPT and64-bitLBA remain separate.
