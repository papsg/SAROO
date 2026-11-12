/* Adapted from paps-g 661fd2d4; actual router with independent payload/list oracles. */
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
static u8 pool[MAX_BLOCKS][2352], incoming[2352];
static TRACK_INFO track;
static BLOCK sector;
static u16 hirq;
static int irq_depth, change_connection;
static unsigned irq_entries, payload_bytes;
static void (*publication_probe)(void);
#undef HIRQ
#define HIRQ hirq
static unsigned log_lines;
int capture_log(const char *text)
{
    assert(!strcmp(text,"  c_false is FF!\n"));log_lines++;return 0;
}
#define printk capture_log
static int disable_irq(void) { irq_entries++; return irq_depth++; }
static void restore_irq(int old)
{
    assert(irq_depth==old+1); irq_depth=old;
    if (!old && change_connection) {
        /* Model a command between allocator completion and append. */
        cdb.filter[cdb.cddev_filter].c_true=0xff;
        change_connection=0;
    }
    if (!old && publication_probe) publication_probe();
}
static void *copy_payload(void *to,const void *from,size_t bytes)
{
    assert(!irq_depth); payload_bytes+=bytes; return memcpy(to,from,bytes);
}
#define memcpy32 copy_payload
#include "filter.inc"

static void setup(unsigned format)
{
    memset(&cdb,0,sizeof cdb); memset(&track,0,sizeof track);
    memset(&sector,0,sizeof sector); memset(pool,0xa5,sizeof pool);
    for(unsigned i=0;i<MAX_SELECTORS;i++) {
        cdb.filter[i].c_true=i; cdb.filter[i].c_false=0xff;
    }
    for(unsigned i=0;i<MAX_BLOCKS;i++) {
        cdb.block[i].data=pool[i]; cdb.block[i].size=-1;
    }
    for(unsigned i=0;i<sizeof incoming;i++) incoming[i]=(u8)(i*13+7);
    track.mode=format==4?3:1; track.sector_size=format==0?2048:2352;
    incoming[15]=format>=2 && format<=3?2:1;
    sector.data=incoming; sector.size=track.sector_size; sector.fad=250;
    sector.fn=7; sector.cn=9; sector.sm=format==3?0x28:8; sector.ci=0x11;
    cdb.sector_size=2048; cdb.block_free=MAX_BLOCKS; cdb.last_buffer=19;
    log_lines=0; irq_depth=0; irq_entries=0; payload_bytes=0; change_connection=0; hirq=0;
    publication_probe=NULL;
}

static void reject_at(unsigned filter,unsigned next)
{
    cdb.filter[filter].mode=0x40;
    cdb.filter[filter].fad=sector.fad+1; cdb.filter[filter].range=1;
    cdb.filter[filter].c_false=next;
}

static void owners(void)
{
    unsigned seen[MAX_BLOCKS]={0},used=0;
    for(unsigned i=0;i<MAX_SELECTORS;i++) {
        PARTITION *p=&cdb.part[i]; BLOCK *b=p->head,*last=NULL;
        int count=0;
        while(b) {
            ptrdiff_t n=b-cdb.block; assert(n>=0 && n<MAX_BLOCKS);
            assert(!seen[n]++ && b->size>0 && ++count<=MAX_BLOCKS);
            used++; last=b; b=b->next;
        }
        assert(p->tail==last && p->numblocks==count);
    }
    assert(used+cdb.block_free==MAX_BLOCKS && !irq_depth);
    for(unsigned i=0;i<MAX_BLOCKS;i++) assert(seen[i]==(cdb.block[i].size!=-1));
}

static void discarded(void)
{
    CDBLOCK before=cdb; unsigned copied=payload_bytes,entries=irq_entries;
    assert(filter_sector(&track,&sector)==1 && cdb.last_buffer==0xff);
    before.last_buffer=0xff;
    assert(!memcmp(&before,&cdb,sizeof cdb));
    assert(copied==payload_bytes && entries==irq_entries); owners();
}

static void delivered(unsigned partition,unsigned count,unsigned format)
{
    assert(cdb.last_buffer==partition && cdb.block_free==(int)(MAX_BLOCKS-count));
    PARTITION *p=&cdb.part[partition]; BLOCK *b=p->head;
    unsigned size=format==3?2324:2048;
    assert(p->numblocks==(int)count);
    for(unsigned i=0;i<count;i++) {
        assert(b && b->fad==sector.fad && b->fn==sector.fn && b->cn==sector.cn);
        assert(b->sm==sector.sm && b->ci==sector.ci && b->size==(int)size);
        if(format==0) {
            for(unsigned j=0;j<24;j++) assert(!b->data[j]);
            assert(!memcmp(b->data+24,incoming,2048));
        } else if(format==1) {
            assert(!memcmp(b->data,incoming,16));
            for(unsigned j=16;j<24;j++) assert(!b->data[j]);
            assert(!memcmp(b->data+24,incoming+16,2048));
        } else assert(!memcmp(b->data,incoming,2352));
        b=b->next;
    }
    assert(!b); owners();
}

static PARTITION before_publish;
static unsigned published, destination, previous_destination;

static void observe_publication(void)
{
    PARTITION *p=&cdb.part[destination];
    if (cdb.last_buffer==previous_destination && p->numblocks==before_publish.numblocks &&
        p->head==before_publish.head && p->tail==before_publish.tail &&
        (!p->tail || !p->tail->next)) return;
    assert(cdb.last_buffer==destination && p->numblocks==before_publish.numblocks+1);
    assert(p->head && p->tail && p->tail!=before_publish.tail && !p->tail->next);
    if (before_publish.tail) assert(before_publish.tail->next==p->tail && p->head==before_publish.head);
    else assert(p->head==p->tail);
    assert(p->tail->fad==sector.fad && p->tail->size==2048);
    published++;
}

static void publication(void)
{
    for (destination=0; destination<MAX_SELECTORS; destination++) {
        setup(0); cdb.filter[0].c_true=destination; cdb.last_buffer=0xff;
        for (unsigned n=1; n<=2; n++) {
            before_publish=cdb.part[destination];
            previous_destination=cdb.last_buffer;
            published=0; publication_probe=observe_publication;
            assert(filter_sector(&track,&sector)==0);
            publication_probe=NULL;
            assert(published==1);
            delivered(destination,n,0);
        }
        /* A caller with interrupts already masked retains that state. */
        BLOCK *b=alloc_block(); assert(b); b->size=2048;
        int key=disable_irq();
        append_block(&cdb.part[destination],b);
        assert(irq_depth==key+1 && cdb.part[destination].tail==b && !b->next);
        restore_irq(key); owners();
    }
}

int main(int argc,char **argv)
{
    assert(argc==2); int test=atoi(argv[1]); assert(test>=0 && test<=13);
    assert(MAX_SELECTORS==24 && MAX_BLOCKS==200);
    if(test==0) {
        for(unsigned f=0;f<MAX_SELECTORS;f++) {
            setup(2); cdb.cddev_filter=f; cdb.filter[f].c_true=0xff; discarded();
        }
    } else if(test==1) {
        for(unsigned f=24;f<256;f++) {
            setup(2); cdb.cddev_filter=f; discarded();
            setup(2); reject_at(0,f); discarded();
        }
    } else if(test==2) {
        for(unsigned f=24;f<255;f++) {
            setup(2); cdb.filter[0].c_true=f; discarded();
        }
    } else if(test==3) {
        for(unsigned n=1;n<=MAX_SELECTORS;n++) for(unsigned start=0;start<n;start++) {
            setup(2); cdb.cddev_filter=start;
            for(unsigned i=0;i<n;i++) reject_at(i,(i+1)%n);
            discarded();
        }
    } else if(test==4) {
        for(unsigned start=0;start<MAX_SELECTORS;start++)
        for(unsigned dest=0;dest<MAX_SELECTORS;dest++) {
            setup(2); cdb.cddev_filter=start;
            for(unsigned i=0;i<MAX_SELECTORS-1;i++)
                reject_at((start+i)%MAX_SELECTORS,(start+i+1)%MAX_SELECTORS);
            cdb.filter[(start+MAX_SELECTORS-1)%MAX_SELECTORS].c_true=dest;
            assert(filter_sector(&track,&sector)==0); delivered(dest,1,2);
        }
    } else if(test==5) {
        for(unsigned disconnected=0;disconnected<3;disconnected++) {
            setup(2); cdb.filter[0].c_true=11;
            assert(filter_sector(&track,&sector)==0); delivered(11,1,2);
            if(disconnected==0) cdb.cddev_filter=0xff;
            else if(disconnected==1) cdb.filter[0].c_true=0xff;
            else reject_at(0,0xff);
            discarded();
        }
    } else if(test==6) {
        setup(2); cdb.filter[0].c_true=3;
        for(unsigned i=0;i<MAX_BLOCKS;i++) assert(filter_sector(&track,&sector)==0);
        delivered(3,MAX_BLOCKS,2); cdb.filter[0].c_true=23;
        unsigned copied=payload_bytes; CDBLOCK before=cdb;
        assert(filter_sector(&track,&sector)==2 && hirq==HIRQ_BFUL);
        assert(!memcmp(&before,&cdb,sizeof cdb) && payload_bytes==copied);
        /* Independently free one owned block, then retry this same sector. */
        PARTITION *p=&cdb.part[3]; BLOCK *b=p->head;
        p->head=b->next; p->numblocks--;
        b->size=-1; b->next=NULL; cdb.block_free++;
        assert(filter_sector(&track,&sector)==0 && cdb.last_buffer==23);
        assert(cdb.part[23].head==b && !cdb.block_free); owners();
    } else if(test==7) {
        for(unsigned format=0;format<5;format++) {
            setup(format); cdb.cddev_filter=6; cdb.filter[6].c_true=17;
            for(unsigned n=1;n<=3;n++) {
                assert(filter_sector(&track,&sector)==0); delivered(17,n,format);
            }
            setup(format); cdb.filter[0].c_true=17;
            for(unsigned n=0;n<4;n++) {
                sector.fad=250+n; incoming[100]=n;
                assert(filter_sector(&track,&sector)==0);
            }
            BLOCK *b=cdb.part[17].head;
            for(unsigned n=0;n<4;n++,b=b->next) {
                unsigned offset=format==0?124:format==1?108:100;
                assert(b && b->fad==250+n && b->data[offset]==n);
            }
            assert(!b); owners();
        }
    } else if(test==8) {
        for(unsigned condition=0;condition<4;condition++)
        for(unsigned mismatch=0;mismatch<2;mismatch++)
        for(unsigned invert=0;invert<2;invert++) {
            setup(2); FILTER *f=&cdb.filter[0];
            f->mode=(1u<<condition)|(invert?0x10:0); f->c_true=8;
            f->fid=sector.fn; f->chan=sector.cn;
            f->smmask=0x08; f->smval=0x08; f->cimask=0x11; f->cival=0x11;
            if(mismatch) {
                if(condition==0) f->fid++;
                else if(condition==1) f->chan++;
                else if(condition==2) f->smval=0;
                else f->cival=0;
            }
            if(mismatch==invert) { assert(filter_sector(&track,&sector)==0); delivered(8,1,2); }
            else discarded();
        }
        const unsigned fad[]={199,200,201,209,210,211};
        for(unsigned i=0;i<sizeof fad/sizeof *fad;i++) {
            setup(2); sector.fad=fad[i]; cdb.filter[0].mode=0x40;
            cdb.filter[0].fad=200; cdb.filter[0].range=10;
            if(fad[i]>=200 && fad[i]<210) {
                assert(filter_sector(&track,&sector)==0); delivered(0,1,2);
            } else discarded();
        }
        setup(2); cdb.filter[0].mode=0x40; cdb.filter[0].fad=sector.fad;
        cdb.filter[0].range=0; discarded();
    } else if(test==9) {
        setup(2); cdb.cddev_filter=5; cdb.filter[5].c_true=9; change_connection=1;
        assert(filter_sector(&track,&sector)==0 && cdb.filter[5].c_true==0xff);
        delivered(9,1,2); discarded();
    } else if(test==10) {
        setup(2);reject_at(0,0xff);discarded();assert(log_lines==1);
        setup(4);reject_at(0,0xff);discarded();assert(!log_lines);
    } else if(test==11) {
        for(unsigned format=0;format<5;format++) {
            setup(format);cdb.filter[0].c_true=17;
            for(unsigned n=1;n<=3;n++) {
                assert(filter_sector(&track,&sector)==0);delivered(17,n,format);
            }
            assert(!log_lines);
        }
    }
    if(test==12)for(unsigned fid=24;fid<256;fid++) {
        setup(2);cdb.cddev_filter=fid;CDBLOCK before=cdb;before.last_buffer=0xff;
        assert(filter_sector(NULL,NULL)==1 && !memcmp(&before,&cdb,sizeof cdb));
        assert(!payload_bytes && !irq_entries && !log_lines);owners();
    }
    if(test==13)publication();
    puts("pass: incoming routing bounds, discard, retry, payload and ownership");
    return 0;
}
