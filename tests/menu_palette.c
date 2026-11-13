/* Adapted from paps-g 9d0cf199; actual public C with host video storage. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* Keep register widths and offsets from the vendor header. */
#include "vdp2.h"
static struct { u32 before, data[0x80000 / 4], after; } vram;
static struct { u32 before, data[1024], after; } cram;
static u32 registers[0x120 / 4];
static u16 vdp1_ram[2];
#undef VDP1_REG_PTMR
#undef VDP1_REG_FBCR
#undef VDP1_REG_TVMR
#undef VDP1_REG_EWDR
#undef VDP1_REG_EWLR
#undef VDP1_REG_EWRR
#undef VDP1_REG_ENDR
#undef VDP1_REG_EDSR
#undef VDP1_RAM
static volatile u16 VDP1_REG_PTMR, VDP1_REG_FBCR, VDP1_REG_TVMR;
static volatile u16 VDP1_REG_EWDR, VDP1_REG_EWLR, VDP1_REG_EWRR;
#undef VDP2_VRAM
#undef VDP2_CRAM
#undef VDP2_REGISTER_BASE
#define VDP2_VRAM ((uintptr_t)vram.data)
#define VDP2_CRAM ((uintptr_t)cram.data)
#define VDP2_REGISTER_BASE ((uintptr_t)registers)
static int check_vdp1_stop;
static unsigned end_writes, settle_reads;
static volatile u16 end_register, draw_status;

static volatile u16 *force_end_register(void)
{
    assert(VDP1_REG_PTMR == 0);
    assert(!end_writes && !settle_reads && vdp1_ram[0] == 0x1234);
    ++end_writes;
    return &end_register;
}

static u16 read_draw_status(void)
{
    assert(end_writes == 1 && end_register == 0 && vdp1_ram[0] == 0x1234);
    ++settle_reads;
    return draw_status;
}

static u16 *command_ram(void)
{
    if (check_vdp1_stop)
        assert(end_writes == 1 && settle_reads == 64);
    return vdp1_ram;
}

#define VDP1_REG_ENDR (*force_end_register())
#define VDP1_REG_EDSR read_draw_status()
#define VDP1_RAM ((uintptr_t)command_ram())

static unsigned scanline, samples, enable_writes, irq_locks, status_register;
static unsigned fbtest_calls;
static u16 latched, mode_during_fbtest;
enum raster_fault { RASTER_NORMAL, RASTER_STALLED, RASTER_SKIP, RASTER_SHIFT };
static enum raster_fault raster_fault;
static int llen, pos_x, pos_y, text_color;
static void (*printk_putc)(int);

static void conio_putc(int ch) { (void)ch; }

void fbtest(void)
{
    mode_during_fbtest = TVMD;
    ++fbtest_calls;
}

#ifdef TEST_DISPLAY_COUNTER
static unsigned read_display(unsigned address)
{
    switch (address) {
    case 0x25f80000: return TVMD;
    case 0x25f80002:
        if (!(EXTEN & 0x0200))
            latched = scanline >= 245 ? scanline + 249 : scanline;
        return EXTEN;
    case 0x25f8000a:
        assert(++samples <= 2 * 65536);
        if (raster_fault != RASTER_STALLED)
            scanline = (scanline + (raster_fault == RASTER_SKIP && scanline == 241 ? 2 : 1)) % 263;
        return latched;
    default:
        assert(0);
        return 0;
    }
}

static void enable_display(unsigned address, unsigned value)
{
    assert(address == 0x25f80000 && TVMD == 0x0010 && value == 0x8010);
    assert(scanline >= 240 && scanline < 262 && (status_register & 0xf0) == 0xf0);
    assert(latched == 242 || latched == 243);
    TVMD = value;
    ++enable_writes;
}

static unsigned lock_display(void)
{
    unsigned saved = status_register;
    assert(saved == 0x371);
    status_register |= 0xf0;
    ++irq_locks;
    if (raster_fault == RASTER_SHIFT) scanline = 0;
    return saved;
}

static void unlock_display(unsigned saved)
{
    assert(status_register == (saved | 0xf0));
    status_register = saved;
}

#define SAROO_VDP2_READ16(address) read_display(address)
#define SAROO_VDP2_WRITE16(address,value) enable_display(address,value)
#define SAROO_VDP2_IRQ_LOCK() lock_display()
#define SAROO_VDP2_IRQ_UNLOCK(saved) unlock_display(saved)
#include "vdp2_display.h"
#endif

#include "slogo.h"
#include "palette.inc"

static const u32 expected_text[16] = {
    0x000000, 0xaa0000, 0x00aa00, 0xaaaa00,
    0x0000aa, 0xaa00aa, 0x0055aa, 0xaaaaaa,
    0x555555, 0xff5555, 0x55ff55, 0xffff55,
    0x5555ff, 0xff55ff, 0x55ffff, 0xffffff,
};

static u32 initial_color(unsigned i) { return 0xa5800000u + i; }

static void seed(int logo)
{
    memset(vram.data, 0xa5, sizeof(vram.data));
    memset(registers, 0xa5, sizeof(registers));
    TVSTAT = VBLANK; /* The flag is forced high while display is off. */
    EXTEN = 0x0300;  /* Previous latch configuration. */
    scanline = samples = enable_writes = irq_locks = fbtest_calls = 0;
    status_register = 0x371;
    latched = mode_during_fbtest = 0;
    raster_fault = RASTER_NORMAL;
    for (unsigned i = 0; i < 1024; ++i) cram.data[i] = initial_color(i);
    vram.before = cram.before = 0x12345678;
    vram.after = cram.after = 0x87654321;
    vdp1_ram[0] = vdp1_ram[1] = 0x1234;
    check_vdp1_stop = 0;
    end_writes = settle_reads = 0;
    end_register = 0xffff;
    draw_status = 0;
    if (logo) {
        *(u16 *)(VDP2_VRAM + 0x848) = 13;
        *(u16 *)(VDP2_VRAM + 0x84c) = 13;
    }
}

static void guards(void)
{
    assert(vram.before == 0x12345678 && cram.before == 0x12345678);
    assert(vram.after == 0x87654321 && cram.after == 0x87654321);
}

static void text_bank(void)
{
    for (unsigned i = 0; i < 16; ++i) assert(cram.data[i] == expected_text[i]);
    for (unsigned i = 16; i < 256; ++i) assert(cram.data[i] == 0);
}

static void startup(int logo)
{
    /* Reseed every entry on each call: stale high text colors must be cleared. */
    for (int repeat = 0; repeat < 2; ++repeat) {
        seed(logo);
        conio_init();
        text_bank();
        for (unsigned i = 256; i < 1024; ++i)
            assert(cram.data[i] == initial_color(i));
        for (unsigned i = 0; i < sizeof(vram.data) / 4; ++i)
            assert(vram.data[i] == 0);
        assert(vdp1_ram[0] == 0x8000 && vdp1_ram[1] == 0x1234);
        assert(RAMCTL == 0x2300 && CHCTLA == 0x1212 && CRAOFA == 0x10);
        assert(MPOFN == 0x10 && BGON == 1 && TVMD == 0x8010);
        guards();
    }
}

static void erase_bounds(void)
{
    const u16 stale[] = {0xffff, 0, 0x8000};
    for (unsigned i = 0; i < sizeof(stale) / sizeof(stale[0]); ++i) {
        seed(i & 1);
        VDP1_REG_PTMR = VDP1_REG_FBCR = VDP1_REG_TVMR = stale[i];
        VDP1_REG_EWDR = VDP1_REG_EWLR = VDP1_REG_EWRR = stale[i];
        vdp_init();
        assert(VDP1_REG_PTMR == 0 && VDP1_REG_FBCR == 0 && VDP1_REG_TVMR == 0);
        assert(VDP1_REG_EWDR == 0 && VDP1_REG_EWLR == 0);
        assert(VDP1_REG_EWRR == 0x0200);
        assert(vdp1_ram[0] == 0x8000 && vdp1_ram[1] == 0x1234);
        guards();
    }
}

static void stop_drawing(void)
{
    for (unsigned status = 0; status < 4; ++status) {
        seed(status & 1);
        check_vdp1_stop = 1;
        VDP1_REG_PTMR = 1;
        draw_status = status;
        vdp_init();
        assert(end_register == 0 && end_writes == 1 && settle_reads == 64);
        assert(vdp1_ram[0] == 0x8000 && vdp1_ram[1] == 0x1234);
        guards();
    }
}

static void coordinates(void)
{
    /* Longword values use the target's big-endian integer/fraction layout. */
    const u32 expected[] = {0, 0, 0x10000, 0x10000, 0, 0, 0x10000, 0x10000};
    for (int pal = 0; pal <= 1; ++pal) {
        for (int logo = 0; logo <= 1; ++logo) {
            seed(logo);
            TVSTAT = VBLANK | (pal ? PAL : 0);
            SCXN2 = 0x4321;
            SCYN3 = 0x1234;
            conio_init();
            for (unsigned i = 0; i < sizeof expected / sizeof expected[0]; ++i)
                assert(registers[0x70 / 4 + i] == expected[i]);
            assert(ZMCTL == 0 && SCRCTL == 0);
            assert(SCXN2 == 0x4321 && SCYN3 == 0x1234);
            guards();
        }
    }
}

static unsigned slot(u32 schedule, unsigned timing)
{
    return (schedule >> (28 - 4 * timing)) & 0xf;
}

static void vram_cycles(void)
{
    for (int logo = 0; logo <= 1; ++logo) {
        seed(logo);
        vdp_init();
        u32 a0 = ((u32)CYCA0L << 16) | CYCA0U;
        u32 a1 = ((u32)CYCA1L << 16) | CYCA1U;
        const unsigned expected_a0[] = {4, 4, 15, 14, 14, 14, 14, 14};
        const unsigned expected_a1[] = {5, 5, 15, 14, 14, 14, 14, 14};
        for (unsigned timing = 0; timing < 8; ++timing) {
            assert(slot(a0, timing) == expected_a0[timing]);
            assert(slot(a1, timing) == expected_a1[timing]);
        }
        assert(CYCB0L == 0xffff && CYCB0U == 0xffff);
        assert(CYCB1L == 0xffff && CYCB1U == 0xffff);
        guards();
    }
}

static void logo_only(void)
{
    seed(1);
    logo_trans();
    assert(memcmp(cram.data, logo_dat, 32) == 0);
    for (unsigned i = 8; i < 1024; ++i) assert(cram.data[i] == initial_color(i));
    assert(memcmp((void *)(VDP2_VRAM + 0x4000), logo_dat + 32,
                  size_logo_dat - 32) == 0);
    for (unsigned y = 0; y < 6; ++y)
        for (unsigned x = 0; x < 20; ++x)
            assert(*(u16 *)(VDP2_VRAM + 0x6000 + ((9 + y) * 64 + 11 + x) * 2)
                   == 0x200 + y * 20 + x);
    assert(MZCTL == 0 && CCCTL == 0 && CCRNA == 31);
    guards();
    seed(0);
    logo_trans();
    for (unsigned i = 0; i < 1024; ++i) assert(cram.data[i] == initial_color(i));
    for (unsigned i = 0; i < sizeof(vram.data) / 4; ++i)
        assert(vram.data[i] == 0xa5a5a5a5);
    guards();
}

static void cover_bank(void)
{
    seed(0);
    vdp_init();
    nbg1_set_cram(0, 0x12, 0x34, 0x56);
    nbg1_set_cram(255, 0xfe, 0xdc, 0xba);
    nbg1_on();
    text_bank();
    assert(cram.data[256] == 0x563412 && cram.data[511] == 0xbadcfe);
    for (unsigned i = 257; i < 511; ++i) assert(cram.data[i] == initial_color(i));
    for (unsigned i = 512; i < 1024; ++i) assert(cram.data[i] == initial_color(i));
    assert(BGON == 3 && CCCTL == 2 && CCRNA == 0x700);
    guards();
}

static void compositing(void)
{
    const unsigned pairs[] = {0x24, 0x2c, 0xd0, 0xd4, 0xe0, 0xe8,
                              0xec, 0xf0, 0xf4, 0x110};
    for (int pal = 0; pal <= 1; ++pal) {
        for (int logo = 0; logo <= 1; ++logo) {
            seed(logo);
            TVSTAT = VBLANK | (pal ? PAL : 0);
            PRIR = 0x5678;
            registers[0x100 / 4] = 0x87654321;
            conio_init();
            assert(MZCTL == 0 && PRINA == 0x0707 && BGON == 1);
            for (unsigned i = 0; i < sizeof pairs / sizeof pairs[0]; ++i)
                assert(registers[pairs[i] / 4] == 0);
            assert(PRIR == 0x5678 && registers[0x100 / 4] == 0x87654321);

            nbg1_on();
            assert(BGON == 3 && CCCTL == 2 && CCRNA == 0x700);
            vdp2_win0(1, 1, 12, 24, 60, 80);
            assert(WCTLA == 0x0300 && WCTLB == 0);
            assert(WPSX0 == 24 && WPSY0 == 24 && WPEX0 == 120 && WPEY0 == 80);
            vdp2_win0(-1, 0, 0, 0, 0, 0);
            assert(WCTLA == 0 && CCCTL == 2);
            text_bank();
            guards();
        }
    }
}

static void ntsc_display(void)
{
    for (unsigned line = 0; line < 263; ++line) {
        seed(0);
        scanline = line;
        conio_init();
        assert(fbtest_calls == 1 && mode_during_fbtest == 0x0010);
        assert(TVMD == 0x8010 && EXTEN == 0 && enable_writes == 1);
        assert(samples > 0 && irq_locks == 1 && status_register == 0x371);
    }
}

static void pal_display(void)
{
    seed(0);
    TVSTAT = PAL | VBLANK;
    conio_init();
    assert(fbtest_calls == 1 && mode_during_fbtest == 0x8010);
    assert(TVMD == 0x8010 && EXTEN == 0x0300);
    assert(!samples && !enable_writes && !irq_locks && status_register == 0x371);
}

static void retry_display(void)
{
    seed(0);
    scanline = 242;
    raster_fault = RASTER_STALLED;
    conio_init();
    assert(TVMD == 0x0010 && !enable_writes && !irq_locks);
#ifdef TEST_DISPLAY_COUNTER
    assert(samples == 65536);
    samples = 0;
    raster_fault = RASTER_NORMAL;
    vdp_display_service();
    assert(TVMD == 0x8010 && enable_writes == 1 && irq_locks == 1);
    unsigned previous_samples = samples;
    vdp_display_service();
    assert(samples == previous_samples && enable_writes == 1 && irq_locks == 1);
    assert(status_register == 0x371);
#endif
}

static void refuse_display(void)
{
    seed(0);
    vdp_init();
    assert(TVMD == 0x0010 && EXTEN == 0);
#ifdef TEST_DISPLAY_COUNTER
    for (unsigned owner = 0x0100; owner <= 0x0200; owner += 0x0100) {
        EXTEN = owner;
        vdp_display_service();
        assert(TVMD == 0x0010 && EXTEN == owner && !samples);
    }
    EXTEN = 0;
    TVMD = 0x0040;
    vdp_display_service();
    assert(TVMD == 0x0040 && !samples && !enable_writes && !irq_locks);
    TVMD = 0x0010;
    raster_fault = RASTER_SKIP;
    vdp_display_service();
    assert(TVMD == 0x0010 && !enable_writes && !irq_locks);
    assert(samples == 65536);
    raster_fault = RASTER_SHIFT;
    samples = 0;
    vdp_display_service();
    assert(TVMD == 0x0010 && !enable_writes && irq_locks && status_register == 0x371);
#endif
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "startup")) startup(0);
    else if (!strcmp(argv[1], "startup-logo")) startup(1);
    else if (!strcmp(argv[1], "erase-bounds")) erase_bounds();
    else if (!strcmp(argv[1], "stop-drawing")) stop_drawing();
    else if (!strcmp(argv[1], "coordinates")) coordinates();
    else if (!strcmp(argv[1], "compositing")) compositing();
    else if (!strcmp(argv[1], "vram-cycles")) vram_cycles();
    else if (!strcmp(argv[1], "logo")) logo_only();
    else if (!strcmp(argv[1], "cover")) cover_bank();
    else if (!strcmp(argv[1], "ntsc")) ntsc_display();
    else if (!strcmp(argv[1], "pal")) pal_display();
    else if (!strcmp(argv[1], "retry")) retry_display();
    else if (!strcmp(argv[1], "refuse")) refuse_display();
    else return 2;
    printf("%s: pass\n", argv[1]);
    return 0;
}
