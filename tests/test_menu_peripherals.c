#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static volatile u8 SF, SR, IREG0, IREG1, IREG2, COMREG;
static volatile u8 DDR1, DDR2, EXLE, IOSEL;
static volatile u16 TVSTAT;
static volatile u8 test_oreg[64];
static unsigned reads, calls, packets;
static u32 clock_now;
static void (*system_manager_interrupt)(void);
static unsigned interrupt_enabled, interrupt_locked;
static u32 get_timer(void) { return clock_now; }
static u32 pad_test_interrupt_lock(void)
{
    unsigned prior = interrupt_locked;
    interrupt_locked = 1;
    return prior;
}
static void pad_test_interrupt_unlock(u32 prior)
{
    interrupt_locked = prior;
}
static void pad_test_interrupt_enable(int enable, void (*handler)(void))
{
    system_manager_interrupt = enable ? handler : NULL;
    interrupt_enabled = enable;
}
#define SMPC_BASE 0x20100000u
static volatile u8 *oreg_at(u32 addr)
{
    assert(addr >= SMPC_BASE + 0x21 && addr <= SMPC_BASE + 0x5f);
    unsigned offset = addr - (SMPC_BASE + 0x21);
    assert((offset & 1) == 0);
    ++reads;
    return &test_oreg[offset];
}
#define REG8(addr) (*oreg_at(addr))
#include "pad.inc"

typedef struct {
    u8 bytes[512];
    unsigned size;
    unsigned expected;
} Packet;

static void byte(Packet *p, u8 b)
{
    assert(p->size < sizeof(p->bytes));
    p->bytes[p->size++] = b;
}

static void device(Packet *p, unsigned type, unsigned size, unsigned buttons, int ext)
{
    assert(size > 0 && size <= 15 && (!ext || size == 15));
    byte(p, (type << 4) | (ext ? 0 : size));
    if (ext) byte(p, size);
    byte(p, (buttons >> 8) ^ 255);
    if (size > 1) byte(p, (buttons & 255) ^ 255);
    for (unsigned i = 2; i < size; ++i) byte(p, (i * 37) & 255);
    if ((type == 0 || type == 1 || type == 3) && size >= 2)
        p->expected |= buttons & 0xfff8;
}

static int poll(void) { ++calls; return pad_read(); }

static void reset(u32 origin)
{
    SF = SR = 0;
    IREG0 = IREG1 = IREG2 = COMREG = 0;
    TVSTAT = 8;
    memset((void *)test_oreg, 0xa5, sizeof(test_oreg));
    clock_now = origin;
    reads = 0;
    pad_init();
#if !PAD_TEST_LEGACY
    assert(interrupt_enabled && system_manager_interrupt != NULL);
#endif
    assert(DDR1 == 0 && DDR2 == 0 && IOSEL == 0 && EXLE == 0);
}

static void begin(void)
{
#if PAD_TEST_LEGACY
    assert(poll() == -1 && COMREG == 0x10 && SF == 1);
    return;
#else
    u8 old_ireg0 = IREG0, old_ireg1 = IREG1, old_ireg2 = IREG2;
    u8 old_command = COMREG, old_sf = SF;
    assert(poll() == -1);
    assert(IREG0 == old_ireg0 && IREG1 == old_ireg1 && IREG2 == old_ireg2);
    assert(COMREG == old_command && SF == old_sf);
    clock_now += EXPECTED_PAD_ADMISSION - 1;
    assert(poll() == -1 && COMREG == old_command && SF == old_sf);
    ++clock_now;
    assert(poll() == -1);
    assert(IREG0 == 0 && IREG1 == 8 && IREG2 == 0xf0 && COMREG == 0x10);
    assert(SF == 1);
#endif
}

static void page(const Packet *p, unsigned offset, unsigned status)
{
    for (unsigned i = 0; i < 32; ++i) {
        test_oreg[i * 2] = offset + i < p->size ? p->bytes[offset + i] : 0xa5;
        test_oreg[i * 2 + 1] = 0x5a;
    }
    SF = (status & 0x20) ? 1 : 0;
    SR = status;
    clock_now += 100;
#if !PAD_TEST_LEGACY
    assert(interrupt_enabled && !interrupt_locked && system_manager_interrupt);
    system_manager_interrupt();
#endif
}

static void transact(const Packet *p, u32 origin)
{
    reset(origin);
    begin();
    unsigned toggle = 0;
    for (unsigned offset = 0; offset < p->size; offset += 32) {
        int more = p->size - offset > 32;
        page(p, offset, 0x80 | (offset ? 0 : 0x40) | (more ? 0x20 : 0));
        int result = poll();
        if (more) {
            toggle ^= 0x80;
            assert(result == -1 && IREG0 == toggle);
        } else {
            assert(result == (int)p->expected);
            /* NPE=0 ends INTBACK in hardware; no redundant BREAK is sent. */
#if !PAD_TEST_LEGACY
            assert(IREG0 == toggle);
#endif
        }
    }
    /* The old raw address has no access hook; compare its behavior separately. */
#if !PAD_TEST_LEGACY
    assert(reads == p->size);
#endif
    /* Completing a sample must not allow a second request in the same blank. */
    COMREG = 0x5a;
    TVSTAT = 8;
    for (unsigned i = 0; i < 3; ++i) assert(poll() == -1);
    assert(COMREG == 0x5a);
    TVSTAT = 0; assert(poll() == -1);
    TVSTAT = 8; SF = 0; begin();
    ++packets;
}

static void direct(void)
{
    /* Explicit wire fixtures, independently of the packet generator. */
    const Packet explicit[] = {
        {{0xf1, 2, 0xfb, 0xff, 0xf0}, 5, 0x0400},
        {{0xf0, 0xf1, 2, 0xef, 0xff}, 5, 0x1000},
        {{0xf1, 2, 0xfb, 0xff, 0xf1, 2, 0xef, 0xff}, 8, 0x1400},
        {{0x30, 0xf1, 2, 0xef, 0xff}, 5, 0x1000},
        {{0xf1, 0x23, 0, 0, 0, 0xf1, 2, 0xef, 0xff}, 9, 0x1000},
        {{0xf1, 0xf9, 0xf1, 2, 0xef, 0xff}, 6, 0x1000},
        {{0xf1, 2, 0xff, 0xf8, 0xf0}, 5, 0},
        {{0xf0, 0xf0}, 2, 0}
    };
    for (unsigned i = 0; i < sizeof(explicit) / sizeof(*explicit); ++i)
        transact(&explicit[i], 1000);
    for (unsigned type = 0; type < 15; ++type)
        for (unsigned size = 1; size <= 15; ++size) {
            Packet p = {0};
            byte(&p, 0xf1); device(&p, type, size, 0x0448, 0);
            byte(&p, 0xf1); device(&p, 0, 2, 0x1000, 0);
            transact(&p, 1000);
        }
}

static void mouse_with_pad(void)
{
    /* Sega SMPC manual tables 3.12 and 3.16: standard pointing ID 23h and
     * legacy Saturn mouse ID E3h. Their flags/buttons are active high and
     * their movement bytes must never become another pad's button state.
     * Issue #319 reports interference with the mouse in either port. */
    const u8 ids[] = {0x23, 0xe3};
    const u8 motion[][2] = {{0,0}, {1,255}, {127,128}, {255,1}};
    for (unsigned id = 0; id < 2; ++id)
    for (unsigned side = 0; side < 2; ++side)
    for (unsigned flags = 0; flags < 256; ++flags)
    for (unsigned move = 0; move < 4; ++move)
    for (unsigned held = 0; held < 2; ++held) {
        Packet p = {0};
        for (unsigned port = 0; port < 2; ++port) {
            byte(&p, 0xf1);
            if (port == side) {
                byte(&p, ids[id]); byte(&p, flags);
                byte(&p, motion[move][0]); byte(&p, motion[move][1]);
            } else {
                byte(&p, 0x02); byte(&p, held ? 0xef : 0xff); byte(&p, 0xff);
            }
        }
        p.expected = held ? 0x1000 : 0;
        transact(&p, 1000);
    }
}

static void taps(void)
{
    for (unsigned a = 0; a < 64; ++a)
        for (unsigned b = 0; b < 64; ++b) {
            Packet p = {0};
            for (unsigned port = 0; port < 2; ++port) {
                byte(&p, 0x16);
                unsigned mask = port ? b : a;
                for (unsigned slot = 0; slot < 6; ++slot) {
                    if (mask & (1u << slot)) device(&p, 0, 2, 8u << slot, 0);
                    else byte(&p, 0xff);
                }
            }
            transact(&p, 1000);
        }
}

static void lengths(void)
{
    unsigned seen = 0;
    for (unsigned a = 1; a <= 15; ++a)
        for (unsigned b = 1; b <= 15; ++b)
          for (unsigned empty_first = 0; empty_first <= 3; ++empty_first) {
            Packet p = {0};
            byte(&p, 0x16);
            device(&p, 2, a, 0xffff, 0);
            device(&p, 4, b, 0xffff, 0);
            for (unsigned i = 0; i < empty_first; ++i) byte(&p, 0xff);
            seen |= 1u << (p.size % 32);
            device(&p, 1, 15, 0x0400, 1);
            for (unsigned i = empty_first; i < 3; ++i) byte(&p, 0xf0 + i);
            byte(&p, 0x04);
            device(&p, 0, 2, 0x1000, 0);
            device(&p, 3, 4, 0x0040, 0);
            byte(&p, 0xff); byte(&p, 0xff);
            transact(&p, 0xffff0000u);
        }
    assert(seen == UINT32_MAX); /* All 32 ID/length/data alignments. */
    Packet max = {0};
    for (unsigned port = 0; port < 2; ++port) {
        byte(&max, 0xef);
        for (unsigned slot = 0; slot < 15; ++slot)
            device(&max, 0, 15, 0x0400, 1);
    }
    assert(max.size == 512);
    transact(&max, 0xfffff000u);
}

static void rejected(const Packet *p, unsigned status)
{
    reset(0); begin(); page(p, 0, status);
    assert(poll() == -1);
    unsigned invalid_status = (status & 0x8f) != 0x80 || (status & 0x40) == 0;
    assert((IREG0 & 0x40) == ((invalid_status || (status & 0x20)) ? 0x40 : 0));
    assert(reads <= 32);
}

static void invalid(void)
{
    Packet p = {{0xf2, 2, 0, 0}, 4, 0};
    rejected(&p, 0xc0);
    p = (Packet){{0xf1, 0, 0}, 3, 0};
    for (unsigned length = 0; length < 256; ++length) {
        if (length == 15) continue;
        p.bytes[2] = length;
        rejected(&p, 0xc0);
    }
    p = (Packet){{0xf0, 0xf0}, 2, 0};
    rejected(&p, 0x40); rejected(&p, 0x80);
    for (unsigned mode = 1; mode < 16; ++mode) rejected(&p, 0xc0 | mode);
    rejected(&p, 0xe0);
    p = (Packet){0};
    byte(&p, 0x16);
    for (unsigned i = 0; i < 6; ++i) device(&p, 1, 15, 0x0400, 1);
    byte(&p, 0xf0);
    rejected(&p, 0xc0); /* First 32 bytes cannot complete this declaration. */
    reset(0); begin(); page(&p, 0, 0xe0); assert(poll() == -1);
    assert(IREG0 == 0x80);
    page(&p, 32, 0xe0); /* A continuation falsely marked as a first page. */
    assert(poll() == -1 && IREG0 == 0xc0);
}

static void busy(void)
{
    reset(0);
    SF = 1; COMREG = 0x5a;
    for (unsigned i = 0; i < 1000; ++i) assert(poll() == -1);
    assert(COMREG == 0x5a && reads == 0 && SF == 1);
    SF = 0; TVSTAT = 0;
    assert(poll() == -1 && COMREG == 0x5a);
    TVSTAT = 8; begin();
}

static void transport(void)
{
    Packet p = {0};
    byte(&p, 0x16);
    for (unsigned i = 0; i < 6; ++i) device(&p, 1, 15, 0x0400, 1);
    byte(&p, 0xf0);
    reset(0xfffff000u); begin();
    SF = 0;
    for (unsigned i = 0; i < 100; ++i) assert(poll() == -1 && reads == 0);
    page(&p, 0, 0xe0);
    assert(SF == 1 && poll() == -1 && reads == 32 && IREG0 == 0x80);
    for (unsigned i = 0; i < 100; ++i) assert(poll() == -1 && reads == 32);
    clock_now = 0xfffff000u + EXPECTED_PAD_ADMISSION + EXPECTED_PAD_TIMEOUT - 1u;
    assert(poll() == -1 && IREG0 == 0x80);
    ++clock_now;
    assert(poll() == -1 && IREG0 == 0xc0 && reads == 32);
    /* Wait for the abandoned command to clear; never overwrite a busy SMPC. */
    TVSTAT = 0; assert(poll() == -1);
    TVSTAT = 8; COMREG = 0x5a;
    assert(poll() == -1 && COMREG == 0x5a);
    SF = 0; begin();
    p = (Packet){{0xf1, 2, 0xef, 0xff, 0xf0}, 5, 0x1000};
    page(&p, 0, 0xc0);
    assert(poll() == 0x1000 && IREG0 == 0);
}

static void timing(void)
{
    reset(UINT32_MAX - 100u);
    assert(poll() == -1 && COMREG == 0 && SF == 0);
    clock_now += EXPECTED_PAD_ADMISSION - 1;
    assert(poll() == -1 && COMREG == 0 && SF == 0);
    TVSTAT = 0;
    assert(poll() == -1 && COMREG == 0 && SF == 0);
    TVSTAT = 8;
    assert(poll() == -1 && COMREG == 0 && SF == 0);
    clock_now += EXPECTED_PAD_ADMISSION;
    assert(poll() == -1 && COMREG == 0x10 && SF == 1);

    /* Foreground work can skip the intervening active-display sample. A
     * deadline from the prior field must not authorize the next VBlank. */
    reset(1000);
    assert(poll() == -1 && COMREG == 0 && SF == 0);
    clock_now += MS2TICK(17);
    assert(poll() == -1 && COMREG == 0 && SF == 0);
    assert(poll() == -1 && COMREG == 0 && SF == 0);
    clock_now += EXPECTED_PAD_ADMISSION;
    assert(poll() == -1 && COMREG == 0x10 && SF == 1);
}

static void lifecycle(void)
{
    reset(0);
#if !PAD_TEST_LEGACY
    pad_suspend();
    assert(!interrupt_enabled && system_manager_interrupt == NULL);
    pad_resume();
    assert(interrupt_enabled && system_manager_interrupt != NULL);
    IREG0 = 0x5a;
    SR = 0xc0;
    system_manager_interrupt();
    assert(IREG0 == 0x5a && reads == 0); /* Idle interrupts are ignored. */
#endif
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "direct")) direct();
    else if (!strcmp(argv[1], "mouse")) mouse_with_pad();
    else if (!strcmp(argv[1], "taps")) taps();
    else if (!strcmp(argv[1], "lengths")) lengths();
    else if (!strcmp(argv[1], "invalid")) invalid();
    else if (!strcmp(argv[1], "busy")) busy();
    else if (!strcmp(argv[1], "transport")) transport();
    else if (!strcmp(argv[1], "timing")) timing();
    else if (!strcmp(argv[1], "lifecycle")) lifecycle();
    else return 2;
    printf("%s: pass (%u packets; %u calls)\n", argv[1], packets, calls);
    return 0;
}
