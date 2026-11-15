ENTRY(Reset_Handler)
MEMORY { CODE(rx) : ORIGIN = 0x00000000, LENGTH = 4M
         RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 4M }
SECTIONS {
 .vectors : { KEEP(*(.vectors)) } >CODE
 .text : { *(.text*) *(.rodata*) } >CODE
 .ARM.exidx : { *(.ARM.exidx*) } >CODE
 .data : { _sdata = .; *(.data*) _edata = .; } >RAM AT>CODE
 _sidata = LOADADDR(.data);
 .bss (NOLOAD) : { . = ALIGN(8); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; } >RAM
}
