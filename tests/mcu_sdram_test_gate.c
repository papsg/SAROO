/* Production startup control flow; the physical memory test is an injected boundary. */
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { LEDEV_BUSY = 1, LEDEV_SDRAM_ERROR = 2, LEDEV_NONE = 0 };
static uint32_t debug_flags;
static unsigned calls, busy_events, error_events, clear_events, pass_logs;
static int error_count, menu_load_reached;
static jmp_buf observation_end;

static void led_event(int event)
{
    if (event == LEDEV_BUSY) { assert(!calls); busy_events++; }
    else if (event == LEDEV_SDRAM_ERROR) { assert(calls); error_events++; }
    else { assert(event == LEDEV_NONE); clear_events++; }
}

static int mem_test(uint32_t address, int bytes)
{
    assert(address == 0x61000000 && bytes == 0x800000);
    assert(busy_events == 1 && !clear_events && !menu_load_reached);
    /* Bound only the host observation, without changing firmware loop flags. */
    if (calls == 5) longjmp(observation_end, 1);
    calls++;
    return calls == 1 ? error_count : 0;
}

static void printk(const char *format, int count)
{
    assert(format && count == (int)calls);
    pass_logs++;
}

static void startup(void)
{
    int retv;
#include "startup-gate.inc"
    menu_load_reached = 1;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    debug_flags = (uint32_t)strtoul(argv[1], NULL, 0);
    error_count = atoi(argv[2]);
    if (!setjmp(observation_end)) startup();
    if (!(debug_flags & 0xc0000000)) {
        assert(menu_load_reached && !calls && !busy_events && !clear_events);
    } else if (debug_flags & 0x40000000) {
        assert(calls == 5 && pass_logs == 5 && !menu_load_reached && !clear_events);
        assert(error_events == (unsigned)(error_count != 0));
    } else {
        assert(calls == 1 && busy_events == 1 && !pass_logs);
        assert(error_events == (unsigned)(error_count != 0));
        assert(clear_events == (unsigned)(error_count == 0));
        assert(menu_load_reached == (error_count == 0));
    }
    puts("startup gate pass");
    return 0;
}
