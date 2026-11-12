/* Adapted from paps-g f285ede8; public UART-shell preservation controls added.
 * Actual task, UART, line editor and RTX dispatcher; time/hardware are modeled. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*task_fn)(void *);
typedef struct { int priority; uint32_t stack_size; } osThreadAttr_t;
enum { osPriorityLow=8, osPriorityNormal=24, osPriorityHigh=40 };
static task_fn created[4];
static int priorities[4], created_count;
static unsigned initialized, configured, boot_loaded, shell_calls;
static unsigned ticks, delays, gif_calls, audio_preemptions, accesses;
static unsigned poll_period, mode;
static const unsigned char *wire;
static size_t wire_size, wire_next;
static unsigned character_phase;
typedef struct { uint32_t ISR, RDR; } UART_REGS;
static UART_REGS uart;

/* Each ready ISR read is followed by the actual source's RDR read. Consume
 * that byte like the peripheral, retaining the byte value for the expression. */
static void *uart_registers(void)
{
    ++accesses;
    if (character_phase) {
        character_phase = 0;
        uart.ISR = 0;
        ++wire_next;
    } else if (uart.ISR & 0x20) character_phase = 1;
    return &uart;
}
#define UART4 ((UART_REGS *)uart_registers())

static void simulate_audio_preemption(void);
static void gif_decode_timer(void)
{
    assert(configured && initialized == 3 && !boot_loaded);
    ++gif_calls;
    if (mode == 2) simulate_audio_preemption();
}
static void osDelay(unsigned amount)
{
    assert(wire_next < wire_size && !uart.ISR);
    assert(amount > 0 && amount <= 20);
    ticks += amount;
    if (++delays % poll_period == 0) {
        uart.RDR = wire[wire_next];
        uart.ISR = 0x20;
    }
}
static void printk(const char *format, ...) { (void)format; }
#include "uart.inc"
#define gets firmware_gets
#include "line-editor.inc"

static void simple_shell(void)
{
    ++shell_calls;
    char line[64];
    assert(gets(line, sizeof line - 1) == 2 && !strcmp(line, "ok"));
}
static void device_init(void) { assert(initialized == 0); ++initialized; }
static int get_build_date(void) { return 0; }
static void sdio_init(void) { assert(initialized == 1); ++initialized; }
static void fs_mount(int arg) { assert(initialized == 2 && arg == 0); ++initialized; }
static void saturn_config(void) { assert(initialized == 3); configured = 1; }
static void fpga_config(void) { assert(initialized == 3); }
static void load_app(void) { assert(initialized == 3); boot_loaded = 1; }
#include "startup-task.inc"

static void led_task(void *arg) { (void)arg; }
static void sirq_task(void *arg) { (void)arg; }
static void disk_task(void *arg) { (void)arg; }
static void osKernelInitialize(void) {}
static void osKernelStart(void) {}
static void *osThreadNew(task_fn function, void *arg, const osThreadAttr_t *attr)
{
    assert(!arg && created_count < 4);
    created[created_count] = function;
    priorities[created_count++] = attr ? attr->priority : osPriorityNormal;
    return (void *)1;
}
#include "task-creation.inc"
#include "saturn-creation.inc"

typedef struct { unsigned priority; } os_thread_t;
static struct { struct { struct { os_thread_t *thread_list; } ready; } thread; } osRtxInfo;
static os_thread_t *current;
static unsigned switches, blocks, queued, removals;
enum { osRtxKernelRunning=2 };
static uint8_t osRtxKernelGetState(void) { return osRtxKernelRunning; }
static os_thread_t *osRtxThreadGetRunning(void) { return current; }
static void osRtxThreadListRemove(os_thread_t *thread)
{ assert(thread == osRtxInfo.thread.ready.thread_list); ++removals; }
static void osRtxThreadBlock(os_thread_t *thread) { assert(thread == current); ++blocks; }
static void osRtxThreadSwitch(os_thread_t *thread) { current = thread; ++switches; }
static void osRtxThreadReadyPut(os_thread_t *thread) { (void)thread; ++queued; }
#include "thread_dispatch.inc"

static void simulate_audio_preemption(void)
{
    os_thread_t background={8}, audio={24};
    current = &background;
    osRtxThreadDispatch(&audio);
    assert(current == &audio);
    ++audio_preemptions;
    /* The audio semaphore wait is modeled; its wakeup priority decision above
     * executes the pinned dispatcher. This is not a complete RTOS simulation. */
}

static void input(const char *data, unsigned period)
{
    wire = (const unsigned char *)data;
    wire_size = strlen(data); wire_next = 0;
    poll_period = period; delays = ticks = accesses = character_phase = 0;
    uart.ISR = 0;
    if (!period) { uart.RDR = wire[0]; uart.ISR = 0x20; }
}

static void background(unsigned audio)
{
    initialized = configured = boot_loaded = shell_calls = gif_calls = 0;
    mode = audio ? 2 : 1;
    input("ok\r", 4);
    main_task(NULL);
    assert(shell_calls == 1 && wire_next == wire_size && delays == 12);
    assert(accesses == 18); /* one readiness probe per idle iteration, no spin */
#ifdef BOOT
    assert(boot_loaded && !configured && !gif_calls && ticks == 240);
#else
    assert(configured && gif_calls == 12 && !boot_loaded);
    assert(ticks == 24);
    assert(audio_preemptions == (audio ? 12u : 0u));
#endif
}

static void editor(void)
{
    initialized = 3; configured = 1;
    /* Original editing/history behavior, with gaps between every input byte. */
    input("ab\bc\r", 2);
    struct { unsigned before; char text[32]; unsigned after; } line={0x1234,{0},0x5678};
    assert(gets(line.text, 31) == 2 && !strcmp(line.text, "ac"));
    assert(line.before == 0x1234 && line.after == 0x5678);
    input("\033[A\r", 2);
    assert(gets(line.text, 31) == 2 && !strcmp(line.text, "ac"));
    input("\033[C", 2);
    assert(gets(line.text, 31) == 2 && !strcmp(line.text, "ac"));
}

static void priorities_and_dispatch(void)
{
    firmware_main(); firmware_saturn_workers();
    assert(created_count == 4);
    assert(created[0] == main_task && priorities[0] == 8);
    assert(created[1] == led_task && priorities[1] == 40);
    assert(created[2] == sirq_task && priorities[2] == 31);
    assert(created[3] == disk_task && priorities[3] == 24);
    for (unsigned explicit = 0; explicit < 2; ++explicit)
        for (unsigned priority = 1; priority <= 40; ++priority) {
            os_thread_t background={8}, ready={priority};
            current = &background; switches = blocks = queued = removals = 0;
            osRtxInfo.thread.ready.thread_list = &ready;
            osRtxThreadDispatch(explicit ? &ready : NULL);
            assert(switches == (priority > 8) && blocks == (priority > 8));
            assert(removals == (!explicit && priority > 8));
            assert(current == (priority > 8 ? &ready : &background));
            assert(queued == (explicit && priority <= 8));
        }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "background")) background(0);
    else if (!strcmp(argv[1], "music")) background(1);
    else if (!strcmp(argv[1], "editor")) editor();
    else if (!strcmp(argv[1], "priority")) priorities_and_dispatch();
    else return 2;
    puts("scheduling: pass");
    return 0;
}
