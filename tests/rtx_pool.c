/* Adapted from paps-g e4953e40. Test-only, using the pinned RTX allocator. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ff.h"
#include "rtx_lib.h"

_Static_assert(OS_THREAD_OBJ_MEM == 0 && OS_THREAD_DEF_STACK_NUM == 0 &&
               OS_THREAD_USER_STACK_SIZE == 0, "Both stacks must use the common pool");

static _Alignas(8) unsigned char parent[32768], current[OS_DYNAMIC_MEM_SIZE];
static unsigned threads, delta;

static void host_call(unsigned op, uintptr_t param)
{
    register unsigned r0 __asm("r0") = op;
    register uintptr_t r1 __asm("r1") = param;
    __asm volatile("bkpt 0xab" : "+r"(r0), "+r"(r1) : : "memory");
}

static void finish(const char *message, unsigned code)
{
    const unsigned status[] = {0x20026, code};
    host_call(4, (uintptr_t)message);
    host_call(0x20, (uintptr_t)status);
    for (;;) {}
}

#define CHECK(x) do { if (!(x)) finish("FAIL: " #x "\n", 1); } while (0)

static void main_task(void *arg) { (void)arg; }
static void led_task(void *arg) { (void)arg; }

osStatus_t osKernelInitialize(void)
{
    CHECK(osRtxMemoryInit(parent, sizeof(parent)));
    CHECK(osRtxMemoryInit(current, sizeof(current)));
    return osOK;
}

osThreadId_t osThreadNew(osThreadFunc_t fn, void *arg, const osThreadAttr_t *a)
{
    CHECK(!arg && a && !a->cb_mem && !a->stack_mem);
    CHECK(threads == 0 ? fn == main_task && a->priority == osPriorityLow :
                        threads == 1 && fn == led_task && a->priority == osPriorityHigh);
    unsigned stack = a->stack_size ? a->stack_size : OS_STACK_SIZE;
    if (threads == 1) {
        CHECK(stack == 512 && sizeof(current) < sizeof(parent));
        delta = 4096 - stack;
    } else {
        CHECK(stack == 4096);
    }
    ++threads;

    /* Same variable-size thread allocations as svcRtxThreadNew. */
    void *cb = osRtxMemoryAlloc(current, sizeof(osRtxThread_t), 1U);
    CHECK(cb && osRtxMemoryAlloc(parent, sizeof(osRtxThread_t), 1U));
    CHECK(osRtxMemoryAlloc(current, stack, 0U) && osRtxMemoryAlloc(parent, 4096, 0U));
    return cb;
}

osStatus_t osKernelStart(void)
{
    CHECK(threads == 2);
    return osOK;
}

#define main saroo_startup
#include "startup.inc"
#undef main

typedef struct { void *before, *after; } pair;

static pair allocate(unsigned size, unsigned type)
{
    pair p = {osRtxMemoryAlloc(parent, size, type), osRtxMemoryAlloc(current, size, type)};
    CHECK((p.before == NULL) == (p.after == NULL));
    if (p.before) CHECK((unsigned char *)p.before - parent ==
                        (unsigned char *)p.after - current + delta);
    return p;
}

static void release(pair p)
{
    CHECK(osRtxMemoryFree(parent, p.before) && osRtxMemoryFree(current, p.after));
}

void Reset_Handler(void)
{
    CHECK(saroo_startup() == 0);
    /* Keep a semaphore live and hold paired directory allocations across levels. */
    CHECK(allocate(sizeof(osRtxSemaphore_t), 1U).after);
    pair dirs[64][2];
    unsigned depth = 0;
    while (depth < sizeof(dirs) / sizeof(dirs[0])) {
        pair name = allocate(256, 0U);
        if (!name.after) break;
        pair info = allocate(sizeof(FILINFO), 0U);
        if (!info.after) { release(name); break; }
        dirs[depth][0] = name;
        dirs[depth++][1] = info;
    }
    CHECK(depth > 32 && depth < sizeof(dirs) / sizeof(dirs[0]));

    /* Fill holes out of order, then unwind and prove the large extent returns. */
    for (unsigned i = 0; i < depth; i += 2) release(dirs[i][1]);
    for (unsigned i = 0; i < depth; i += 2) {
        dirs[i][1] = allocate(sizeof(FILINFO) - 8, 0U);
        CHECK(dirs[i][1].after);
    }
    for (unsigned i = depth; i-- > 0;) {
        release(dirs[i][1]);
        release(dirs[i][0]);
    }
    pair large = allocate(24000, 0U);
    CHECK(large.after && !allocate(1024, 0U).after);
    release(large);
    finish("PASS: startup, directory depth, hole reuse and exhaustion match the parent\n", 0);
}

__asm(".section .vectors, \"a\"\n.word 0x20040000\n.word Reset_Handler\n.previous\n");
