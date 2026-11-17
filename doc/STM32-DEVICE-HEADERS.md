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

## SSR-139: encode the third SDRAM command-mode bit as bit 2

[ST RM0433 Rev. 8, §22.9.5, printed pp.873–874](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
defines `FMC_SDCMR.MODE` at bits 2:0. The legal command values include load-mode
register `100`, self-refresh `101` and power-down `110`; value `111` is reserved.
Bank 1 is independently selected at bit 4. The exact DFP 2.3.1
[`stm32h750xx.h`](../Firm_MCU/inc/stm32h750xx.h) instead defined its individual
`MODE_2` helper as `0x3`, overlapping bits 0 and 1. A bank-1 load-mode word
thus became `0x13` rather than `0x14`. The locally preserved ST CMSIS-H7
V1.10.4 release notes (15 March 2024) independently identify `0x4` as the
correction. This is a header-encoding defect; no selected firmware caller of
`FMC_SDCMR` was found.

This ports paps-g `2789ea4e` / SSR-139. The owned H750 header is the exact
2.3.1 raw file except line 11545, which changes the helper and its numeric
comment to `0x4`. The raw SHA-256 is
`bf0d085e5063e8da5000dea16acdc76c212aec382d1685ae9263113da5ffa8a2`;
the selected SHA-256 is
`9b628779956b51e260bf9544ccfae05eb472dd10f83fe92db93b9ba1a7bc6afe`.
The wrapper's own `SSR-138` bytes are unchanged. GNU dependencies confirm
both owned headers are selected; the raw DFP remains available as a control.
The full ST source notice and [license](../Firm_MCU/inc/LICENSE-ST-CMSIS) are
retained. No original tpunix issue or PR was identified.

With the public SSR-138 wrapper and raw peripheral header, the target compiler
fails nine independent `SSR-139` assertions: bit position/disjointness and
the upper legal command/bank words. All lower-mode, target-bank and
refresh-count controls pass. The owned header passes the same contract and
controls; the exact raw-provider reversal still fails only the contract.
The fixture checks value `7` as algebra only, without treating that reserved
encoding as an operational command. The adjacent SSR-138 version, component
and alias checks all pass after the H750 header is owned. Use
[`stm32-check --case sdram`](../tools/README.md#pinned-stm32-device-header-contracts-offline)
with each provider/mode and a fresh ignored output.

Fresh standard 166-vector GNU App LTO and non-LTO images are byte-identical
to SSR-138 at 59,632/63,960 loaded bytes and 50,904 static DTCM bytes. The
large owned header affects public definitions, not currently linked code.
These checks do not issue an SDRAM command, prove a populated SDRAM bank,
resolve the separate mode-register-width question, or qualify MDK, physical
timing, Saturn hardware or games. No device was touched.

## SSR-140: retain all five QSPI FIFO-threshold bits

[ST RM0433 Rev. 8, §23.5.1, printed pp.895–896](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
defines `QUADSPI_CR.FTHRES` at bits 12:8. Values 0–31 represent threshold
levels 1–32 in indirect mode. The pinned DFP 2.3.1 H750 header instead masks
only bits 11:8 and provides helpers only for bits 0–3. Threshold 16 therefore
extracts as zero; replacing threshold 16 with 7 leaves the old bit 12 set.
The locally preserved ST CMSIS-H7 V1.10.3 release notes (23 September 2022)
also identify the field-width correction. No direct current application
consumer of `FTHRES` or `QUADSPI_CR` was found.

This ports paps-g `d032da23` / SSR-140 on top of SSR-139. The owned header
widens only the threshold mask/comment to bits 4:0 and adds the fifth helper
at `0x1000`; the earlier SDRAM correction and all other raw definitions remain
unchanged. The selected H750 SHA-256 is
`757f408fe5808650e7b85f14f61b1265e4b917396064eb2fecae671b9b122022`.
The exact raw 2.3.1 hash, selection and full BSD notice remain in the
[provenance record](../Firm_MCU/inc/stm32h750xx-provenance.json). No original
tpunix issue or PR was identified.

The raw selected header fails 37 compiler diagnostics: the five-bit mask and
helper are wrong/missing, supported high values are truncated, and replacing a
high threshold retains its stale bit. The low 0/1/7/15 values and neighboring
field controls pass. The owned header passes all 32 threshold insertions and
extractions, the high-to-low replacement, unrelated-bit preservation and the
same valid controls. The raw-provider reversal still fails the contract while
its controls pass. Use [`stm32-check --case qspi`](../tools/README.md#pinned-stm32-device-header-contracts-offline)
with each provider/mode and fresh ignored outputs. These are compiler
expressions, not QSPI transactions.

Both standard GNU App images remain byte-identical to SSR-139 at 59,632/63,960
loaded bytes and 50,904 static DTCM bytes, with 166 vectors each. The prior
version and SDRAM contracts pass against the selected header. No FIFO timing,
interrupt signaling, target execution, MDK compile, physical MCU or Saturn/game
qualification is claimed. No device was touched.

## SSR-141: separate timer compare values from channel grouping

[ST RM0433 Rev. 8, §38.4.24, printed pp.1615–1617](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
defines TIM1/TIM8 `CCR5` as a 16-bit compare value at bits 15:0, with
independent grouping controls `GC5C1/2/3` at bits 29–31. The raw DFP 2.3.1
header masked all 32 bits as the compare value. Thus reading a compare value
with grouping set includes grouping bits; a masked value-only update clears
those controls. This happens with legal register values and requires no
reserved-bit assumption. The locally preserved ST CMSIS-H7 V1.10.7 release
notes (4 February 2026) also identify a CCR5 definition correction. No direct
`CCR5` consumer was found in selected application source.

This ports paps-g `a126b1b8` / SSR-141 on top of SSR-140. Only the compare
mask/comment line changes to `0x0000ffff`; the register remains 32 bits,
grouping masks and adjacent CCR6 remain intact. The selected H750 SHA-256 is
`2735a958d50ba09181e77c6e7b3d02270d200d00fe6b1782d4389a9f123ab242`.
The exact raw source and ST BSD terms remain recorded in
[provenance](../Firm_MCU/inc/stm32h750xx-provenance.json). No original tpunix
issue or PR was identified.

The raw target compiler fails 58 assertions: mask/disjointness plus value reads
and updates across seven nonzero legal grouping combinations and four compare
values. The zero-group combinations, group helper bits, adjacent CCR6 and
register offsets pass as valid controls. The owned header passes all eight
group combinations and four values, including updates that retain grouping;
the raw-provider reversal still fails the contract and passes controls. Use
[`stm32-check --case ccr5`](../tools/README.md#pinned-stm32-device-header-contracts-offline)
with each provider/mode and fresh ignored outputs. These are compiler
expressions without timer execution.

Both standard GNU App images remain byte-identical to SSR-140 at 59,632/63,960
loaded bytes and 50,904 static DTCM bytes, with 166 vectors each. The prior
version, SDRAM and QSPI contracts pass against the selected header. Timer
preload, update-event and output behavior, MDK compilation, physical MCU and
Saturn/game qualification remain untested. No device was touched.

## SSR-142: isolate the channel-one DAC busy flag

[ST RM0433 Rev. 8, §§26.7.14 and 26.7.17, printed pp.1073–1074 and
1077](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
assigns `DAC_SR.BWST1` to bit 15 and the independent channel-two DMA underrun
flag to bit 29. The latter is cleared by writing one; the busy flag is cleared
by hardware after sample-time synchronization. A channel-one sample-time
write during operation depends on its own busy flag. The raw DFP 2.3.1 H750
header instead defines the busy mask as `0x20008000`, which includes both
bits. With only channel-two underrun set, a channel-one busy test is therefore
falsely true. This is a header-field defect; the locally preserved later ST
header retains it, and no direct current application DAC-status caller was
found.

This ports paps-g `688c5c06` / SSR-142 on top of SSR-141. Only the `BWST1`
mask/comment line changes to `0x00008000`; the alias follows that mask.
Channel-two underrun, all other status flags, register storage/layout and the
earlier header corrections remain intact. The final selected H750 SHA-256 is
`2866b495da1aefeac91bf8ebffd045f1fb07185f59b0b6025090161b5ee7ba1e`.
The pinned raw source, complete ST notice and BSD license remain in
[provenance](../Firm_MCU/inc/stm32h750xx-provenance.json). No original tpunix
issue or PR was identified.

The raw selected header fails 50 compiler assertions: mask/disjointness, 16
false busy predicates and 32 wrong busy extractions across all 64 combinations
of the six documented status bits. Cases without channel-two underrun,
neighboring flags and the SR/SHSR offsets pass. The owned header passes all
64 combinations and valid controls; the raw-provider reversal still fails
the contract. Use [`stm32-check --case dac`](../tools/README.md#pinned-stm32-device-header-contracts-offline)
with each provider/mode and fresh ignored outputs. The fixture reads
constant status words only; it never models a status-register write.

Both standard GNU App images remain byte-identical to SSR-141 at 59,632/63,960
loaded bytes and 50,904 static DTCM bytes, with 166 vectors each. All four
earlier version/SDRAM/QSPI/timer contracts pass against the final selected
headers. No DAC transaction, sample-time synchronization, analog output,
MDK compile, physical MCU or Saturn/game qualification is claimed. No device
was touched.

## SSR-143: put Ethernet receive states in the receive field

[ST RM0433 Rev. 8, Ethernet DMA debug status register `ETH_DMADSR`, printed
pp. 2924–2925](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
places receive process state in bits 11:8 and transmit process state in bits
15:12. The selected DFP 2.3.1 H750 header already gives the aggregate receive
field the correct position and mask, but its six nonzero named receive states
all select the transmit field. For example, receive-fetching status `0x100`
cannot match the header's `ETH_DMADSR_RPS_FETCHING` value `0x1000`.

This directly ports paps-g `db250f11` / SSR-143 after SSR-142. The six named
receive positions and their numeric comments move down four bits; the factors,
aliases, aggregate fields, transmit definitions, stopped state and register
layout remain unchanged. The selected H750 SHA-256 becomes
`b2177497393186402d3f5c63aeefeb67359581ace668f0b70f7fcbfe54cfb197`.
The pinned raw source, complete ST notice and BSD license remain recorded in
[provenance](../Firm_MCU/inc/stm32h750xx-provenance.json). No original tpunix
issue or PR was identified.

The public target-compiler contract reproduces 84 failing named receive
comparisons on the public parent, across seven receive states, seven transmit
states and both bit-zero values. Its field-extraction, transmit-name and layout
controls compile. After correction, the selected header passes both control
and contract; the pinned raw provider still fails the same 84 comparisons and
passes its controls. Run [`stm32-check --case ethernet`](../tools/README.md#pinned-stm32-device-header-contracts-offline)
with selected/original providers and contract/control modes in fresh ignored
output directories. Both standard 166-vector GNU App builds produce images and
ELFs byte-identical to SSR-142: 59,632/63,960 loaded bytes for LTO/non-LTO and
50,904 static DTCM bytes. No direct `ETH_DMADSR` application consumer was found.
These compile expressions do not execute the Ethernet peripheral or establish
an existing SAROO network/game failure, MDK build result or physical behavior.

## SSR-144: preserve DMA2D packed color components

[ST RM0433 Rev. 8, §§18.5.16–18.5.17, printed pp. 742–743](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
defines the RGB565 output red/green fields as bits 15:11/10:5 and the ARGB1555
red/green fields as bits 14:10/9:5. The selected DFP 2.3.1 H750 header shifts
the values `0xf8`, `0x7e`, `0x7c` and `0x3e` by the correct starting positions
to make masks. Those values are too wide once shifted: the resulting masks
include neighboring or reserved bits and omit part of the intended component.

This directly ports paps-g `4cfad282` / SSR-144 after SSR-143. Four mask factors
become `0x1f`, `0x3f`, `0x1f` and `0x1f` for RGB565 red/green and ARGB1555
red/green, respectively. Positions, names, neighboring components, other color
formats and register layout remain unchanged. The selected H750 SHA-256 is
`87691b861be57fe4542c92ea75a6ca3bee93997064c71cd8f2a045bf3dc65edd`.
The pinned raw source, complete ST notice and BSD license remain recorded in
[provenance](../Firm_MCU/inc/stm32h750xx-provenance.json). No original tpunix
issue or PR was identified.

The public target-compiler contract fails each of the four mask widths on the
public parent and pinned raw provider, plus disjointness, full-scale extraction
and neighbor-preserving replacement; unaffected component/format and layout
controls pass. After correction, the selected provider passes control and
contract while the raw provider still fails the seven contract assertions.
Run [`stm32-check --case dma2d`](../tools/README.md#pinned-stm32-device-header-contracts-offline)
in fresh ignored output directories. All six earlier selected header contracts
pass. Both standard 166-vector GNU App images and ELFs match SSR-143 byte for
byte: 59,632/63,960 loaded bytes for LTO/non-LTO and 50,904 static DTCM bytes.
No direct DMA2D output-color consumer was found. Compiler expressions cannot
qualify transfer timing, CLUT access rules, rendered pixels, MDK compilation or
physical Saturn/game behavior.

## SSR-145: address EXTI D3 pending line 25

[ST RM0433 Rev. 8, §20.6.4, printed pp. 773–775](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
places line 25's D3 pending mask at bit 25 of `EXTI_D3PMR1`; bits 24:22 are
reserved. The selected DFP 2.3.1 H750 header instead gives its named line-25
mask position 24. Constructing an enable word therefore sets a reserved bit,
and clearing an existing line-25 bit with that mask leaves it enabled. The
clear-source selector in `EXTI_D3PCR1H` is a different field.

This directly ports paps-g `3dc1e892` / SSR-145 after SSR-144. Only the
line-25 position and numeric mask comment change; its factor, alias, other
lines, clear-source selector and register layout remain unchanged. The selected
H750 SHA-256 is
`c9906fd910d7b7a3be63f4b25b9b8722f4a13f9a5259a8d560e841e3986ebffe`.
The pinned raw source, complete ST notice and BSD license remain recorded in
[provenance](../Firm_MCU/inc/stm32h750xx-provenance.json). No original tpunix
issue or PR was identified.

The public parent and pinned raw provider fail five target-compiler assertions
for position, mask, reserved-bit exclusion, enable/clear and preservation of
other supported lines. Neighboring lines, the separate clear-source selector
and register offsets pass as valid controls. Corrected selected and raw
controls pass; selected contract passes while raw contract retains the five
failures. Run [`stm32-check --case exti`](../tools/README.md#pinned-stm32-device-header-contracts-offline)
in fresh ignored output directories. All seven earlier selected header
contracts pass. Both standard 166-vector GNU App images and ELFs match SSR-144
byte for byte: 59,632/63,960 loaded bytes for LTO/non-LTO and 50,904 static
DTCM bytes. No direct `EXTI_D3PMR1` application consumer was found. Word
construction does not qualify an actual pending request, D3 power routing,
MDK compilation or physical Saturn/game behavior.
