# Optional contiguous allocation safety (SSR-013)

The optional `f_expand` search previously combined free clusters at opposite
physical heap ends. That is not a contiguous extent. Allocation could persist
a FAT link past the heap before returning an error; the search also missed a
real span crossing its starting cursor. Reset the run at the physical end,
after recognizing a valid end-aligned span, and bound the scan to heap size
plus requested clusters minus one. This covers a cursor-crossing linear span
without joining the heap ends. Validate request/complete interval before mutation
and before narrowing the cluster count. The retained exFAT bitmap search uses
the same linear-span rule. Admitted MAX_EXFAT bounds keep its DWORD budget valid.

This adapts paps-g `491f8b63`/SSR-013. ChaN's original FatFs copyright and license
remain intact. Public source/fixture integration is by papsg. The requirement
is FatFs R0.15 **f_expand**, **Description**, **opt**, and its **FR_DENIED**
conditions: the result must describe physically contiguous space, and preparation
mode must support subsequent contiguous allocation. See the matching locally
retained [official FatFs reference](https://elm-chan.org/fsw/ff/doc/expand.html).
No Sega-specific API rule or original tpunix issue/PR is identified for this
independent finding; no online research was needed.

## Reproduce offline

```sh
python3 -B -m unittest discover -s tests -p test_fatfs_expand.py -v
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B -m unittest discover -s tests -p test_fatfs_expand.py -v
```

Only copied host configurations enable `FF_USE_EXPAND`. Production keeps it0.
The App/retained-format variants execute the actual FatFs public API on sparse
FAT32 files. Their independent complete FAT/directory/FSInfo oracle establishes
unique ownership, no orphan/duplicate/out-of-range chains, maximum linear free
span and unchanged filler chain; exact payloads and contiguous probe chains
are checked after close.

Twenty public cases cover both preparation/allocation modes, split/end/head/
cursor layouts and oversized requests in both variants. All pass under
ASan/UBSan and optimization. The parent fails eight outcomes across split and
cursor layouts. Its two allocation witnesses independently retain a persisted
link from the final heap cluster to the first out-of-heap number. Complete
pre-operation images/ownership proofs, postimages, hashes, source copies and
results remain in unique `tests/build/fatfs-expand/case-*` directories.
A failed expansion may follow empty-file creation; complete image identity
is not promised. Refusal checks preserve the FAT, hint and empty-file size.

A separate actual private helper checks29,688 bitmap/origin/request combinations
against an independent linear enumeration. Original code returns1,200 invalid
spans and misses611. Corrected code has zero invalid/missed spans and no writes.
Its filesystem/bitmap state is manufactured; this does not qualify mounted
exFAT expansion. The initial fixture indentation failure remains preserved
separately from the complete original-provider defect witness.

Fresh standard GNU App/166-vector builds retain every loaded byte of the
accepted SSR-012 parent in LTO and non-LTO modes. Load stays59,488/63,760 bytes,
ITCM free6,048/1,776, data200/204 and BSS50,696/50,688. Both static DTCM spans
remain50,896, with unchanged reserved stack/address gap. Neither optional
expansion nor exFAT helper enters these App images. No optional-enabled target
resource budget, complete BOOT link or new peak-stack measurement is claimed.
Menu/FPGA and production feature selections are unchanged.

This repairs a conditional dependency API. Physical SD/Saturn, mounted-exFAT
expansion, FAT12/16 expansion, I/O-failure atomicity, real file/game failures
and whole-filesystem qualification remain outside these checks. No device
operation or firmware installation occurred.
