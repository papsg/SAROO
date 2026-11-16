/* Papsg's event-wakeup check for the SSR-069 ring-retirement prerequisite,
 * adapted from paps-g 6e96f528. Real APIs, IRQs, PendSV and blocked waiter. */
static osEventFlagsId_t retire_events[17];
static volatile unsigned seen[17], occupancy, armed, phase, warm;
static volatile unsigned first_done, second_done, callbacks, completed;
static volatile uint32_t wait_value;
static void (*original_callback)(os_event_flags_t *);

static void observe_callback(os_event_flags_t *event) {
    unsigned index = 0;
    while (index < 17 && retire_events[index] != (osEventFlagsId_t)event) index++;
    CHECK(index < 17);
    seen[index]++;
    callbacks++;
    original_callback(event);
}

static void retire_waiter(void *argument) {
    (void)argument;
    wait_value = osEventFlagsWait(retire_events[0], 1, osFlagsNoClear, osWaitForever);
    completed++;
    osThreadExit();
}

void queue_retire_probe(uint16_t *count) {
    if (armed && count == &osRtxInfo.isr_queue.cnt) {
        CHECK(__get_IPSR() == 14 && !__get_PRIMASK());
        armed = 0;
        NVIC_SetPendingIRQ((IRQn_Type)1);
        __DSB(); __ISB();
    }
}

static void first_irq(void) {
    CHECK(__get_IPSR() == 16);
    if (!phase) {
        for (unsigned i = 0; i < warm; i++)
            CHECK(osEventFlagsSet(retire_events[0], 0) == 0);
        return;
    }
    for (unsigned i = 0; i < occupancy; i++)
        CHECK(osEventFlagsSet(retire_events[i], 1) == 1);
    CHECK(osRtxInfo.isr_queue.cnt == occupancy);
    first_done = 1;
}

static void second_irq(void) {
    CHECK(__get_IPSR() == 17);
    CHECK(osEventFlagsSet(retire_events[16], 1) == 1);
    second_done = 1;
}

static void invoke_first(void) {
    NVIC_SetPendingIRQ((IRQn_Type)0);
    __DSB(); __ISB();
}

static void main_task(void *argument) {
    (void)argument;
    SysTick->CTRL = 0;
    CHECK(osRtxInfo.isr_queue.max == 16);
    for (unsigned i = 0; i < 17; i++) {
        retire_events[i] = osEventFlagsNew(NULL);
        CHECK(retire_events[i]);
    }
    original_callback = osRtxInfo.post_process.event_flags;
    osRtxInfo.post_process.event_flags = observe_callback;
    NVIC_SetPriority((IRQn_Type)0, 6); NVIC_EnableIRQ((IRQn_Type)0);
    NVIC_SetPriority((IRQn_Type)1, 5); NVIC_EnableIRQ((IRQn_Type)1);
    unsigned cases = 0, lost = 0;
    for (unsigned offset = 0; offset < 16; offset++) {
        for (occupancy = 1; occupancy <= 16; occupancy++) {
            for (unsigned i = 0; i < 17; i++)
                CHECK(!(osEventFlagsClear(retire_events[i], 0x7fffffff) & osFlagsError));
            phase = armed = 0;
            warm = (offset + 16 - osRtxInfo.isr_queue.in) % 16;
            invoke_first();
            CHECK(!osRtxInfo.isr_queue.cnt && osRtxInfo.isr_queue.in == offset &&
                  osRtxInfo.isr_queue.out == offset);
            for (unsigned i = 0; i < 17; i++) seen[i] = 0;
            callbacks = first_done = second_done = completed = 0;
            osThreadAttr_t attr = {.name="retire-waiter", .priority=osPriorityAboveNormal, .stack_size=1024};
            osThreadId_t waiter = osThreadNew(retire_waiter, NULL, &attr);
            CHECK(waiter && osThreadGetState(waiter) == osThreadBlocked);
            phase = armed = 1;
            invoke_first();
            CHECK(first_done && second_done && !armed && !osRtxInfo.isr_queue.cnt);
            CHECK(callbacks == occupancy + 1);
            if (seen[0] != 1 || seen[16] != 1) {
                CHECK(occupancy == 16 && !seen[0] && seen[16] == 2);
                CHECK(!completed && osThreadGetState(waiter) == osThreadBlocked);
                CHECK(osEventFlagsGet(retire_events[0]) == 1);
                lost++;
                /* An ordinary thread-context Set recovers the lost wakeup. */
                CHECK(osEventFlagsSet(retire_events[0], 1) == 1);
            }
            CHECK(completed == 1 && wait_value == 1);
            for (unsigned i = 1; i < 16; i++) CHECK(seen[i] == (i < occupancy));
            CHECK(!osRtxEventFlagsId(retire_events[0])->thread_list);
            cases++;
        }
    }
    for (unsigned i = 0; i < 17; i++) CHECK(osEventFlagsDelete(retire_events[i]) == osOK);
    output("RETIRE cases="); number(cases);
    output(" lost="); number(lost);
    output(" controls="); number(cases-lost);
    output(" recovered=1\n");
    finish(lost ? 42 : 0);
}
