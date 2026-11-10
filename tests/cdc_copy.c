/* Adapted from paps-g 8432f7d8; independent list/payload oracles, host IRQ model. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"
static CDBLOCK cdb;
static u8 sector_bytes[MAX_BLOCKS][2352];
static u16 reply[4], hirq, hirq_clear, st_control,st_status;
#undef SSCR1
#undef SSCR2
#undef SSCR3
#undef SSCR4
#undef HIRQ
#undef HIRQ_CLR
#undef ST_CTRL
#undef ST_STAT
#define SSCR1 reply[0]
#define SSCR2 reply[1]
#define SSCR3 reply[2]
#define SSCR4 reply[3]
#define HIRQ hirq
#define HIRQ_CLR hirq_clear
#define ST_CTRL st_control
#define ST_STAT st_status
#define SSLOG(...) ((void)0)
static unsigned log_count;
int fixture_printk(const char *text)
{
    assert(!strcmp(text,"  c_false is FF!\n"));log_count++;return 0;
}
#define printk fixture_printk
static int irq_depth, lose_after, last_free;
#ifdef CDC_OWNERSHIP_HOOKS
static void (*irq_restore_hook)(void),(*boundary_log_hook)(void);
static unsigned fixture_log_mask,fixture_log_used;
static char fixture_log_text[1024];
static void ownership_log(unsigned mask,const char *format,...)
{
    if (!(fixture_log_mask&mask)) return;
    assert(!irq_depth);
    va_list args;va_start(args,format);
    int n=vsnprintf(fixture_log_text+fixture_log_used,sizeof fixture_log_text-fixture_log_used,format,args);
    va_end(args);assert(n>=0 && (unsigned)n<sizeof fixture_log_text-fixture_log_used);
    fixture_log_used+=n;
    if(boundary_log_hook){void (*hook)(void)=boundary_log_hook;boundary_log_hook=NULL;hook();}
}
#undef SSLOG
#define SSLOG(mask,...) ownership_log(mask,__VA_ARGS__)
#endif
static size_t copied_bytes;
static int disable_irq(void) { return irq_depth++; }
static void restore_irq(int old)
{
    assert(irq_depth==old+1); irq_depth=old;
    /* Another modeled buffer consumer takes capacity after two allocations.
     * This fault injection is not evidence of production scheduling. */
    if (!old && lose_after && cdb.block_free<last_free) {
        last_free=cdb.block_free;
        if (!--lose_after) {
            PARTITION *p=&cdb.part[2];
            for (unsigned i=0;i<MAX_BLOCKS;i++) if(cdb.block[i].size==-1) {
                BLOCK *b=&cdb.block[i]; b->size=2352; b->next=NULL;
                if(p->tail) p->tail->next=b; else p->head=b;
                p->tail=b; p->numblocks++; p->size+=2352;
            }
            cdb.block_free=0;
        }
    }
#ifdef CDC_OWNERSHIP_HOOKS
    if(!old && irq_restore_hook){void (*hook)(void)=irq_restore_hook;irq_restore_hook=NULL;hook();}
#endif
}
static void disk_task_wakeup(void) { assert(0); }
static __attribute__((unused)) void osDelay(int ticks) { (void)ticks; assert(0); }
static void wait_pause_ok(void) { assert(0); }
static void set_report(u8 status) { reply[0]=(u16)status<<8; }
static void *counted_memcpy(void *to,const void *from,size_t n)
{
    assert(!irq_depth); copied_bytes+=n; return memcpy(to,from,n);
}
#undef memcpy
#define memcpy counted_memcpy
#include "copy.inc"
#undef memcpy

/* Independent initial lists: production append/unlink do not seed them. */
static void setup(unsigned source_count,unsigned free_count)
{
#ifdef CDC_OWNERSHIP_HOOKS
    irq_restore_hook=boundary_log_hook=NULL;fixture_log_mask=fixture_log_used=0;fixture_log_text[0]=0;
#endif
    assert(source_count+free_count<=MAX_BLOCKS);
    memset(&cdb,0,sizeof cdb);
    for(unsigned i=0;i<MAX_SELECTORS;i++) {
        cdb.part[i].size=-1;cdb.filter[i].c_true=i;cdb.filter[i].c_false=0xff;
    }
    for(unsigned i=0;i<MAX_BLOCKS;i++) {
        BLOCK *b=&cdb.block[i]; b->data=sector_bytes[i];
        b->size=i<source_count || i>=source_count+free_count ? 2352 : -1;
        b->fad=150+i; b->cn=i; b->fn=i+1; b->sm=i+2; b->ci=i+3;
        memset(b->data,i,2352);
        if(b->size!=-1) {
            PARTITION *p=&cdb.part[i<source_count?0:2];
            if(p->tail) p->tail->next=b; else p->head=b;
            if(p->size<0) p->size=0;
            p->tail=b; p->numblocks++; p->size+=2352;
        }
    }
    cdb.block_free=free_count; cdb.sector_size=2352; cdb.status=STAT_PAUSE;
    lose_after=0; last_free=free_count; copied_bytes=0; allocation_count=0; log_count=0; hirq=0;
}
static void command(int moving,unsigned src,unsigned dst,unsigned pos,unsigned count)
{
    cdb.cr1=((moving?0x66:0x65)<<8)|dst;
    cdb.cr2=pos; cdb.cr3=src<<8; cdb.cr4=count;
}
static int execute(int moving) { return moving?move_sector_data():copy_sector_data(); }
static void check_reply(unsigned expected)
{
    for(unsigned status=0;status<256;status++) {
        cdb.status=status;
        for(unsigned repeat=0;repeat<2;repeat++) {
            memset(reply,0xa5,sizeof reply);
            assert(get_copy_error()==0);
            assert((reply[0]>>8)==status && (reply[0]&0xff)==expected);
            assert(!reply[1] && !reply[2] && !reply[3] && !irq_depth);
            cdb.cr3=23<<8; assert(get_filter_subheader()==0);
        }
    }
}
static void check_owners(void)
{
    unsigned seen[MAX_BLOCKS]={0}, used=0;
    for(unsigned p=0;p<MAX_SELECTORS;p++) {
        PARTITION *part=&cdb.part[p]; BLOCK *b=part->head,*tail=NULL;
        unsigned count=0,bytes=0;
        while(b) {
            ptrdiff_t i=b-cdb.block; assert(i>=0 && i<MAX_BLOCKS);
            assert(!seen[i]++ && b->size>0 && ++count<=MAX_BLOCKS);
            bytes+=b->size; used++; tail=b; b=b->next;
        }
        assert(part->numblocks==(int)count && part->tail==tail);
        assert(part->size==(int)bytes || (!count && part->size==-1));
    }
    assert(used+cdb.block_free==MAX_BLOCKS);
    for(unsigned i=0;i<MAX_BLOCKS;i++) assert(seen[i]==(cdb.block[i].size!=-1));
}
static void check_list(unsigned part,const unsigned *ids,unsigned count,int same_blocks)
{
    BLOCK *b=cdb.part[part].head;
    assert(cdb.part[part].numblocks==(int)count);
    for(unsigned i=0;i<count;i++) {
        unsigned id=ids[i]; assert(b && b->fad==150+id);
        assert(b->cn==(u8)id && b->fn==(u8)(id+1));
        assert(b->sm==(u8)(id+2) && b->ci==(u8)(id+3));
        if(same_blocks) assert(b==&cdb.block[id]);
        for(unsigned j=0;j<2352;j++) assert(b->data[j]==(u8)id);
        b=b->next;
    }
    assert(!b);
}
static void make_error(void)
{
    setup(4,4); command(0,0,1,0,4); lose_after=2;
    assert(execute(0)==HIRQ_ECPY);
    unsigned ids[]={0,1};check_list(1,ids,2,0);check_owners();check_reply(1);
}
static void check_refusal(int moving,u8 status)
{
    CDBLOCK before=cdb;
    assert(execute(moving)==0 && (reply[0]>>8)==status);
    assert(!memcmp(&before,&cdb,sizeof cdb) && !copied_bytes && !irq_depth);
    check_reply(1);
}
static void move_shapes(void)
{
    const unsigned sizes[]={1,2,4,17,100,200};
    for(unsigned nidx=0;nidx<sizeof sizes/sizeof *sizes;nidx++) {
        unsigned n=sizes[nidx],starts[]={0,n/2,n-1};
        for(unsigned si=0;si<3;si++) for(unsigned tail=0;tail<2;tail++)
        for(unsigned same=0;same<2;same++) {
            unsigned start=starts[si],count=tail?n-start:1;
            setup(n,0);command(1,0,same?0:1,start,count);
            assert(execute(1)==HIRQ_ECPY && !copied_bytes && !cdb.block_free);
            unsigned src[MAX_BLOCKS],dst[MAX_BLOCKS],ns=0,nd=0;
            for(unsigned i=0;i<n;i++) {
                if(i>=start && i<start+count) dst[nd++]=i; else src[ns++]=i;
            }
            if(same) { memcpy(src+ns,dst,nd*sizeof *dst);ns+=nd; }
            else check_list(1,dst,nd,1);
            check_list(0,src,ns,1);check_owners();check_reply(0);
        }
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);int test=atoi(argv[1]);assert(test>=0 && (test<=10 || (test>=20 && test<=27)));
    assert(MAX_BLOCKS==200 && MAX_SELECTORS==24);check_reply(0);
    if(test==1 || test==4 || test==5) {
        int moving=test!=1; setup(4,test==5?0:4);command(moving,0,1,0,4);
        assert(execute(moving)==HIRQ_ECPY);
        unsigned ids[]={0,1,2,3};check_list(1,ids,4,moving);
        assert(copied_bytes==(moving?0:4*2352));check_owners();check_reply(0);
    } else if(test==2) {
        make_error();setup(4,0);command(0,0,1,0,4);check_refusal(0,STAT_WAIT|STAT_PAUSE);
    } else if(test==3) make_error();
    else if(test==6 || test==7) {
        make_error();setup(4,4);check_reply(1);command(test==7,0,1,0,4);
        assert(execute(test==7)==HIRQ_ECPY);check_owners();check_reply(0);
    } else if(test>=8 && test<=10) {
        make_error();cdb.cr1=test==8?0x0401:test==9?0x0400:0x0481;
        cdb.status=STAT_PAUSE;
        int result=init_cdblock();
        assert(result==(test==9?0:HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST));
        check_reply(test==8?0:1);
        if(test==8) { assert(cdb.block_free==MAX_BLOCKS);check_owners(); }
    } else if(test==20 || test==21) {
        int moving=test==21;make_error();
        for(unsigned v=24;v<256;v++) for(unsigned dest=0;dest<2;dest++) {
            setup(4,4);command(moving,dest?0:v,dest?v:1,0,1);
            check_refusal(moving,STAT_REJECT);
        }
    } else if(test==22 || test==23) {
        int moving=test==23;make_error();
        const unsigned bad[][3]={{4,0,0},{4,4,1},{4,5,0xffff},{4,0xfffe,1},
            {4,0xffff,2},{4,0,5},{0,0xffff,0xffff},{0,0,1},{4,1,65534}};
        for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) {
            setup(bad[i][0],4);command(moving,0,1,bad[i][1],bad[i][2]);
            check_refusal(moving,STAT_WAIT|STAT_PAUSE);
        }
        for(unsigned transfer=1;transfer<=TRANS_PUT;transfer++) {
            setup(4,4);command(moving,0,1,0,1);cdb.trans_type=transfer;
            check_refusal(moving,STAT_WAIT|STAT_PAUSE);
        }
    } else if(test==24) move_shapes();
    else if(test==25) {
        const unsigned range[][2]={{0,0xffff},{1,0xffff},{0xffff,1},{0xffff,0xffff}};
        for(unsigned r=0;r<4;r++) for(unsigned same=0;same<2;same++) {
            setup(4,4);command(0,0,same?0:23,range[r][0],range[r][1]);
            assert(execute(0)==HIRQ_ECPY);
            unsigned start=range[r][0]==0xffff?3:range[r][0],n=4-start,ids[8],count=0;
            if(same) for(unsigned i=0;i<4;i++) ids[count++]=i;
            for(unsigned i=start;i<4;i++) ids[count++]=i;
            check_list(same?0:23,ids,count,0);check_owners();check_reply(0);
            assert(copied_bytes==n*2352);
        }
    } else if(test==27) {
        /* Baseline-valid copy payload/free-count and reset controls, without
         * claiming the known-broken old destination byte accounting. */
        setup(4,4);command(0,0,1,0,4);assert(execute(0)==HIRQ_ECPY);
        unsigned ids[]={0,1,2,3};check_list(0,ids,4,1);check_list(1,ids,4,0);
        assert(!cdb.block_free && copied_bytes==4*2352);check_reply(0);
        cdb.status=STAT_PAUSE;cdb.cr1=0x0401;assert(init_cdblock()==(HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST));
        assert(cdb.block_free==MAX_BLOCKS);check_owners();
    } else if(test==26) {
        for(int pos=-1;pos<=4;pos++) {
            setup(4,0);remove_block(&cdb.part[0],pos);
            unsigned ids[4],n=0;for(int i=0;i<4;i++) if(i!=pos) ids[n++]=i;
            check_list(0,ids,n,1);check_owners();assert(!copied_bytes);
            assert(cdb.block_free==(pos>=0 && pos<4));
        }
    }
    puts("pass: copy/move admission, ownership, payload, outcome and reset");
    return 0;
}
