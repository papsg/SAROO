# Bounded menu text prerequisite

Run `make -C tests check-menu-text` with a C compiler supporting AddressSanitizer
and UndefinedBehaviorSanitizer, plus Python 3. The tests use no Saturn I/O.

This is our prerequisite correction for the reviewed language additions in
[original #256](https://github.com/tpunix/SAROO/pull/256),
[#299](https://github.com/tpunix/SAROO/pull/299),
[#324](https://github.com/tpunix/SAROO/pull/324),
[#360](https://github.com/tpunix/SAROO/pull/360) and
[#107](https://github.com/tpunix/SAROO/pull/107). It is not attributed to those
translation authors. The existing v0.8 Greek game error already needs 56 UTF-8
bytes including NUL for ordinary error code 1, beyond its 40-byte destination.
The old `sprintf` macro tells the formatter that destination has 1,024 bytes.

The fixture includes the actual formatter and language source. Python extracts
the unchanged hardware-free `menu_error` function and the first actual game-error
call from the production source; a status-display stub observes their output.
The fixture also recognizes the original three-line error call, allowing the
same regression to reproduce its stack overflow on the original source. It
does not execute the hardware-dependent menu event handler.

Checks cover 12,880 length/capacity pairs, guard bytes, termination, custom
return values, invalid capacities, signed extrema, independent libc numeric
oracles, sign/alignment, huge widths, CRLF and console callback behavior. The
translation cases require complete game/file/disc errors for all registered
languages with six error values, plus both 64-byte selector-title formats at
0, 1, 214 and `INT_MAX` pages. Normal labels remain complete; extreme values
must match libc's bounded byte-prefix result. These are test values, not a claim
that the original MCU enforces a particular catalog-size bound.

The firmware formatter retains its custom return convention: stored bytes
**including NUL**, or zero for invalid arguments. Its existing unsupported
precision/modifier, skipped float and 32-bit word conventions remain; this is
not a complete libc `printf` implementation. `menu_error` keeps its temporary
scratch on the synchronous error path. The six formatted error paths and two
selector-title paths pass real destination sizes; the global legacy `sprintf`
macro is unchanged for other callers whose destinations have different owners.
Only `printk.c` is compiled with `-fno-builtin`, so the compiler preserves the
custom formatter's semantics and null checks instead of assuming a libc API.

These checks establish source/host behavior. They do not qualify Saturn drawing,
physical timing, game playback, every remaining formatting caller or editorial
translation quality. The available cross-compiler can compile the changed
SH-2 objects; its missing original C library remains a complete-link limitation.
