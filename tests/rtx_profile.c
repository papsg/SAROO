/* Adapted from paps-g 1b49c143; pinned Arm extracts retain their own notices. */
/* Pinned RTX startup/tick/delay code with bounded host hardware/list mocks. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "profile-macros.h"

typedef unsigned bool_t;
enum { FALSE=0, TRUE=1, osRtxKernelRunning=2, osOK=0 };
#define osWaitForever UINT32_MAX
enum { osRtxThreadWaitingDelay, osRtxThreadWaitingThreadFlags,
       osRtxThreadWaitingEventFlags, osRtxThreadWaitingMutex,
       osRtxThreadWaitingSemaphore, osRtxThreadWaitingMemoryPool,
       osRtxThreadWaitingMessageGet, osRtxThreadWaitingMessagePut };
typedef struct thread {
    unsigned priority, state;
    uint32_t delay;
    struct thread *delay_next, *delay_prev;
} os_thread_t;
static struct {
    struct { uint32_t tick; } kernel;
    struct {
        os_thread_t *idle, *delay_list;
        struct { os_thread_t *next; } run;
        struct { os_thread_t *thread_list; } ready;
        struct { unsigned timeout, tick; os_thread_t *thread; } robin;
    } thread;
    struct { os_thread_t *thread; void (*tick)(void); } timer;
} osRtxInfo;
static int os_idle_thread_attr, os_timer_thread_attr, os_timer_mq_attr;
static const struct {
    const void *idle_thread_attr, *timer_thread_attr, *timer_mq_attr;
    unsigned timer_mq_mcnt;
} osRtxConfig = {
    &os_idle_thread_attr,
#include "timer-config.inc"
};
static os_thread_t idle, timer, *ready[8];
static unsigned created, ready_count, removed, acknowledged, dispatched;
static void osRtxIdleThread(void *arg) { (void)arg; }
static void osRtxTimerThread(void *arg) { (void)arg; }
static os_thread_t *svcRtxThreadNew(void (*fn)(void *), void *arg, const void *attr)
{
    assert(!arg); ++created;
    if (fn==osRtxIdleThread) { assert(attr==&os_idle_thread_attr); return &idle; }
    assert(fn==osRtxTimerThread && attr==&os_timer_thread_attr); return &timer;
}
#define osRtxThreadId(value) (value)
static void OS_Tick_AcknowledgeIRQ(void) { ++acknowledged; }
static void osRtxThreadDispatch(os_thread_t *thread) { assert(!thread); ++dispatched; }
static void osRtxThreadListRemove(os_thread_t *thread) { assert(thread); ++removed; }
static void osRtxThreadReadyPut(os_thread_t *thread)
{ assert(ready_count<8); ready[ready_count++]=thread; }
static unsigned osRtxKernelGetState(void) { return osRtxKernelRunning; }
static void osRtxThreadSwitch(os_thread_t *thread) { (void)thread; }
#define EvrRtxThreadPreempted(...) ((void)0)
#define EvrRtxDelayCompleted(...) ((void)0)
#define EvrRtxThreadFlagsWaitTimeout(...) ((void)0)
#define EvrRtxEventFlagsWaitTimeout(...) ((void)0)
#define EvrRtxMutexAcquireTimeout(...) ((void)0)
#define EvrRtxSemaphoreAcquireTimeout(...) ((void)0)
#define EvrRtxMemoryPoolAllocTimeout(...) ((void)0)
#define EvrRtxMessageQueueGetTimeout(...) ((void)0)
#define EvrRtxMessageQueuePutTimeout(...) ((void)0)
#define EvrRtxThreadUnblocked(...) ((void)0)
#include "thread-startup.inc"
#include "delay-tick.inc"
#include "system-tick.inc"

static struct { uint32_t CR1, ARR, CNT, SR; } hw_timer;
#define TIM6 (&hw_timer)
static unsigned sem_tim6=7, waits, releases;
static int osSemaphoreAcquire(unsigned sem, uint32_t timeout)
{ assert(sem==sem_tim6 && timeout==osWaitForever); ++waits; return osOK; }
static int osSemaphoreRelease(unsigned sem)
{ assert(sem==sem_tim6); ++releases; return osOK; }
#include "audio-timer.inc"
#include "audio-irq.inc"

static void selected_startup(void)
{
    assert(OS_TIMER_THREAD_STACK_SIZE==0 && OS_TIMER_CB_QUEUE==0);
    assert(!osRtxConfig.timer_thread_attr && !osRtxConfig.timer_mq_attr);
    assert(osRtxThreadStartup() && created==1 && osRtxInfo.thread.idle==&idle);
    assert(!osRtxInfo.timer.thread && !osRtxInfo.timer.tick);
}
static void ticks(void)
{
    assert(OS_TICK_FREQ==100 && OS_ROBIN_ENABLE==0);
    os_thread_t a={.state=osRtxThreadWaitingDelay,.delay=2};
    os_thread_t b={.state=osRtxThreadWaitingSemaphore,.delay=0,.delay_prev=&a};
    os_thread_t c={.state=osRtxThreadWaitingMutex,.delay=3,.delay_prev=&b};
    a.delay_next=&b; b.delay_next=&c;
    osRtxInfo.thread.delay_list=&a;
    osRtxInfo.kernel.tick=UINT32_MAX-1;
    for (unsigned i=1; i<=5; ++i) {
        osRtxTick_Handler();
        assert(acknowledged==i && dispatched==i);
        assert(osRtxInfo.kernel.tick==(uint32_t)(UINT32_MAX-1+i));
        assert(ready_count==(i<2?0:i<5?2:3));
        if (i==2) assert(osRtxInfo.thread.delay_list==&c && !c.delay_prev);
    }
    assert(ready[0]==&a && ready[1]==&b && ready[2]==&c && removed==3);
    assert(!osRtxInfo.thread.delay_list && !osRtxInfo.timer.tick);
    osRtxTick_Handler(); assert(ready_count==3 && dispatched==6);
}
static void audio(void)
{
    const int us[]={0,1,10000,13500}; const uint32_t reload[]={2,1,1000,1350};
    for (unsigned i=0; i<4; ++i) {
        memset(&hw_timer,0xff,sizeof hw_timer);
        hw_delay(us[i]);
        assert(waits==i+1 && TIM6->ARR==reload[i] && !TIM6->CNT && TIM6->CR1==13);
        TIM6_DAC_IRQHandler(); assert(releases==i+1 && !TIM6->SR);
    }
}
int main(int argc, char **argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"startup")) selected_startup();
    else if (!strcmp(argv[1],"restore")) {
        assert(OS_TIMER_THREAD_STACK_SIZE==256 && OS_TIMER_CB_QUEUE==4);
        assert(osRtxThreadStartup() && created==2 && osRtxInfo.timer.thread==&timer && osRtxInfo.thread.idle==&idle);
    }
    else if (!strcmp(argv[1],"ticks")) ticks();
    else if (!strcmp(argv[1],"audio")) audio();
    else return 2;
    puts("pass"); return 0;
}
