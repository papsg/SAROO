/* Adapted by papsg from paps-g SSR-123 (8d211442). Test-only public RTX
 * operations on M7 SVC, IRQ and PendSV paths; no board services. */
static osMessageQueueId_t admission_queue;
static osEventFlagsId_t admission_events[16];
static uint8_t admission_existing[4][64], admission_payload[64];
static unsigned admission_size, admission_capacity, admission_base;
static unsigned admission_context, admission_stage, admission_fill;
static unsigned admission_warmup, admission_attempts, admission_cases;
static volatile unsigned admission_waiter_done;
static osStatus_t admission_expected;

static void admission_pattern(uint8_t *bytes, unsigned index) {
    for (unsigned i=0; i<64; i++) bytes[i]=(uint8_t)(0x20+index+i*17);
}
static void admission_get(const uint8_t *wanted, uint8_t wanted_priority) {
    uint8_t bytes[66],priority=255;
    memset(bytes,0xee,sizeof(bytes));
    CHECK(osMessageQueueGet(admission_queue,bytes+1,&priority,0)==osOK);
    CHECK(bytes[0]==0xee && bytes[admission_size+1]==0xee);
    CHECK(priority==wanted_priority);
    CHECK(memcmp(bytes+1,wanted,admission_size)==0);
}
static void admission_waiter(void *argument) {
    (void)argument;
    uint8_t bytes[64],priority=0;
    CHECK(osMessageQueueGet(admission_queue,bytes,&priority,osWaitForever)==osOK);
    CHECK(priority==173 && memcmp(bytes,admission_payload,admission_size)==0);
    admission_waiter_done++;
    osThreadExit();
}
static void admission_produce(void) {
    if (admission_context==1) __disable_irq();
    CHECK(__get_PRIMASK()==(admission_context!=0));
    if (admission_stage==0) {
        for (unsigned i=0; i<admission_warmup; i++)
            CHECK(osEventFlagsSet(admission_events[0],1)==1);
    } else {
        for (unsigned i=0; i<admission_fill; i++)
            CHECK(osEventFlagsSet(admission_events[i],1)==1);
        CHECK(osRtxInfo.isr_queue.cnt==admission_fill);
        CHECK(osMessageQueuePut(NULL,admission_payload,173,0)==osErrorParameter);
        CHECK(osMessageQueuePut(admission_queue,NULL,173,0)==osErrorParameter);
        CHECK(osMessageQueuePut(admission_queue,admission_payload,173,1)==osErrorParameter);
        CHECK(((os_message_queue_t *)admission_queue)->mp_info.used_blocks==admission_base);
        CHECK(osRtxInfo.isr_queue.cnt==admission_fill);
        for (unsigned i=0; i<admission_attempts; i++) {
            osStatus_t status=osMessageQueuePut(admission_queue,admission_payload,173,0);
            unsigned used=((os_message_queue_t *)admission_queue)->mp_info.used_blocks;
            if (status!=admission_expected || used!=admission_base+(status==osOK)) {
                output("ADMISSION status=");number((uint32_t)status);
                output(" used=");number(used);
                output(" pending=");number(osRtxInfo.isr_queue.cnt);
                output(" errors=");number(notifications);output("\n");
            }
            CHECK(status==admission_expected);
            CHECK(used==admission_base+(status==osOK));
            CHECK(osMessageQueueGetCount(admission_queue)==admission_base);
            CHECK(osRtxInfo.isr_queue.cnt==admission_fill+(status==osOK));
            CHECK(notifications==0);
        }
    }
    CHECK(__get_PRIMASK()==(admission_context!=0));
    if (admission_context==1) __enable_irq();
}
static void queue_irq(void) {
    CHECK(__get_IPSR()==16 && !__get_PRIMASK());
    admission_produce();
}
static void admission_invoke(void) {
    CHECK(!__get_IPSR() && !__get_PRIMASK());
    if (admission_context==2) {
        __disable_irq();admission_produce();__enable_irq();__DSB();__ISB();
    } else {
        NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();
    }
    CHECK(!__get_PRIMASK() && !osRtxInfo.isr_queue.cnt);
}
static void main_task(void *argument) {
    (void)argument;SysTick->CTRL=0;
    CHECK(osRtxInfo.isr_queue.max==16);
    NVIC_SetPriority((IRQn_Type)0,5);NVIC_EnableIRQ((IRQn_Type)0);
    for (unsigned i=0; i<16; i++) {
        admission_events[i]=osEventFlagsNew(NULL);
        CHECK(admission_events[i]);
    }
    const unsigned sizes[]={1,4,12,64},capacities[]={1,4};
    for (unsigned offset=0; offset<16; offset++)
    for (unsigned context=0; context<3; context++)
    for (unsigned size=0; size<4; size++)
    for (unsigned count=0; count<(TEST_MESSAGE_SCENARIO==6?1u:2u); count++) {
        admission_size=sizes[size];admission_capacity=capacities[count];
        admission_context=context;
        admission_queue=osMessageQueueNew(admission_capacity,admission_size,NULL);
        CHECK(admission_queue);
        admission_base=admission_capacity-1;notifications=0;
        for (unsigned i=0; i<admission_base; i++) {
            admission_pattern(admission_existing[i],i);
            CHECK(osMessageQueuePut(admission_queue,admission_existing[i],0,0)==osOK);
        }
        admission_pattern(admission_payload,4);
        admission_waiter_done=0;
        if (TEST_MESSAGE_SCENARIO==6) {
            osThreadAttr_t attr={.name="admission-receiver",
                                 .priority=osPriorityAboveNormal,.stack_size=1024};
            osThreadId_t receiver=osThreadNew(admission_waiter,NULL,&attr);
            CHECK(receiver && osThreadGetState(receiver)==osThreadBlocked);
        }
        /* Advance the real ring via public operations and actual PendSV drains. */
        admission_stage=0;
        admission_warmup=(offset+16-osRtxInfo.isr_queue.in)%16;
        admission_invoke();
        CHECK(osRtxInfo.isr_queue.in==offset);
        admission_stage=1;
        admission_fill=TEST_MESSAGE_SCENARIO==5?15:16;
        admission_attempts=admission_fill==16?3:1;
        admission_expected=admission_fill==16?osErrorResource:osOK;
        admission_invoke();
        if (TEST_MESSAGE_SCENARIO==6)
            CHECK(admission_waiter_done==0 &&
                  ((os_message_queue_t *)admission_queue)->thread_list!=NULL);
        CHECK(((os_message_queue_t *)admission_queue)->mp_info.used_blocks==
              admission_base+(admission_expected==osOK));
        CHECK(osMessageQueueGetCount(admission_queue)==
              admission_base+(admission_expected==osOK));
        if (admission_expected==osOK) admission_get(admission_payload,173);
        for (unsigned i=0; i<admission_base; i++) admission_get(admission_existing[i],0);
        CHECK(((os_message_queue_t *)admission_queue)->mp_info.used_blocks==0);
        admission_base=0;admission_fill=0;admission_attempts=1;
        admission_expected=osOK;
        admission_invoke();
        if (TEST_MESSAGE_SCENARIO==6) CHECK(admission_waiter_done==1);
        else {
            CHECK(osMessageQueueGetCount(admission_queue)==1);
            if (admission_capacity==1) {
                admission_base=1;admission_expected=osErrorResource;
                admission_invoke();
            }
            admission_get(admission_payload,173);
        }
        CHECK(osMessageQueueGetCount(admission_queue)==0);
        CHECK(((os_message_queue_t *)admission_queue)->mp_info.used_blocks==0);
        CHECK(((os_message_queue_t *)admission_queue)->thread_list==NULL);
        CHECK(notifications==0);
        CHECK(osMessageQueueDelete(admission_queue)==osOK);
        admission_cases++;
    }
    for (unsigned i=0; i<16; i++) CHECK(osEventFlagsDelete(admission_events[i])==osOK);
    output("ADMIT cases=");number(admission_cases);
    output(" scenario=");number(TEST_MESSAGE_SCENARIO);
    output(" notifications=");number(notifications);
    output(" recovered=1\n");finish(0);
}
