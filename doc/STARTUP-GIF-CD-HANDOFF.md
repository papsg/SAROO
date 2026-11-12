# Menu startup, GIF display, and CD handoff

## Validate the startup language ID

Adapted from paps-g `1ad9fb40` (SATURN-MENU-010). The SH-2 menu
reads the 32-bit language ID from startup metadata. It checks both
ends of the signed range before indexing the language table. Values
outside the table select the existing default, ID 0. The 19 valid
selections and language cycling retain their behavior.

The upstream report records invalid startup metadata. Its firmware
predates the language table indexing in this fix, so the original
report's cause remains open.
[Upstream issue #9](https://github.com/tpunix/SAROO/issues/9),
[v0.8 menu language source](https://github.com/tpunix/SAROO/blob/ba387b083bb9d2a001ae6a22a24d6e6785e515bd/Firm_Saturn/language.c).

```sh
make -C tests check-language
make -C tests check-language BUILD_DIR=build/language-release \
  CFLAGS='-std=c99 -O2 -g -Wall -Wextra -Werror' SANITIZERS=
```

Run this test against the parent `Firm_Saturn/language.c` to reproduce
the out-of-bounds access for `INT_MIN`. The parent's 19-language
lookup and cycling controls pass. The port passes 65,540 signed ID
cases and all 2,280 existing lookups in both profiles. The linked
menu contains 81,380 code/data bytes and 24,064 bytes of body space.
Malformed startup metadata has been exercised on the host.
