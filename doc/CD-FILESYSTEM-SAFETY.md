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
