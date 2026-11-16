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
