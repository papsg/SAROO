/* MCU-CDC-007: adapted from paps-g 3f7bba1a; independent ownership/FIFO oracles. */
#define main copy_fixture_main
#include "cdc_copy.c"
#undef main

static u16 fifo_rcnt,fifo_input[2048];
static unsigned fifo_pos,fifo_words,fifo_samples;
static int min_num,require_ack;
_Alignas(8) static u8 startup_info[0x0e00+100*sizeof(TRACK_INFO)+2048];
#undef ST_STAT
#undef FIFO_DATA
#undef FIFO_STAT
#undef FIFO_RCNT
#define ST_STAT st_status
#define FIFO_DATA read_fifo()
#define FIFO_STAT fifo_occupancy()
#define FIFO_RCNT fifo_rcnt
static u16 read_fifo(void)
{
    assert(!irq_depth && fifo_pos<fifo_words);
    if(require_ack)assert(st_status==STIRQ_DAT);
    return fifo_input[fifo_pos++];
}
/* FPGA05:11 used-word bits plus full bit11,2048 words. No bus timing model. */
static u16 fifo_occupancy(void)
{
    fifo_samples++;
    unsigned available=fifo_words-fifo_pos;
    assert(available<=2048);return available==2048?0x0800:available;
}
static void fill_fifo(u8 *data,int size) { (void)data;(void)size;assert(0); }
#include "put.inc"

static void private_owners(unsigned expected)
{
    unsigned seen[MAX_BLOCKS]={0},reserved=0,used=0;
    BLOCK *bt=cdb.trans_type==TRANS_PUT ? &cdb.block[cdb.trans_block_start] : NULL;
    while(bt) {
        ptrdiff_t i=bt-cdb.block;assert(i>=0 && i<MAX_BLOCKS && !seen[i]++);
        assert(bt->size>=0 && bt->size<=cdb.trans_size && ++reserved<=200);
        bt=bt->next;
    }
    assert(reserved==expected);
    for(unsigned p=0;p<24;p++) {
        unsigned count=0,bytes=0;BLOCK *tail=NULL;
        for(bt=cdb.part[p].head;bt;bt=bt->next) {
            ptrdiff_t i=bt-cdb.block;assert(i>=0 && i<MAX_BLOCKS && !seen[i]++);
            assert(bt->size>0 && ++count<=200);bytes+=bt->size;tail=bt;
        }
        assert(cdb.part[p].numblocks==(int)count && cdb.part[p].tail==tail);
        assert(cdb.part[p].size==(int)bytes || (!count && cdb.part[p].size==-1));
        used+=count;
    }
    assert(reserved+used+cdb.block_free==200 && !irq_depth);
    for(unsigned i=0;i<200;i++) assert(seen[i]==(cdb.block[i].size!=-1));
}
static void start_put(unsigned count,unsigned size)
{
    cdb.put_sector_size=size;cdb.cr3=5<<8;cdb.cr4=count;
    fifo_pos=fifo_words=fifo_samples=0;st_control=0;
    assert(put_sector_data()==HIRQ_DRDY);
}
static void offer_words(unsigned words,unsigned salt)
{
    assert(words<=2048 && fifo_pos==fifo_words);
    fifo_pos=0;fifo_words=words;
    for(unsigned i=0;i<words;i++) fifo_input[i]=(u16)(i*13+salt);
}
static void word_reply(unsigned words)
{
    assert((reply[0]>>8)==cdb.status);
    assert((((unsigned)(reply[0]&255)<<16)|reply[1])==words);
    assert(!reply[2] && !reply[3]);
}
static void put_reservation(void)
{
    for(unsigned count=1;count<=200;count++) {
        setup(0,200);cdb.cddev_filter=5;cdb.filter[11].c_false=5;
        cdb.filter[5].c_true=7;cdb.last_buffer=19;start_put(count,2048);
        assert(cdb.block_free==200-count && cdb.cddev_filter==0xff);
        assert(cdb.filter[11].c_false==0xff && cdb.part[5].numblocks==0);
        assert((reply[0]>>8)==(STAT_TRNS|cdb.status));
        private_owners(count);
        /* All remaining capacity may be consumed after DRDY without affecting PUT. */
        while(cdb.block_free) { BLOCK *b=alloc_block();assert(b);append_block(&cdb.part[12],b); }
        unsigned allocations=allocation_count;
        for(unsigned n=0;n<count;n++) {
            offer_words(1024,n);trans_handle();assert(fifo_pos==1024);
            assert(!cdb.part[7].numblocks);private_owners(count);
        }
        assert(!cdb.trans_bk && !cdb.trans_block && !cdb.block_free);
        assert(end_trans()==HIRQ_EHST);word_reply(count*1024);
        assert(cdb.part[7].numblocks==(int)count && cdb.last_buffer==19);
        assert(allocation_count==allocations && !copied_bytes);check_owners();
    }
}
static void put_partial(void)
{
    const unsigned sizes[]={2048,2336,2340,2352};
    for(unsigned t=0;t<4;t++) for(unsigned words=0;words<=2048;words++) {
        setup(0,200);memset(sector_bytes,0xa5,sizeof sector_bytes);
        unsigned size=sizes[t],offs=t?2352-size:24;
        start_put(3,size);offer_words(words,43);
        /* DataEnd must consume even sub-threshold input with no DAT interrupt. */
        assert(end_trans()==HIRQ_EHST);word_reply(words);
        assert(fifo_pos==words && !cdb.trans_type && !cdb.trans_block && !cdb.trans_bk);
        assert(!(st_control&(0x0200|STIRQ_DAT)) && cdb.part[5].numblocks==3);
        unsigned consumed=0;
        for(BLOCK *b=cdb.part[5].head;b;b=b->next) {
            assert(b->size==(int)size);
            for(unsigned i=0;i<offs;i++)assert(b->data[i]==0);
            for(unsigned i=0;i<size;i+=2) {
                u16 value=consumed<words ? (u16)(consumed*13+43) : 0xa5a5;
                assert(b->data[offs+i]==(value&255) && b->data[offs+i+1]==(value>>8));
                consumed++;
            }
        }
        check_owners();
        assert(end_trans()==HIRQ_EHST);word_reply(0xffffff);
        trans_handle();check_owners();assert(!cdb.cdwnum);
    }
}
static void put_fifo_edges(void)
{
    setup(0,200);start_put(4,2352);private_owners(4);
    trans_handle();assert(!fifo_pos && !cdb.cdwnum && cdb.trans_bk==4 && fifo_samples<=1);
    const unsigned chunks[]={1,255,256,1023,1024,2048,96};
    unsigned total=0;
    for(unsigned i=0;i<sizeof chunks/sizeof *chunks;i++) {
        offer_words(chunks[i],i);unsigned snapshots=fifo_samples;
        trans_handle();assert(fifo_samples-snapshots<=1);total+=chunks[i];
        assert(fifo_pos==chunks[i] && cdb.cdwnum==(int)total*2);private_owners(4);
    }
    assert(end_trans()==HIRQ_EHST);word_reply(total);check_owners();
    /* Extra input cannot overrun the reserved list or increase the word count. */
    setup(0,200);start_put(1,2048);offer_words(2048,7);trans_handle();
    assert(fifo_pos==1024 && !cdb.trans_bk && !cdb.trans_block);
    trans_handle();assert(fifo_pos==1024);
    assert(end_trans()==HIRQ_EHST);word_reply(1024);check_owners();
}
static void put_routes(void)
{
    for(unsigned disconnected=0;disconnected<2;disconnected++) {
        setup(0,200);start_put(3,2352);cdb.last_buffer=19;
        cdb.filter[5]=(FILTER){.mode=1,.fid=7,.c_true=3,.c_false=6};
        cdb.filter[6]=(FILTER){.mode=1,.fid=8,.c_true=4,.c_false=0xff};
        for(unsigned n=0;n<3;n++) {
            offer_words(1176,n);
            fifo_input[6]=0x3412;fifo_input[7]=0x0256;
            fifo_input[8]=7+n;fifo_input[9]=0;
            trans_handle();assert(fifo_pos==1176);private_owners(3);
            assert(!cdb.part[3].numblocks && !cdb.part[4].numblocks);
        }
        if(disconnected)cdb.filter[5].c_true=0xff;
        assert(end_trans()==HIRQ_EHST);word_reply(3528);
        assert(cdb.part[3].numblocks==!disconnected && cdb.part[4].numblocks==1);
        assert(cdb.part[5].numblocks==0 && cdb.block_free==198+disconnected);
        assert(cdb.last_buffer==19 && !log_count);check_owners();
        if(!disconnected) {
            BLOCK *a=cdb.part[3].head,*b=cdb.part[4].head;
            assert(a && b && a->fad==56606 && b->fad==56606);
            assert(a->fn==7 && b->fn==8 && !a->cn && !b->cn);
        }
    }
}
static void put_failure(void)
{
    for(unsigned lost=1;lost<=6;lost++) {
        setup(0,6);cdb.put_sector_size=2352;cdb.cr3=5<<8;cdb.cr4=6;
        lose_after=lost;st_control=0;fifo_pos=fifo_words=0;
        int result=put_sector_data();
        if(lost<6) {
            assert(result==HIRQ_EHST && !cdb.trans_type && !st_control);
            assert(cdb.block_free==lost);check_owners();
        } else {
            assert(result==HIRQ_DRDY);private_owners(6);
            assert(end_trans()==HIRQ_EHST);word_reply(0);check_owners();
        }
    }
}
static void put_reset(void)
{
    const unsigned modes[]={0,0x04,0x10,0x40,0x80};
    for(unsigned i=0;i<sizeof modes/sizeof *modes;i++) {
        setup(2,198);start_put(3,2048);offer_words(255,2);trans_handle();
        cdb.cr1=modes[i];cdb.cr3=5<<8;
        assert(reset_filter()==HIRQ_ESEL);private_owners(3);
        assert(end_trans()==HIRQ_EHST);word_reply(255);check_owners();
    }
    for(unsigned flag=0;flag<3;flag++) {
        setup(0,200);start_put(3,2048);offer_words(255,2);trans_handle();
        cdb.cr1=flag==2?0x80:flag;
        init_cdblock();
        if(flag==1) {
            assert(!cdb.trans_type && !cdb.trans_block && !cdb.trans_bk);
            assert(cdb.block_free==200);check_owners();trans_handle();assert(!cdb.cdwnum);
            assert(end_trans()==HIRQ_EHST);word_reply(0xffffff);
        } else {
            private_owners(3);assert(end_trans()==HIRQ_EHST);word_reply(255);check_owners();
        }
    }
}
static void put_input_theft(void)
{
    for(unsigned method=0;method<4;method++) {
        setup(0,200);start_put(3,2048);offer_words(255,2);trans_handle();
        if(method==0){cdb.cr3=5<<8;assert(set_cdev_connect()==HIRQ_ESEL);}
        if(method==1){cdb.cr1=2;cdb.cr2=5;cdb.cr3=6<<8;assert(set_filter_connection()==HIRQ_ESEL);}
        if(method>=2){cdb.cr1=method==2?0x20:0x24;cdb.cr3=0xff00;assert(reset_filter()==HIRQ_ESEL);}
        assert(cdb.trans_part_index==0xff && hirq==HIRQ_EHST);private_owners(3);
        cdb.cr3=5<<8;cdb.cr4=1;CDBLOCK before=cdb;
        assert(!put_sector_data() && (reply[0]>>8)==(STAT_WAIT|cdb.status));
        assert(!memcmp(&before,&cdb,sizeof cdb));
        offer_words(128,4);unsigned snapshots=fifo_samples;trans_handle();
        assert(fifo_pos==0 && fifo_samples==snapshots);
        assert(end_trans()==HIRQ_EHST);word_reply(0xffffff);
        assert(cdb.block_free==200);check_owners();
        if(method==0)assert(cdb.cddev_filter==5);
        if(method==1)assert(cdb.filter[6].c_false==5);
        /* A subsequent transfer recovers normally, taking ownership anew. */
        start_put(1,2048);offer_words(1024,9);trans_handle();
        assert(end_trans()==HIRQ_EHST);word_reply(1024);check_owners();
    }
}

static void admission(void)
{
    for(unsigned fid=24;fid<256;fid++) {
        setup(0,200);cdb.cr3=fid<<8;cdb.cr4=1;CDBLOCK before=cdb;
        assert(!put_sector_data() && (reply[0]>>8)==STAT_REJECT);
        assert(!memcmp(&before,&cdb,sizeof cdb) && !allocation_count);
    }
    const unsigned counts[]={0,201,65535};
    for(unsigned n=0;n<3;n++) {
        setup(0,200);cdb.cddev_filter=5;cdb.cr3=5<<8;cdb.cr4=counts[n];CDBLOCK before=cdb;
        assert(!put_sector_data() && (reply[0]>>8)==(STAT_WAIT|cdb.status));
        assert(!memcmp(&before,&cdb,sizeof cdb) && !allocation_count);
    }
    for(unsigned type=1;type<=TRANS_PUT;type++) {
        setup(0,200);cdb.trans_type=type;cdb.cr3=5<<8;cdb.cr4=1;CDBLOCK before=cdb;
        assert(!put_sector_data() && (reply[0]>>8)==(STAT_WAIT|cdb.status));
        assert(!memcmp(&before,&cdb,sizeof cdb) && !allocation_count);
    }
}
static void defaults_and_write(void)
{
    setup(0,200);ss_sw_init();check_owners();
    assert(cdb.sector_size==2048 && cdb.put_sector_size==2048);
    for(unsigned i=0;i<24;i++)assert(cdb.filter[i].c_true==i && !cdb.filter[i].range);
    start_put(1,2048);offer_words(1024,19);trans_handle();
    assert(end_trans()==HIRQ_EHST);word_reply(1024);check_owners();
    cdb.sector_size=2352;cdb.put_sector_size=2336;
    for(unsigned i=0;i<24;i++)cdb.filter[i].fid=55;
    cdb.cr1=1;init_cdblock();check_owners();
    assert(cdb.sector_size==2048 && cdb.put_sector_size==2048);
    start_put(1,2048);offer_words(1024,21);trans_handle();
    assert(end_trans()==HIRQ_EHST);word_reply(1024);check_owners();
}
static void noncontiguous(void)
{
    setup(10,190);
    for(int i=0;i<5;i++)remove_block(&cdb.part[0],i+1);
    const unsigned ids[]={1,3,5,7,9,10,11,12};
    start_put(8,2048);assert(cdb.trans_block_start==1);private_owners(8);
    BLOCK *p=&cdb.block[cdb.trans_block_start];
    for(unsigned i=0;i<8;i++,p=p->next)assert(p==&cdb.block[ids[i]]);
    assert(!p);assert(end_trans()==HIRQ_EHST);word_reply(0);check_owners();
    p=cdb.part[5].head;
    for(unsigned i=0;i<8;i++,p=p->next)assert(p==&cdb.block[ids[i]]);
    assert(!p);
    setup(199,1);start_put(1,2048);assert(cdb.trans_block_start==199);private_owners(1);
    assert(end_trans()==HIRQ_EHST);word_reply(0);
    assert(cdb.part[5].head==&cdb.block[199]);check_owners();
}
static void acknowledgment_and_idle(void)
{
    setup(0,200);start_put(1,2352);offer_words(255,37);st_status=0;
    require_ack=1;trans_handle();require_ack=0;
    assert(fifo_pos==255 && cdb.cdwnum==510);
    assert(end_trans()==HIRQ_EHST);word_reply(255);check_owners();
    CDBLOCK before=cdb;u16 control=st_control,status=st_status;unsigned snapshots=fifo_samples;
    trans_handle();assert(!memcmp(&before,&cdb,sizeof cdb) && fifo_samples==snapshots);
    assert(st_control==control && st_status==status);
}
static void unrelated_input_and_captured_length(void)
{
    setup(0,200);start_put(1,2048);cdb.last_buffer=19;
    cdb.cr3=6<<8;assert(set_cdev_connect()==HIRQ_ESEL);
    cdb.cr1=2;cdb.cr2=6;cdb.cr3=8<<8;assert(set_filter_connection()==HIRQ_ESEL);
    assert(cdb.trans_part_index==5 && hirq!=HIRQ_EHST);private_owners(1);
    cdb.cr1=1;cdb.cr2=7<<8;cdb.cr3=5<<8;assert(set_filter_connection()==HIRQ_ESEL);
    /* Length changes during a transfer are prohibited by Sega. This checks
     * the captured-length invariant, not acceptance of such a hardware command. */
    cdb.put_sector_size=2352;offer_words(1024,77);trans_handle();
    assert(!cdb.trans_bk && cdb.trans_size==2048);
    assert(end_trans()==HIRQ_EHST);word_reply(1024);
    assert(cdb.part[7].numblocks==1 && cdb.part[7].size==2048 && cdb.last_buffer==19);
    assert(cdb.filter[8].c_false==6 && !log_count);check_owners();
}
static void baseline_payload_control(void)
{
    const unsigned sizes[]={2048,2336,2340,2352};
    for(unsigned n=0;n<4;n++) {
        setup(0,200);start_put(1,sizes[n]);offer_words(sizes[n]/2,19);
        unsigned turns=0;while(cdb.trans_bk) { assert(++turns<4);trans_handle(); }
        assert(end_trans()==HIRQ_EHST && fifo_pos==fifo_words);
        BLOCK *b=cdb.part[5].head;unsigned offset=n?2352-sizes[n]:24;
        assert(b && !b->next && b->size==(int)sizes[n]);
        for(unsigned i=0;i<sizes[n]/2;i++) {
            u16 expected=(u16)(i*13+19);
            assert(b->data[offset+i*2]==(expected&255) && b->data[offset+i*2+1]==(expected>>8));
        }
        /* Counts/private publication/bytes are intentionally not baseline
         * oracles here: separate cases reproduce those known defects. */
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<13);
    if(test==0)admission();else if(test==1)put_reservation();else if(test==2)put_partial();
    else if(test==3)put_fifo_edges();else if(test==4)put_routes();else if(test==5)put_failure();
    else if(test==6)put_reset();else if(test==7)put_input_theft();
    else if(test==8)defaults_and_write();else if(test==9)noncontiguous();
    else if(test==10)acknowledgment_and_idle();
    else if(test==11)unrelated_input_and_captured_length();else baseline_payload_control();
    puts("pass: actual host write admission, reservation, FIFO, count and ownership");return 0;
}
