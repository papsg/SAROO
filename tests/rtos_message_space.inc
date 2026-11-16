/* Adapted by papsg from paps-g SSR-068 amendment 03fa6494.
 * Available-slot oracle follows successful public Put/Get ownership. */
static osMessageQueueId_t queue;
static unsigned cap,size,phase,masked,seed;
static volatile unsigned irq_count,space_checks,wrong_space;
static unsigned char data[80],receive[80];
static void pattern(unsigned n){for(unsigned i=0;i<size;i++)data[i]=(unsigned char)(seed+n*11+i*7);}
static void put(unsigned n){pattern(n);CHECK(osMessageQueuePut(queue,data,0,0)==osOK);}
static void get(unsigned n){
 memset(receive,0xa5,sizeof receive);uint8_t prio=0xff;CHECK(osMessageQueueGet(queue,receive,&prio,0)==osOK && !prio);
 for(unsigned i=0;i<size;i++)CHECK(receive[i]==(unsigned char)(seed+n*11+i*7));
 for(unsigned i=size;i<sizeof receive;i++)CHECK(receive[i]==0xa5);
}
static void space(unsigned expected){unsigned got=osMessageQueueGetSpace(queue);space_checks++;if(got!=expected)wrong_space++;}
static void queue_irq(void){
 CHECK(__get_IPSR()==16 && !__get_PRIMASK());if(masked)__disable_irq();
 if(phase==0){
  for(unsigned i=0;i<cap;i++){put(i);space(cap-i-1);CHECK(!osMessageQueueGetCount(queue));}
  pattern(99);CHECK(osMessageQueuePut(queue,data,0,0)==osErrorResource);space(0);
 }else if(phase==1){
  for(unsigned i=0;i<cap;i++){get(i);space(1);put(i+32);space(0);}
 }else{
  CHECK(phase==2);for(unsigned i=0;i<cap;i++){get(i);space(i+1);}
 }
 CHECK(__get_PRIMASK()==masked);if(masked)__enable_irq();irq_count++;
}
static void invoke(void){unsigned prior=irq_count;NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();CHECK(irq_count==prior+1 && !__get_PRIMASK() && !osRtxInfo.isr_queue.cnt);}
static void main_task(void *p){
 (void)p;SysTick->CTRL=0;NVIC_SetPriority((IRQn_Type)0,5);NVIC_EnableIRQ((IRQn_Type)0);
 const unsigned caps[]={1,3,5,8},sizes[]={1,4,12,64};unsigned histories=0;
 for(masked=0;masked<2;masked++)for(unsigned ci=0;ci<4;ci++)for(unsigned si=0;si<4;si++)for(unsigned cycle=0;cycle<2;cycle++){
  cap=caps[ci];size=sizes[si];seed=cycle+ci*17+si*3;
  queue=osMessageQueueNew(cap,sizes[si],NULL);CHECK(queue && osMessageQueueGetCapacity(queue)==cap && osMessageQueueGetMsgSize(queue)==size);
  for(phase=0;phase<3;phase++){
   CHECK(!osMessageQueueGetCount(queue));space(cap);
   if(phase)for(unsigned i=0;i<cap;i++)put(i);
   invoke();CHECK(osMessageQueueGetCount(queue)==(phase==2?0:cap));space(phase==2?cap:0);
   if(phase!=2)for(unsigned i=0;i<cap;i++)get(i+(phase==1?32:0));
   CHECK(!((os_message_queue_t*)queue)->mp_info.used_blocks);histories++;
  }
  /* Same public sequence in thread context remains a valid control. */
  for(unsigned i=0;i<cap;i++){put(i);space(cap-i-1);}
  for(unsigned i=0;i<cap;i++){get(i);space(i+1);}
  CHECK(!((os_message_queue_t*)queue)->mp_info.used_blocks && !osRtxInfo.isr_queue.cnt);histories++;
  CHECK(osMessageQueueDelete(queue)==osOK);
 }
 NVIC_DisableIRQ((IRQn_Type)0);output("SPACE histories=");number(histories);output(" checks=");number(space_checks);output(" wrong=");number(wrong_space);output(" recovered=1\n");finish(wrong_space?42:0);
}
