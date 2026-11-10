/* MCU-CDC-006: paps-g 50100444/3f7bba1a, independently seeded configuration. */
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
_Alignas(8) static u8 payload[MAX_BLOCKS][2352];
_Alignas(8) static u8 info_bytes[0xe00+100*sizeof(TRACK_INFO)+2048];
static u16 reply[4],st_control,st_status,hirq,hirq_clear;
static int irq_depth;
static unsigned pauses,wakeups;
#undef SSCR1
#undef SSCR2
#undef SSCR3
#undef SSCR4
#undef ST_CTRL
#undef ST_STAT
#undef HIRQ
#undef HIRQ_CLR
#define SSCR1 reply[0]
#define SSCR2 reply[1]
#define SSCR3 reply[2]
#define SSCR4 reply[3]
#define ST_CTRL st_control
#define ST_STAT st_status
#define HIRQ hirq
#define HIRQ_CLR hirq_clear
#define SSLOG(...) ((void)0)
static int disable_irq(void) { return irq_depth++; }
static void restore_irq(int prior) { assert(irq_depth==prior+1);irq_depth=prior; }
static void set_report(u8 status) { reply[0]=(u16)status<<8; }
static void disk_task_wakeup(void) { wakeups++; }
static void osDelay(int value) { (void)value;assert(0); }
static void wait_pause_ok(void)
{ assert(cdb.pause_request && !irq_depth);cdb.pause_request=0;pauses++; }
#include "connections.inc"

static void setup(void)
{
    memset(&cdb,0,sizeof cdb);memset(reply,0xa5,sizeof reply);
    cdb.block_free=MAX_BLOCKS;cdb.status=STAT_PAUSE;cdb.sector_size=2048;
    for(unsigned i=0;i<MAX_BLOCKS;i++) {
        cdb.block[i].data=payload[i];cdb.block[i].size=-1;
    }
    for(unsigned i=0;i<MAX_SELECTORS;i++) {
        cdb.part[i].size=-1;cdb.filter[i].c_true=i;cdb.filter[i].c_false=0xff;
    }
    cdb.cddev_filter=0xff;irq_depth=0;pauses=wakeups=0;st_control=hirq=hirq_clear=0;
}
static void conditions(FILTER *f)
{
    f->fad=150;f->range=100;f->mode=0x4f;f->fid=8;f->chan=3;
    f->smmask=9;f->smval=8;f->cimask=5;f->cival=4;
}
static void assert_conditions(const FILTER *f,int cleared)
{
    assert(f->fad==(cleared?0:150) && f->range==(cleared?0:100));
    assert(f->mode==(cleared?0:0x4f) && f->fid==(cleared?0:8));
    assert(f->chan==(cleared?0:3) && f->smmask==(cleared?0:9));
    assert(f->smval==(cleared?0:8) && f->cimask==(cleared?0:5));
    assert(f->cival==(cleared?0:4));
}
static void reject_command(int device)
{
    CDBLOCK before;memcpy(&before,&cdb,sizeof cdb);
    assert((device?set_cdev_connect():set_filter_connection())==0);
    assert((reply[0]>>8)==STAT_REJECT && !memcmp(&before,&cdb,sizeof cdb));
    assert(!irq_depth);
}
static void ownership(void)
{
    for(unsigned f=0;f<24;f++)for(unsigned target=0;target<24;target++) {
        setup();unsigned other=(f+1)%24;cdb.cddev_filter=target;
        cdb.filter[other].c_false=target;cdb.filter[f].c_true=target;
        cdb.cr1=2;cdb.cr2=0xff00|target;cdb.cr3=f<<8;
        assert(set_filter_connection()==HIRQ_ESEL);
        assert(cdb.cddev_filter==0xff && cdb.filter[f].c_false==target);
        assert(cdb.filter[other].c_false==0xff && cdb.filter[f].c_true==target);
        cdb.cr3=target<<8;assert(set_cdev_connect()==HIRQ_ESEL);
        assert(cdb.cddev_filter==target);
        for(unsigned i=0;i<24;i++)assert(cdb.filter[i].c_false==0xff);
        cdb.cr1=1;cdb.cr2=target<<8;cdb.cr3=other<<8;
        assert(set_filter_connection()==HIRQ_ESEL);
        assert(cdb.cddev_filter==target && cdb.filter[f].c_true==target);
        assert(cdb.filter[other].c_true==target && !irq_depth);
    }
}
static void validation(void)
{
    for(unsigned invalid=24;invalid<255;invalid++) {
        setup();cdb.cr3=invalid<<8;reject_command(1);
        cdb.cr1=3;cdb.cr2=0xff;cdb.cr3=invalid<<8;reject_command(0);
        cdb.cr3=3<<8;cdb.cr2=(7<<8)|invalid;reject_command(0);
        cdb.cr2=(invalid<<8)|7;reject_command(0);
        cdb.cr1=1;cdb.cr2=(7<<8)|invalid;
        assert(set_filter_connection()==HIRQ_ESEL && cdb.filter[3].c_true==7);
        cdb.cr1=2;cdb.cr2=(invalid<<8)|7;
        assert(set_filter_connection()==HIRQ_ESEL && cdb.filter[3].c_false==7);
    }
    setup();cdb.cddev_filter=7;cdb.filter[3].c_false=8;
    cdb.cr3=0xff00;assert(set_cdev_connect()==HIRQ_ESEL);
    assert(cdb.cddev_filter==0xff && cdb.filter[3].c_false==8);
    cdb.cr1=3;cdb.cr2=0xffff;cdb.cr3=3<<8;
    assert(set_filter_connection()==HIRQ_ESEL);
    assert(cdb.filter[3].c_true==0xff && cdb.filter[3].c_false==0xff);
    cdb.cr3=0xff00;reject_command(0);
    setup();cdb.cr1=0;cdb.cr2=0xfefd;cdb.cr3=3<<8;
    CDBLOCK before;memcpy(&before,&cdb,sizeof cdb);
    assert(set_filter_connection()==HIRQ_ESEL && !memcmp(&before,&cdb,sizeof cdb));
}
static void mode_reset(void)
{
    for(unsigned i=0;i<24;i++) {
        setup();conditions(&cdb.filter[i]);cdb.filter[i].c_true=19;cdb.filter[i].c_false=20;
        cdb.cr1=0xff;cdb.cr3=i<<8;assert(set_filter_mode()==HIRQ_ESEL);
        assert_conditions(&cdb.filter[i],1);
        assert(cdb.filter[i].c_true==19 && cdb.filter[i].c_false==20);
    }
    for(unsigned invalid=24;invalid<256;invalid++) {
        setup();cdb.cr1=0x80;cdb.cr3=invalid<<8;
        CDBLOCK before;memcpy(&before,&cdb,sizeof cdb);
        assert(!set_filter_mode() && (reply[0]>>8)==STAT_REJECT);
        assert(!memcmp(&before,&cdb,sizeof cdb));
        cdb.cr1=0;memcpy(&before,&cdb,sizeof cdb);
        assert(!reset_filter() && (reply[0]>>8)==STAT_REJECT);
        assert(!memcmp(&before,&cdb,sizeof cdb));
    }
}
static void reset_flags(void)
{
    for(unsigned mode=1;mode<256;mode++)for(unsigned ignored=0;ignored<2;ignored++) {
        setup();cdb.cddev_filter=7;
        for(unsigned i=0;i<24;i++) {
            conditions(&cdb.filter[i]);cdb.filter[i].c_true=(i+2)%24;
            cdb.filter[i].c_false=(i+1)%24;
        }
        cdb.cr1=mode;cdb.cr3=ignored?0xff00:0;
        assert(reset_filter()==HIRQ_ESEL && !pauses && !irq_depth);
        assert(cdb.cddev_filter==(mode&0x20?0xff:7));
        for(unsigned i=0;i<24;i++) {
            FILTER *f=&cdb.filter[i];assert_conditions(f,!!(mode&0x10));
            assert(f->c_true==(mode&0x40?i:(i+2)%24));
            assert(f->c_false==(mode&(0x80|0x20)?0xff:(i+1)%24));
        }
    }
}
static void pause_policy(void)
{
    const unsigned modes[]={0x10,0x20,0x30,0x04,0x24,0,0};
    for(unsigned i=0;i<7;i++) {
        setup();TRACK_INFO track={.mode=1};cdb.play_track=&track;
        cdb.play_type=PLAYTYPE_SECTOR;cdb.cddev_filter=7;cdb.cr1=modes[i];
        cdb.cr3=(i==6?0:(modes[i]&4?0:7))<<8;
        assert(reset_filter()==HIRQ_ESEL);
        assert(pauses==(unsigned)(!!(modes[i]&4) || i==5));
        assert(!cdb.pause_request && !irq_depth);
    }
    setup();cdb.play_type=PLAYTYPE_SECTOR;cdb.cddev_filter=7;cdb.cr1=4;
    assert(reset_filter()==HIRQ_ESEL && !pauses); /* no active track */
    setup();TRACK_INFO audio={.mode=3};cdb.play_track=&audio;
    cdb.play_type=PLAYTYPE_SECTOR;cdb.cddev_filter=7;cdb.cr1=4;
    assert(reset_filter()==HIRQ_ESEL && !pauses);
}
static void startup(void)
{
    memset(&cdb,0xa5,sizeof cdb);ss_sw_init();
    assert(cdb.sector_size==2048 && cdb.put_sector_size==2048);
    assert(cdb.block_free==200 && cdb.status==STAT_PAUSE && cdb.fad==150);
    assert(cdb.ctrladdr==0x41 && cdb.track==1 && cdb.index==1);
    assert(cdb.cddev_filter==0xff && cdb.last_buffer==0xff && !cdb.trans_type);
    for(unsigned i=0;i<200;i++)assert(cdb.block[i].size==-1 && cdb.block[i].data==payload[i]);
    for(unsigned i=0;i<24;i++) {
        assert_conditions(&cdb.filter[i],1);
        assert(cdb.filter[i].c_true==i && cdb.filter[i].c_false==0xff);
        assert(cdb.part[i].size==-1 && !cdb.part[i].head && !cdb.part[i].tail);
    }
}
static void software_init(void)
{
    const unsigned flags[]={1,0,0x80,0x81};
    for(unsigned n=0;n<4;n++) {
        setup();cdb.iflag=0x12;cdb.sector_size=2352;cdb.put_sector_size=2336;
        cdb.trans_type=TRANS_PUT;cdb.trans_block=&cdb.block[1];cdb.trans_bk=4;
        for(unsigned i=0;i<24;i++)conditions(&cdb.filter[i]);
        st_control=STIRQ_DAT|0x0200|0x0040;cdb.cr1=0x0400|flags[n];
        CDBLOCK before;memcpy(&before,&cdb,sizeof cdb);
        assert(init_cdblock()==(n==1?0:HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST));
        if(n) {
            if(n==1)before.iflag=0;
            assert(!memcmp(&before,&cdb,sizeof cdb));
            assert(st_control==(STIRQ_DAT|0x0200|0x0040));
        } else {
            assert(cdb.sector_size==2048 && cdb.put_sector_size==2048);
            assert(!cdb.trans_type && !cdb.trans_block && !cdb.trans_bk);
            assert(st_control==0x0040 && cdb.block_free==200);
            for(unsigned i=0;i<24;i++)assert_conditions(&cdb.filter[i],1);
        }
    }
}
static void unpublished_owner(void)
{
    setup();BLOCK *owned=alloc_block();assert(owned && cdb.block_free==199);
    owned->size=0; /* valid allocated, not partition-owned; PUT prerequisite */
    cdb.cr1=4;cdb.cr3=0xff00;assert(reset_filter()==HIRQ_ESEL);
    assert(owned->size==0 && cdb.block_free==199);
    for(unsigned i=0;i<24;i++)assert(!cdb.part[i].head);
    free_block(owned);assert(cdb.block_free==200);
}
static void baseline_control(void)
{
    setup();cdb.cr3=3<<8;assert(set_cdev_connect()==HIRQ_ESEL && cdb.cddev_filter==3);
    cdb.cr1=1;cdb.cr2=9<<8;cdb.cr3=7<<8;
    assert(set_filter_connection()==HIRQ_ESEL && cdb.filter[7].c_true==9);
    cdb.cr1=0x42;assert(set_filter_mode()==HIRQ_ESEL && cdb.filter[7].mode==0x42);
    assert(!irq_depth);
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<9);
    if(test==0)ownership();else if(test==1)validation();else if(test==2)mode_reset();
    else if(test==3)reset_flags();else if(test==4)pause_policy();
    else if(test==5)startup();else if(test==6)software_init();
    else if(test==7)unpublished_owner();else baseline_control();
    puts("pass: actual connection ownership, validation, resets and defaults");return 0;
}
