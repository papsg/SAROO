/* Public integration control for paps-g's SATURN-MENU-004 timer correction. */
#define main peripheral_control_main
#include "test_menu_peripherals.c"
#undef main
#include "repeat.inc"

/* Reuse the public controller register/IRQ fixture. Real INTBACK admission and
 * completion precede each real conio_getc sample. All times are synthetic. */
static u32 controller_sample(u32 observed, int held)
{
    const Packet pressed = {{0xf1, 2, 0xfb, 0xff, 0xf0}, 5, 0x0400};
    const Packet released = {{0xf1, 2, 0xff, 0xff, 0xf0}, 5, 0};
    clock_now = observed - EXPECTED_PAD_ADMISSION - 100u;
    TVSTAT = 0;
    assert(conio_getc() == 0);
    TVSTAT = 8;
    SF = 0;
    unsigned before = reads;
    begin();
    page(held ? &pressed : &released, 0, 0xc0); /* another 100 ticks */
    assert(clock_now == observed);
    u32 result = conio_getc();
    assert(reads - before == 5 && pad_state == 3);
    COMREG = 0x5a;
    for (unsigned i = 0; i < 3; ++i) assert(conio_getc() == 0);
    assert(COMREG == 0x5a);           /* no second INTBACK in this blank */
    return result;
}

static u32 reference_ticks(unsigned ms)
{
    return (uint64_t)ms * HZ / 1000;
}

int main(int argc, char **argv)
{
    const u32 origins[] = {0, 0x7fff0000u, 0xffff0000u, 0x80010000u, 0x12345678u};
    assert(argc == 3);
    unsigned index = (unsigned)strtoul(argv[1], NULL, 10);
    unsigned late = (unsigned)strtoul(argv[2], NULL, 10);
    assert(index < 5 && late < 2);
    u32 start = origins[index] + 1000;
    reset(start - EXPECTED_PAD_ADMISSION - 100u);
    for (unsigned i = 0; i < 3; ++i)
        assert(controller_sample(start + i * 1000, 1) == 0);
    start += 3000;
    const u32 event = (0x0400u << 16) | 0x0400;
    assert(controller_sample(start, 1) == event);
    u32 due = start + reference_ticks(500);
    assert(controller_sample(due - 200u, 1) == 0);
    clock_now = due;
    assert(conio_getc() == 0);        /* no new completed sample */
    due += 200u;                     /* allow the next packet to follow in time */
    if (late) due += reference_ticks(300);
    assert(controller_sample(due, 1) == event);
    for (unsigned i = 0; i < 40; ++i) {
        u32 next = due + reference_ticks(100);
        assert(controller_sample(next - 200u, 1) == 0);
        assert(controller_sample(next, 1) == event);
        due = next;
    }
    /* Release cancels repeat immediately; its event retains the old debounce. */
    for (unsigned i = 1; i <= 5; ++i)
        assert(controller_sample(due + i * 1000, 0) ==
               (i == 5 ? 0x0400u << 16 : 0));
    assert(controller_sample(due + reference_ticks(500), 0) == 0);
    assert(interrupt_enabled && !interrupt_locked);
    puts("transport-repeat: pass");
    return 0;
}
