# Native save-tool safety

This series ports independently reviewed maintenance corrections to the native
`tools/savetool` CLI. The public v0.8 archive formats and capacities remain the
compatibility target. Browser, console firmware and physical storage acceptance
have separate scopes.

## SAVETOOL-002: resizing an existing MEMS save

On public starting commit `54d0c7244db481b9218447dc902b928b5e6bb760`, overwriting
a save replaces its date and copies its payload using the **old stored size and
allocation**. A 32-byte save replaced by 64 bytes exports only 32; replacing 64
bytes with 32 exports an extra 32 bytes. The same error crosses the 960-byte
inline boundary and affects zero-length and maximum-size replacements.

The primary format/behavior reference is the baseline
[`sr_mems.c`](https://github.com/papsg/SAROO/blob/54d0c7244db481b9218447dc902b928b5e6bb760/tools/savetool/sr_mems.c),
specifically `MEMSHEADER`, `access_data`, `sr_mems_import` and the block bitmap.
MEMS is a project-defined archive: its 1,024-byte blocks, eight reserved blocks,
8,064 bitmap positions, 960 inline bytes and 479-entry terminated data table
establish the layout and 490,496-byte maximum payload.

The selected save's header and directory position remain stable. A private
bitmap and bounded block list plan replacement storage, allowing reuse of its
old data blocks even when no other free space exists. Header/old-data references
and capacity are checked before publication. On success, the size, block table,
bitmap and recounted free-block total are committed, then the replacement data
and date are written. Existing name/comment/language overwrite semantics remain.
Equal-size overwrites retain their previous byte layout.

The temporary plan contains a 1,008-byte bitmap and 958-byte block list; there
is no extra full-archive copy or firmware allocation. Capacity/layout refusal
preserves the complete archive. This protects the resize path; full cross-save
metadata admission, incoming-file validation, new-save allocation and checked
file replacement are separate subsequent members of this series. The current
legacy writer still has its previously documented persistence limits.

### Origin and attribution

Resizing is our independently discovered correction, first implemented in the
maintenance workspace on 2026-09-05 as `3eada90f` (author `paps-g`). This public
port revalidates it against public main and additionally bounds the selected
overwrite header before accessing it. It does not import the private build or
patch-application framework.

Damián Parrino / Bucanero's
[original PR #372](https://github.com/tpunix/SAROO/pull/372) fixes the separate
date-pointer error and is already integrated. Its correction, original credit
and same-size preservation controls remain intact. SRO/BUP support also remains
the existing implementation from
[#269](https://github.com/tpunix/SAROO/pull/269) and
[#270](https://github.com/tpunix/SAROO/issues/270)/
[#271](https://github.com/tpunix/SAROO/pull/271); those contributions are not
imported again. Existing source notices are preserved.

### Reproduce and verify

From the public repository root, with Python 3 and a C compiler:

```sh
make -C tests check-native-save check-save-endian check-web-save
cc -std=gnu11 -O2 -g -Wall -fno-strict-aliasing tools/savetool/*.c -o tests/build/savetool/sst-release
SAROO_SAVETOOL_BINARY="$PWD/tests/build/savetool/sst-release" python3 -B -m unittest discover -s tests -p 'test_mems_*.py' -v
```

The new three-method CLI suite exercises thirteen successful resize scenarios
and six rejected layouts/capacity cases. It checks old-block reuse, inline/table
transitions, zero and maximum lengths, stable identity/metadata, complete
allocation/free-count consistency, unchanged neighbors and all bytes outside
the permitted mutation extents. Fresh CLI processes export the exact payload,
date and CRC; read-only exports and incoming saves remain unchanged. The five
existing MEMS overwrite methods continue to cover same-size and directory-edge
controls. Both ASan/UBSan and the normal O2 build pass.

To exercise the same assertions against the public starting point:

```sh
mkdir -p tests/build/save-baseline
git archive 54d0c7244db481b9218447dc902b928b5e6bb760 tools/savetool | tar -x -C tests/build/save-baseline
cc -std=gnu11 -O1 -g -fno-strict-aliasing -fsanitize=address,undefined -fno-omit-frame-pointer tests/build/save-baseline/tools/savetool/*.c -o tests/build/save-baseline/sst
SAROO_SAVETOOL_BINARY="$PWD/tests/build/save-baseline/sst" python3 -B -m unittest discover -s tests -p test_mems_resize.py -v
```

The baseline command is expected to fail the new resize assertions. Its
malformed-header control also exposes the old out-of-bounds access. Existing
same-size overwrite controls pass at that baseline. The corrected path passes
all nineteen new scenarios. Existing unsigned-field and browser/native
interchange checks also pass; the browser runtime sources are unchanged.

These are generated-file host checks on macOS. ASan/UBSan bounds and undefined
behavior checks remain enabled; leak reporting is disabled for the original
CLI's process-lifetime buffers. No user saves, physical SD card, Saturn or
firmware are accessed. Host checks do not establish power-loss durability or
Saturn game acceptance.
