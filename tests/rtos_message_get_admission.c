/* Adapted by papsg from paps-g b1cd920c (SSR-134).
 * Real public queue Get/Put, IRQ/SVC/PendSV and returning overflow hook.
 * SysTick is stopped; this fixture checks ownership, not physical timing. */
#include <stdint.h>
#include "rtx_lib.h"
uint32_t SystemCoreClock=25000000U;
static uint64_t boot_stack[512];
static osMessageQueueId_t queue;
static osEventFlagsId_t events[16];
static osSemaphoreId_t retire_gate;
static unsigned size,capacity,context,fill,phase,nullable;
static uint8_t payload[32][64];
static volatile unsigned done,notifications,gap_armed,gap_fired;
static volatile osStatus_t returned;
static volatile unsigned wake_order[3];
static unsigned cases,lost,refused,controls,naked,retries,gaps,empty_controls;
static unsigned read_token,read_priority,multi_removed;
static uint8_t present[32];
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
{ if(!valid){output("GET FAIL line=");number(line);output("\n");finish(2);} }
#define CHECK(x) require(!!(x),__LINE__)
uint32_t osRtxErrorNotify(uint32_t code,void *object)
{ CHECK(code==osRtxErrorISRQueueOverflow && object==queue);++notifications;return 0; }
void osRtxIdleThread(void *p) { (void)p;for(;;)__WFI(); }
static void fault(void) { output("FAULT\n");finish(4); }
static void pend_irq(void)
{ NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB(); }
static void sender(void *p)
{
    unsigned id=(unsigned)(uintptr_t)p;
    CHECK(osMessageQueuePut(queue,payload[20+id],200+id,osWaitForever)==osOK);
    CHECK(done<3);wake_order[done++]=id;
    (void)osSemaphoreAcquire(retire_gate,osWaitForever);CHECK(0);
}
static osStatus_t get_checked_irq(unsigned token,unsigned priority)
{
        os_message_queue_t *mq=queue;
        unsigned count=osMessageQueueGetCount(queue),used=mq->mp_info.used_blocks;
        unsigned queued=osRtxInfo.isr_queue.cnt;
        os_message_t *head=mq->msg_first;CHECK((head!=NULL)==(count!=0));
        uint8_t header[sizeof(*head)];if(head)memcpy(header,head,sizeof header);
        uint8_t bytes[66],prio=0xed;memset(bytes,0xcc,sizeof bytes);
        CHECK(osMessageQueueGet(NULL,bytes+1,&prio,0)==osErrorParameter);
        CHECK(osMessageQueueGet(queue,NULL,&prio,0)==osErrorParameter);
        CHECK(osMessageQueueGet(queue,bytes+1,&prio,1)==osErrorParameter);
        CHECK(osRtxInfo.isr_queue.cnt==queued && osMessageQueueGetCount(queue)==count);
        osStatus_t status=osMessageQueueGet(queue,bytes+1,nullable?NULL:&prio,0);
        if(status==osOK){
            CHECK(count && !memcmp(bytes+1,payload[token],size));
            CHECK(bytes[0]==0xcc && bytes[size+1]==0xcc);
            CHECK(prio==(nullable?0xed:priority));
            CHECK(osMessageQueueGetCount(queue)==count-1);
            CHECK(osMessageQueueGetSpace(queue)==capacity-count+1 && mq->mp_info.used_blocks==used-1);
            if(TEST_SCENARIO==7){CHECK(present[token]);present[token]=0;}
        }else{
            CHECK(status==osErrorResource && (!count || queued==16));
            CHECK(mq->msg_first==head && (!head || !memcmp(header,head,sizeof header)));
            CHECK(osMessageQueueGetCount(queue)==count && osMessageQueueGetSpace(queue)==capacity-count);
            CHECK(mq->mp_info.used_blocks==used && prio==0xed);
            for(unsigned i=0;i<sizeof bytes;++i)CHECK(bytes[i]==0xcc);
        }
        return status;
}
static void producer(void)
{
    CHECK(__get_IPSR()==(context==2?0U:16U));
    if(context==1)__disable_irq();
    CHECK(__get_PRIMASK()==(context!=0));
    for(unsigned i=0;i<fill;++i)CHECK(osEventFlagsSet(events[i],1)==1);
    if(phase){
        if(TEST_SCENARIO==7 && phase==1){
            multi_removed=0;
            for(unsigned i=0;i<3;++i){
                returned=get_checked_irq(i,128-i);
                if(returned!=osOK)break;
                ++multi_removed;
            }
        }else returned=get_checked_irq(read_token,read_priority);
    }
    CHECK(__get_PRIMASK()==(context!=0));
    if(context==1)__enable_irq();
}
void IRQ0_Handler(void) { producer(); }
static void invoke(void)
{
    CHECK(!__get_PRIMASK() && !__get_IPSR());
    if(context==2){__disable_irq();producer();__enable_irq();__DSB();__ISB();}
    else pend_irq();
    CHECK(!__get_PRIMASK() && !osRtxInfo.isr_queue.cnt);
}
/* Inserted only into the owned provider copy, immediately after failed
 * allocation and before the sender's wait-list publication. */
void queue_get_gap_probe(os_message_queue_t *mq)
{
    if(!gap_armed || mq!=(os_message_queue_t *)queue)return;
    CHECK(__get_IPSR()==11 && !mq->thread_list);
    /* If allocation/publication is protected, the real pending IRQ must wait
     * until that existing exclusion ends; never force it through PRIMASK. */
    gap_armed=0;++gap_fired;pend_irq();
}
static void check_read(unsigned id,unsigned priority)
{
    uint8_t bytes[66],prio=0xed;memset(bytes,0xcc,sizeof bytes);
    CHECK(osMessageQueueGet(queue,bytes+1,&prio,0)==osOK);
    CHECK(!memcmp(bytes+1,payload[id],size));
    CHECK(bytes[0]==0xcc && bytes[size+1]==0xcc && prio==priority);
    if(TEST_SCENARIO==7){CHECK(present[id]);present[id]=0;}
}
static void create_queue(unsigned bytes,unsigned count,unsigned seed,unsigned offset)
{
    size=bytes;capacity=count;done=0;read_token=0;read_priority=128;
    memset(present,0,sizeof present);
    for(unsigned id=0;id<32;++id)for(unsigned i=0;i<64;++i)
        payload[id][i]=(uint8_t)(id*37U+i*11U+seed);
    queue=osMessageQueueNew(capacity,size,NULL);CHECK(queue);
    for(unsigned i=0;i<capacity;++i){
        CHECK(osMessageQueuePut(queue,payload[i],128-i,0)==osOK);present[i]=1;
    }
    phase=0;fill=offset;invoke();phase=1;
}
static osThreadId_t start_sender(void)
{
    osThreadAttr_t a={.name="get-admission-sender",.priority=osPriorityAboveNormal,.stack_size=1024};
    osThreadId_t id=osThreadNew(sender,NULL,&a);CHECK(id);return id;
}
static void recover_stranded(void)
{
    CHECK(osMessageQueueGetSpace(queue)==1);
    CHECK(osMessageQueuePut(queue,payload[31],255,0)==osOK);
    check_read(31,255);CHECK(done==1);++retries;
}
static void retire(osThreadId_t id)
{
    CHECK(done==1 && ((os_thread_t *)id)->state==osRtxThreadWaitingSemaphore);
    CHECK(osMessageQueueGetCount(queue)==capacity && !osMessageQueueGetSpace(queue));
    /* Check the same queue after its waiting episode: a stale in-flight bit
     * would incorrectly refuse this full-ring Get with no sender left. */
    phase=2;read_token=20;read_priority=200;fill=16;invoke();CHECK(returned==osOK);++naked;
    for(unsigned i=1;i<capacity;++i)check_read(i,128-i);
    CHECK(!osMessageQueueGetCount(queue) && osMessageQueueGetSpace(queue)==capacity);
    CHECK(!((os_message_queue_t *)queue)->mp_info.used_blocks);
    fill=16;invoke();CHECK(returned==osErrorResource);++empty_controls;
    CHECK(osThreadTerminate(id)==osOK && osMessageQueueDelete(queue)==osOK);
    ++cases;
}
static unsigned token_priority(unsigned id)
{ return id==31?255:(id>=20?200+id-20:128-id); }
static unsigned oracle_head(void)
{
    unsigned head=32,count=0;
    for(unsigned i=0;i<32;++i)if(present[i]){
        ++count;if(head==32 || token_priority(i)>token_priority(head))head=i;
    }
    CHECK(count==osMessageQueueGetCount(queue) && head<32);return head;
}
static void publish_expected(unsigned *seen)
{
    const unsigned order[]={1,2,0};CHECK(done<=3);
    while(*seen<done){
        unsigned index=*seen;CHECK(wake_order[index]==order[index]);
        unsigned token=20+wake_order[index];CHECK(!present[token]);present[token]=1;++*seen;
    }
}
static void partial_batch_cases(void)
{
    const unsigned sizes[]={1,4,12,64},offsets[]={0,7,15},fills[]={14,15,16};
    for(unsigned s=0;s<4;++s)for(context=0;context<3;++context)
    for(unsigned o=0;o<3;++o)for(unsigned f=0;f<3;++f){
        create_queue(sizes[s],3,s+o+f,offsets[o]);nullable=(s+context+o)&1U;
        osThreadId_t ids[3];
        for(unsigned i=0;i<3;++i){
            osThreadAttr_t a={.name="queue-get-priority",.priority=i?osPriorityHigh:osPriorityAboveNormal,.stack_size=1024};
            ids[i]=osThreadNew(sender,(void *)(uintptr_t)i,&a);CHECK(ids[i]);
            CHECK(((os_thread_t *)ids[i])->state==osRtxThreadWaitingMessagePut);
        }
        CHECK(!done);fill=fills[f];invoke();
        CHECK(done==16-fill);controls+=done;unsigned seen=0;publish_expected(&seen);
        if(returned!=osOK){CHECK(multi_removed==16-fill);++refused;}
        else{CHECK(multi_removed==3 && osMessageQueueGetSpace(queue)==3-done);lost+=3-done;}
        while(done<3){
            if(returned==osOK){
                CHECK(osMessageQueuePut(queue,payload[31],255,0)==osOK);
                present[31]=1;check_read(31,255);
            }else{
                read_token=oracle_head();read_priority=token_priority(read_token);phase=2;fill=0;
                invoke();CHECK(returned==osOK);
                /* Preserve the refusal/retry path for the next remaining sender. */
                returned=osErrorResource;
            }
            ++retries;publish_expected(&seen);
        }
        CHECK(seen==3 && !((os_message_queue_t *)queue)->thread_list);
        for(unsigned i=0;i<3;++i)CHECK(((os_thread_t *)ids[i])->state==osRtxThreadWaitingSemaphore);
        phase=2;read_token=oracle_head();read_priority=token_priority(read_token);fill=16;
        invoke();CHECK(returned==osOK);++naked;
        while(osMessageQueueGetCount(queue)){
            unsigned id=oracle_head();check_read(id,token_priority(id));
        }
        fill=16;invoke();CHECK(returned==osErrorResource);++empty_controls;
        CHECK(!((os_message_queue_t *)queue)->mp_info.used_blocks);
        for(unsigned i=0;i<3;++i)CHECK(osThreadTerminate(ids[i])==osOK);
        CHECK(osMessageQueueDelete(queue)==osOK);++cases;
    }
}
static void full_ring_cases(void)
{
    const unsigned sizes[]={1,4,12,64},counts[]={1,3,16},offsets[]={0,7,15};
    for(unsigned s=0;s<4;++s)for(unsigned c=0;c<3;++c)
    for(context=0;context<3;++context)for(unsigned o=0;o<3;++o)
    for(unsigned full=0;full<2;++full){
        create_queue(sizes[s],counts[c],s+c+o+full,offsets[o]);
        nullable=(s+c+context+o)&1U;
        /* A full deferred ring must not prevent a Get with no sender to wake. */
        fill=16;invoke();CHECK(returned==osOK);++naked;
        CHECK(osMessageQueuePut(queue,payload[0],128,0)==osOK);
        osThreadId_t id=start_sender();CHECK(!done);
        CHECK(((os_thread_t *)id)->state==osRtxThreadWaitingMessagePut);
        fill=full?16:15;invoke();
        if(returned!=osOK){CHECK(full && !done);++refused;fill=0;invoke();CHECK(returned==osOK);++retries;}
        else if(!done){CHECK(full);++lost;recover_stranded();}
        else{CHECK(!full);++controls;}
        retire(id);
    }
}
static void wait_publication_cases(void)
{
    const unsigned sizes[]={1,4,12,64},offsets[]={0,7,15},fills[]={0,15,16};
    for(unsigned s=0;s<4;++s)for(context=0;context<2;++context)
    for(unsigned o=0;o<3;++o)for(unsigned f=0;f<3;++f){
        create_queue(sizes[s],1,s+o+f,offsets[o]);nullable=(s+context+o)&1U;
        fill=fills[f];gap_armed=1;unsigned before=gap_fired;
        osThreadId_t id=start_sender();CHECK(!gap_armed && gap_fired==before+1);++gaps;
        CHECK(!osRtxInfo.isr_queue.cnt);
        if(returned!=osOK){
            CHECK(fill==16 && !done);++refused;fill=0;invoke();CHECK(returned==osOK);++retries;
        }else if(!done){++lost;recover_stranded();}
        else ++controls;
        retire(id);
    }
}
static void main_task(void *p)
{
    (void)p;SysTick->CTRL=0;CHECK(osRtxInfo.isr_queue.max==16);
    NVIC_SetPriority((IRQn_Type)0,5);NVIC_EnableIRQ((IRQn_Type)0);
    retire_gate=osSemaphoreNew(1,0,NULL);CHECK(retire_gate);
    for(unsigned i=0;i<16;++i){events[i]=osEventFlagsNew(NULL);CHECK(events[i]);}
    if(TEST_SCENARIO==8)wait_publication_cases();
    else if(TEST_SCENARIO==7)partial_batch_cases();else full_ring_cases();
    for(unsigned i=0;i<16;++i)CHECK(osEventFlagsDelete(events[i])==osOK);
    CHECK(osSemaphoreDelete(retire_gate)==osOK);
    output("GET cases=");number(cases);output(" lost=");number(lost);
    output(" refused=");number(refused);output(" controls=");number(controls);
    output(" naked=");number(naked);output(" retries=");number(retries);
    output(" gaps=");number(gaps);output(" empty=");number(empty_controls);
    output(" notifications=");number(notifications);output("\n");
    finish(lost?42:0);
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
    osThreadAttr_t a={.name="queue-get-admission",.priority=osPriorityNormal,.stack_size=4096};
    CHECK(osThreadNew(main_task,NULL,&a));(void)osKernelStart();CHECK(0);
}
__attribute__((section(".vectors"),used)) void (*const vectors[17])(void)={
    (void (*)(void))(boot_stack+512),Reset_Handler,fault,fault,fault,fault,fault,
    0,0,0,0,SVC_Handler,fault,0,PendSV_Handler,SysTick_Handler,IRQ0_Handler
};
