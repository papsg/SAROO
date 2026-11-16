/* paps-g fac81eb1 / SSR-066; public adaptation by papsg. */
/* The complete selected menu header, including the real public helpers. */
#include "main.h"
extern int printf(const char *, ...);
_Static_assert(sizeof(u32)==4 && sizeof(int)==4,"32-bit event fixture");
static unsigned checks,failures,event_calls,mask_calls;
static void check(int value)
{
    checks++;
    if(!value && failures++<8)printf("release predicate mismatch at check %u\n",checks);
}
/* Independent per-bit oracle, avoiding the helper's aggregate expression. */
static int released(u32 event,u32 mask)
{
    if(mask>65535u)return 0;
    for(unsigned bit=0;bit<16;bit++)if(mask&(1u<<bit))
        if(!((event>>(16+bit))&1u) || ((event>>bit)&1u))return 0;
    return 1;
}
static u32 next_event(void) {event_calls++;return 0x80000000u;}
static u32 next_mask(void) {mask_calls++;return PAD_RIGHT;}
int main(void)
{
#if TEST_UP
    for(u32 mask=0;mask<=65535u;mask++) {
        u32 events[]={mask*65536u,mask*65537u,mask,0,
                      (mask^0x5a5au)*65536u+(mask^65535u),0xffffffffu};
        for(unsigned i=0;i<sizeof events/sizeof *events;i++)
            check(BUTTON_UP(events[i],mask)==released(events[i],mask));
    }
    check(BUTTON_UP((-2147483647-1),PAD_RIGHT));
    check(!BUTTON_UP(0x80008000u,PAD_RIGHT));
    check(!BUTTON_UP(0x00008000u,PAD_RIGHT));
    check(BUTTON_UP(next_event(),next_mask()));
    check(event_calls==1 && mask_calls==1);
    u32 stat=(u32)PAD_A*65536u,bt=PAD_A;
    check(BUTTON_UP(stat++,bt++));
    check(stat==(u32)PAD_A*65536u+1 && bt==PAD_A+1);
    check(BUTTON_UP(0xffffffffu,0x10000u)==released(0xffffffffu,0x10000u));
#else
    /* An existing active helper and a release reference are valid controls. */
    u32 press=(u32)PAD_A*65537u,release=(u32)PAD_A*65536u;
    check(BUTTON_DOWN(press,PAD_A));check(!BUTTON_DOWN(release,PAD_A));
    check(released(release,PAD_A));check(!released(press,PAD_A));
#endif
    printf("button helpers: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
