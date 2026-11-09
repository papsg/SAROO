/* Adapted from paps-g 90db7fe2: actual clock/repeat/scroll functions. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
enum { STATUS, HIGH, LOW, CONTROL };
static volatile u8 timer_storage[4];
static unsigned trace_enabled, trace_count, rollover_injected;
static unsigned trace[32];
static int inject_rollover;
static volatile u8 *timer_register(unsigned reg)
{
    if (trace_enabled) { assert(trace_count<32);trace[trace_count++]=reg; }
    if (inject_rollover && !rollover_injected && reg==LOW) {
        timer_storage[HIGH]=0;timer_storage[LOW]=0x21;timer_storage[STATUS]=2;
        rollover_injected=1;
    }
    return timer_storage+reg;
}
#define FTCSR (*timer_register(STATUS))
#define FRCH (*timer_register(HIGH))
#define FRCL (*timer_register(LOW))
#define TCR (*timer_register(CONTROL))
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
#include "timer.inc"

static const u32 origins[] = {0, 0x7fff0000u, 0xffff0000u, 0x80010000u, 0x12345678u};
static u32 ticks(unsigned ms) { return (uint64_t)ms * 208496 / 1000; }

/* Set hardware state to an independently chosen 32-bit observation. */
static void at(u32 time)
{
    timer_base = time & 0xffff0000u;
    FRCH = (time >> 8) & 255;
    FRCL = time & 255;
    FTCSR = 0;
}

static void clock_case(unsigned which)
{
    const u32 base[] = {0x7fff0000u, 0xffff0000u, 0x00010000u};
    const u32 expected[] = {0x80000021u, 0x00000021u, 0x00020021u};
    timer_base = base[which];
    FRCH = 0; FRCL = 0x21; FTCSR = 2;
    assert(get_timer() == expected[which]);
    assert((FTCSR & 2) == 0);
    assert(get_timer() == expected[which]);
    FRCL = 0x22;
    assert(get_timer() == expected[which] + 1);
}

static void reset_case(void)
{
    timer_base = 0x120000;
    FRCH = 0xff; FRCL = 0xfe; FTCSR = 0x8e;
    reset_timer();
    assert(FTCSR == 0x8c);
    assert(TCR == 2 && FRCH == 0 && FRCL == 0);
    assert(get_timer() == 0);
    FRCL = 4;
    assert(get_timer() == 4);
}

static void reference_order_case(void)
{
    timer_base=0x10000;timer_storage[HIGH]=0xff;timer_storage[LOW]=0xfe;
    timer_storage[STATUS]=0;trace_count=0;trace_enabled=1;inject_rollover=1;
    assert(get_timer()==0x20021 && rollover_injected);
    trace_enabled=0;inject_rollover=0;
    const unsigned expected[]={STATUS,HIGH,LOW,STATUS,STATUS,HIGH,LOW,STATUS,STATUS};
    assert(trace_count==sizeof(expected)/sizeof(expected[0]));
    assert(!memcmp(trace,expected,sizeof expected) && !timer_storage[STATUS]);
    timer_base=0x120000;timer_storage[HIGH]=0xff;timer_storage[LOW]=0xfe;
    timer_storage[STATUS]=0x8e;trace_count=0;trace_enabled=1;
    reset_timer();trace_enabled=0;
    const unsigned reset_order[]={CONTROL,HIGH,LOW,STATUS};
    assert(trace_count==4 && !memcmp(trace,reset_order,sizeof reset_order));
    assert(timer_storage[STATUS]==0x8c && !get_timer());
}
static void no_overflow_reset_case(void)
{
    timer_base=0x120000;timer_storage[HIGH]=0xff;timer_storage[LOW]=0xfe;
    timer_storage[STATUS]=0x8d;reset_timer();
    assert(timer_storage[STATUS]==0x8d && !get_timer());
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
    else if (!strcmp(argv[1], "reset-clear")) no_overflow_reset_case();
    else if (!strcmp(argv[1], "repeat")) repeat_case(origins[value], 0);
    else if (!strcmp(argv[1], "late")) repeat_case(origins[value], 1);
    else if (!strcmp(argv[1], "scroll")) scroll_case(origins[value]);
    else return 2;
    printf("%s: pass\n", argv[1]);
    return 0;
}
