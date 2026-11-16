/* paps-g 3ed0238f / SSR-110; public adaptation by papsg. */
/* Exercise the real public press helper in the complete prepared menu header. */
#include "main.h"
extern int printf(const char *, ...);
_Static_assert(sizeof(int)==4 && sizeof(u32)==4,"32-bit event contract");
static unsigned checks,failures,event_calls,mask_calls;
static void check(int value)
{
    checks++;
    if(!value && failures++<8)printf("press predicate mismatch at check %u\n",checks);
}
/* An independent per-button oracle; no aggregate packed-mask expression. */
static int pressed(u32 event,unsigned mask)
{
    for(unsigned bit=0;bit<16;bit++)if(mask&(1u<<bit))
        if(!((event>>(16+bit))&1u) || !((event>>bit)&1u))return 0;
    return 1;
}
static void selection(unsigned mask)
{
    u32 events[]={mask*65537u,mask*65536u,mask,0,0xffffffffu,
                  (mask^0x5a5au)*65536u+(mask^0xffffu),0xa55a5aa5u};
    /* The published PAD_* constants and natural callers have type int. */
    int button=(int)mask;
    for(unsigned i=0;i<sizeof events/sizeof *events;i++)
        check(BUTTON_DOWN(events[i],button)==pressed(events[i],mask));
}
static u32 next_event(void) {event_calls++;return (u32)PAD_A*65537u;}
static int next_mask(void) {mask_calls++;return PAD_A;}
int main(int argc,char **argv)
{
    if(argc!=2 || argv[1][0]<'0' || argv[1][0]>'5' || argv[1][1])return 2;
    switch(argv[1][0]) {
    case '0': for(unsigned mask=0;mask<32768u;mask++)selection(mask);break;
    case '1': selection(PAD_RIGHT);break;
    case '2': selection(PAD_RIGHT|PAD_UP);break;
    case '3': selection(0xfff8u);break;
    case '4': for(unsigned mask=0;mask<=65535u;mask++)selection(mask);break;
    case '5':
        check(BUTTON_DOWN(next_event(),next_mask()));
        check(event_calls==1 && mask_calls==1);
        break;
    }
    printf("press case %c: %u checks, %u failures\n",argv[1][0],checks,failures);
    return failures?1:0;
}
