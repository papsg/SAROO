/* Isolated preceding-source probes, not a whole previous-firmware execution.
 * Keep the original sector-count units. Actual old functions are extracted;
 * register/wakeup/free boundaries below are explicit host models. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"
static CDBLOCK cdb;
static u16 reply[4],hirq;
#undef SSCR1
#undef SSCR2
#undef SSCR3
#undef SSCR4
#undef HIRQ
#define SSCR1 reply[0]
#define SSCR2 reply[1]
#define SSCR3 reply[2]
#define SSCR4 reply[3]
#define HIRQ hirq
#define SSLOG(...) ((void)0)
static void disk_task_wakeup(void) {}
static int fad_to_track(int fad) { assert(fad>=150);return 1; }
static void set_report(u8 status) { SSCR1=(u16)status<<8; }
static void set_status(u8 status) { SSCR1=(u16)status<<8; }
static void free_partition(PARTITION *pt) { pt->head=pt->tail=NULL;pt->numblocks=0;pt->size=-1; }
#include "legacy.inc"
static void le32(u8 *p,u32 n) { for(unsigned i=0;i<4;i++)p[i]=(u8)(n>>(8*i)); }
static unsigned record(u8 *p,unsigned id,unsigned len)
{
    memset(p,0,len);p[0]=len;p[25]=id<2?2:0;p[32]=1;p[33]=id<2?id:'A';
    le32(p+2,1000+id);le32(p+10,4096+id);return len;
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<7);
    u8 *data=calloc(1,24+2048);assert(data);u8 *buf=data+24;
    BLOCK block={.data=data,.size=2048};
    cdb.part[0].head=&block;cdb.filter[0].c_true=0;
    cdb.dir_filter=0;cdb.cdir_size=1;cdb.cdir_lba=100;cdb.rdir_state=2;
    if(test==0) { fill_fileinfo();assert(cdb.cdir_pfnum==0); }
    if(test==1) {
        for(unsigned id=0;id<8;id++)record(buf+252*id,id,252);
        buf[2016]=40;fill_fileinfo();assert(cdb.cdir_pfnum==0);
    }
    if(test==2 || test==6) {
        for(unsigned id=0;id<4;id++)record(buf+34*id,id,34);
        fill_fileinfo();assert(cdb.cdir_pfid==2 && cdb.cdir_pfnum==2);
        assert(cdb.fi[2].lba==1002 && cdb.fi[3].size==4099);
        if(test==2)assert(cdb.cdir_drend==1);
    }
    if(test==3) {
        cdb.dir_filter=2;cdb.cr3=7<<8;cdb.cr4=2;
        assert(!read_dir() && cdb.dir_filter==7);
    }
    if(test==4) { const u8 bytes[]={0,0,0,128};assert(isonum_733((u8*)bytes)==0x80000000u); }
    if(test==5) { cdb.part[0].head=NULL;handle_diread(); }
    free(data);puts("pass: isolated actual preceding parser/control");return 0;
}
