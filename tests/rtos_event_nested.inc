/* Papsg adaptation of the independent paps-g SSR-069 nested-IRQ oracle.
 * Hooks only choose IRQ timing; ownership, flags and waits use actual RTX. */
static osEventFlagsId_t events[17];
static unsigned context,phase,fill,warm,armed;
static uint32_t wanted,initial,update,high_bit,options;
static volatile uint32_t low_result,low_observed,high_result,high_observed,wait_value;
static volatile unsigned completed,hooks,high_calls;
static void waiter(void *p){(void)p;wait_value=osEventFlagsWait(events[16],wanted,options,osWaitForever);completed++;osThreadExit();}
void event_publication_probe(os_event_flags_t *ef){
 if(armed && phase==1 && ef==(os_event_flags_t*)events[16]){
  CHECK(__get_IPSR()==(context==2?0u:17u));CHECK(__get_PRIMASK()==(context!=0));armed=0;hooks++;
  NVIC_SetPendingIRQ((IRQn_Type)0);__DSB();__ISB();
 }
}
static void high_irq(void){
 CHECK(__get_IPSR()==16 && !__get_PRIMASK());high_calls++;
 high_observed=osEventFlagsGet(events[16]);CHECK(high_observed==(initial|(context?update:0)));
 high_result=osEventFlagsSet(events[16],high_bit);
 if(fill==14)CHECK(high_result==(high_observed|high_bit));
 else CHECK(fill==15 && high_result==osFlagsErrorResource && osEventFlagsGet(events[16])==high_observed);
}
static void produce(void){
 unsigned expect_mask=phase!=0 && context!=0;CHECK(__get_PRIMASK()==expect_mask);
 if(!phase){for(unsigned i=0;i<warm;i++)CHECK(osEventFlagsSet(events[0],1)==1);return;}
 if(phase==1)for(unsigned i=0;i<fill;i++)CHECK(osEventFlagsSet(events[i],1)==1);
 low_result=osEventFlagsSet(events[16],update);low_observed=osEventFlagsGet(events[16]);CHECK(__get_PRIMASK()==expect_mask);
}
static void low_irq(void){
 CHECK(__get_IPSR()==17 && !__get_PRIMASK());unsigned mask=phase && context==1;if(mask)__disable_irq();produce();if(mask){CHECK(__get_PRIMASK()==1);__enable_irq();__DSB();__ISB();}
}
static void invoke(void){
 CHECK(!__get_IPSR() && !__get_PRIMASK());
 if(phase && context==2){__disable_irq();produce();CHECK(__get_PRIMASK()==1);__enable_irq();__DSB();__ISB();}
 else{NVIC_SetPendingIRQ((IRQn_Type)1);__DSB();__ISB();}
 CHECK(!__get_PRIMASK() && !osRtxInfo.isr_queue.cnt);
}
static void main_task(void *p){
 (void)p;SysTick->CTRL=0;CHECK(osRtxInfo.isr_queue.max==16);
 NVIC_SetPriority((IRQn_Type)0,4);NVIC_SetPriority((IRQn_Type)1,8);NVIC_EnableIRQ((IRQn_Type)0);NVIC_EnableIRQ((IRQn_Type)1);
 for(unsigned i=0;i<17;i++){events[i]=osEventFlagsNew(NULL);CHECK(events[i]);}
 unsigned cases=0,refused=0,retries=0;
 for(unsigned offset=0;offset<16;offset++)for(unsigned bit=0;bit<31;bit++)for(unsigned mode=0;mode<4;mode++)for(context=0;context<3;context++)for(fill=14;fill<=16;fill++){
  for(unsigned i=0;i<17;i++)CHECK(!(osEventFlagsClear(events[i],0x7fffffff)&osFlagsError));
  update=1u<<bit;uint32_t other=1u<<((bit+1)%31),unrelated=1u<<((bit+2)%31);high_bit=1u<<((bit+3)%31);
  wanted=update|((mode&1)?other:0);initial=unrelated|((mode&1)?other:0);options=((mode&1)?osFlagsWaitAll:osFlagsWaitAny)|((mode&2)?osFlagsNoClear:0);
  CHECK(osEventFlagsSet(events[16],initial)==initial);completed=0;wait_value=0;
  osThreadAttr_t attr={.name="nested-event",.priority=osPriorityAboveNormal,.stack_size=1024};osThreadId_t t=osThreadNew(waiter,NULL,&attr);CHECK(t && osThreadGetState(t)==osThreadBlocked);
  phase=0;armed=0;warm=(offset+16-osRtxInfo.isr_queue.in)%16;invoke();CHECK(osRtxInfo.isr_queue.in==offset && osRtxInfo.isr_queue.out==offset);
  unsigned prior_hooks=hooks,prior_high=high_calls;phase=1;armed=1;invoke();uint32_t expected;
  if(fill==16){
   CHECK(hooks==prior_hooks && high_calls==prior_high && low_result==osFlagsErrorResource && low_observed==initial);
   CHECK(!completed && osThreadGetState(t)==osThreadBlocked && osEventFlagsGet(events[16])==initial);refused++;
   armed=0;phase=2;invoke();CHECK(low_result==(initial|update));expected=initial|update;retries++;
  }else{
   CHECK(hooks==prior_hooks+1 && high_calls==prior_high+1 && !armed);
   uint32_t low_expected=initial|update|((fill==14 && !context)?high_bit:0);
   CHECK(low_result==low_expected && low_observed==low_expected);expected=initial|update|(fill==14?high_bit:0);
  }
  CHECK(completed==1 && wait_value==expected && !notifications);
  CHECK(osEventFlagsGet(events[16])==((options&osFlagsNoClear)?expected:expected&~wanted));
  CHECK(!((os_event_flags_t*)events[16])->thread_list && !osRtxInfo.isr_queue.cnt);cases++;
 }
 for(unsigned i=0;i<17;i++)CHECK(osEventFlagsDelete(events[i])==osOK);
 output("NESTEVENT cases=");number(cases);output(" refused=");number(refused);output(" retries=");number(retries);output(" hooks=");number(hooks);output(" high=");number(high_calls);output(" notifications=");number(notifications);output(" recovered=1\n");finish(0);
}
