/* Compile/link qualification only. Original App limits: 64KiB ITCM, 128KiB DTCM.
 * No persistent/device address is programmed by this build. */
ENTRY(Reset_Handler)
MEMORY {
  ITCM (rx) : ORIGIN = 0, LENGTH = 64K
  DTCM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
SECTIONS {
  .isr_vector : { KEEP(*(.isr_vector)) } > ITCM
  .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) } > ITCM
  .ARM.extab : { *(.ARM.extab*) } > ITCM
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > ITCM
  .preinit_array : {
    __preinit_array_start = .; KEEP(*(.preinit_array*)) __preinit_array_end = .;
  } > ITCM
  .init_array : {
    __init_array_start = .; KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) __init_array_end = .;
  } > ITCM
  .fini_array : {
    __fini_array_start = .; KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array)) __fini_array_end = .;
  } > ITCM
  . = ALIGN(4);
  _etext = .;
  .data : {
    . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .;
  } > DTCM AT > ITCM
  _sidata = LOADADDR(.data);
  _image_end = LOADADDR(.data) + SIZEOF(.data);
  .bss (NOLOAD) : {
    . = ALIGN(8); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .;
  } > DTCM
  _end = .; end = .;
  _stack_top = ORIGIN(DTCM) + LENGTH(DTCM);
  __initial_sp = _stack_top;
  ASSERT(_image_end <= ORIGIN(ITCM) + LENGTH(ITCM), "App exceeds 64KiB ITCM")
  ASSERT(_ebss + 1024 + 512 <= _stack_top, "No original minimum stack/heap gap")
}
