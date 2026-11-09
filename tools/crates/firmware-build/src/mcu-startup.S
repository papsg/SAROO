/* GNU link-check CRT. Vector names and HardFault instructions are adapted from:
 * Copyright (c) 2018 STMicroelectronics. All rights reserved.
 * Author: MCD Application Team, startup_stm32h750xx.s.
 * The original ST component is licensed under the BSD 3-Clause license:
 * https://opensource.org/licenses/BSD-3-Clause
 * New verification glue is under this crate's MIT license. The original
 * startup and its notices remain in Firm_MCU/Startup. This is not an installed
 * bootloader, USB integration or added runtime diagnostic. */
.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb
.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.thumb_func
Reset_Handler:
    bl SystemInit
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:  cmp r1, r2
    bcs 2f
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
2:  ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
3:  cmp r1, r2
    bcs 4f
    str r3, [r1], #4
    b 3b
4:  bl __libc_init_array
    bl main
5:  b 5b

.section .text.Default_Handler,"ax",%progbits
.global Default_Handler
.thumb_func
Default_Handler:
    b .

/* Same instruction sequence as the original ST/SAROO HardFault handler. */
.section .text.HardFault_Handler,"ax",%progbits
.global HardFault_Handler
.thumb_func
HardFault_Handler:
    mrs r1, msp
    mov r0, lr
    and r0, #4
    cbz r0, 6f
    mrs r1, psp
6:  push {r4-r11}
    mov r0, sp
    ldr r2, =do_hardfault
    bx r2
    b .

.section .text._init,"ax",%progbits
.global _init
.thumb_func
_init:
    bx lr
.global _fini
.thumb_func
_fini:
    bx lr
