# CD filesystem, directory streaming and XA safety

Starts at public `5d221d0`. Port selected **paps-g** contributions `8e465233`
and `05852d90` as separately verified source changes. Preserve existing notices,
log formats and masks. These source defects are not attributed to a named game's
failure. Host fixtures and GNU builds do not qualify physical Saturn timing,
concurrency, games or original-MDK firmware.

## MCU-CDIO-001: complete sector-input admission

Original: producer core of paps-g `8e465233`. The preceding `get_sector` ignores
the seek result and accepts every successful read, even when the first requested
sector is incomplete. Subtracting the alignment offset from a shorter unsigned
read count can also publish an invalid cache extent.

FatFs documents the separate [f_lseek result and read-only EOF clipping](https://elm-chan.org/fsw/ff/doc/lseek.html),
and [f_read result and actual byte count](https://elm-chan.org/fsw/ff/doc/read.html).
The bundled [FatFs R0.15 implementation](https://github.com/papsg/SAROO/blob/5d221d0/Firm_MCU/FatFS/ff.c)
supplies both APIs. The fix checks the seek before reading and requires a complete
first physical sector, including the existing 512-byte alignment offset, before
publishing cache/data. Complete short cache fills remain valid; it does not
require all 32 KiB. EOF-clipped seeks yield insufficient input and are refused.
Existing cache hits, payload layout, metadata, error trace and normal error-stop
behavior remain. Subcode-sidecar I/O and wider file-offset arithmetic are outside
this selected port.

Six new actual-source methods pass ASan/UBSan and O2: all nonzero FatFs seek/read
result values; 8,192 incomplete-input alignment edges; 6,144 complete-first,
one-extra and full-cache fills; cache-hit/boundary/retry payload controls; five
physical formats, track lookup and null-output controls. The four data-sector
sizes are 2048, 2324, 2336 and 2352 bytes; audio uses 2352. A 2340-byte host
transfer format is not a physical CUE input. The preceding source fails the seek
and incomplete-input groups while passing four controls. Six compiled mutants
are rejected by behavioral assertions or sanitizers, including ignored seek,
missing alignment, one-byte-short admission, unnecessary full-cache rejection
and rounded-up cache bounds. FatFs, track and lock boundaries are host models.

All 91 CD methods pass both profiles. The complete GNU App/RTX/166-vector MCU
link remains 58,708 load bytes, with 6,828 ITCM bytes free and static DTCM 60,296.
All 56 MCU input hashes match the receipt and cached SDK hashes match the prior
checkpoint. Direct producer/disk frames remain 48/80 bytes; these exclude other
callees and exceptions. No state, workspace, allocation, payload copy, I/O on
valid input, mask or wait is added. A failed seek avoids the old unwanted read.

```sh
python3 tests/test_cdc_sector_input.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_sector_input.py
make -C tests check-cdc-command-buffer
# Baseline/mutant override: SAROO_CDC_INPUT_SOURCE=/path/to/saturn_main.c
```

Directory-error ownership is deliberately retained with the MCU-CDC-011 worker
continuation: retaining active directory state without that worker would leave
the old parser unable to complete failure cleanup. This checkpoint completes
the input-admission core, not that lifecycle. Directory streaming and selected
XA handling follow separately. No installed firmware or hardware is accessed.

## MCU-CDC-011: bounded directory streaming and file commands

Original: selected paps-g `8e465233`, requiring the preceding input-admission
core and existing routing/output-ownership ports. This also completes the
MCU-CDIO-001 directory-error continuation. These are self-reported source
findings; no original upstream issue/PR is asserted.

Sega [ST-162 §6.2.1/6.2.2, printed p.52](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=40)
defines self/parent plus up to 254 ordinary entries and retained metadata before
file access. [§6.2.3(1), (2), (4), printed p.53](https://antime.kapsi.fi/sega/files/ST-162-062094.pdf#page=41)
requires at least one free sector, the selected input/same-number true output,
buffer clearing before access, EFLS completion and invalidation of interrupted
directory information. The [later §8.2.8 functions8.1–8.6](https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/hon/p08_18.htm)
defines directory-only changes, self/parent retention on later pages, the last-
record flag, queries and abort preserving buffers/selectors. The cached English
archive is a translation; its later scope signature includes the last-record
flag absent from the older PDF signature. Do not conflate those versions.

The old code retains every directory sector before parsing, uses an eight-bit
sector count and a 16-bit first ID, trusts previous output connections and
unchecked records/heads, and derives the final-page flag from a file attribute.
The replacement uses exact byte lengths and 32-bit record/page positions. It
routes through the requested working selector, immediately detaches one delivered
block under a short mask, parses outside that mask and frees it before the next
sector. Public deletion/reuse cannot recycle that private block. This reuses the
existing pool and table; no second payload buffer or record queue is allocated.

[ECMA-119 second edition §6.8.1, printed p.8, and §9.1, printed pp.27–30](https://ecma-international.org/wp-content/uploads/ECMA-119_2nd_edition_december_1987.pdf)
defines sector-contained records, extent/length fields, extended attributes,
identifier lengths and padding. Check the complete record/name extent before
reading fields, use unsigned shifts and checked extended-attribute addition,
and bound the last sector by its exact declared length. Check self/parent
identity and retain them on every page. Store at most254 requested entries;
one additional valid record distinguishes a full page from the final page.
This is bounded supported-format parsing, not exhaustive ISO9660 validation.

Descriptor reads check CD001/version, supported2048-byte logical blocks and the
root record/extent. Leading boot/supplementary descriptors may be skipped;
a terminator without a primary descriptor fails. The initial descriptor remains
FAD166, so multi-session selection is not implemented. File commands reject
unavailable metadata and invalid IDs/24-bit spans before pausing or clearing a
destination. Snapshot required file scalars before waits; round byte lengths up
without overflowing addition. Queries preserve full IDs and self/parent access;
scope queries do not generate a new completion interrupt. Empty all-file queries
and malformed/unavailable metadata use the contribution's defensive REJECT
policy, not measured original-drive error responses.

Completion publishes validity/status/idle ownership and EFLS together under a
short mask, then returns directly to the worker wait point. Input/track errors
retain directory ownership until that completion, without falling into ordinary
play-end code after a newer command starts. Abort, reset and both worker pause
paths invalidate unfinished metadata while preserving remaining public buffers.
The public adaptation keeps main's existing atomic pause acknowledgement. An
internal descriptor-to-record transition also preserves a pause arriving during
parsing; only the worker acknowledgement retires that request. This narrow
handoff correction is necessary for the new streaming lifecycle.

All original log formats/masks remain, verified as a multiset against the
preceding source. Unlike the private contribution, the public port retains
per-record traces. A separate optional helper copies at most219 name bytes into
a 256-byte stack array, then uses the original format. It never writes a
terminator into sector data or logs while masked. Directory-size traces retain
sector units, while internal state uses exact bytes. Logging can still block;
it is not a new diagnostic or a speed improvement.

Eleven actual-source directory methods pass ASan/UBSan and O2:1,512 selector/
count/page combinations;65,540 records over8,193 sectors; all fixed-header/name/
padding bounds and unsigned fields; descriptor/root transitions; exact file/FIFO
replies, full IDs/self/parent; valid/refused spans and pre-wait snapshots; abort/
pause/private reuse/redirects; atomic completion; pending phase-transition pause;
and unchanged payloads/log controls. With199 of200 slots occupied, a300-sector
directory reaches ID2000 and retains254 entries after282 reads, preserving the
other199 blocks. This demonstrates modeled pool progress, not physical scheduling.

Six isolated preceding-source probes fail after compiling, with one valid
metadata control passing: empty-count underflow, crossing records, wrong final
flag, stale selector, signed shift and missing head. They preserve the old
sector-count unit; they are not a whole old-firmware run. Twenty compiled
behavioral mutants are detected. Seven producer methods include four directory
error/track cases with retained ownership; ordinary error-stop controls remain.
All103 CD methods pass both profiles; strengthened snapshot/log/redirect tests
are included in final reruns. Eleven disc/save handoff methods also pass. The
obsolete two-method directory setup differential is retired, its source/results
preserved in earlier commits, in favor of actual records/callers/ownership tests.
Host IRQ, FIFO, track, FatFs and scheduler boundaries remain explicit models.

Complete GNU MCU load grows580 bytes:58,708→59,288, leaving6,248 ITCM bytes.
Static DTCM grows16 aligned bytes to60,312 for wider state and parser position.
Pool, persistent payload workspace, required stacks and allocations stay unchanged.
The56 repository input hashes and identical SDK hashes bind the receipt.
Direct producer/disk/command frames remain48/80/24; directory start8 calls shared
file start32, parser8, handle72, finish16, change24 and file read40. The optional
name logger uses288 bytes. Those individual frames exclude other callees and
exceptions; no physical peak or cycle claim is made. Parsing remains linear in
visited records, including skipped prefixes. Detach/completion masks cover
metadata only; per-sector routing retains its existing payload copy. Abort/reset
waits and blocking logs retain their stated limits.

```sh
python3 tests/test_cdc_directory.py
SAROO_CDC_PROFILE=release python3 tests/test_cdc_directory.py
make -C tests check-cdc-command-buffer
# Reproduce the old isolated probes with source extracted from525dc55:
python3 tests/check_cdc_directory_baseline.py /path/to/old/saturn_cdc.c
# Candidate/mutant overrides: SAROO_CDC_DIRECTORY_SOURCE, SAROO_CDC_DIRECTORY_MAIN
```

XA metadata, file-number selection and interleave geometry follow separately.
Broader session/default drive-range/EOF/PEND/tray behavior and asynchronous
copy/move/MPEG ownership remain separate; this is not complete CD-block or
named-game qualification. No hardware or installed firmware is accessed.
