/* Adapted from paps-g 90db7fe2: actual clock/repeat/scroll functions. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
/* CPU-side model of the measured CS2 aliases. The single-transfer
 * low address repeats the high half. Each production REG16 is retained. */
enum { TIMER_HIGH = 0x2589700c, TIMER_LOW = 0x2581700e, TIMER_WRONG_LOW = 0x2589700e };
static u32 counter, step;
static unsigned reads, inject_carry, trace_count;
static u32 trace[6];
static int trace_enabled, reset_write;
static volatile uint16_t sampled_word, reset_value;

static volatile uint16_t *timer_register(u32 address)
{
    if (reset_write) {
        assert(address == TIMER_HIGH);
        return &reset_value;
    }
    if (trace_enabled) {
        assert(trace_count < sizeof trace / sizeof(*trace));
        trace[trace_count++] = address;
    }
    assert(address == TIMER_HIGH || address == TIMER_LOW || address == TIMER_WRONG_LOW);
    sampled_word = address == TIMER_LOW ? (uint16_t)counter : (uint16_t)(counter >> 16);
    if (inject_carry && (address & 2)) {
        counter++;
        inject_carry = 0;
    }
    if (step) {
        assert((reads % 3 == 1) == ((address & 2) != 0));
        if (++reads % 3 == 0)
            counter += step;
    }
    return &sampled_word;
}

#define REG16(address) (*timer_register(address))
static volatile uint16_t TVSTAT;
static int pad_value;
static int pad_read(void) { return pad_value; }
typedef struct { int current; char items[2][96]; } MENU_DESC;
static int sel_state, sel_oob, sel_offset, draws, uploads;
static int draw_menu_item_select(int index, char *item)
{
    assert(index == 1 && strcmp(item, "scrolling title") == 0);
    ++draws;
    return 0;
}
static void put_select_item(int index) { assert(index == 1); ++uploads; }

/* Scrolling starts with the display ready. The palette fixture exercises
 * the real display service, including its startup and retry paths. */
static void vdp_display_service(void) __attribute__((unused));
static void vdp_display_service(void) {}
#include "timer.inc"

static const u32 origins[] = {0, 0x7fff0000u, 0xffff0000u, 0x80010000u, 0x12345678u};
static u32 ticks(unsigned ms) { return ms * 1000u; }

static void at(u32 time) { counter = time; }

static void clock_case(unsigned which)
{
    static const u32 times[] = {0x80000021u, 0xfffffffdu, 0x00020021u};
    at(times[which]);
    assert(get_timer() == times[which]);
    at(times[which] + 3);
    assert(get_timer() == times[which] + 3);
}

static void reset_case(void)
{
    reset_value = 0xffff;
    reset_write = 1;
    reset_timer();
    assert(reset_value == 0);
}

static void reference_order_case(void)
{
    at(0x1234ffff);
    trace_enabled = 1;
    inject_carry = 1;
    assert(get_timer() == 0x12350000 && !inject_carry);
    const u32 expected[] = {TIMER_HIGH, TIMER_LOW, TIMER_HIGH,
                            TIMER_HIGH, TIMER_LOW, TIMER_HIGH};
    assert(trace_count == sizeof expected / sizeof(*expected));
    assert(!memcmp(trace, expected, sizeof expected));
}

static void sleep_case(unsigned which)
{
    static const struct { u32 origin, count, scale, advance; } cases[] = {
        {0x7ffffffc, 0, 1, 7},
        {0xfffffff0, 0, 1000, 17},
        {0xffffffff, 100, 1, 19},
        {0xfffffffe, UINT32_MAX, 1, 1000017},
        {0x80000100, UINT32_MAX, 1000, 900000019},
    };
    at(cases[which].origin);
    step = cases[which].advance;
    if (cases[which].scale == 1)
        usleep(cases[which].count);
    else
        msleep(cases[which].count);
    assert(reads >= 6 && reads % 3 == 0);
    uint64_t elapsed = ((uint64_t)reads / 3 - 1) * step;
    uint64_t duration = (uint64_t)(cases[which].count ? cases[which].count : 1)
                        * cases[which].scale;
    assert(elapsed >= duration && elapsed < duration + step);
}

static void conversion_case(void)
{
    u32 value = UINT32_MAX;
    assert(TICK2MS(value) == 4294967u);
    assert(TICK2US(value++) == UINT32_MAX && value == 0);
    assert(MS2TICK(value++) == 0 && value == 1);
}

static u32 sample(u32 time, int buttons)
{
    at(time); pad_value = buttons;
    return conio_getc();
}

static u32 press(u32 start)
{
    const u32 key = 0x40;
    for (unsigned i = 0; i < 3; ++i) assert(sample(start, key) == 0);
    u32 event = sample(start, key);
    assert(event == (key << 16 | key));
    return event;
}

static void repeat_case(u32 start, int late)
{
    u32 event = press(start);
    u32 due = start + ticks(500);
    assert(sample(start + 1, 0x40) == 0);
    assert(sample(due - 1, 0x40) == 0);
    if (late) due += ticks(300);
    /* No-data samples must neither repeat nor mutate the pending input. */
    assert(sample(due, -1) == 0);
    assert(sample(due, 0x40) == event);
    for (unsigned i = 0; i < 40; ++i) {
        assert(sample(due, 0x40) == 0);
        assert(sample(due + 1, 0x40) == 0);
        assert(sample(due + ticks(100) - 1, 0x40) == 0);
        due += ticks(100);
        assert(sample(due, 0x40) == event);
    }
    /* A release must cancel held repeats before its debounce completes. */
    assert(sample(due + 1, 0) == 0);
    assert(sample(due + ticks(100), 0) == 0);
    assert(sample(due + ticks(100), 0) == 0);
    assert(sample(due + ticks(100), 0) == 0);
    assert(sample(due + ticks(100), 0) == (0x40u << 16));
    for (unsigned i = 0; i < 20; ++i) assert(sample(due + ticks(500), 0) == 0);
}

static void scroll_case(u32 start)
{
    MENU_DESC menu = {.current = 1, .items = {"first", "scrolling title"}};
    sel_oob = 1;
    at(start); menu_timer(&menu); assert(sel_state == 1);
    at(start + 1); menu_timer(&menu); assert(sel_state == 1);
    at(start + ticks(1000) - 1); menu_timer(&menu); assert(sel_state == 1);
    at(start + ticks(1000)); menu_timer(&menu); assert(sel_state == 2);
    menu_timer(&menu); assert(sel_state == 3 && sel_offset == 1 && draws == 1);
    TVSTAT = 0; menu_timer(&menu); assert(sel_state == 3 && uploads == 0);
    /* Independently put the second wait at the same boundary. */
    at(start); TVSTAT = 8; menu_timer(&menu); assert(sel_state == 4 && uploads == 1);
    at(start + 1); menu_timer(&menu); assert(sel_state == 4);
    at(start + ticks(1000) - 1); menu_timer(&menu); assert(sel_state == 4);
    at(start + ticks(1000)); menu_timer(&menu); assert(sel_state == 5);
    menu_timer(&menu); assert(sel_state == 6 && sel_offset == 0 && draws == 2);
    TVSTAT = 0; menu_timer(&menu); assert(sel_state == 6 && uploads == 1);
    TVSTAT = 8; menu_timer(&menu); assert(sel_state == 0 && uploads == 2);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    unsigned value = (unsigned)strtoul(argv[2], NULL, 10);
    assert(value < 5);
    if (!strcmp(argv[1], "clock")) { assert(value < 3); clock_case(value); }
    else if (!strcmp(argv[1], "reset")) reset_case();
    else if (!strcmp(argv[1], "order")) reference_order_case();
    else if (!strcmp(argv[1], "sleep")) sleep_case(value);
    else if (!strcmp(argv[1], "conversion")) conversion_case();
    else if (!strcmp(argv[1], "repeat")) repeat_case(origins[value], 0);
    else if (!strcmp(argv[1], "late")) repeat_case(origins[value], 1);
    else if (!strcmp(argv[1], "scroll")) scroll_case(origins[value]);
    else return 2;
    printf("%s: pass\n", argv[1]);
    return 0;
}
