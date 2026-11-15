! Native replacement of the existing DLCT v20240517 AIP at 0x06002f00.
! Adapted by papsg from the original tpunix/yzb Vandal Hearts workaround.
! Link at 0x06002f00; only the first 122 text bytes replace game source.
! The final alignment padding is excluded, preserving the next function.
! Preserve the native read/filter/BIOS decoder, data adjustment and VDP calls.
! The packed-source NOP is separate; this routine never patches decoded code.
.section .text
.global _start
_start:
 add #-16,r15
.Lwait:
 bsr native_status
 mov #0,r4
 tst #0x40,r0
 bt .Lwait
 mov.l .Ldest,r12
 bsr native_filter
 mov.l r12,@r15
 mov #0x40,r6
 shll8 r6
 shll r6
 add r15,r6
 add #16,r6
 mov.l .Lbytes,r5
 bsr native_read
 mov #0,r4
 mov #18,r4
 shll8 r4
 neg r4,r4
 add r13,r4
 mov.w .Lbios,r2
 mov.l @r2,r8
 mov.l @r4,r6
 shlr r6
 add #2,r4
 jsr @r8
 mov r15,r5
 mov.w .Lbias,r9
 mov.w .Lcorrection,r10
 add r12,r10
 mov.l @r10,r3
 sub r9,r3
 mov.l r3,@r10
 add #16,r15
 mov.l .Lclock,r1
 mov.l @r1,r1
 jsr @r1
 mov #1,r4
 bsr native_video_init
 nop
 mov #1,r3
 mov.l .Lshadow,r0
 mov.w .Ltvmd,r1
 mov.l @r0,r2
 mov.w r1,@r2
 bsr native_video_apply
 mov.l r3,@(8,r0)
 jmp @r12
 nop
.Lbios: .word 0x05f8
.Lbias: .word 0x564a
.Lcorrection: .word 0x01b0
.balign 4
.Ldest: .long 0x06040000
.Lbytes: .long 0x0002f800
.Lclock: .long 0x06000320
.Lshadow: .long 0x06002d7c
.Ltvmd: .word 0x8001
.set native_status,_start+0x3ca
.set native_filter,_start+0x208
.set native_read,_start+0x10a
.set native_video_init,_start-0x96c
.set native_video_apply,_start-0xc1c
