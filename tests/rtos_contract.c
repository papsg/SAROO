/* Adapted by papsg from paps-g's SSR-007 offline fixture.
 * Actual RTX SVC/exception/context-switch test on a Cortex-M7 QEMU CPU.
 * The kernel, scheduler, objects and irq_cm4f.S are production providers.
 * QEMU memory, startup and semihosting replace SAROO board/device services. */
#include <stdint.h>
#include <string.h>
#include "rtx_lib.h"

uint32_t SystemCoreClock = 25000000;
static uint64_t boot_stack[512];
static unsigned assertions;
static unsigned idle_delay_checked;
static osStatus_t idle_relative, idle_absolute;

static void output(const char *text) {
    register uint32_t operation __asm__("r0") = 4;
    register const char *argument __asm__("r1") = text;
    __asm__ volatile("bkpt 0xab" : "+r"(operation) : "r"(argument) : "memory");
}
static void number(uint32_t value) {
    char text[11] = "0x00000000";
    for (unsigned i=0; i<8; i++) text[9-i]="0123456789abcdef"[(value>>(4*i))&15];
    output(text);
}
__attribute__((noreturn)) static void finish(unsigned status) {
    uint32_t block[2] = {0x20026,status};
    register uint32_t operation __asm__("r0") = 0x20;
    register uint32_t *argument __asm__("r1") = block;
    __asm__ volatile("bkpt 0xab" : "+r"(operation) : "r"(argument) : "memory");
    for (;;) {}
}
static void require(int valid, unsigned line) {
    assertions++;
    if (!valid) { output("FAIL line="); number(line); output("\n"); finish(2); }
}
#define CHECK(x) require(!!(x),__LINE__)
uint32_t osRtxErrorNotify(uint32_t code, void *object) {
    (void)object; output("RTX error="); number(code); output("\n"); finish(3);
}
__attribute__((noreturn)) void osRtxIdleThread(void *argument) {
    (void)argument;
    for (;;) {
        if (!idle_delay_checked) {
            CHECK(osRtxInfo.thread.ready.thread_list==NULL);
            idle_relative=osDelay(1);
            idle_absolute=osDelayUntil(osKernelGetTickCount()+100);
            idle_delay_checked=1;
        }
        __WFI();
    }
}
static void fault(void) { output("FAULT\n"); finish(4); }

static void main_task(void *argument) {
    CHECK(argument==(void *)0x9173);
    uint32_t before=osKernelGetTickCount();
    CHECK(osDelay(2)==osOK);
    CHECK((uint32_t)(osKernelGetTickCount()-before)>=2);
    CHECK(idle_delay_checked);
    CHECK(osDelayUntil(osKernelGetTickCount()-1)==osErrorParameter);
    CHECK(osDelayUntil(osKernelGetTickCount())==osErrorParameter);
    before=osKernelGetTickCount();
    CHECK(osDelayUntil(before+2)==osOK);
    CHECK((uint32_t)(osKernelGetTickCount()-before)>=2);
    __disable_irq();
    CHECK(osDelay(1)==osErrorISR);
    CHECK(osDelayUntil(osKernelGetTickCount()+1)==osErrorISR);
    __enable_irq();
    output("PASS positive, expired, current and masked-IRQ controls\n");
    osStatus_t zero=osDelay(0);
    CHECK(osKernelLock()==0);
    osStatus_t locked_relative=osDelay(2);
    osStatus_t locked_absolute=osDelayUntil(osKernelGetTickCount()+100);
    CHECK(osKernelGetState()==osKernelLocked);
    CHECK(osKernelUnlock()==1);
    output("DELAY zero=");number((uint32_t)zero);
    output(" locked=");number((uint32_t)locked_relative);
    output(",");number((uint32_t)locked_absolute);
    output(" idle=");number((uint32_t)idle_relative);
    output(",");number((uint32_t)idle_absolute);output("\n");
    CHECK(zero==osErrorParameter);
    CHECK(locked_relative==osError && locked_absolute==osError);
    CHECK(idle_relative==osError && idle_absolute==osError);
    CHECK(osKernelLock()==0);
    osRtxInfo.kernel.tick=UINT32_MAX-2;
    CHECK(osKernelUnlock()==1);
    CHECK(osDelayUntil(1)==osOK);
    CHECK(osKernelGetTickCount()>=1 && osKernelGetTickCount()<100);
    output("PASS delay assertions=");number(assertions);output("\n");finish(0);
}
void Reset_Handler(void) {
    extern uint32_t _sidata[],_sdata[],_edata[],_sbss[],_ebss[];
    uint32_t *s=_sidata;
    for (uint32_t *p=_sdata;p<_edata;p++) *p=*s++;
    for (uint32_t *p=_sbss;p<_ebss;p++) *p=0;
    SCB->CPACR|=0xfu<<20; __DSB(); __ISB();
    NVIC_SetPriorityGrouping(3);
    CHECK(osKernelInitialize()==osOK);
    osThreadAttr_t attr={.name="svc-main",.priority=osPriorityNormal};
    CHECK(osThreadNew(main_task,(void *)0x9173,&attr));
    (void)osKernelStart();
    CHECK(0);
}
extern void SVC_Handler(void),PendSV_Handler(void),SysTick_Handler(void);
__attribute__((section(".vectors"),used))
void (* const vectors[16])(void)={
    (void (*)(void))(boot_stack+512),Reset_Handler,fault,fault,fault,fault,fault,
    0,0,0,0,SVC_Handler,fault,0,PendSV_Handler,SysTick_Handler
};
