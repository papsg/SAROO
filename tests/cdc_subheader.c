/* Actual reply/type fixture adapted from paps-g f0da2b0b. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"
static CDBLOCK cdb, before;
static u16 reply[4];
#undef SSCR1
#undef SSCR2
#undef SSCR3
#undef SSCR4
#define SSCR1 reply[0]
#define SSCR2 reply[1]
#define SSCR3 reply[2]
#define SSCR4 reply[3]
#define SSLOG(...) ((void)0)
#include "reply.inc"
static unsigned cases, failures;
static int use_setter;
static void check(unsigned index, unsigned status, unsigned channel)
{
    cdb.status=status;
    FILTER *f=&cdb.filter[index];
    if (use_setter) {
        cdb.cr1=0x4200|channel;
        cdb.cr2=((u8)(channel+1)<<8)|(u8)(channel+2);
        cdb.cr3=(index<<8)|(u8)(channel+3);
        cdb.cr4=((u8)(channel+4)<<8)|(u8)(channel+5);
        assert(set_filter_subheader()==HIRQ_ESEL);
    } else {
        f->chan=channel; f->smmask=channel+1; f->cimask=channel+2;
        f->fid=channel+3; f->smval=channel+4; f->cival=channel+5;
    }
    assert(f->chan==channel && f->smmask==(u8)(channel+1) && f->cimask==(u8)(channel+2));
    assert(f->fid==(u8)(channel+3) && f->smval==(u8)(channel+4) && f->cival==(u8)(channel+5));
    cdb.cr1=0x4300; cdb.cr2=0; cdb.cr3=index<<8; cdb.cr4=0;
    memcpy(&before,&cdb,sizeof cdb);
    memset(reply,0xa5,sizeof reply);
    int hirq=get_filter_subheader();
    /* The specified high/low byte fields are checked independently. */
    if ((reply[0]>>8)!=status || (reply[0]&0xff)!=channel) ++failures;
    assert((reply[1]>>8)==f->smmask && (reply[1]&0xff)==f->cimask);
    assert(reply[2]==f->fid);
    assert((reply[3]>>8)==f->smval && (reply[3]&0xff)==f->cival);
    assert(hirq==0 && !memcmp(&before,&cdb,sizeof cdb));
    ++cases;
}
int main(int argc,char **argv)
{
    assert(argc==2 && MAX_SELECTORS==24);
    int zero=!strcmp(argv[1],"zero");
    use_setter=!strcmp(argv[1],"roundtrip");
    memset(&cdb,0x35,sizeof cdb);
    if (zero || use_setter) {
        for(unsigned index=0;index<MAX_SELECTORS;index++)
            for(unsigned channel=0;channel<256;channel++)check(index,use_setter?channel^0xa5:0,channel);
        printf("%u replies %s; %u status/channel failures\n",cases,failures?"fail":"pass",failures);
        return failures?1:0;
    }
    for(unsigned status=0;status<256;status++)
        for(unsigned channel=0;channel<256;channel++) check(0,status,channel);
    for(unsigned index=0;index<MAX_SELECTORS;index++)
        for(unsigned channel=0;channel<256;channel++) check(index,channel^0xa5,channel);
    printf("%u replies %s; %u status/channel failures\n",cases,failures?"fail":"pass",failures);
    return failures?1:0;
}
