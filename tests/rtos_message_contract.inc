/* Adapted by papsg from paps-g SSR-068 (8b5cbe9b). Test-only real
 * RTX APIs, SVC/IRQ/PendSV and blocked threads; no board services. */
static osMessageQueueId_t queue;
static unsigned capacity,msg_size,active_messages,phase;
static unsigned cases,gets,reput_failures,thread_controls,after_controls;
static unsigned last_used,last_pending;
static unsigned irq_mask;
static uint8_t incoming[16][64],replacement[16][64],order[16];
static volatile unsigned walk_armed,walk_step,walk_target,walk_fired,walk_origin;
static volatile unsigned waiter_done,wake_order[4];
static unsigned waiter_kind,waiter_index,waiter_reput;
static void pend_irq(void) {NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();}
static uint8_t priority(unsigned index,int second) {
    return (uint8_t)(((index*(second?3u:5u))%4u)*40u);
}
static void prepare(unsigned count,int second) {
    for(unsigned i=0;i<count;i++)order[i]=(uint8_t)i;
    for(unsigned i=1;i<count;i++) {
        uint8_t value=order[i];unsigned j=i;
        while(j && priority(order[j-1],second)<priority(value,second)) {
            order[j]=order[j-1];j--;
        }
        order[j]=value;
    }
}
static void pattern(uint8_t *bytes,unsigned index,unsigned base) {
    for(unsigned i=0;i<64;i++)bytes[i]=(uint8_t)(base+index+i*17);
}
static void get_checked(unsigned index,int second,unsigned nullable) {
    uint8_t bytes[66],prio=255;
    for(unsigned i=0;i<66;i++)bytes[i]=0xee;
    CHECK(osMessageQueueGet(queue,bytes+1,nullable?NULL:&prio,0)==osOK);
    const uint8_t *want=second?replacement[index]:incoming[index];
    for(unsigned i=0;i<msg_size;i++)CHECK(bytes[i+1]==want[i]);
    CHECK(bytes[0]==0xee && bytes[msg_size+1]==0xee);
    if(!nullable)CHECK(prio==priority(index,second));
}
static void queue_irq(void) {
    CHECK(__get_IPSR()==16 && !__get_PRIMASK());
    if(TEST_MESSAGE_SCENARIO==2) {
        CHECK(osMessageQueuePut(queue,incoming[15],240,0)==osOK);
        return;
    }
    if(TEST_MESSAGE_SCENARIO==1) {
        if(waiter_kind==1) {
            CHECK(osMessageQueuePut(queue,replacement[0],173,0)==osOK);
        } else {
            uint8_t bytes[64],prio;
            unsigned count=waiter_kind==3?capacity:1;
            for(unsigned n=0;n<count;n++) {
                CHECK(osMessageQueueGet(queue,bytes,&prio,0)==osOK);
                for(unsigned i=0;i<msg_size;i++)CHECK(bytes[i]==incoming[waiter_index][i]);
                CHECK(prio==priority(waiter_index,0));
                if(waiter_reput)CHECK(osMessageQueuePut(queue,replacement[0],173,0)==osOK);
            }
        }
        return;
    }
    if(irq_mask)__disable_irq();
    for(unsigned i=0;i<active_messages;i++) {
        get_checked(order[i],0,i%2);gets++;
        CHECK(__get_PRIMASK()==irq_mask);
        CHECK(osMessageQueueGetCount(queue)==active_messages-i-1);
        CHECK(osMessageQueueGetSpace(queue)==capacity-active_messages+i+1);
    }
    last_used=((os_message_queue_t *)queue)->mp_info.used_blocks;
    last_pending=osRtxInfo.isr_queue.cnt;
    if(phase==1)for(unsigned i=0;i<capacity;i++) {
        osStatus_t status=osMessageQueuePut(queue,replacement[i],priority(i,1),0);
        CHECK(__get_PRIMASK()==irq_mask);
        if(status!=osOK){CHECK(status==osErrorResource);reput_failures++;}
    }
    if(irq_mask)__enable_irq();
}
/* Test hook selects only an actual higher-priority interrupt schedule. */
void message_walk_probe(os_message_queue_t *mq) {
    if(!walk_armed || mq!=(os_message_queue_t *)queue)return;
    CHECK(__get_PRIMASK()==1 && __get_IPSR()==walk_origin);
    if(++walk_step==walk_target) {
        walk_armed=0;
        NVIC_SetPendingIRQ((IRQn_Type)1);__DSB();__ISB();
    }
}
static void walk_irq(void) {
    CHECK(__get_IPSR()==17 && !__get_PRIMASK());walk_fired++;
    uint8_t bytes[64],prio;
    CHECK(osMessageQueueGet(queue,bytes,&prio,0)==osOK);
    CHECK(prio==0);
    for(unsigned i=0;i<msg_size;i++)CHECK(bytes[i]==incoming[0][i]);
    CHECK(osMessageQueuePut(queue,replacement[0],0,0)==osOK);
}
static void expect_bytes(const uint8_t *want,uint8_t priority_value) {
    uint8_t bytes[66],prio=255;
    for(unsigned i=0;i<66;i++)bytes[i]=0xee;
    CHECK(osMessageQueueGet(queue,bytes+1,&prio,0)==osOK);
    for(unsigned i=0;i<msg_size;i++)CHECK(bytes[i+1]==want[i]);
    CHECK(prio==priority_value && bytes[0]==0xee && bytes[msg_size+1]==0xee);
}
static void walks(void) {
    const unsigned counts[]={2,3,8,16},sizes[]={1,4,12,64};
    for(unsigned origin=0;origin<2;origin++)for(unsigned c=0;c<4;c++)
    for(unsigned s=0;s<4;s++)for(unsigned step=1;step<counts[c];step++) {
        capacity=counts[c];msg_size=sizes[s];
        queue=osMessageQueueNew(capacity,msg_size,NULL);CHECK(queue);
        for(unsigned i=0;i<capacity-1;i++) {
            pattern(incoming[i],i,0x20);
            CHECK(osMessageQueuePut(queue,incoming[i],0,0)==osOK);
        }
        pattern(incoming[15],15,0x40);pattern(replacement[0],0,0x80);
        walk_origin=origin?14:11;walk_target=step;walk_step=0;walk_armed=1;
        unsigned fired=walk_fired;
        if(origin)pend_irq();
        else CHECK(osMessageQueuePut(queue,incoming[15],240,0)==osOK);
        CHECK(!walk_armed && walk_fired==fired+1 && !osRtxInfo.isr_queue.cnt);
        CHECK(osMessageQueueGetCount(queue)==capacity);
        expect_bytes(incoming[15],240);
        for(unsigned i=1;i<capacity-1;i++)expect_bytes(incoming[i],0);
        expect_bytes(replacement[0],0);
        CHECK(!osMessageQueueGetCount(queue));
        CHECK(!((os_message_queue_t *)queue)->mp_info.used_blocks);
        CHECK(osMessageQueueDelete(queue)==osOK);cases++;
    }
    output("WALK cases=");number(cases);output(" interrupts=");number(walk_fired);
    output(" recovered=1\n");finish(0);
}
static void waiter_task(void *p) {
    unsigned index=(unsigned)(uintptr_t)p;
    if(waiter_kind==1) {
        uint8_t bytes[64],prio=0;
        CHECK(osMessageQueueGet(queue,bytes,&prio,osWaitForever)==osOK);
        CHECK(prio==173);
        for(unsigned i=0;i<msg_size;i++)CHECK(bytes[i]==replacement[0][i]);
    } else {
        CHECK(osMessageQueuePut(queue,incoming[index],priority(index,0),osWaitForever)==osOK);
    }
    CHECK(waiter_done<4);wake_order[waiter_done++]=index;osThreadExit();
}
static osThreadId_t start_waiter(unsigned index,osPriority_t priority_value) {
    osThreadAttr_t attr={.name="queue-waiter",.priority=priority_value,.stack_size=1024};
    osThreadId_t thread=osThreadNew(waiter_task,(void *)(uintptr_t)index,&attr);CHECK(thread);
    CHECK(osThreadGetState(thread)==osThreadBlocked);
    return thread;
}
static void waiters(void) {
    const unsigned sizes[]={1,4,12,64};
    for(unsigned s=0;s<4;s++)for(unsigned cycle=0;cycle<4;cycle++)
    for(unsigned kind=0;kind<4;kind++) {
        msg_size=sizes[s];capacity=kind==3?3:1;
        queue=osMessageQueueNew(capacity,msg_size,NULL);CHECK(queue);
        waiter_done=0;waiter_kind=kind;waiter_reput=(kind==2);waiter_index=0;
        for(unsigned i=0;i<4;i++)pattern(incoming[i],i,0x20+cycle);
        pattern(replacement[0],0,0x80+cycle);
        if(kind!=1)for(unsigned i=0;i<capacity;i++)
            CHECK(osMessageQueuePut(queue,incoming[0],priority(0,0),0)==osOK);
        start_waiter(1,osPriorityAboveNormal);
        if(kind==3) {
            start_waiter(2,osPriorityHigh);start_waiter(3,osPriorityHigh);
            /* Three reads in one IRQ produce three queue callbacks. */
            pend_irq();
            CHECK(waiter_done==3 && wake_order[0]==2 && wake_order[1]==3 && wake_order[2]==1);
            /* Independent known priorities: index 3=120, 2=80, 1=40. */
            expect_bytes(incoming[3],120);expect_bytes(incoming[2],80);expect_bytes(incoming[1],40);
        } else {
            pend_irq();
            if(kind==2) {
                CHECK(waiter_done==0 && ((os_message_queue_t *)queue)->thread_list);
                expect_bytes(replacement[0],173);
            }
            CHECK(waiter_done==1 && wake_order[0]==1);
            if(kind!=1)expect_bytes(incoming[1],priority(1,0));
        }
        CHECK(!osMessageQueueGetCount(queue) && !osRtxInfo.isr_queue.cnt);
        CHECK(!((os_message_queue_t *)queue)->mp_info.used_blocks);
        CHECK(!((os_message_queue_t *)queue)->thread_list);
        CHECK(osMessageQueueDelete(queue)==osOK);cases++;
    }
    output("WAITER cases=");number(cases);output(" recovered=1\n");finish(0);
}
static void basics(void) {
    const unsigned counts[]={1,2,8,16},sizes[]={1,4,12,64};
    for(unsigned c=0;c<4;c++)for(unsigned s=0;s<4;s++) {
        capacity=counts[c];msg_size=sizes[s];
        queue=osMessageQueueNew(capacity,msg_size,NULL);CHECK(queue);
        CHECK(osMessageQueueGetCapacity(queue)==capacity);
        CHECK(osMessageQueueGetMsgSize(queue)==msg_size);
        for(unsigned cycle=0;cycle<4;cycle++) {
            for(unsigned i=0;i<capacity;i++) {
                pattern(incoming[i],i,0x20+cycle);pattern(replacement[i],i,0x60+cycle);
                CHECK(osMessageQueuePut(queue,incoming[i],priority(i,0),0)==osOK);
            }
            unsigned before=reput_failures;
            prepare(capacity,0);active_messages=capacity;phase=1;irq_mask=cycle%2;pend_irq();
            CHECK(!osRtxInfo.isr_queue.cnt && !__get_PRIMASK());
            unsigned failed=reput_failures-before;
            CHECK(failed==0 || failed==capacity);
            CHECK(osMessageQueueGetCount(queue)==capacity-failed);
            if(!failed) {
                prepare(capacity,1);
                for(unsigned i=0;i<capacity;i++)get_checked(order[i],1,i%2);
            }
            CHECK(osMessageQueueGetCount(queue)==0 && osMessageQueueGetSpace(queue)==capacity);
            CHECK(((os_message_queue_t *)queue)->mp_info.used_blocks==0);
            /* Thread Get/Put and IRQ Get followed by thread Put controls. */
            CHECK(osMessageQueuePut(queue,incoming[0],priority(0,0),0)==osOK);
            get_checked(0,0,0);
            CHECK(osMessageQueuePut(queue,incoming[0],priority(0,0),0)==osOK);
            thread_controls++;
            prepare(1,0);active_messages=1;phase=2;irq_mask=0;pend_irq();
            CHECK(!osRtxInfo.isr_queue.cnt && !osMessageQueueGetCount(queue));
            CHECK(osMessageQueuePut(queue,replacement[0],priority(0,1),0)==osOK);
            get_checked(0,1,0);after_controls++;
            /* Reset consumes the same detach helper and must return every slot. */
            for(unsigned i=0;i<capacity;i++)
                CHECK(osMessageQueuePut(queue,incoming[i],priority(i,0),0)==osOK);
            CHECK(osMessageQueueReset(queue)==osOK);
            CHECK(!osMessageQueueGetCount(queue) && osMessageQueueGetSpace(queue)==capacity);
            CHECK(!((os_message_queue_t *)queue)->mp_info.used_blocks);
            CHECK(osMessageQueuePut(queue,replacement[0],priority(0,1),0)==osOK);
            get_checked(0,1,0);cases++;
        }
        CHECK(osMessageQueueDelete(queue)==osOK);
    }
    output("MESSAGE cases=");number(cases);output(" gets=");number(gets);
    output(" failed_reputs=");number(reput_failures);
    output(" thread_controls=");number(thread_controls);output(" after_controls=");number(after_controls);
    output(" last_used=");number(last_used);output(" last_pending=");number(last_pending);
    output(" recovered=1\n");finish(reput_failures?42:0);
}
static void main_task(void *p) {
    (void)p;SysTick->CTRL=0;
    NVIC_SetPriority((IRQn_Type)0,5);NVIC_EnableIRQ((IRQn_Type)0);
    NVIC_SetPriority((IRQn_Type)1,4);NVIC_EnableIRQ((IRQn_Type)1);
    if(TEST_MESSAGE_SCENARIO==2)walks();
    if(TEST_MESSAGE_SCENARIO==1)waiters();
    basics();
}
