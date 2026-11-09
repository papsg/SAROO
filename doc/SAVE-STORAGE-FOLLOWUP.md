# Save and storage follow-up

These independently revalidated ports follow public `9ff69d6`. They preserve
the original repository layout and completed safety work. Host fixtures and
target builds do not establish physical SD, RTOS, BIOS or game qualification.

## SAVETOOL-007: clear only the exported header

Each native exporter clears 32 KiB of scratch, or 1 MiB for MEMS, before
writing a 64-byte header and payload. The existing validated readers overwrite
every emitted payload byte; the Saturn reader rejects incomplete chains/reads
before publishing output. Clear only `BUP_HEADER_SIZE` (64). The entire 1 MiB
scratch allocation remains; this reduces writes, not static storage.

This is an independently discovered optimization by `paps-g`, originally
`99127a6ad2e12128a716d05cbc83a08f913b83eb` (2026-09-06). This public port separates
clearing from its discarded-CRC optimization. Existing source/dependency
notices and format authorship remain intact. The existing BUP contribution by
Damián Parrino / Bucanero in [PR #269](https://github.com/tpunix/SAROO/pull/269),
and SRO extension/CRC contribution in [PR #271](https://github.com/tpunix/SAROO/pull/271),
are retained rather than reimplemented. The inspected cached
[issue #270 format clarification](https://github.com/tpunix/SAROO/issues/270#issuecomment-2615753729)
places the big-endian SRO CRC at `0x0c`, covering bytes `0x10` through EOF.

The actual [three exporters and validated readers](https://github.com/papsg/SAROO/tree/9ff69d6/tools/savetool)
and [`bup_format.h`](https://github.com/papsg/SAROO/blob/9ff69d6/tools/savetool/bup_format.h)
establish the project-defined formats and 64-byte header bound. Sega does not
define SAROO's host scratch-clearing policy. This change preserves complete
SRO/BUP headers, payloads, filenames, reserved bytes, retained CRCs, archive
contents and the already checked output-publication path.

Three public methods exercise **100 poisoned-buffer exports** and **eight BUP
import/re-export scenarios**. Generated MEMS, SAROO slot, plain Saturn and
odd-byte Saturn archives cover 25 layout/size combinations, both formats,
zero length, inline/block/table boundaries and maximum supported payloads.
Every complete file is compared with independently assembled Python bytes;
SRO CRCs use Python zlib. Changed import metadata and both re-exports are
checked, and exports leave the complete archive unchanged.

The fixture stages actual public C. It replaces only each export's scratch
clear with a wrapper that poisons the entire scratch allocation with `0xa5`,
then performs and counts the requested clear. A wrapper counts actual CRC
calls/bytes; the original CRC implementation still runs. These hooks exist
only in test builds. Input/staged source, hashes, compiler logs and results
remain in distinct `tests/build/export-work` cohorts. The shipped CLI gains
no diagnostics or test switches.

The original exports pass the byte/round-trip controls but fail all **50
reduced-clearing assertions**. ASan/UBSan and normal O2 candidates pass all
three methods. This demonstrates unnecessary work, not a baseline output
corruption defect. Deliberate full-clear restoration, incomplete header
clearing and an omitted payload byte are detected independently.
All **50 native save methods** (thirteen MEMS and 37 save-tool methods) pass
with the sanitizer CLI and with the uninstrumented O2 CLI override. Existing
direct fault/reader helpers retain their sanitizer builds in either run.

```sh
python3 -B -m unittest discover -s tests -p test_savetool_export_work.py -v
SAROO_SAVETOOL_PROFILE=release python3 -B -m unittest discover -s tests -p test_savetool_export_work.py -v
make -C tests check-native-save
```

Uninstrumented ARM64 O2 text grows **15,592→15,612 bytes**; zero-fill storage
stays **1,082,580 bytes**. Native executable padding is not code size. Exporter
direct frames stay 224/208/224 bytes for SAROO/MEMS/Saturn; all firmware inputs
remain unchanged. These individual frames exclude callees and stack peaks. There is no new
buffer or allocation. This checkpoint still computes the original CRC for
both formats; skipping the discarded BUP CRC is a separate evaluation.
No timing, physical-device, power-loss or game improvement is inferred.

### Separate evaluation: skip only the discarded BUP CRC

All three exporters build an SRO header and calculate its CRC before the BUP
branch replaces the entire 64-byte header. BUP has no field retaining that
CRC. Skip it only for `exp_type == 1`, keeping the existing SRO calculation
over metadata and payload. Format selection, output bytes, imports, archive
ownership and checked publication remain unchanged. This ports the other
independently evaluated part of `paps-g`'s `99127a6a`; the format/source/request
citations and original notices above apply.

The same 100 poisoned exports and eight import/re-export scenarios pass
ASan/UBSan and O2. The preceding checkpoint passes complete-byte controls but
fails **25 BUP work assertions**, while every SRO work control passes. Across
the workloads, BUP CRC calls/bytes fall **one / N+48 → zero / zero**, and SRO
remains **one / N+48**. At the 490,496-byte MEMS payload limit this avoids
scanning 490,544 bytes. Restoring the redundant CRC is detected by all 25 BUP
work controls; omitting the retained SRO CRC fails 25 independent complete-file
comparisons. This is measured work removal, not a claim of previously corrupt
BUP exports.

The complete 50-method native suite passes with sanitizer and O2 CLIs; existing
direct fault helpers retain their sanitizer builds. Uninstrumented ARM64 O2
text adds **20 more bytes**, reaching **15,632**. Zero-fill storage remains
1,082,580 bytes and all comparable direct frames remain unchanged. Firmware
inputs are unchanged. Public reproduction commands are the same as above;
no physical or end-to-end timing improvement is claimed.
