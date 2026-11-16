/* Adapted by papsg from paps-g 3feed85e/SSR-132.
 * Current complete RTX providers and real M7 exceptions under QEMU.
 * Nominal25MHz platform/test IRQs are not SAROO hardware qualification. */
#include <stdint.h>
#include "rtx_lib.h"

uint32_t SystemCoreClock=25000000U;
static uint64_t boot_stack[512];
static uint32_t interval,irq_runs,bad_phase,bad_order,controls,wrap_queries;
static uint32_t probe_tick;
static uint32_t last_sample_tick;
static volatile unsigned hook_enabled;
static volatile unsigned resume_enabled;

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
{
    if(!valid){output("HARNESS FAIL line=");number(line);output("\n");finish(2);}
}
#define CHECK(x) require(!!(x),__LINE__)
uint32_t osRtxErrorNotify(uint32_t code,void *object)
{ (void)object;output("RTX ERROR=");number(code);finish(3); }
void osRtxIdleThread(void *argument) { (void)argument;for(;;)__WFI(); }
static void fault(void) { output("FAULT\n");finish(4); }

/* Inserted immediately after the selected tick acknowledgement, before its
 * software increment. A coherent trial may defer IRQ0 until after publication. */
void syscounter_ack_probe(void)
{
    if(TEST_SYSCOUNTER_MODE==3 && hook_enabled){
        hook_enabled=0;probe_tick=osRtxInfo.kernel.tick;
        NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();
    }
}

void syscounter_block_probe(void)
{
    if((TEST_SYSCOUNTER_MODE==4 || TEST_SYSCOUNTER_MODE==7) && hook_enabled){
        hook_enabled=0;
        unsigned spin=0;
        while(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk) && ++spin<1000000U)__NOP();
        CHECK(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk);
        probe_tick=osRtxInfo.kernel.tick;
    }
}

void syscounter_resume_probe(void)
{
    if(TEST_SYSCOUNTER_MODE==7 && resume_enabled){
        resume_enabled=0;NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();
    }
}

uint32_t syscounter_observe_overflow(void)
{
    if(TEST_SYSCOUNTER_MODE==5 && hook_enabled){
        unsigned spin=0;
        while(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk) && ++spin<1000000U)__NOP();
        CHECK(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk);
        uint32_t flag=OS_Tick_GetOverflow();
        hook_enabled=0;NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();
        return flag;
    }
    return OS_Tick_GetOverflow();
}

static void sample_pending(void)
{
    uint32_t old=osKernelGetTickCount();
    if(TEST_SYSCOUNTER_MODE==4 || TEST_SYSCOUNTER_MODE==7){
        CHECK(osKernelGetState()==osKernelSuspended);
        CHECK(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk));
    }else if(TEST_SYSCOUNTER_MODE==10){
        CHECK(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk));
    }else if(TEST_SYSCOUNTER_MODE!=3){
        unsigned spin=0;
        while(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk) && ++spin<1000000U)__NOP();
        CHECK(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk);
    }else{
        CHECK(SCB->SHCSR&SCB_SHCSR_SYSTICKACT_Msk);
        CHECK(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk));
    }
    uint32_t base=(TEST_SYSCOUNTER_MODE==3 ? probe_tick+1U:
                   (TEST_SYSCOUNTER_MODE==9 || TEST_SYSCOUNTER_MODE==10 ? old:old+1U))*interval;
    if(base==0U)++wrap_queries;
    uint32_t previous=0;
    for(unsigned i=0;i<8;++i){
        uint32_t mask=__get_PRIMASK();
        uint32_t value=osKernelGetSysTimerCount();
        CHECK(__get_PRIMASK()==mask);
        /* Unsigned differences admit the legitimate32-bit timer wrap. */
        if(value-base>=interval)++bad_phase;
        if(i && value-previous>=interval/4U)++bad_order;
        previous=value;
    }
    CHECK(osKernelGetTickCount()==old);
    last_sample_tick=old;
    ++irq_runs;
}
void IRQ0_Handler(void) { sample_pending(); }

static void main_task(void *argument)
{
    (void)argument;interval=OS_Tick_GetInterval();
    CHECK(interval==250000U && osKernelGetTickFreq()==100U);
    NVIC_SetPriority((IRQn_Type)0,0);NVIC_EnableIRQ((IRQn_Type)0);
    if(TEST_SYSCOUNTER_MODE==2){
        __disable_irq();osRtxInfo.kernel.tick=UINT32_MAX-3U;__enable_irq();
    }
    if(TEST_SYSCOUNTER_MODE==6){
        CHECK(osKernelGetTickCount()==0U);
        if(osKernelGetSysTimerCount()>=interval)++bad_phase;
    }
    for(unsigned i=0;i<32;++i){
        unsigned before=irq_runs;
        if(TEST_SYSCOUNTER_MODE==10){
            CHECK(osDelay(1)==osOK);sample_pending();
        }else if(TEST_SYSCOUNTER_MODE==9){
            CHECK(osDelay(1)==osOK);
            uint32_t mask=__get_PRIMASK();__disable_irq();
            uint32_t position=OS_Tick_GetCount();
            /* QEMU's delay wakeup need not land in the first quarter. Keep
             * headroom, and prove no natural rollover before/after the whole
             * software-pending observation instead of assuming that phase. */
            if(position>=3U*interval/4U){
                output("MANUAL PRECONDITION round=");number(i);
                output(" position=");number(position);
                output(" tick=");number(osRtxInfo.kernel.tick);output("\n");
            }
            CHECK(position<3U*interval/4U && !(SysTick->CTRL&SysTick_CTRL_COUNTFLAG_Msk));
            SCB->ICSR=SCB_ICSR_PENDSTSET_Msk;sample_pending();
            CHECK(OS_Tick_GetCount()>=position && !(SysTick->CTRL&SysTick_CTRL_COUNTFLAG_Msk));
            __set_PRIMASK(mask);
        }else if(TEST_SYSCOUNTER_MODE==3){
            hook_enabled=1;CHECK(osDelay(1)==osOK);CHECK(irq_runs==before+1U);
        }else if(TEST_SYSCOUNTER_MODE==4 || TEST_SYSCOUNTER_MODE==7){
            CHECK(osDelay(1)==osOK);hook_enabled=1;
            (void)osKernelSuspend();CHECK(!hook_enabled);
            if(TEST_SYSCOUNTER_MODE==7){
                (void)osKernelGetSysTimerCount();resume_enabled=1;osKernelResume(5);
                CHECK(!resume_enabled && irq_runs==before+1U);
            }else{sample_pending();osKernelResume(0);}
            CHECK(osKernelGetState()==osKernelRunning);
        }else if(TEST_SYSCOUNTER_MODE==5){
            CHECK(osDelay(1)==osOK);hook_enabled=1;
            (void)osKernelGetSysTimerCount();CHECK(!hook_enabled && irq_runs==before+1U);
        }else{
            CHECK(osDelay(1)==osOK);
            if(TEST_SYSCOUNTER_MODE==1){
                uint32_t mask=__get_PRIMASK();__disable_irq();sample_pending();__set_PRIMASK(mask);
            }else{
                NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();
                CHECK(irq_runs==before+1U);
            }
        }
        while(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk){}
        if(TEST_SYSCOUNTER_MODE==8){
            /* State-equivalence control for a complete software-tick lap:
             * retire a real tick, then reuse the old32-bit value. This does
             * not simulate years of scheduling or physical uptime. */
            uint32_t mask=__get_PRIMASK();__disable_irq();
            CHECK(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk));
            uint32_t committed=osRtxInfo.kernel.tick;
            osRtxInfo.kernel.tick=last_sample_tick;
            if(osKernelGetSysTimerCount()-last_sample_tick*interval>=interval)++bad_phase;
            osRtxInfo.kernel.tick=committed;__set_PRIMASK(mask);
        }
        uint32_t a=osKernelGetSysTimerCount(),b=osKernelGetSysTimerCount();
        CHECK(b-a<interval/4U);++controls;
    }
    CHECK(irq_runs==32U && controls==32U);
    if(TEST_SYSCOUNTER_MODE==2)CHECK(wrap_queries>0U);
    output("SYSCOUNTER mode=");number(TEST_SYSCOUNTER_MODE);output(" runs=");number(irq_runs);
    output(" phase=");number(bad_phase);output(" order=");number(bad_order);
    output(" controls=");number(controls);output(" wraps=");number(wrap_queries);output("\n");
    finish(bad_phase || bad_order ? 42:0);
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
    if(TEST_SYSCOUNTER_MODE==6){
        __disable_irq();CHECK(OS_Tick_Setup(100,SysTick_Handler)==0);OS_Tick_Enable();
        unsigned spin=0;
        while(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk) && ++spin<1000000U)__NOP();
        CHECK(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk);
        (void)osKernelGetSysTimerCount();OS_Tick_Disable();__enable_irq();
    }
    if(TEST_SYSCOUNTER_MODE==10){
        __disable_irq();CHECK(OS_Tick_Setup(100,SysTick_Handler)==0);OS_Tick_Enable();
        unsigned spin=0;
        while(!(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk) && ++spin<1000000U)__NOP();
        CHECK(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk);
        CHECK(OS_Tick_GetOverflow()==1U);CHECK(OS_Tick_GetOverflow()==0U);
        OS_Tick_Disable();CHECK((SysTick->CTRL&7U)==6U);
        CHECK(OS_Tick_Setup(0,SysTick_Handler)==-1);OS_Tick_Enable();
        CHECK((SysTick->CTRL&7U)==7U);CHECK(SCB->ICSR&SCB_ICSR_PENDSTSET_Msk);
        OS_Tick_Disable();CHECK(OS_Tick_Setup(1000,SysTick_Handler)==0);
        CHECK(OS_Tick_GetInterval()==25000U && OS_Tick_GetOverflow()==0U);
        __enable_irq();
    }
    osThreadAttr_t a={.name="syscounter",.priority=osPriorityNormal,.stack_size=2048};
    CHECK(osThreadNew(main_task,0,&a));(void)osKernelStart();CHECK(0);
}
__attribute__((section(".vectors"),used)) void (*const vectors[17])(void)={
    (void (*)(void))(boot_stack+512),Reset_Handler,fault,fault,fault,fault,fault,
    0,0,0,0,SVC_Handler,fault,0,PendSV_Handler,SysTick_Handler,IRQ0_Handler
};
