# Pinned STM32H750 device-header corrections

The App and BootLoader select `Keil.STM32H7xx_DFP.2.3.1` for STM32H750VBTx and
list `Firm_MCU/inc` among their user include directories. The GNU App build
places that directory before the pack's device include directory. These
corrections retain the exact ST header bodies and notices, with changes limited
to the documented definitions. [ST's BSD 3-Clause terms](../Firm_MCU/inc/LICENSE-ST-CMSIS)
and each source-file notice accompany the owned headers. The raw pack remains
unchanged. The relevant source hashes and selection limits are in the adjacent
provenance JSON files.

## SSR-138: build the device version from declared components

The [selected ST wrapper](../Firm_MCU/inc/stm32h7xx.h) declares version components
1, 9, 0, 0 at byte positions 31:24 through 7:0. The raw DFP 2.3.1 aggregate
instead names four unrelated, undefined identifiers. Expanding that public
macro in a target compiler fails. Supplying those aliases as 2, 3, 4, 5 makes
the old aggregate `0x02030405`, contrary to its declared `0x01090000` version.
The wrapper's own version declarations are the direct contract; [WG14 N1570,
§6.5.1 ¶2, printed p.78](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
also requires expression identifiers to have appropriate declarations.

This ports paps-g `10a51a75` / SSR-138 into the original public include layout.
Only wrapper lines 110–113 change, replacing the four undefined operands with
the four declared version components. The exact raw wrapper SHA-256 is
`71fa0474c33bb7a63b0f2222a65d7ecaa618140e8a15131c12c8eda158d77ed5`;
the owned SHA-256 is
`a26f9b5a4ca9b3e910c0abeae128130b05b446e1333b54b0f5902d8d39680140`.
The H750 peripheral header still resolves from the pinned SDK at this
checkpoint. No unrelated definitions, project sources or runtime logic change.
No original tpunix issue or PR was identified.

The public `stm32-check` fixture compiles the actual selected header with the
App's GNU flags and include order. The original contract fails on four
undeclared operands, while its component-only control passes. The original
alias-contamination control fails the independent aggregate assertion.
Selected contract, component-only and alias-contamination cases all compile.
Compiler dependency files confirm the owned wrapper and raw peripheral header;
the tool pins both raw source hashes. Use the [reproducible command](../tools/README.md#pinned-stm32-device-header-contracts-offline)
with a fresh output for each provider/mode.

Fresh standard 166-vector GNU App LTO and non-LTO builds pass with exact
parent ELF and flat-image hashes and unchanged load/RAM: respectively
59,632/63,960 loaded bytes, 5,904/1,576 free ITCM bytes and 50,904 static DTCM
bytes. The aggregate has no direct consumer in the selected application source.
The definition is an exported, conditional compile-time correction; it does
not establish a normal game defect. GNU dependency checks establish the owned
header selection; an MDK compile, optional profiles, physical MCU behavior and
Saturn/game qualification remain untested. No device was touched.
