# FatFs filename review

## SSR-087: no empty-on-error requirement for the selected release

Disposition at public `e725755`: **do not port the historical name-clear**.
The observed nonempty filename after an error is real, but the alleged
requirement is not established for this pinned FatFs release. No runtime
correction is justified by the reviewed callers. This is an evidence-supported
rejection of the defect classification, not a claim that error output is valid.

The version-matched author's [R0.15 archive](https://elm-chan.org/fsw/ff/arc/ff15.zip),
`documents/doc/readdir.html`, **Description** and **Sample Code**, promises an
empty filename at end of directory with no error. Its example checks FRESULT
before examining the filename. `documents/doc/sfileinfo.html`, **Members,
fname[]**, likewise associates the sentinel with no remaining item. Neither
establishes the stronger empty-on-error promise used by the original finding.
The locally preserved archive SHA-256 is
`e0d76654d877e6c74be5ea3c395808794d495169514e98cbf6046168b8f4f070`;
its header and the public provider both use `FF_DEFINED=80286`.
No live unversioned page was fetched or assumed to describe this release.

The public provider's `f_readdir` tail, from `DIR_READ_FILE` through return,
matches that archive exactly. It fills information before advancing the
directory and can return the advance error without clearing the name. Earlier
failures can retain caller output. Separate workspace/recovery changes mean
that whole-provider or whole-function equality would be an incorrect claim.
Null-output rewind, original errors and retry state remain unchanged here.

All three external source callers check FRESULT first: `list_entries` and
`find_cue_iso` in `Firm_MCU/Saturn/cdimg.c`, and `scan_dir` in
`Firm_MCU/FatFS/file_utils.c`. Their `&&`/`||` conditions prevent a failed
read from reaching filename checks or entry consumers. C11 N1570
[§§6.5.13p4 and 6.5.14p4, printed p.99](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
defines that left-to-right, short-circuit evaluation. The internal find wrapper
is disabled by the current `FF_USE_FIND=0`. Other configurations/callers and
unrelated error propagation are outside this conclusion. No Sega rule defines
this MCU library's output-on-error policy.

Paps-g `399d72df` originally added a bounded name-clear. Its later independent
review withdrew the versioned API-defect claim and retained that stricter
behavior only as private project hardening. Papsg's public revalidation retains
the selected library/caller contract instead; it does not import the optional
policy extension or misattribute it to ChaN. Original source, notices and
historical evidence remain preserved. No original tpunix issue/PR is identified.

Existing public regressions are reproducible without a private preparer:

```sh
PYTHONPATH=tests python3 -B -m unittest -v \
  test_mcu_save_discovery.DiscDiscoveryTests.test_directory_failures_do_not_publish_partial_candidates_or_catalogs \
  test_mcu_save_discovery.DiscDiscoveryTests.test_catalog_filtering_order_lookup_and_repeated_scans \
  test_mcu_save_discovery.DiscDiscoveryTests.test_actual_bin_consumer_rejects_stale_failed_and_empty_catalogs \
  test_mcu_save_discovery.DiscDiscoveryTests.test_bin_folder_errors_empty_the_catalog_and_allow_retry
```

All four methods/23 synthetic process cases pass with ASan/UBSan. The real
catalog/discovery functions use public FatFs; the test wrapper injects a failed
second read while retaining the earlier filename. Errors publish no partial
catalog/candidate, cleanup and retries recover, and ordinary scans remain
valid. This exercises two normal consumers, not the optional recursive scanner
or every real block-error position. It does not establish physical SDIO/media
fault frequency, Saturn behavior or games. No runtime source changes, allocations,
I/O, target load/stack costs or fresh target-build claims accompany this
documentation-only disposition. The preceding verified MCU inputs are unchanged.

## SSR-097: preserve complete supported UTF-8 leaf identities

The public parent accepts names that its directory output cannot represent.
With the UTF-8 API, a 255-unit LFN may need 765 bytes, and an OEM 8.3 name may
expand to 34 bytes including its dot. The old 255/12-byte output budgets cause
successful reads to return an alias or `?`. This is supported-name identity and
discovery loss in SAROO's configuration. FatFs documents its fallback; it is
not an upstream conversion-rule violation or evidence of a named game failure.

The locally preserved versioned [R0.15 archive](https://elm-chan.org/fsw/ff/arc/ff15.zip),
`documents/doc/config.html`, **FF_LFN_BUF, FF_SFN_BUF**, gives the UTF-8 maxima
765/34 and documents fallback when LFN output is insufficient. See the archive
hash in SSR-087 above. The same archive's `readdir.html`, **Description**, explains
the inaccessible-name fallback. Microsoft *FAT: General Overview of On-Disk
Format*, v1.03 (6 December 2000), **FAT Directory Structure**, printed pp.23–24,
and its short-name rules on printed p.29, defines eleven OEM bytes split 8.3,
the implied dot and permitted high-byte characters. Those locally retained
primary documents were read; no Sega-specific filename rule is inferred.

Adapted from paps-g `b6b04d6d`/SSR-097, with papsg's public-layout integration
and additional boundary corpus. ChaN's source/notices remain intact. FatFs
output capacities are now 765/34 before NUL. The MCU and menu headers declare
the same 766-byte terminated catalog leaf; the MCU asserts agreement with
`FILINFO.fname`, and host checks compare the paired header constants. The
existing bounded binary-folder decoder accepts that full leaf. Ordinary game
labels retain their existing path reader. In the public layout, the existing paired headers carry this interface
constant; no shared-header framework is a prerequisite. No original tpunix issue/PR is identified.

On-disk names still have at most 255 UTF-16 units. Shared offsets, directory
tags, entry count and the variable string pool retain their original bounds.
BIN request paths still fit 239 bytes plus NUL; game/image path workspaces
remain 256 bytes including NUL. A complete leaf can be listed while its full
launch path is refused. No alias or truncation bypasses those path checks.
Update the paired menu with the MCU to decode binary-folder leaves above the
old limit. Labels still use bounded 128-/96-byte formatting/storage; full glyph
coverage, complete display and clipping at a Unicode boundary are unqualified.

Reproduce with the existing synthetic-media and menu adapters:

```sh
PYTHONPATH=tests python3 -B -m unittest -v test_mcu_save_discovery test_menu_bin_browser
SAROO_FIRMWARE_SAVE_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_mcu_save_discovery
SAROO_MENU_BIN_PROFILE=release PYTHONPATH=tests \
  python3 -B -m unittest -v test_menu_bin_browser
```

The selected 15 methods/64 C processes pass ASan/UBSan and O2, including five
original UTF-8 cases and four independent identities: ASCII, a 255-byte mixed
leaf, a 505-byte supplementary-plane leaf using exactly 255 UTF-16 units, and
a 34-byte SFN-only identity. The latter authors a real ASCII file/payload with
FatFs, changes only its eleven OEM bytes to CP437 `0xb3`, verifies no LFN, then
remounts and opens/reads its exact UTF-8 identity before enumeration. The
original 20-byte SFN-only ISO case also passes actual image discovery. The
generic 64-bit provider passes the same nine cases on FAT32: two more methods.
This is not exFAT-media qualification. No MKFS change is needed by these cases.

Reverting only the two output capacities in a copied `Firm_MCU/FatFS` directory
and setting `SAROO_FATFS_DIR` to it yields six identity failures among nine
processes, preserving the 204-byte, ASCII and exact 255-byte controls. It retains
every other current provider correction. The old menu source selected through
`SAROO_MENU_BIN_SOURCE` fails only the full-leaf method; six ordinary controls
pass. Seven selected menu methods pass both profiles, retaining the long leaf
and its short neighbor, refusing its overlong launch/child path before file I/O,
launching the short neighbor and refusing an unterminated 766-byte leaf.
Copied inputs, hashes, logs and results are retained. Two initial source-adapter
compile failures occurred before any name test; they are not defect witnesses.

Distinct 765-byte leaves test the actual producer's unchanged string pool:
173 entries fit with 602 bytes remaining; entry 174 requires 766 bytes and
returns `-FR_NOT_ENOUGH_CORE`, publishes zero and frees the scan allocation.
Deleting that one file lets a fresh scan publish all 173 again. Larger names
therefore reduce entry density; no larger shared region is reserved.

`FILINFO` grows 280→812 bytes in the selected provider (generic output is 816).
Catalog/discovery callers allocate it on the heap and release completed scans.
The linked App excludes `scan_dir`; `FF_FS_RPATH=1` excludes `f_getcwd`'s local
object. The real compiled RTX common allocator passes the existing QEMU
startup/depth/hole-reuse/exhaustion comparison with both old/new output sizes.
Its deliberately bounded model holds 22 directory pairs instead of 44 in the
unchanged 29,184-byte pool, with parent/current admission and offsets equal.
This is the cost of larger objects in that scenario, not a whole-program heap
peak or a promised recursive-directory depth.

Both fresh standard 166-vector App builds pass. LTO adds 28 load bytes to 59,536
(6,000 free); non-LTO adds 24 to 63,792 (1,744 free). Data/BSS stay 200/50,696 and
204/50,688; static DTCM span stays 50,896. All 70 public MCU inputs match their
staged cohorts; only the configuration, `cdc.h` and `cdimg.c` intentionally
change. Menu code/data grows 16 bytes to 87,928, BSS stays 9,508 and reserved stack
16 KiB; body margin is 17,252. Its fixed 488,770-byte image size and font are
unchanged, while linked bytes differ. All 40 menu inputs match. Direct frames
remain 112/104 for catalog scanning, 88/80 for discovery and 40/24 for readdir
(LTO/non-LTO); menu decoder 16 and label formatting 164 remain. These exclude
callee/IRQ peaks. Source, host/QEMU checks and GNU links do not qualify MDK,
physical media, UI/glyphs, STM32 scheduling, stack/heap peaks, Saturn or games.
