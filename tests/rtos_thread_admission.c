/* Adapted by papsg from paps-g 6a244892/SSR-133.
 * Public thread flags, real M7 IRQ/PendSV and a returning overflow hook.
 * SysTick is deliberately stopped: this is an ownership/wakeup fixture,
 * not a target timing or normal-firmware workload. */
#include <stdint.h>
#include "rtx_lib.h"
uint32_t SystemCoreClock=25000000U;
static uint64_t boot_stack[512];
static osEventFlagsId_t events[16];
static osSemaphoreId_t retire_gate;
static osThreadId_t target;
static volatile unsigned phase,fill,masked,completed,notifications;
static volatile uint32_t returned,observed,wait_value;
static uint32_t wanted,options,initial,update;
static unsigned cases,lost,refusals,controls,retries,repeated;
static void output(const char *text)
{
    register uint32_t op __asm__("r0")=4;
    register const char *arg __asm__("r1")=text;
    __asm__ volatile("bkpt 0xab":"+r"(op):"r"(arg):"memory");
}
static void number(uint32_t v)
{
    char s[11]="0x00000000";
    for(unsigned i=0;i<8;++i)s[9-i]="0123456789abcdef"[(v>>(4*i))&15U];
    output(s);
}
__attribute__((noreturn)) static void finish(unsigned status)
{
    uint32_t block[2]={0x20026,status};
    register uint32_t op __asm__("r0")=0x20;
    register uint32_t *arg __asm__("r1")=block;
    __asm__ volatile("bkpt 0xab":"+r"(op):"r"(arg):"memory");
    for(;;){}
}
static void require(int valid,unsigned line)
{ if(!valid){output("THREAD FAIL line=");number(line);output("\n");finish(2);} }
#define CHECK(x) require(!!(x),__LINE__)
uint32_t osRtxErrorNotify(uint32_t code,void *object)
{
    CHECK(code==osRtxErrorISRQueueOverflow && object==target);
    ++notifications;return 0;
}
void osRtxIdleThread(void *p) { (void)p;for(;;)__WFI(); }
static void fault(void) { output("FAULT\n");finish(4); }
static void pend_irq(void)
{ NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB(); }
static void waiter(void *p)
{
    (void)p;CHECK(osThreadFlagsSet(osThreadGetId(),initial)==initial);
    wait_value=osThreadFlagsWait(wanted,options,osWaitForever);++completed;
    /* Keep the control block alive for independent post-wakeup inspection. */
    (void)osSemaphoreAcquire(retire_gate,osWaitForever);CHECK(0);
}
void IRQ0_Handler(void)
{
    CHECK(__get_IPSR()==16 && !__get_PRIMASK());
    if(masked)__disable_irq();
    if(phase==1)for(unsigned i=0;i<fill;++i)CHECK(osEventFlagsSet(events[i],1)==1);
    unsigned queued=osRtxInfo.isr_queue.cnt;
    uint32_t before=((os_thread_t *)target)->thread_flags;
    CHECK(osThreadFlagsSet(NULL,1)==osFlagsErrorParameter);
    CHECK(osThreadFlagsSet(target,0x80000000U)==osFlagsErrorParameter);
    CHECK(osRtxInfo.isr_queue.cnt==queued && ((os_thread_t *)target)->thread_flags==before);
    if(phase==4){
        returned=osThreadFlagsSet(target,update);
        CHECK(returned==osFlagsErrorResource);
        CHECK(osRtxInfo.isr_queue.cnt==queued &&
              ((os_thread_t *)target)->thread_flags==before);
    }else if(phase==3){
        CHECK(osThreadFlagsSet(target,4)==(before|4U));
        CHECK(osThreadFlagsSet(target,4)==(before|4U));
        CHECK(osThreadFlagsSet(target,0)==(before|4U));
        CHECK(osRtxInfo.isr_queue.cnt==3);++repeated;
    }else if(TEST_SCENARIO==1){
        returned=osThreadFlagsSet(target,1);
        if(!(returned&osFlagsError)){
            CHECK(returned==(before|1U));returned=osThreadFlagsSet(target,2);
            CHECK(returned==(before|3U));
        }
    }else returned=osThreadFlagsSet(target,update);
    observed=((os_thread_t *)target)->thread_flags;
    CHECK(__get_PRIMASK()==masked);
    if(phase==1)CHECK(osRtxInfo.isr_queue.cnt==16);
    if(masked)__enable_irq();
}
static void main_task(void *p)
{
    (void)p;SysTick->CTRL=0;CHECK(osRtxInfo.isr_queue.max==16);
    NVIC_SetPriority((IRQn_Type)0,5);NVIC_EnableIRQ((IRQn_Type)0);
    retire_gate=osSemaphoreNew(1,0,NULL);CHECK(retire_gate);
    for(unsigned i=0;i<16;++i){events[i]=osEventFlagsNew(NULL);CHECK(events[i]);}
    for(unsigned cycle=0;cycle<4;++cycle)
    for(unsigned mode=0;mode<(TEST_SCENARIO==1?2U:4U);++mode)
    for(unsigned mask=0;mask<2;++mask)for(unsigned full=0;full<2;++full){
        for(unsigned i=0;i<16;++i)CHECK(!(osEventFlagsClear(events[i],0x7fffffffU)&osFlagsError));
        if(TEST_SCENARIO==1){wanted=3;initial=4;update=3;
            options=osFlagsWaitAll|((mode&1U)?osFlagsNoClear:0U);
        }else{wanted=(mode&1U)?3U:1U;initial=(mode&1U)?5U:4U;update=(mode&1U)?2U:1U;
            options=((mode&1U)?osFlagsWaitAll:osFlagsWaitAny)|((mode&2U)?osFlagsNoClear:0U);}
        completed=0;wait_value=0;
        osThreadAttr_t attr={.name="thread-flags-waiter",.priority=osPriorityAboveNormal,.stack_size=1024};
        target=osThreadNew(waiter,NULL,&attr);CHECK(target);
        CHECK(!completed && osThreadGetState(target)==osThreadBlocked);
        CHECK(((os_thread_t *)target)->state==osRtxThreadWaitingThreadFlags);
        phase=1;fill=full?16U:(TEST_SCENARIO==1?14U:15U);masked=mask;pend_irq();
        CHECK(!osRtxInfo.isr_queue.cnt && !__get_PRIMASK());
        if(returned&osFlagsError){
            CHECK(full && returned==osFlagsErrorUnknown && !completed && observed==initial);
            CHECK(((os_thread_t *)target)->thread_flags==initial);++refusals;
        }else{
            CHECK(returned==(initial|update) && observed==(initial|update));
            if(!completed){CHECK(full && osThreadGetState(target)==osThreadBlocked);++lost;}
            else{CHECK(!full);++controls;}
        }
        if(!completed){phase=2;pend_irq();CHECK(completed==1);++retries;}
        CHECK(completed==1 && wait_value==(initial|update));
        uint32_t remaining=(options&osFlagsNoClear)?(initial|update):4U;
        CHECK(((os_thread_t *)target)->thread_flags==remaining);
        CHECK(((os_thread_t *)target)->state==osRtxThreadWaitingSemaphore);
        phase=3;pend_irq();CHECK(completed==1 && ((os_thread_t *)target)->thread_flags==(remaining|4U));
        CHECK(!osRtxInfo.isr_queue.cnt && !__get_PRIMASK());
        os_thread_t *thread=(os_thread_t *)target;
        uint8_t saved_state=thread->state;
        thread->state=osRtxThreadTerminated;
        phase=4;pend_irq();
        thread->state=saved_state;
        CHECK(completed==1 && thread->thread_flags==(remaining|4U));
        CHECK(osThreadTerminate(target)==osOK);++cases;
    }
    for(unsigned i=0;i<16;++i)CHECK(osEventFlagsDelete(events[i])==osOK);
    CHECK(osSemaphoreDelete(retire_gate)==osOK);
    output("THREAD cases=");number(cases);output(" lost=");number(lost);
    output(" refused=");number(refusals);output(" controls=");number(controls);
    output(" retries=");number(retries);output(" repeated=");number(repeated);
    output(" notifications=");number(notifications);output("\n");finish(lost?42:0);
}
extern void SVC_Handler(void),PendSV_Handler(void),SysTick_Handler(void);
void Reset_Handler(void)
{
    extern uint32_t _sidata[],_sdata[],_edata[],_sbss[],_ebss[];
    uint32_t *s=_sidata;
    for(uint32_t *p=_sdata;p<_edata;++p)*p=*s++;
    for(uint32_t *p=_sbss;p<_ebss;++p)*p=0;
    SCB->CPACR|=0xfU<<20;__DSB();__ISB();NVIC_SetPriorityGrouping(3);
    CHECK(osKernelInitialize()==osOK);
    osThreadAttr_t attr={.name="thread-admission",.priority=osPriorityNormal};
    CHECK(osThreadNew(main_task,NULL,&attr));(void)osKernelStart();CHECK(0);
}
__attribute__((section(".vectors"),used)) void (*const vectors[17])(void)={
    (void (*)(void))(boot_stack+512),Reset_Handler,fault,fault,fault,fault,fault,
    0,0,0,0,SVC_Handler,fault,0,PendSV_Handler,SysTick_Handler,IRQ0_Handler
};
