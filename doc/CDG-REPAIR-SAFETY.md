# CD+G repair safety

The desktop `cdgfix` utility repairs 24-symbol CD+G graphics packets in
logical `.cdg` files and in SAROO's supported raw `.sub` layout. The safety
series adapts paps-g `fee3293f` to the original public entry point. The
four defects were found while reviewing the auxiliary documentation in
[upstream PR #352][pr352]. The original tpu source credits the Linux
Reed–Solomon library. The port keeps its decoder, encoder, field and
generator tables byte-identical. [Original v0.8 source][baseline]

## HOST-CDG-004 — commit a complete packet repair

The original corrector applied P and Q to the live packet. If P succeeds
and Q fails, another corrected packet can cause the partly changed packet
to be saved. Q can also change the header protected by P after P already
corrected an error. That case skips the old P parity rebuild.

The public adaptation applies both layers to a packet copy, keeps a failed
packet intact, and rebuilds P whenever Q makes a correction. Parity
regeneration retains the upper two bits of every logical parity symbol.
The correction limits follow the original decoder. [Packet correction](../tools/cdgtools/cdgfix.c)

Run `make -C tests check-cdgfix`. Generated packets use independent
polynomial division in GF(64). Against public `fb8084d`, the three
targeted regressions fail and clean output plus all 24 single-symbol
corrections pass. The updated source passes all five cases under
ASan/UBSan and optimization. These tests verify decoded bytes in generated
files. Intended graphics, physical media and playback remain unqualified.

## HOST-CDG-003 — retain raw subchannel bits and process the final packet

The original SAROO layout stores 96-byte frames as eight 12-byte channel
blocks. Its 24-symbol packet interleave reaches offset 191. The old raw
converter cleared all R–W bits and reconstructed only the offsets it
processed. Its strict end condition also omitted a complete packet when
exactly 192 bytes remained. [Original raw converters][baseline]

The public port uses that original interleave and edits only the six R–W
bits at each corrected symbol. It checks the packet whose last symbol
lands at the end of the file. The P/Q subchannel bits and raw symbols
outside a successfully repaired packet retain their values. The
frame alignment check rejects a partial raw frame.

The independent fixture transposes all eight channels from generated
symbols and compares every output byte. The previous checkpoint fails
ten raw cases with two or more frames. The single-frame control
contains no complete packet and is preserved. The updated tool passes
all eleven sizes, a clean raw file and the logical packet controls.
The x86 and x64 MinGW builds pass and the former raw-converter size
warning is gone.

## HOST-CDG-001 and HOST-CDG-002 — check input and publish complete files

The original path buffers the whole file, narrows its extent to `int` and
truncates the input before writing. At public `898c8b9`, a forced two-byte
write to a generated 24-byte file reports success and loses the remaining
22 bytes. The same baseline accepts a damaged packet with an incomplete
trailing byte and changes its file. The port validates the command, file
type and whole packet or frame extent before repair. It uses 64-bit
positions and a default 6,336-byte scan window. The raw path carries up to
192 bytes between batches. On the first correction, an earlier unchanged
prefix is copied through an 8 KiB buffer. Clean files keep their original
bytes and file identity. [Original file handling][baseline]

The file is staged beside the input on the same volume. A short output write
is retried and a permanent write, read or close failure stops publication.
The tool rechecks the open input and its pathname before replacing it.
POSIX checks the staged file's sync and close, then renames it and checks
the containing directory's sync. macOS copies the original ACLs and extended
attributes. The other POSIX path preserves mode and ownership.
[POSIX write][write] · [POSIX rename][rename] · [POSIX fsync][fsync]

Windows keeps the MinGW build and drag-and-drop entry point. It stages in a
protected directory, flushes the completed file and passes a backup name to
the native metadata-preserving replacement API. If replacement fails after
moving the old file, the tool verifies the backup's file identity and restores
it. A failed restore keeps the recovery copy and reports its location.
[Windows ReplaceFileW return values and remarks][replacefile]

A symlink resolves to the file it names. Other hardlinks continue to reference
the prior file after replacement. The identity recheck detects observed edits.
The filesystem calls do not provide a conditional rename against another
writer's last-moment change. Power-loss behavior and native Windows filesystem
metadata remain unqualified. [File handling source](../tools/cdgtools/cdgfix_file.h)

### Build and use

```sh
make -C tools/cdgtools cdgfix
tools/cdgtools/cdgfix file.cdg
tools/cdgtools/cdgfix --format sub file.sub
```

Automatic format selection uses the case-insensitive `.cdg` or `.sub` suffix.
The original first-byte heuristic is retained for unknown suffixes.
`--format cdg|sub` selects the interpretation explicitly. The raw option
uses SAROO's channel-separated layout. Exit 0 means the scan completed,
1 means an input or filesystem error, and 2 means one or more uncorrectable
packets were retained. Successful repairs can still be saved with exit 2.
A failure after replacement is reported separately.

### Public verification

Run `make -C tests check-cdgfix`. The default POSIX build uses ASan/UBSan.
`SAROO_CDG_PROFILE=release` selects the optimized test build and
`SAROO_CDG_BATCH_FRAMES=1` exercises the smallest scan window. MinGW users
can run the two Python test files with `CC=gcc` in their test environment.
The syscall fault fixture runs on POSIX and the replacement rollback fixture
runs on Windows. Both build from the public source. Generated inputs verify
complete bytes after short writes, failed publication, concurrent edits,
clean scans, and late first repairs in logical and raw files.
[Test cases](../tests/test_cdgfix.py) · [Publication faults](../tests/test_cdgfix_io.py)

[pr352]: https://github.com/tpunix/SAROO/pull/352
[baseline]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/tools/cdgtools/cdgfix.c
[write]: https://pubs.opengroup.org/onlinepubs/9799919799/functions/write.html
[rename]: https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html
[fsync]: https://pubs.opengroup.org/onlinepubs/9799919799/functions/fsync.html
[replacefile]: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew#return-value
