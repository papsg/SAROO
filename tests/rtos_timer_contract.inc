/* papsg adaptation of paps-g SSR-009. Optional software-timer application for the actual Cortex-M7 kernel fixture. */
static volatile uint32_t callback_ticks[2][4], callback_count[2];
static osSemaphoreId_t timer_signal;
static unsigned timer_phases;
static void timer_callback(void *argument) {
    CHECK(argument==(void *)0x51 || argument==(void *)0x52);
    unsigned index=(uintptr_t)argument-0x51;
    uint32_t slot=callback_count[index];
    CHECK(slot<4);
    callback_ticks[index][slot]=osKernelGetTickCount();
    callback_count[index]=slot+1;
    CHECK(osSemaphoreRelease(timer_signal)==osOK);
}
static void timer_reset_observation(void) {
    CHECK(osSemaphoreGetCount(timer_signal)==0);
    callback_count[0]=callback_count[1]=0;
}
static void timer_collect(unsigned count) {
    while(count--) CHECK(osSemaphoreAcquire(timer_signal,100)==osOK);
}
static void timer_phase(osTimerId_t timer,uint32_t first,uint32_t repeat,unsigned running) {
    timer_reset_observation();
    CHECK(osTimerStart(timer,first)==osOK);
    CHECK(osDelay(1)==osOK);
    if(!running) CHECK(osTimerStop(timer)==osOK);
    CHECK(osTimerStart(timer,repeat)==osOK);
    CHECK(osTimerIsRunning(timer));
    timer_collect(3);
    CHECK(osTimerStop(timer)==osOK);
    CHECK(callback_count[0]==3);
    uint32_t a=callback_ticks[0][1]-callback_ticks[0][0];
    uint32_t b=callback_ticks[0][2]-callback_ticks[0][1];
    output("TIMER interval=");number(a);output(",");number(b);output("\n");
    CHECK(a==repeat && b==repeat);
    timer_phases++;
}
static void main_task(void *argument) {
    CHECK(argument==(void *)0x9173);
    timer_signal=osSemaphoreNew(16,0,NULL);CHECK(timer_signal);
    CHECK(osTimerStart(NULL,1)==osErrorParameter);
    osTimerId_t timer=osTimerNew(timer_callback,osTimerPeriodic,(void *)0x51,NULL);
    CHECK(timer);
    timer_phase(timer,10,3,1);
    timer_phase(timer,10,3,0);
    timer_phase(timer,3,3,1);
    timer_phase(timer,3,7,1);
    CHECK(osTimerDelete(timer)==osOK);

    timer_reset_observation();
    timer=osTimerNew(timer_callback,osTimerOnce,(void *)0x51,NULL);CHECK(timer);
    CHECK(osTimerStart(timer,2)==osOK && osDelay(1)==osOK);
    uint32_t start=osKernelGetTickCount();
    CHECK(osTimerStart(timer,4)==osOK);
    CHECK(osTimerStart(timer,0)==osErrorParameter && osTimerIsRunning(timer));
    timer_collect(1);
    CHECK(callback_ticks[0][0]-start>=4);
    CHECK(osDelay(6)==osOK);
    CHECK(callback_count[0]==1 && !osTimerIsRunning(timer));
    CHECK(osTimerDelete(timer)==osOK);
    timer_phases++;

    timer_reset_observation();
    timer=osTimerNew(timer_callback,osTimerPeriodic,(void *)0x51,NULL);
    osTimerId_t other=osTimerNew(timer_callback,osTimerPeriodic,(void *)0x52,NULL);
    CHECK(timer && other);
    unsigned same_tick=0;
    for(unsigned attempt=0;attempt<8 && !same_tick;++attempt) {
        uint32_t tick=osKernelGetTickCount();
        CHECK(osTimerStart(timer,3)==osOK && osTimerStart(other,3)==osOK);
        same_tick=osKernelGetTickCount()==tick;
        if(!same_tick) {
            CHECK(osTimerStop(timer)==osOK && osTimerStop(other)==osOK);
            CHECK(!callback_count[0] && !callback_count[1]);
        }
    }
    CHECK(same_tick);
    timer_collect(6);
    CHECK(osTimerStop(timer)==osOK && osTimerStop(other)==osOK);
    CHECK(callback_count[0]==3 && callback_count[1]==3);
    for(unsigned i=0;i<3;++i) {
        CHECK(callback_ticks[0][i]==callback_ticks[1][i]);
        if(i) CHECK(callback_ticks[0][i]-callback_ticks[0][i-1]==3);
    }
    CHECK(osTimerDelete(timer)==osOK && osTimerDelete(other)==osOK);
    CHECK(osSemaphoreDelete(timer_signal)==osOK);
    timer_phases++;
    output("PASS timers=");number(timer_phases);output("\n");finish(0);
}
