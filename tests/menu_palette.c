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
#undef VDP1_RAM
static volatile u16 VDP1_REG_PTMR, VDP1_REG_FBCR, VDP1_REG_TVMR;
static volatile u16 VDP1_REG_EWDR, VDP1_REG_EWLR;
#undef VDP2_VRAM
#undef VDP2_CRAM
#undef VDP2_REGISTER_BASE
#define VDP2_VRAM ((uintptr_t)vram.data)
#define VDP2_CRAM ((uintptr_t)cram.data)
#define VDP2_REGISTER_BASE ((uintptr_t)registers)
#define VDP1_RAM ((uintptr_t)vdp1_ram)

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
    for (unsigned i = 0; i < 1024; ++i) cram.data[i] = initial_color(i);
    vram.before = cram.before = 0x12345678;
    vram.after = cram.after = 0x87654321;
    vdp1_ram[0] = vdp1_ram[1] = 0x1234;
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
        vdp_init();
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

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "startup")) startup(0);
    else if (!strcmp(argv[1], "startup-logo")) startup(1);
    else if (!strcmp(argv[1], "logo")) logo_only();
    else if (!strcmp(argv[1], "cover")) cover_bank();
    else return 2;
    printf("%s: pass\n", argv[1]);
    return 0;
}
