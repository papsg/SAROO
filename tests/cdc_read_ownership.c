/* MCU-CDC-010: adapted from paps-g 3d2c405e, actual lists/commands/FIFO. */
#define CDC_OWNERSHIP_HOOKS
#define CDC_BOUNDS_MAIN bounds_fixture_main
#include "cdc_bounds.c"
static void memcpy32(void *out,const void *in,unsigned bytes) { memcpy(out,in,bytes); }
#include "ownership.inc"

static void start_read(unsigned type,unsigned part,unsigned start,unsigned count)
{
    cdb.cr3=part<<8;cdb.cr2=start;cdb.cr4=count;fifo_words=0;fifo_status=0;
    assert((type==TRANS_DATA?get_sector_data():get_del_sector_data())==(HIRQ_DRDY|HIRQ_EHST));
}
static unsigned result_words(void) { return ((unsigned)(reply[0]&255)<<16)|reply[1]; }
static void read_owners(unsigned count)
{
    unsigned seen[200]={0},used=0;
    for(unsigned p=0;p<24;p++) {
        BLOCK *b=cdb.part[p].head,*tail=NULL;unsigned n=0,bytes=0;
        while(b) {
            ptrdiff_t id=b-cdb.block;assert(id>=0 && id<200 && !seen[id]++);
            assert(b->size>0 && ++n<=200);bytes+=b->size;used++;tail=b;b=b->next;
        }
        assert(cdb.part[p].numblocks==(int)n && cdb.part[p].tail==tail);
        assert(cdb.part[p].size==(int)bytes || (!n && cdb.part[p].size==-1));
    }
    if(cdb.trans_type==TRANS_DATA_DEL) {
        BLOCK *b=&cdb.block[cdb.trans_block_start];unsigned n=0;
        while(b) {
            ptrdiff_t id=b-cdb.block;assert(id>=0 && id<200 && !seen[id]++);
            assert(b->size>0 && ++n<=count);used++;b=b->next;
        }
        assert(n==count);
    }
    assert(used+cdb.block_free==200 && !irq_depth);
    for(unsigned i=0;i<200;i++)assert(seen[i]==(cdb.block[i].size!=-1));
}
static void finish_read(unsigned sectors,unsigned words)
{
    unsigned services=0;
    while(!cdb.trans_finish){assert(++services<=sectors);trans_handle();}
    assert(fifo_words==words);
    assert(end_trans()==HIRQ_EHST && result_words()==words && !cdb.trans_type && !cdb.trans_block);
    check_owners();
}
static void detached_ranges(void)
{
    const unsigned lengths[]={1,2,17,199,200};
    for(unsigned k=0;k<5;k++)for(unsigned part=0;part<24;part++)
    for(unsigned shape=0;shape<3;shape++)for(unsigned tail=0;tail<2;tail++) {
        unsigned n=lengths[k],start=shape==0?0:shape==1?n/2:n-1,count=tail?n-start:1;
        setup(n,0);
        if(part){if(part==2){cdb.part[3]=cdb.part[2];cdb.part[2]=(PARTITION){.size=-1};}
            cdb.part[part]=cdb.part[0];cdb.part[0]=(PARTITION){.size=-1};}
        start_read(TRANS_DATA_DEL,part,start,count);
        assert(!cdb.block_free && cdb.part[part].numblocks==(int)(n-count));read_owners(count);
        BLOCK *b=cdb.part[part].head;
        for(unsigned i=0;i<n;i++)if(i<start || i>=start+count){assert(b==&cdb.block[i]);b=b->next;}
        assert(!b);finish_read(count,count*1176);assert(cdb.block_free==count);
        for(unsigned i=0;i<count;i++)for(unsigned w=0;w<1176;w++)
            assert(fifo_output[i*1176+w]==((start+i)*0x101));
    }
    setup(200,0);
    for(unsigned i=0;i<200;i++) {
        unsigned id=(i*73+37)%200;
        cdb.block[id].next=i==199?NULL:&cdb.block[((i+1)*73+37)%200];
    }
    cdb.part[0].head=&cdb.block[37];cdb.part[0].tail=&cdb.block[(199*73+37)%200];
    start_read(TRANS_DATA_DEL,0,1,198);read_owners(198);finish_read(198,198*1176);
    for(unsigned i=0;i<198;i++)for(unsigned w=0;w<1176;w++)
        assert(fifo_output[i*1176+w]==(((i+1)*73+37)%200)*0x101);
}
static void cancellation(void)
{
    for(unsigned type=TRANS_DATA;type<=TRANS_DATA_DEL;type++)for(unsigned op=0;op<5;op++) {
        setup(6,194);start_read(type,0,1,3);unsigned produced=fifo_words;
        cdb.cr3=0;cdb.cr2=0;cdb.cr4=1;
        if(op==0)assert(del_sector_data()==HIRQ_EHST);
        if(op==1)free_partition(&cdb.part[0]);
        if(op==2)remove_block(&cdb.part[0],0);
        if(op==3)assert(cal_actual_size()==HIRQ_ESEL);
        if(op==4){cdb.cr1=0;assert(reset_filter()==HIRQ_ESEL);}
        assert(cdb.trans_type==type && cdb.trans_part_index==255 && cdb.trans_finish);
        assert(!(st_control&STIRQ_DAT) && (hirq&HIRQ_EHST));
        read_owners(type==TRANS_DATA_DEL?3:0);trans_handle();assert(fifo_words==produced);
        assert(end_trans()==HIRQ_EHST && result_words()==0xffffff);check_owners();
    }
}
static void preserved_connections(void)
{
    for(unsigned type=TRANS_DATA;type<=TRANS_DATA_DEL;type++)for(unsigned op=0;op<4;op++) {
        setup(4,194);start_read(type,0,1,2);cdb.cr3=2<<8;cdb.cr2=0;cdb.cr4=1;
        if(op==0)assert(del_sector_data()==HIRQ_EHST);
        if(op==1)free_partition(&cdb.part[2]);
        if(op==2)assert(cal_actual_size()==HIRQ_ESEL);
        if(op==3){cdb.cr1=0x10;assert(reset_filter()==HIRQ_ESEL);}
        assert(cdb.trans_part_index==0 && !cdb.trans_finish);read_owners(type==TRANS_DATA_DEL?2:0);
        finish_read(2,2352);
    }
}
static void incoming_append(void)
{
    static u8 data[2352];TRACK_INFO track={.mode=1};BLOCK incoming={.size=2048,.data=data,.fad=2000};
    for(unsigned type=TRANS_DATA;type<=TRANS_DATA_DEL;type++) {
        setup(4,196);start_read(type,0,1,3);cdb.cddev_filter=0;
        assert(filter_sector(&track,&incoming)==0);assert(cdb.part[0].tail->fad==2000);
        read_owners(type==TRANS_DATA_DEL?3:0);finish_read(3,3528);
        assert(cdb.part[0].tail->fad==2000 && cdb.part[0].numblocks==(type==TRANS_DATA_DEL?2:5));
    }
}
static unsigned recycled;
static void recycle_freed(void)
{
    BLOCK *b=alloc_block();assert(b);b->size=2352;b->fad=5000+recycled;
    append_block(&cdb.part[0],b);recycled++;
    if(recycled<7)irq_restore_hook=recycle_freed;
}
static void detached_before_release(void)
{
    assert(!cdb.part[0].head && !cdb.part[0].tail && !cdb.part[0].numblocks);
    assert(cdb.block_free==193);irq_restore_hook=recycle_freed;
}
static void freeing_preemptions(void)
{
    setup(7,193);recycled=0;irq_restore_hook=detached_before_release;
    free_partition(&cdb.part[0]);
    assert(recycled==7 && cdb.part[0].numblocks==7 && cdb.block_free==193);check_owners();
    for(unsigned i=0;i<7;i++)assert(cdb.block[i].fad==5000+i);
}
static void free_during_log(void) { assert(!cdb.trans_type);free_partition(&cdb.part[0]); }
static void shrink_during_log(void) { assert(!cdb.trans_type);remove_block(&cdb.part[0],3); }
static void logging_boundary(void)
{
    const unsigned masks[]={_BUFIO,_TRANS};
    for(unsigned type=TRANS_DATA;type<=TRANS_DATA_DEL;type++)for(unsigned mask=0;mask<2;mask++)
    for(unsigned shrink=0;shrink<2;shrink++) {
        setup(4,196);cdb.cr3=0;cdb.cr2=0;cdb.cr4=shrink?4:2;fifo_words=0;
        boundary_log_hook=shrink?shrink_during_log:free_during_log;fixture_log_mask=masks[mask];
        assert(!(type==TRANS_DATA?get_sector_data():get_del_sector_data()));
        assert(!cdb.trans_type && !fifo_words && (reply[0]>>8)==(STAT_WAIT|cdb.status));
        fixture_log_mask=0;check_owners();
    }
}
static void admission_ranges(void)
{
    const unsigned lengths[]={0,1,2,17,199,200};
    for(unsigned l=0;l<6;l++)for(unsigned part=0;part<24;part++) {
        unsigned n=lengths[l],positions[]={0,1,n?n-1:0,n,n+1,65535};
        for(unsigned a=0;a<6;a++)for(unsigned b=0;b<6;b++)for(unsigned type=1;type<=2;type++) {
            seed(n,part);cdb.cr2=positions[a];cdb.cr4=positions[b];
            int start=positions[a]==65535?(int)n-1:(int)positions[a];
            int count=positions[b]==65535?(int)n-start:(int)positions[b];
            int valid=start>=0 && start<(int)n && count>0 && count<=(int)n-start;
            CDBLOCK before=cdb;int result=type==1?get_sector_data():get_del_sector_data();
            if(valid){assert(result==(HIRQ_DRDY|HIRQ_EHST));read_owners(type==2?count:0);
                /* Ownership-only checkpoint: early read counts are separate012. */
                assert(end_trans()==HIRQ_EHST);}
            else {assert(!result && !fifo_words && !memcmp(&before,&cdb,sizeof cdb));
                assert((reply[0]>>8)==(STAT_WAIT|cdb.status));}
            check_owners();
        }
    }
}
static void software_resets(void)
{
    for(unsigned type=TRANS_DATA;type<=TRANS_DATA_DEL;type++)for(unsigned reset=0;reset<2;reset++) {
        setup(6,194);start_read(type,0,1,3);unsigned words=fifo_words;
        cdb.cr1=reset;cdb.play_type=0;init_cdblock();
        if(reset) {
            assert(!cdb.trans_type && !cdb.trans_block && cdb.block_free==200);
            trans_handle();assert(fifo_words==words);
            assert(end_trans()==HIRQ_EHST && result_words()==0xffffff);check_owners();
        } else {assert(cdb.trans_type==type);read_owners(type==TRANS_DATA_DEL?3:0);finish_read(3,3528);}
    }
}
static void valid_log_controls(void)
{
    for(unsigned type=TRANS_DATA;type<=TRANS_DATA_DEL;type++) {
        setup(4,196);fixture_log_mask=_BUFIO|_TRANS;start_read(type,0,1,2);
        const char *expected=type==TRANS_DATA?
            "get_sector_data: part=0(4) spos=1 snum=2\ntrans_start: pt=0 start=1 size=2\n":
            "get_del_sector_data: part=0(4) spos=1 snum=2\n                   : part=0(4) spos=1 snum=2\ntrans_start: pt=0 start=1 size=2\n";
        assert(!strcmp(expected,fixture_log_text));fixture_log_mask=0;
        /* Baseline-valid payload/log control does not assert private internals. */
        unsigned services=0;while(!cdb.trans_finish){assert(++services<4);trans_handle();}
        assert(fifo_words==2352 && end_trans()==HIRQ_EHST);check_owners();
    }
}
static void incomplete_getdel(void)
{
    setup(2,198);cdb.block[0].next=NULL;cdb.cr2=0;cdb.cr3=0;cdb.cr4=2;CDBLOCK before=cdb;fifo_words=0;
    assert(!get_del_sector_data() && (reply[0]>>8)==(STAT_WAIT|cdb.status));
    assert(!memcmp(&before,&cdb,sizeof cdb) && !fifo_words);
    cdb.block[0].next=&cdb.block[1];check_owners();
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<10);
    if(test==0)detached_ranges();else if(test==1)cancellation();else if(test==2)preserved_connections();
    else if(test==3)incoming_append();else if(test==4)freeing_preemptions();else if(test==5)logging_boundary();
    else if(test==6)admission_ranges();else if(test==7)software_resets();else if(test==8)valid_log_controls();
    else incomplete_getdel();puts("pass: actual read ownership, cancellation, release and log boundaries");return 0;
}
