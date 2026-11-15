    ! Adapted from paps-g e2029f04; upstream music service 8928b7da.
    ! Link at 0x0600A3B8, replacing the US first program's original loader.
    ! Its only direct pointer literal is 0x06007324. Redirect 0x060044E0 to copy_ready.
    ! Preserve SAROO's complete-read guard and fixed music buffer/copy extent.
    ! 0x02000F04 is the existing sector-reader pointer slot, not executable code.
    ! The flag uses P2 so the two hooks do not depend on a cached data alias.
    .section .text
    .global preload, copy_ready, ready
preload:
    sts.l pr, @-r15
    mov.l ready_address, r1
    mov #0, r0
    mov.b r0, @r1
    mov.l reader_slot, r0
    mov.l @r0, r0
    mov.l music_fad, r4
    mov.l music_size, r5
    mov.l music_buffer, r6
    jsr @r0
    nop
    mov.l music_size, r1
    cmp/eq r1, r0
    movt r0
    mov.l ready_address, r1
    mov.b r0, @r1
    lds.l @r15+, pr
    rts
    nop
    .align 2
ready_address:
    .long ready + 0x20000000
reader_slot:
    .long 0x02000f04
music_fad:
    .long 2612
music_size:
    .long 0x54f00
music_buffer:
    .long 0x22400000
copy_ready:
    mov.l copy_flag, r1
    mov.b @r1, r0
    tst r0, r0
    bt copy_done
    mov.l copy_source, r1
    mov.l copy_destination, r2
    mov.l copy_words, r3
copy_loop:
    mov.l @r1+, r0
    mov.l r0, @r2
    add #4, r2
    dt r3
    bf copy_loop
copy_done:
    rts
    nop
    .align 2
copy_flag:
    .long ready + 0x20000000
copy_source:
    .long 0x22400000
copy_destination:
    .long 0x002aa000
copy_words:
    .long 0x54f00 / 4
ready:
    .byte 0
    .align 2
