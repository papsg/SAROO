/* Adapted by papsg from paps-g SSR-124 (f987e9d4). Test-only public RTX
 * pool/event APIs with actual M7 SVC, IRQ and PendSV paths. */
static osMemoryPoolId_t pool,foreign_pool;
static osEventFlagsId_t admission_events[16];
static osThreadId_t admission_threads[3];
static void *owned[3],*received[3],*foreign_block;
static unsigned block_size,capacity,context,stage,fill,warmup,attempts,releases;
static volatile unsigned done,order[3],bad,pending,used;
static volatile osStatus_t status;
static osStatus_t expected;
static unsigned cases;
static void pend_pool_irq(void) {
    NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();
}
static void admission_waiter(void *argument) {
    unsigned index=(unsigned)(uintptr_t)argument;
    void *block=osMemoryPoolAlloc(pool,osWaitForever);
    CHECK(block && index<3 && done<3);
    received[index]=block;order[done++]=index;
    *(uint32_t *)block=0x5a000000u+index;
    osThreadExit();
}
static void produce(void) {
    if (context==1) __disable_irq();
    CHECK(__get_PRIMASK()==(context!=0));
    if (stage==0) {
        for (unsigned i=0; i<warmup; i++)
            CHECK(osEventFlagsSet(admission_events[0],1)==1);
    } else {
        for (unsigned i=0; i<fill; i++)
            CHECK(osEventFlagsSet(admission_events[i],1)==1);
        CHECK(osRtxInfo.isr_queue.cnt==fill);
        unsigned initial=osMemoryPoolGetCount(pool);
        CHECK(osMemoryPoolFree(NULL,owned[0])==osErrorParameter);
        CHECK(osMemoryPoolFree(pool,NULL)==osErrorParameter);
        CHECK(osMemoryPoolFree(pool,foreign_block)==osErrorParameter);
        CHECK(osMemoryPoolGetCount(pool)==initial && osRtxInfo.isr_queue.cnt==fill);
        for (unsigned i=0; i<attempts; i++) {
            status=osMemoryPoolFree(pool,owned[0]);
            used=osMemoryPoolGetCount(pool);pending=osRtxInfo.isr_queue.cnt;
            unsigned callback=TEST_POOL_FREE_SCENARIO!=2 && expected==osOK;
            if (status!=expected || used!=initial-(expected==osOK) ||
                pending!=fill+callback || notifications) {bad=1;break;}
            if (status==osErrorResource) {
                for (unsigned j=0; j<block_size; j++)
                    CHECK(((uint8_t *)owned[0])[j]==(uint8_t)(0xa0u+j));
            } else if (TEST_POOL_FREE_SCENARIO==2) {
                /* No waiter: reuse without a needless deferred callback. */
                owned[0]=osMemoryPoolAlloc(pool,0);
                CHECK(owned[0]!=NULL);
            }
        }
        if (!bad && TEST_POOL_FREE_SCENARIO==3) {
            for (unsigned i=1; i<releases; i++) {
                osStatus_t result=osMemoryPoolFree(pool,owned[i]);
                if (result!=(i==2?osErrorResource:osOK)) {
                    output("POOL-WAIT i=");number(i);
                    output(" result=");number((uint32_t)result);
                    output(" pending=");number(osRtxInfo.isr_queue.cnt);
                    output(" used=");number(osMemoryPoolGetCount(pool));output("\n");
                }
                CHECK(result==(i==2?osErrorResource:osOK));
                if (i==2) for (unsigned j=0; j<block_size; j++)
                    CHECK(((uint8_t *)owned[i])[j]==(uint8_t)(0xa0u+j));
            }
        }
    }
    CHECK(__get_PRIMASK()==(context!=0));
    if (context==1) __enable_irq();
}
static void pool_irq(void) {
    CHECK(__get_IPSR()==16 && !__get_PRIMASK());produce();
}
static void invoke(void) {
    CHECK(!__get_IPSR() && !__get_PRIMASK());
    if (context==2) {
        __disable_irq();produce();__enable_irq();__DSB();__ISB();
    } else pend_pool_irq();
    CHECK(!osRtxInfo.isr_queue.cnt && !__get_PRIMASK());
    if (bad) {
        output("POOL-ADMISSION status=");number((uint32_t)status);
        output(" irq_used=");number(used);output(" pending=");number(pending);
        output(" count=");number(osMemoryPoolGetCount(pool));
        output(" space=");number(osMemoryPoolGetSpace(pool));
        output(" done=");number(done);output(" errors=");number(notifications);
        output(" state=");number(TEST_POOL_FREE_SCENARIO==2?0:
                               (uint32_t)osThreadGetState(admission_threads[0]));
        output("\n");CHECK(!bad);
    }
}
static void main_task(void *argument) {
    (void)argument;SysTick->CTRL=0;
    CHECK(osRtxInfo.isr_queue.max==16);
    NVIC_SetPriority((IRQn_Type)0,5);NVIC_EnableIRQ((IRQn_Type)0);
    for (unsigned i=0; i<16; i++) {
        admission_events[i]=osEventFlagsNew(NULL);CHECK(admission_events[i]);
    }
    foreign_pool=osMemoryPoolNew(1,4,NULL);CHECK(foreign_pool);
    foreign_block=osMemoryPoolAlloc(foreign_pool,0);CHECK(foreign_block);
    const unsigned sizes[]={4,8,12,64};
    for (unsigned offset=0; offset<16; offset++)
    for (context=0; context<3; context++)
    for (unsigned size=0; size<4; size++) {
        capacity=TEST_POOL_FREE_SCENARIO==3?3:1;block_size=sizes[size];
        done=bad=notifications=0;releases=capacity;
        pool=osMemoryPoolNew(capacity,block_size,NULL);CHECK(pool);
        for (unsigned i=0; i<capacity; i++) {
            owned[i]=osMemoryPoolAlloc(pool,0);CHECK(owned[i]);
            for (unsigned j=0; j<block_size; j++)
                ((uint8_t *)owned[i])[j]=(uint8_t)(0xa0u+j);
            received[i]=NULL;
        }
        void *first=owned[0];
        if (TEST_POOL_FREE_SCENARIO!=2) for (unsigned i=0; i<capacity; i++) {
            osThreadAttr_t attr={.name="pool-admission",
                                 .priority=i?osPriorityHigh:osPriorityAboveNormal,
                                 .stack_size=1024};
            admission_threads[i]=osThreadNew(admission_waiter,(void *)(uintptr_t)i,&attr);
            CHECK(admission_threads[i] && osThreadGetState(admission_threads[i])==osThreadBlocked);
        }
        stage=0;warmup=(offset+16-osRtxInfo.isr_queue.in)%16;
        invoke();CHECK(osRtxInfo.isr_queue.in==offset);
        stage=1;fill=TEST_POOL_FREE_SCENARIO==1?15:
                     TEST_POOL_FREE_SCENARIO==3?14:16;
        expected=TEST_POOL_FREE_SCENARIO==0?osErrorResource:osOK;
        attempts=TEST_POOL_FREE_SCENARIO==0 || TEST_POOL_FREE_SCENARIO==2?3:1;
        invoke();
        if (TEST_POOL_FREE_SCENARIO==0) {
            CHECK(!done && osThreadGetState(admission_threads[0])==osThreadBlocked);
            CHECK(osMemoryPoolGetCount(pool)==capacity && osMemoryPoolGetSpace(pool)==0);
            fill=0;attempts=1;expected=osOK;invoke();
        } else if (TEST_POOL_FREE_SCENARIO==3) {
            CHECK(done==2 && order[0]==1 && order[1]==2);
            CHECK(osThreadGetState(admission_threads[0])==osThreadBlocked);
            CHECK(osMemoryPoolGetCount(pool)==capacity && osMemoryPoolGetSpace(pool)==0);
            owned[0]=owned[2];fill=0;attempts=1;releases=1;
            expected=osOK;invoke();
            CHECK(done==3 && order[2]==0);
        }
        if (TEST_POOL_FREE_SCENARIO==2) {
            CHECK(done==0 && osMemoryPoolGetCount(pool)==1);
            CHECK(osMemoryPoolFree(pool,owned[0])==osOK);
        } else {
            CHECK(done==capacity && osMemoryPoolGetCount(pool)==capacity);
            for (unsigned i=0; i<capacity; i++) {
                CHECK(received[i] && *(uint32_t *)received[i]==0x5a000000u+i);
                for (unsigned j=0; j<i; j++) CHECK(received[i]!=received[j]);
                if (capacity==1) CHECK(received[i]==first);
                CHECK(osMemoryPoolFree(pool,received[i])==osOK);
            }
        }
        CHECK(osMemoryPoolGetCount(pool)==0 && osMemoryPoolGetSpace(pool)==capacity);
        CHECK(((os_memory_pool_t *)pool)->thread_list==NULL && notifications==0);
        CHECK(osMemoryPoolDelete(pool)==osOK);cases++;
    }
    CHECK(osMemoryPoolFree(foreign_pool,foreign_block)==osOK);
    CHECK(osMemoryPoolDelete(foreign_pool)==osOK);
    for (unsigned i=0; i<16; i++) CHECK(osEventFlagsDelete(admission_events[i])==osOK);
    output("POOL-ADMIT cases=");number(cases);
    output(" scenario=");number(TEST_POOL_FREE_SCENARIO);
    output(" recovered=1\n");finish(0);
}
