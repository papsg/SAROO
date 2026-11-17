/* Adapted from paps-g SSR-161 / 5cc06c78. Public CMSIS calls on an
 * emulated M7; no forged mutex/thread ownership or SAROO hardware access. */
#include <stdint.h>
#include <string.h>
#include "rtx_lib.h"

#ifndef OBSERVE_PRIORITIES
#define OBSERVE_PRIORITIES 0
#endif
uint32_t SystemCoreClock = 25000000;
static uint64_t boot_stack[512];
static unsigned assertions, cases, wrong_after, wrong_cleanup;
static void output(const char *text) {
    register uint32_t operation __asm__("r0") = 4;
    register const char *argument __asm__("r1") = text;
    __asm__ volatile("bkpt 0xab" : "+r"(operation) : "r"(argument) : "memory");
}
static void number(uint32_t value) {
    char text[11] = "0x00000000";
    for (unsigned i=0; i<8; i++) text[9-i] = "0123456789abcdef"[(value>>(4*i))&15];
    output(text);
}
__attribute__((noreturn)) static void finish(unsigned status) {
    uint32_t block[2] = {0x20026, status};
    register uint32_t operation __asm__("r0") = 0x20;
    register uint32_t *argument __asm__("r1") = block;
    __asm__ volatile("bkpt 0xab" : "+r"(operation) : "r"(argument) : "memory");
    for (;;) {}
}
static void require(int valid, unsigned line) {
    assertions++;
    if (!valid) { output("FAIL line="); number(line); output("\n"); finish(2); }
}
#define CHECK(x) require(!!(x), __LINE__)
uint32_t osRtxErrorNotify(uint32_t code, void *object) {
    (void)object; output("RTX error="); number(code); output("\n"); finish(3);
}
__attribute__((noreturn)) void osRtxIdleThread(void *argument) {
    (void)argument; for (;;) __WFI();
}
static void fault(void) { output("FAULT\n"); finish(4); }

struct waiter { unsigned index, done; osStatus_t expected; };
static struct {
    unsigned deleting, attributes, order, recursive, base_mode, queued_a, robust;
    osMutexId_t mutex[3];
    osThreadId_t thread[3];
    struct waiter waiter[3];
} state;
static const unsigned orders[3][3] = {{0,1,2}, {1,0,2}, {1,2,0}};
static void waiting_thread(void *argument) {
    struct waiter *w = argument;
    osStatus_t status = osMutexAcquire(state.mutex[w->index], osWaitForever);
    CHECK(status == w->expected);
    if (status == osOK) {
        CHECK(osMutexGetOwner(state.mutex[w->index]) == osThreadGetId());
        CHECK(osMutexRelease(state.mutex[w->index]) == osOK);
    }
    w->done = 1;
}
static void owner_thread(void *argument) {
    CHECK(argument == &state && __get_IPSR() == 0 && __get_PRIMASK() == 0);
    CHECK(osKernelGetState() == osKernelRunning);
    unsigned base = state.base_mode == 1 ? 20 : 24;
    CHECK(osThreadSetPriority(osThreadGetId(), (osPriority_t)base) == osOK);
    for (unsigned i=0; i<3; i++) {
        osMutexAttr_t attr = {.name="owned", .attr_bits=
            ((state.attributes & (1U<<i)) ? osMutexPrioInherit : 0) |
            (state.robust ? osMutexRobust : 0) |
            ((i==0 && state.recursive) ? osMutexRecursive : 0)};
        state.mutex[i] = osMutexNew(&attr);
        CHECK(state.mutex[i]);
    }
    for (unsigned i=0; i<3; i++) CHECK(osMutexAcquire(state.mutex[orders[state.order][i]],0) == osOK);
    if (state.recursive) CHECK(osMutexAcquire(state.mutex[0],0) == osOK);
    /* Queue in ascending priority so each new waiter runs and really blocks. */
    const unsigned sequence[3] = {2,1,0}, priority[3] = {48,40,32};
    for (unsigned n=0; n<(state.queued_a ? 3U : 2U); n++) {
        unsigned i=sequence[n];
        state.waiter[i] = (struct waiter){.index=i,
            .expected=(i==0 && state.deleting) ? osErrorResource : osOK};
        osThreadAttr_t attr = {.name="waiter", .attr_bits=osThreadJoinable,
            .stack_size=768, .priority=(osPriority_t)priority[i]};
        state.thread[i] = osThreadNew(waiting_thread,&state.waiter[i],&attr);
        CHECK(state.thread[i] && osThreadGetState(state.thread[i]) == osThreadBlocked);
    }
    if (state.base_mode == 2) {
        base=52; /* A new base above every donor must survive restoration. */
        CHECK(osThreadSetPriority(osThreadGetId(), (osPriority_t)base) == osOK);
    }
    unsigned expected=base, before_expected=base;
    for (unsigned i=0; i<3; i++) {
        CHECK(osMutexGetOwner(state.mutex[i]) == osThreadGetId());
        if ((state.attributes & (1U<<i)) && (i!=0 || state.queued_a) && priority[i]>before_expected)
            before_expected=priority[i];
        if (i && (state.attributes & (1U<<i)) && priority[i]>expected) expected=priority[i];
    }
    unsigned before=osThreadGetPriority(osThreadGetId());
    CHECK(before == before_expected);
    if (state.recursive && !state.deleting) {
        CHECK(osMutexRelease(state.mutex[0]) == osOK);
        CHECK(osMutexGetOwner(state.mutex[0]) == osThreadGetId());
        CHECK(osThreadGetPriority(osThreadGetId()) == (osPriority_t)before);
        if (state.queued_a) CHECK(osThreadGetState(state.thread[0]) == osThreadBlocked);
    }
    CHECK((state.deleting ? osMutexDelete(state.mutex[0]) : osMutexRelease(state.mutex[0])) == osOK);
    unsigned after=osThreadGetPriority(osThreadGetId());
    wrong_after += after != expected;
    if (OBSERVE_PRIORITIES && (state.attributes==7 || state.base_mode==2))
        CHECK(after == expected);
    if (!OBSERVE_PRIORITIES) CHECK(after == expected);
    CHECK(osMutexRelease(state.mutex[1]) == osOK);
    CHECK(osMutexRelease(state.mutex[2]) == osOK);
    for (unsigned i=0; i<3; i++) if (state.thread[i]) {
        CHECK(osThreadJoin(state.thread[i]) == osOK && state.waiter[i].done);
    }
    if (!state.deleting) CHECK(osMutexDelete(state.mutex[0]) == osOK);
    CHECK(osMutexDelete(state.mutex[1]) == osOK && osMutexDelete(state.mutex[2]) == osOK);
    unsigned cleanup=osThreadGetPriority(osThreadGetId());
    wrong_cleanup += cleanup != base;
    if (OBSERVE_PRIORITIES && (state.attributes==7 || state.base_mode==2))
        CHECK(cleanup == base);
    output("CASE mask="); number(state.attributes);
    output(" delete="); number(state.deleting);
    output(" order="); number(state.order);
    output(" recursive="); number(state.recursive);
    output(" base_mode="); number(state.base_mode);
    output(" queued_a="); number(state.queued_a);
    output(" robust="); number(state.robust);
    output(" before="); number(before);
    output(" after="); number(after);
    output(" cleanup="); number(cleanup); output("\n");
    if (!OBSERVE_PRIORITIES) CHECK(cleanup == base);
    cases++;
}
static void main_task(void *argument) {
    CHECK(argument == (void *)0x161);
    CHECK(osThreadSetPriority(osThreadGetId(),osPriorityBelowNormal) == osOK);
    for (unsigned op=0; op<2; op++) for (unsigned mask=0; mask<8; mask++)
    for (unsigned order=0; order<3; order++) for (unsigned recursive=0; recursive<2; recursive++)
    for (unsigned base=0; base<3; base++) for (unsigned queued=0; queued<2; queued++)
    for (unsigned robust=0; robust<2; robust++) {
        memset(&state,0,sizeof state);
        state.deleting=op; state.attributes=mask; state.order=order; state.recursive=recursive;
        state.base_mode=base; state.queued_a=queued; state.robust=robust;
        osThreadAttr_t attr={.name="owner", .attr_bits=osThreadJoinable,
            .stack_size=1536, .priority=osPriorityNormal};
        osThreadId_t owner=osThreadNew(owner_thread,&state,&attr);
        CHECK(owner && osThreadJoin(owner) == osOK);
    }
    CHECK(cases == 1152);
    output("SUMMARY cases="); number(cases);
    output(" wrong_after="); number(wrong_after);
    output(" wrong_cleanup="); number(wrong_cleanup);
    output(" assertions="); number(assertions); output("\n"); finish(0);
}
void Reset_Handler(void) {
    extern uint32_t _sidata[],_sdata[],_edata[],_sbss[],_ebss[];
    uint32_t *source=_sidata;
    for (uint32_t *p=_sdata; p<_edata;) *p++=*source++;
    for (uint32_t *p=_sbss; p<_ebss;) *p++=0;
    SCB->CPACR|=0xfu<<20; __DSB(); __ISB(); NVIC_SetPriorityGrouping(3);
    CHECK(osKernelInitialize() == osOK);
    osThreadAttr_t attr={.name="supervisor", .priority=osPriorityNormal};
    CHECK(osThreadNew(main_task,(void *)0x161,&attr));
    (void)osKernelStart(); CHECK(0);
}
extern void SVC_Handler(void),PendSV_Handler(void),SysTick_Handler(void);
__attribute__((section(".vectors"),used))
void (*const vectors[16])(void)={
    (void (*)(void))(boot_stack+512),Reset_Handler,fault,fault,fault,fault,fault,
    0,0,0,0,SVC_Handler,fault,0,PendSV_Handler,SysTick_Handler
};
