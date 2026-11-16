# Retained SD BOOT image admission (SSR-027)

The existing `BOOT`-only `load_app` read a file directly into address-zero ITCM
and entered its vectors even after a short/error read. Its file size was also
passed to the read request without checking the destination extent. This port
bounds the original FSIZE_t to10–65,536 bytes before narrowing, attempts exactly
one close per successful open, and requires successful read/close plus the
exact byte count before reading vectors or changing CPU state. Refusals use
the existing no-firmware indication.

The initial descending-stack pointer must be8-byte aligned within the selected
DTCM/AXI/D2/D3 RAM intervals. Lower bounds are exclusive, upper bounds inclusive.
The reset pointer must indicate Thumb and leave its first complete halfword
inside the received image after the two vector words. This is basic admission,
without instruction decoding, authenticity or complete handoff validation.
All edits stay inside existing BOOT scope; no loader or feature is added or
installed, and standard App does not define BOOT.

This adapts paps-g `52632dd7`/SSR-027; original firmware attribution remains,
with papsg's public integration/test adaptation identified. No original tpunix
issue/PR is identified for this independent finding. Primary local requirements:

- FatFs R0.15 **f_read**, **Parameters**, **Return Values**, **Description**: status
  and returned byte count describe successful transfer; a short successful read
  is possible. [Official f_read](https://elm-chan.org/fsw/ff/doc/read.html).
- STM32 **RM0433 Rev8 §2.4, p136** identifies64KiB ITCM at0 and the system RAM;
  **§2.3.2, Table7, pp130–131** gives the selected RAM address intervals.
  [ST reference manual](https://www.st.com/resource/en/reference_manual/dm00314099-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf#page=136).
- Arm **AAPCS32 2025Q1 §5.1, Table1**, Code pointer, gives the Thumb-state bit;
  **§6.2.1.2**, Stack constraints at a public interface, requires8-byte alignment.
  [Arm primary specification](https://github.com/ARM-software/abi-aa/blob/2025Q1/aapcs32/aapcs32.rst).

The actual source's address-zero image/vector convention supplies its load
contract. No Sega-specific rule applies to this MCU loader. Cached/local
evidence was read; no online research or private preparation/USB prerequisite
was imported.

## Reproduce offline

```sh
python3 -B -m unittest discover -s tests -p test_retained_boot_loader.py -v
SAROO_FIRMWARE_SAVE_PROFILE=release python3 -B -m unittest discover -s tests -p test_retained_boot_loader.py -v
```

The exact complete production function is extracted with explicit file/vector/
CPU seams. Files, payload writes, address-zero reads and entry are substitutes.
All four methods/22 child processes pass under sanitizers and optimization,
observing48 admission decisions. Controls include ordinary/minimum/maximum
images, eight valid stack boundaries and open failure. Refusals cover eight
size cases (including a64-bit original extent before narrowing), eight read/
close/count cases, eleven invalid stacks, eight invalid reset pointers and an
odd-sized image with only one entry byte remaining. Call counts, refusal
indication and absence of vector/CPU/entry effects on earlier failures are checked.
The actual parent fails17 child cases, while its complete/open/stack controls
pass. Original failures, copied source, commands and results are preserved.

Use the maintained offline builder with the same explicit SDK/compiler options:

```sh
firmware-build mcu --repo . --out tests/build/UNUSED \
  --toolchain ARM-GNU-ROOT --cmsis CMSIS-ROOT \
  --stm32 STM32-INCLUDE --rtx CMSIS-RTOS2 --boot-object
```

The optional second object compiles complete Main/main.c with BOOT=1 and no
LTO, separately from the unchanged standard App link. Its actual instruction
branches precede MSP/CONTROL changes. Source/object hashes, symbols and complete
disassembly are retained. `load_app` text grows140→276 bytes and its string
section121→147; compiler direct frame88→104. These are unlinked conditional
section/frame costs; no whole-BOOT size, padding, peak stack or execution claim
is inferred. The proprietary vendor loader's identity remains unknown.

Fresh LTO/non-LTO App/166-vector checks retain every loaded byte and all resources
from SSR-013:59,488/63,760 load bytes,6,048/1,776 ITCM free,200/204 data and
50,696/50,688 BSS, with50,896 static DTCM span and unchanged stack/address gap.
The conditional object and its stack report are excluded from App linking and
App-frame accounting; a leaked loader symbol would fail verification.
Menu/FPGA and existing firmware feature selections remain unchanged.

These checks do not establish real FatFs/processor reads, installed-loader
behavior, image authenticity, complete instructions, sufficient runtime stack,
RTOS/PSP/MSP/CONTROL, interrupt/cache handoff or returning-entry safety. They do
not qualify a complete safe bootloader, physical SD/Saturn or games. No hardware
operation, firmware installation, new runtime task or diagnostic feature occurred.
