# Unsigned save-tool field reads

The offline save-editor adaptation of
[Damián Parrino's PR #374](https://github.com/tpunix/SAROO/pull/374) exposed a
prerequisite in this fork's existing native save utility. A valid interchange
fixture reaches `get_be32` with a first byte of `0x80` or greater. Its `u8`
operand is promoted to signed `int`, and shifting it left 24 bits exceeds
`INT_MAX`. UBSan reproduces this on preceding public main `9429f53`; it is
not a malformed input or a browser parser failure.

The reader now widens bytes to `u32` before shifting. It reconstructs all 32
bits with defined unsigned arithmetic, preserving the existing big-endian
format and unaligned byte access. This changes one expression in the native
host utility. It does not change archive layout, validation policy, firmware
or the browser's runtime architecture.

SRO signatures, CRC fields and raw save metadata are project-specific; their
primary format references are the existing
[`main.c`](https://github.com/papsg/SAROO/blob/9429f53/tools/savetool/main.c)
and [`bup_format.h`](https://github.com/papsg/SAROO/blob/9429f53/tools/savetool/bup_format.h),
rather than a Sega command/timing requirement. The browser contribution is
credited to its proposer; this byte-reader correction and regression are our
adaptation prerequisite, not a change taken from the original PR.

Run from the repository root:

```sh
python3 -B -m unittest discover -s tests -p test_save_endian.py -v
```

The actual source helper passes 4,096 combinations spanning every high byte,
four lower-byte patterns and four alignments under ASan/UBSan. Restoring the
old signed expression fails on the high-bit case, while its 2,048 controls with the high byte below `0x80` still pass. A complete native CLI import/export also preserves a
valid high-bit CRC, an unsigned raw `0xffffffff` date word, the selected payload and
every unedited archive byte. These are generated host-format checks, not
physical Saturn save qualification.

The original CLI retains process-lifetime input buffers. Leak reporting is
disabled in its generated CLI scenarios; address and undefined-behavior checks
remain enabled.
