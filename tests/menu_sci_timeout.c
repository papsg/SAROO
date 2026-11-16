/* papsg adaptation of paps-g SSR-015. Exact production receive function; independent modeled register readiness. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint8_t u8;
_Static_assert(INT_MAX==2147483647,"32-bit int required");
static volatile u8 status_register,data_register=0x48;
static uint64_t accesses,data_reads,ready_at;
static volatile u8 *status_address(void)
{
    ++accesses;
    if(ready_at && accesses>=ready_at) status_register|=0x40;
    return &status_register;
}
static volatile u8 *data_address(void) { ++data_reads;return &data_register; }
#define SSR (*status_address())
#define RDR (*data_address())
#include "sci-timeout.inc"
int main(int argc,char **argv)
{
    assert(argc==3 || argc==4);
    long parsed=strtol(argv[1],NULL,10);assert(parsed>=INT_MIN && parsed<=INT_MAX);
    int timeout=(int)parsed;ready_at=strtoull(argv[2],NULL,10);
    u8 seed=argc==4?(u8)strtoul(argv[3],NULL,0):0;
    assert(!(seed&0x40));status_register=seed;
    if(ready_at==1) status_register|=0x40;
    uint64_t budget=timeout>0?(uint64_t)timeout*1500:0;
    int expected=ready_at && ready_at<=budget?0x48:-1;
    uint64_t expected_accesses=expected>=0?ready_at+1:budget; /* Ready acknowledgement adds one store. */
    int actual=sci_getc(timeout);
    printf("sci timeout=%d ready=%llu result=%d accesses=%llu expected=%llu data_reads=%llu\n",
        timeout,(unsigned long long)ready_at,actual,(unsigned long long)accesses,
        (unsigned long long)expected_accesses,(unsigned long long)data_reads);
    assert(actual==expected && accesses==expected_accesses);
    assert(data_reads==(unsigned)(expected>=0));
    assert(status_register==(seed|(timeout<=0 && ready_at==1?0x40:0)));
    return 0;
}
