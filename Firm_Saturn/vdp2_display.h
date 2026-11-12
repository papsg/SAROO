#ifndef SAROO_VDP2_DISPLAY_H
#define SAROO_VDP2_DISPLAY_H
/* ST-058 2.4/2.5 and Bulletin37 items3/6: DISP=0 forces VBLANK=1;
 * reading EXTEN with EXLTEN=0 latches the unshifted ten-bit counter.
 * This owner uses only normal 320x224/240 noninterlace, external sync off.
 * Qualify the off-display phase mapping before physical acceptance. */
#ifndef SAROO_VDP2_READ16
#define SAROO_VDP2_READ16(at) (*(volatile unsigned short *)(at))
#endif
#ifndef SAROO_VDP2_WRITE16
#define SAROO_VDP2_WRITE16(at,value) (*(volatile unsigned short *)(at)=(value))
#endif
#ifndef SAROO_VDP2_IRQ_LOCK
static inline unsigned saroo_vdp2_irq_lock(void)
{
    unsigned sr;
    __asm__ volatile("stc sr,%0" : "=r"(sr) :: "memory");
    __asm__ volatile("ldc %0,sr" :: "r"(sr|0xf0u) : "memory","t");
    return sr;
}
static inline void saroo_vdp2_irq_unlock(unsigned sr)
{ __asm__ volatile("ldc %0,sr" :: "r"(sr) : "memory","t"); }
#define SAROO_VDP2_IRQ_LOCK() saroo_vdp2_irq_lock()
#define SAROO_VDP2_IRQ_UNLOCK(sr) saroo_vdp2_irq_unlock(sr)
#endif

/* Finite sample budget, not a wall-clock timeout. No IRQ/vector/timer owner is
 * acquired. A failed attempt leaves DISP off; menu polling may retry. */
static int saroo_vdp2_display_enable(unsigned off_mode)
{
    if(off_mode!=0 && off_mode!=0x10u)return 0;
    unsigned on_mode=off_mode|0x8000u;
    if(SAROO_VDP2_READ16(0x25f80000u)==on_mode)return 1;
    unsigned height=off_mode ? 240u : 224u;
    unsigned previous=0x400u,tail=0,settled=0;
    for(unsigned remaining=65536;remaining;--remaining) {
        if(SAROO_VDP2_READ16(0x25f80000u)!=off_mode)return 0;
        unsigned exten=SAROO_VDP2_READ16(0x25f80002u);
        if(exten&0x0300u)return 0;
        unsigned line=SAROO_VDP2_READ16(0x25f8000au)&0x03ffu;
        /* Observe a new frame after the mode request. This avoids admitting
         * against the old vertical resolution or an initial stale sample. */
        if(line>=256u)tail=1;
        else if(tail && line<32u)settled=1;
        if(settled && previous+1u==line && line==height+2u) {
            unsigned sr=SAROO_VDP2_IRQ_LOCK();
            int enabled=0;
            if(SAROO_VDP2_READ16(0x25f80000u)==off_mode &&
               !(SAROO_VDP2_READ16(0x25f80002u)&0x0300u)) {
                unsigned fresh=SAROO_VDP2_READ16(0x25f8000au)&0x03ffu;
                /* Interior of the first blanking lines; never the counter's
                 * later nonlinear/rollover range. Recheck after IRQ masking. */
                if(fresh>=height+2u && fresh<=height+3u) {
                    SAROO_VDP2_WRITE16(0x25f80000u,on_mode);
                    enabled=1;
                }
            }
            SAROO_VDP2_IRQ_UNLOCK(sr);
            if(enabled)return 1;
        }
        previous=line;
    }
    return 0;
}
#endif
