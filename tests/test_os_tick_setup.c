/* Adapted by papsg from paps-g a90f6e3c/SSR-130. Complete selected OS Tick
 * provider with observable register objects.
 * This checks C admission/ownership, not physical SysTick progress/timing. */
#include "os_tick.h"
#include "systick_device.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

uint32_t SystemCoreClock;
static TickRegisters timer;
static CoreRegisters core;
static unsigned tick_accesses, core_accesses, handler_calls;
TickRegisters *tick_registers(void) { ++tick_accesses; return &timer; }
CoreRegisters *core_registers(void) { ++core_accesses; return &core; }
static void handler(void) { ++handler_calls; }
static void reset(void)
{
    SystemCoreClock=400000000;
    if(OS_Tick_Setup(100,handler)!=0){fputs("valid reset refused\n",stderr);return;}
    timer.CTRL=0x10007;timer.LOAD=0x654321;timer.VAL=0x123456;timer.CALIB=0x123;
    for(unsigned i=0;i<12;++i)core.SHPR[i]=(uint8_t)(0x60+i);
    core.ICSR=0x21;tick_accesses=core_accesses=handler_calls=0;
}
static unsigned cases, failed, zero_admitted;
static void check(uint32_t clock,uint32_t frequency)
{
    reset();SystemCoreClock=clock;
    TickRegisters previous,expected;CoreRegisters old_core,expected_core;
    memcpy(&previous,&timer,sizeof timer);memcpy(&expected,&timer,sizeof timer);
    memcpy(&old_core,&core,sizeof core);memcpy(&expected_core,&core,sizeof core);
    /* Use a wide period oracle: no unsigned reload underflow or shared gate. */
    uint64_t period=frequency ? (uint64_t)clock/frequency:0;
    int wanted=frequency && period>=2 && period<=0x1000000 ? 0:-1;
    int32_t result=OS_Tick_Setup(frequency,handler);
    int good=result==wanted && !handler_calls;
    if(wanted==0){
        expected.CTRL=6;expected.LOAD=(uint32_t)(period-1);expected.VAL=0;
        expected_core.SHPR[11]=0xff;
        good&=!memcmp(&expected,&timer,sizeof timer) && !memcmp(&expected_core,&core,sizeof core);
        good&=tick_accesses==3 && core_accesses==1;
    }else{
        good&=!memcmp(&previous,&timer,sizeof timer) && !memcmp(&old_core,&core,sizeof core);
        good&=!tick_accesses && !core_accesses;
    }
    if(result==0 && timer.LOAD==0)++zero_admitted;
    if(!good){
        if(failed<8)fprintf(stderr,"clock=%" PRIu32 " frequency=%" PRIu32 " result=%" PRId32 " wanted=%d load=%" PRIu32 "\n",clock,frequency,result,wanted,timer.LOAD);
        ++failed;
    }
    ++cases;
}
static uint32_t random_word(uint32_t *state)
{
    uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;return *state=x;
}
static int matrix(void)
{
    static const uint32_t clocks[]={0,1,2,3,100,200,1000,16777215,16777216,16777217,400000000,480000000,UINT32_MAX};
    static const uint32_t rates[]={0,1,2,3,23,24,100,255,256,1000,200000000,200000001,400000000,400000001,480000000,UINT32_MAX};
    for(unsigned i=0;i<sizeof clocks/sizeof clocks[0];++i)
        for(unsigned j=0;j<sizeof rates/sizeof rates[0];++j)check(clocks[i],rates[j]);
    uint32_t state=0x06173001;
    for(unsigned i=0;i<10000;++i){uint32_t clock=random_word(&state);check(clock,random_word(&state));}
    printf("{\"cases\":%u,\"failed\":%u,\"zero_reload_admitted\":%u}\n",cases,failed,zero_admitted);
    return failed!=0;
}
static int pending(const char *mode)
{
    reset();SystemCoreClock=400000000;
    if(OS_Tick_Setup(100,handler))return 2;
    OS_Tick_Enable();core.ICSR=SCB_ICSR_PENDSTSET_Msk;
    OS_Tick_Disable();
    if(timer.CTRL!=6 || core.ICSR!=SCB_ICSR_PENDSTCLR_Msk)return 3;
    TickRegisters previous;CoreRegisters old_core;
    memcpy(&previous,&timer,sizeof timer);memcpy(&old_core,&core,sizeof core);
    uint32_t frequency=400000000;
    int success=!strcmp(mode,"pending-valid");
    if(success)frequency=1000;
    else if(!strcmp(mode,"pending-frequency-zero"))frequency=0;
    else if(!strcmp(mode,"pending-overflow"))frequency=1;
    else if(!strcmp(mode,"pending-beyond-clock"))frequency=400000001;
    tick_accesses=core_accesses=0;
    int32_t result=OS_Tick_Setup(frequency,handler);
    int good=success ? result==0 && timer.LOAD==399999 && timer.CTRL==6:
        result==-1 && !tick_accesses && !core_accesses &&
        !memcmp(&previous,&timer,sizeof timer) && !memcmp(&old_core,&core,sizeof core);
    OS_Tick_Enable();
    uint32_t wanted=success ? SCB_ICSR_PENDSTCLR_Msk:SCB_ICSR_PENDSTSET_Msk;
    good&=timer.CTRL==7 && core.ICSR==wanted;
    /* A consumed saved-pending event must not be reissued a second time. */
    core.ICSR=0;OS_Tick_Disable();OS_Tick_Enable();good&=core.ICSR==0;
    printf("{\"mode\":\"%s\",\"result\":%" PRId32 ",\"pending_preserved\":%s}\n",mode,result,good?"true":"false");
    return !good;
}
static int getters(void)
{
    reset();timer.LOAD=77;timer.VAL=12;timer.CTRL=0x10007;SystemCoreClock=400000000;
    if(OS_Tick_GetIRQn()!=-1 || OS_Tick_GetClock()!=400000000 ||
       OS_Tick_GetInterval()!=78 || OS_Tick_GetCount()!=65 || OS_Tick_GetOverflow()!=1)return 1;
    tick_accesses=0;OS_Tick_AcknowledgeIRQ();if(tick_accesses!=1)return 2;
    /* Plain objects do not model COUNTFLAG read-clear or interrupt timing. */
    puts("{\"getters\":true,\"read_side_effects_modeled\":false}");return 0;
}
int main(int argc,char **argv)
{
    if(argc!=2)return 99;
    if(!strcmp(argv[1],"matrix"))return matrix();
    if(!strcmp(argv[1],"getters"))return getters();
    if(!strncmp(argv[1],"pending-",8))return pending(argv[1]);
    return 98;
}
