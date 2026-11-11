# Save ownership and expansion RAM follow-up

## Compact the configuration parser

The loader derives the 12 setting names, their byte offsets and lengths
from one definition. Each MCU table entry falls from 12 to 8 bytes. A
compile-time check bounds the name pool, and the parser uses the stored
lengths. Numeric parsing keeps the carry in a wide temporary before
checking the 32-bit limit. The linked Arm digit loop uses UMLAL in
place of UDIV. Setting order, case, accepted values, error handling and
the existing two-pass validation and publication are preserved.
[Existing configuration contract](CONFIGURATION-SAFETY.md) ·
[Shipped configuration](saroocfg.txt) ·
[Preceding public parser](https://github.com/papsg/SAROO/blob/f04106d7ed9dc25c800df97728ba8f6c3cf0e8a9/Firm_MCU/Saturn/saturn_utils.c#L216-L269).

Adapted from paps-g's `3ec942a4`. The preceding code and the port both
pass all 20 configuration methods, including 328 loader cases. The
added controls cover case-sensitive names, repeated assignments,
leading zeroes and 32-bit overflow. Run `make -C tests check-config`
and repeat with `SAROO_CONFIG_PROFILE=release`. The port passes
ASan/UBSan and O2. To test the preceding sources, set
`SAROO_CONFIG_LOADER_DIR` and `SAROO_CONFIG_MATCH_SOURCE` as documented
in the respective test modules.

The public GNU App load drops from 62,812 to 62,752 bytes. Static DTCM
stays at 60,216 bytes. This comparison measures linked bytes and target
instructions. Configuration load latency on hardware remains unmeasured.
