/* Adapted by papsg from paps-g SSR-069, d3b1f4d5.
 * Real blocked threads and public APIs; common bootstrap is test-only. */
static osEventFlagsId_t events[17];
static volatile unsigned completed,phase,fill,masked;
static volatile uint32_t returned,observed,wait_value;
static volatile unsigned wake_order[3];
static uint32_t wanted,options,initial,update;
static unsigned cases,lost,refusals,controls,retries;
static void pend_irq(void) {NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();}
static void waiter(void *p) {
    wait_value=osEventFlagsWait(events[16],wanted,options,osWaitForever);
    CHECK(completed<3);wake_order[completed++]=(unsigned)(uintptr_t)p;osThreadExit();
}
static void event_irq(void) {
    CHECK(__get_IPSR()==16 && !__get_PRIMASK());
    if(masked)__disable_irq();
    if(phase==1)for(unsigned i=0;i<fill;i++)CHECK(osEventFlagsSet(events[i],1)==1);
    unsigned pending=osRtxInfo.isr_queue.cnt;
    CHECK(osEventFlagsSet(NULL,update)==osFlagsErrorParameter);
    CHECK(osEventFlagsSet(events[16],0x80000000u)==osFlagsErrorParameter);
    CHECK(osRtxInfo.isr_queue.cnt==pending);
    returned=osEventFlagsSet(events[16],update);
    CHECK(__get_PRIMASK()==masked);
    observed=osEventFlagsGet(events[16]);
    CHECK(osRtxInfo.isr_queue.cnt==(phase==1?16:1));
    if(masked)__enable_irq();
}
static void start_waiter(unsigned index,osPriority_t priority) {
    osThreadAttr_t attr={.name="event-priority",.priority=priority,.stack_size=1024};
    osThreadId_t thread=osThreadNew(waiter,(void *)(uintptr_t)index,&attr);CHECK(thread);
    CHECK(osThreadGetState(thread)==osThreadBlocked);
}
static void multiple_waiters(void) {
    for(unsigned cycle=0;cycle<2;cycle++)for(unsigned mode=0;mode<4;mode++)
    for(unsigned mask=0;mask<2;mask++)for(unsigned full=0;full<2;full++) {
        for(unsigned i=0;i<17;i++)CHECK(!(osEventFlagsClear(events[i],0x7fffffff)&osFlagsError));
        wanted=(mode&1)?3:1;initial=(mode&1)?5:4;update=(mode&1)?2:1;
        options=((mode&1)?osFlagsWaitAll:osFlagsWaitAny)|((mode&2)?osFlagsNoClear:0);
        CHECK(osEventFlagsSet(events[16],initial)==initial);completed=0;
        start_waiter(1,osPriorityAboveNormal);start_waiter(2,osPriorityHigh);start_waiter(3,osPriorityHigh);
        phase=1;fill=full?16:15;masked=mask;pend_irq();
        if(full) {
            CHECK(returned==osFlagsErrorResource && !completed && observed==initial);
            phase=2;pend_irq();
        }
        CHECK(returned==(initial|update));
        if(options&osFlagsNoClear)CHECK(completed==3);
        else {
            CHECK(completed==1 && wake_order[0]==2);
            update=wanted;phase=2;pend_irq();CHECK(completed==2 && wake_order[1]==3);
            pend_irq();CHECK(completed==3 && wake_order[2]==1);
        }
        CHECK(wake_order[0]==2 && wake_order[1]==3 && wake_order[2]==1);
        CHECK(wait_value==(initial|wanted));
        CHECK(osEventFlagsGet(events[16])==((options&osFlagsNoClear)?(initial|wanted):4));
        CHECK(!osRtxInfo.isr_queue.cnt && !((os_event_flags_t *)events[16])->thread_list);
        CHECK(!__get_PRIMASK());cases++;
    }
    CHECK(!notifications);
    for(unsigned i=0;i<17;i++)CHECK(osEventFlagsDelete(events[i])==osOK);
    output("MULTI cases=");number(cases);output(" recovered=1\n");finish(0);
}
static void legacy_callbacks(void) {
    for(unsigned count=14;count<=16;count++) {
        unsigned prior=notifications;
        __disable_irq();
        for(unsigned i=0;i<count;i++)osRtxPostProcess(osRtxObject(events[i]));
        CHECK(osRtxInfo.isr_queue.cnt==count && notifications==prior);
        osRtxPostProcess(osRtxObject(events[16]));
        CHECK(osRtxInfo.isr_queue.cnt==(count==16?16:count+1));
        CHECK(notifications==prior+(count==16));
        CHECK(__get_PRIMASK()==1);
        __enable_irq();__DSB();__ISB();
        CHECK(!osRtxInfo.isr_queue.cnt && !__get_PRIMASK());
        for(unsigned i=0;i<17;i++)CHECK(!osEventFlagsGet(events[i]));
    }
    CHECK(notifications==1);
    for(unsigned i=0;i<17;i++)CHECK(osEventFlagsDelete(events[i])==osOK);
    output("LEGACY cases=0x00000003 overflows=0x00000001 recovered=1\n");finish(0);
}
static void main_task(void *p) {
    (void)p;SysTick->CTRL=0;
    CHECK(osRtxInfo.isr_queue.max==16);
    NVIC_SetPriority((IRQn_Type)0,5);NVIC_EnableIRQ((IRQn_Type)0);
    for(unsigned i=0;i<17;i++){events[i]=osEventFlagsNew(NULL);CHECK(events[i]);}
    if(TEST_EVENT_SCENARIO==3)legacy_callbacks();
    if(TEST_EVENT_SCENARIO==1)multiple_waiters();
    for(unsigned cycle=0;cycle<4;cycle++)for(unsigned mode=0;mode<4;mode++)
    for(unsigned mask=0;mask<2;mask++)for(unsigned full=0;full<2;full++) {
        for(unsigned i=0;i<17;i++)CHECK(!(osEventFlagsClear(events[i],0x7fffffff)&osFlagsError));
        wanted=(mode&1)?3:1;initial=(mode&1)?5:4;update=(mode&1)?2:1;
        options=((mode&1)?osFlagsWaitAll:osFlagsWaitAny)|((mode&2)?osFlagsNoClear:0);
        CHECK(osEventFlagsSet(events[16],initial)==initial);
        completed=0;wait_value=0;
        osThreadAttr_t attr={.name="event-waiter",.priority=osPriorityAboveNormal,.stack_size=1024};
        osThreadId_t thread=osThreadNew(waiter,NULL,&attr);CHECK(thread);
        CHECK(!completed && osThreadGetState(thread)==osThreadBlocked);
        phase=1;fill=full?16:15;masked=mask;pend_irq();
        CHECK(!osRtxInfo.isr_queue.cnt && !__get_PRIMASK());
        if(returned&osFlagsError) {
            CHECK(full && returned==osFlagsErrorResource);
            CHECK(!completed && observed==initial && osEventFlagsGet(events[16])==initial);
            refusals++;
        } else {
            CHECK(returned==(initial|update) && observed==(initial|update));
            if(!completed) {
                CHECK(full && osThreadGetState(thread)==osThreadBlocked);
                CHECK(osEventFlagsGet(events[16])==(initial|update));lost++;
            } else {CHECK(!full);controls++;}
        }
        if(!completed) {
            /* Retry only after actual PendSV capacity recovery. */
            phase=2;pend_irq();CHECK(completed==1);retries++;
        }
        CHECK(completed==1 && wait_value==(initial|update));
        CHECK(osEventFlagsGet(events[16])==((options&osFlagsNoClear)?(initial|update):4));
        CHECK(!((os_event_flags_t *)events[16])->thread_list);cases++;
    }
    for(unsigned i=0;i<17;i++)CHECK(osEventFlagsDelete(events[i])==osOK);
    output("EVENT cases=");number(cases);output(" lost=");number(lost);
    output(" refused=");number(refusals);output(" controls=");number(controls);
    output(" retries=");number(retries);output(" notifications=");number(notifications);
    output(" recovered=1\n");finish(lost?42:0);
}
