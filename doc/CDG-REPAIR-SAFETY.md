# CD+G repair safety

The desktop `cdgfix` utility repairs 24-symbol CD+G graphics packets in
logical `.cdg` files and in SAROO's supported raw `.sub` layout. The safety
series adapts paps-g `fee3293f` to the original public entry point. The
four defects were found while reviewing the auxiliary documentation in
[upstream PR #352][pr352]. The original tpu source credits the Linux
Reed–Solomon library. The port keeps its decoder, encoder, field and
generator tables byte-identical. [Original v0.8 source][baseline]

## HOST-CDG-004 — commit a complete packet repair

The existing corrector applies P and Q to the live packet. If P succeeds
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

[pr352]: https://github.com/tpunix/SAROO/pull/352
[baseline]: https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/tools/cdgtools/cdgfix.c
