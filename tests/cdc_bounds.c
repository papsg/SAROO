/* MCU-CDC-008/009: adapted from paps-g 89e644f6; independent command/FIFO oracles. */
#define main copy_fixture_main
#include "cdc_copy.c"
#undef main

#ifdef TEST_AXI_FILE_INFO
static struct { u32 before; FILE_INFO table[256]; u32 after; } directory_memory;

static void bounds_setup(unsigned source_count, unsigned free_count)
{
    setup(source_count, free_count);
    directory_memory.before = directory_memory.after = 0x76543210;
    memset(directory_memory.table, 0, sizeof(directory_memory.table));
    cdb.fi = directory_memory.table;
}
#define setup bounds_setup
#endif

static inline __attribute__((unused)) void legacy_file_info_wire(void *wire)
{
#ifdef TEST_AXI_FILE_INFO
    (void)wire;
#else
    cdb.FINFO = wire;
#endif
}

static int old_numblocks,min_num;
static u16 fifo_status,fifo_rcnt;
static u16 fifo_output[MAX_BLOCKS*1176];
static u16 fifo_input[3*1176];
static unsigned fifo_input_count,fifo_input_pos;
static unsigned fifo_words,wake_count,track_count;
static int check_fifo_publication;
static u16 *fifo_slot(void)
{
    if (fifo_input_pos<fifo_input_count) {
        assert(!irq_depth);
        return &fifo_input[fifo_input_pos++];
    }
    assert(!irq_depth && fifo_words<sizeof fifo_output/sizeof *fifo_output);
    if (check_fifo_publication) {
        assert(!fifo_rcnt && !(st_control & (0x0200 | STIRQ_DAT)));
        assert(st_status == 0x1234);
    }
    return &fifo_output[fifo_words++];
}
#undef FIFO_DATA
#undef FIFO_STAT
#undef FIFO_RCNT
#undef ST_STAT
#undef printk
#define FIFO_DATA (*fifo_slot())
static unsigned fifo_status_word(void)
{
    if (!fifo_input_count) return fifo_status;
    unsigned remaining=fifo_input_count-fifo_input_pos;
    return remaining>=2048 ? 0x0800 : remaining;
}
#define FIFO_STAT fifo_status_word()
#define FIFO_RCNT fifo_rcnt
#define ST_STAT st_status
#define printk(...) ((void)0)
static int (*file_pause_hook)(void);
static int pause_disc(void) { if(file_pause_hook) return file_pause_hook(); assert(!cdb.play_type);return 0; }
static int fad_to_track(int fad) { assert(fad>=150);track_count++;return 1; }
static __attribute__((unused)) void cdc_delay(int ticks) { (void)ticks;assert(0); }
static void bounds_wakeup(void) { wake_count++; }
#define disk_task_wakeup bounds_wakeup
#include "bounds.inc"
#undef disk_task_wakeup

static void seed(unsigned count,unsigned selector)
{
    setup(count,200-count);
    if(selector){cdb.part[selector]=cdb.part[0];cdb.part[0]=(PARTITION){0};}
    cdb.cr3=selector<<8;fifo_words=wake_count=track_count=0;
    fifo_status=fifo_rcnt=st_status=0;
}
static void invalid_selectors(void)
{
    int (*const handlers[])(void)={set_filter_range,get_filter_range,
        set_filter_subheader,get_filter_subheader,get_filter_mode,
        get_filter_connection,get_sector_num,cal_actual_size,get_sector_info,
        get_sector_data,get_del_sector_data,del_sector_data,change_dir,read_dir,read_file};
    for(unsigned h=0;h<sizeof handlers/sizeof *handlers;h++)
    for(unsigned selector=24;selector<256;selector++) {
        seed(3,0);cdb.cr1=0x12;cdb.cr2=0;cdb.cr3=(selector<<8)|0xff;cdb.cr4=1;
        cdb.play_type=PLAYTYPE_SECTOR;cdb.actsize=1234;cdb.cdir_lba=16;
        CDBLOCK before=cdb;memset(reply,0xa5,sizeof reply);
        assert(handlers[h]()==0 && (reply[0]>>8)==STAT_REJECT);
        assert(!memcmp(&before,&cdb,sizeof cdb) && !fifo_words && !wake_count && !track_count && !irq_depth);
        check_owners();
    }
}
static void valid_filter_registers(void)
{
    for(unsigned selector=0;selector<24;selector++) {
        seed(0,0);cdb.status=0xa5;cdb.cr3=(selector<<8)|0xcd;cdb.cr4=0xef12;
        cdb.cr1=0x4012;cdb.cr2=0x3456;
        assert(set_filter_range()==HIRQ_ESEL);
        assert(cdb.filter[selector].fad==0x123456 && cdb.filter[selector].range==0xcdef12);
        for(unsigned i=0;i<24;i++)if(i!=selector)assert(!cdb.filter[i].fad && !cdb.filter[i].range);
        assert(get_filter_range()==0 && reply[0]==0xa512 && reply[1]==0x3456);
        assert(reply[2]==0xcd && reply[3]==0xef12);
        assert(set_filter_subheader()==HIRQ_ESEL);
        assert(get_filter_subheader()==0 && reply[0]==0xa512 && reply[1]==0x3456);
        assert(reply[2]==0xcd && reply[3]==0xef12);
        cdb.filter[selector].mode=0x5f;cdb.filter[selector].c_true=23;cdb.filter[selector].c_false=0xff;
        assert(get_filter_mode()==0 && reply[0]==0xa55f && !reply[1] && !reply[2] && !reply[3]);
        assert(get_filter_connection()==0 && reply[0]==0xa500 && reply[1]==0x17ff && !reply[2] && !reply[3]);
        assert(get_sector_num()==0 && reply[0]==0xa500 && !reply[3]);check_owners();
    }
}
static void info_reply(const BLOCK *b)
{
    assert(reply[0]==((u16)cdb.status<<8 | ((b->fad>>16)&255)));
    assert(reply[1]==(b->fad&65535));
    assert(reply[2]==((u16)b->fn<<8 | b->cn));
    assert(reply[3]==((u16)b->sm<<8 | b->ci));
}
static void sector_positions(void)
{
    seed(200,0);cdb.status=0xa5;
    for(unsigned i=0;i<200;i++)cdb.block[i].fad=0x12ff80+i;
    for(unsigned pos=0;pos<65536;pos++) {
        cdb.cr2=pos;CDBLOCK before=cdb;
        assert(get_sector_info()==0 && !memcmp(&before,&cdb,sizeof cdb));
        if(pos<200 || pos==65535)info_reply(&cdb.block[pos==65535?199:pos]);
        else assert((reply[0]>>8)==STAT_REJECT);
    }
    check_owners();
}
static void empty_and_broken_info(void)
{
    for(unsigned selector=0;selector<24;selector++) {
        seed(1,selector);cdb.cr2=0xffff;
        assert(!get_sector_info());info_reply(&cdb.block[0]);check_owners();
        seed(0,selector);cdb.cr2=0xffff;CDBLOCK before=cdb;
        assert(!get_sector_info() && (reply[0]>>8)==STAT_REJECT && !memcmp(&before,&cdb,sizeof cdb));
        cdb.cr2=0;assert(!get_sector_info() && (reply[0]>>8)==STAT_REJECT);check_owners();
    }
    seed(2,0);cdb.block[0].next=NULL;cdb.cr2=1;
    CDBLOCK before=cdb;assert(!get_sector_info() && (reply[0]>>8)==STAT_REJECT);
    assert(!memcmp(&before,&cdb,sizeof cdb));
    cdb.block[0].next=&cdb.block[1];check_owners();
}
static void busy_precedence(void)
{
    for(unsigned type=1;type<=TRANS_PUT;type++)for(unsigned selector=24;selector<256;selector++) {
        seed(1,0);cdb.trans_type=type;cdb.cr3=selector<<8;CDBLOCK before=cdb;
        assert(!get_sector_data() && (reply[0]>>8)==(STAT_WAIT|cdb.status));
        assert(!get_del_sector_data() && (reply[0]>>8)==(STAT_WAIT|cdb.status));
        assert(!memcmp(&before,&cdb,sizeof cdb) && !fifo_words && !irq_depth);
    }
}
static void read_delete_controls(void)
{
    for(unsigned selector=0;selector<24;selector++)for(unsigned count=1;count<=17;count++) {
        seed(17,selector);cdb.cr2=17-count;cdb.cr4=0xffff;
        assert(del_sector_data()==HIRQ_EHST && cdb.part[selector].numblocks==(int)(17-count));
        check_owners();cdb.cr2=0xffff;cdb.cr4=1;
        if(count==17){CDBLOCK before=cdb;assert(!get_sector_data());assert(!memcmp(&before,&cdb,sizeof cdb));}
        else {
            assert(get_sector_data()==HIRQ_DRDY);
            trans_handle();assert(cdb.trans_finish && fifo_words==1176);
            assert(end_trans()==HIRQ_EHST);check_owners();
        }
    }
}
static void valid_file_selectors(void)
{
    for(unsigned selector=0;selector<24;selector++) {
        seed(0,0);cdb.cr3=(selector<<8)|0xff;cdb.cr4=0xffff;
        cdb.root_lba=20;cdb.root_size=3*2048;
        assert(!change_dir() && cdb.dir_filter==(int)selector && cdb.cddev_filter==(int)selector);
        assert(cdb.filter[selector].mode==0x40 && cdb.filter[selector].fad==170 && cdb.filter[selector].range==3);
        assert(wake_count==1 && track_count==1);
        seed(0,0);cdb.cr3=selector<<8;cdb.cr4=7;cdb.cdir_lba=20;cdb.cdir_size=3*2048;cdb.rdir_state=3;
        assert(!read_dir() && cdb.cdir_pfid==7 && cdb.cddev_filter==(int)selector);
        assert(cdb.filter[selector].fad==170 && wake_count==1 && track_count==1);
        seed(0,0);cdb.cr3=selector<<8;cdb.cr4=2;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;cdb.rdir_state=3;
        cdb.fi[2]=(FILE_INFO){.lba=20,.size=4096};
        assert(!read_file() && cdb.cddev_filter==(int)selector && cdb.play_type==PLAYTYPE_FILE);
        assert(cdb.play_fad_start==170 && cdb.play_fad_end==172 && wake_count==1 && track_count==1);
        check_owners();
    }
}
static void calculated_size_control(void)
{
    for(unsigned selector=0;selector<24;selector++) {
        seed(17,selector);cdb.sector_size=2352;cdb.cr2=0;cdb.cr4=17;
        assert(cal_actual_size()==HIRQ_ESEL && cdb.actsize==17*2352);
        assert(!get_actual_size() && (((unsigned)(reply[0]&255)<<16)|reply[1])==17*1176);
        check_owners();
    }
}
static void size_reply(unsigned words)
{
    unsigned original=cdb.actsize;
    for(unsigned repeat=0;repeat<3;repeat++) {
        assert(!get_actual_size());
        assert((reply[0]>>8)==cdb.status && (((unsigned)(reply[0]&255)<<16)|reply[1])==words);
        assert(!reply[2] && !reply[3] && cdb.actsize==(int)original);
    }
}
static void calculation_ranges(void)
{
    const unsigned lengths[]={0,1,2,17,200};
    for(unsigned l=0;l<sizeof lengths/sizeof *lengths;l++) {
        unsigned n=lengths[l];seed(n,0);cdb.sector_size=2336;
        unsigned boundaries[]={0,1,n?n-1:0,n,n+1,0xff,0x100,0xfffe,0xffff};
        for(unsigned si=0;si<9;si++)for(unsigned ni=0;ni<9;ni++) {
            cdb.cr2=boundaries[si];cdb.cr4=boundaries[ni];cdb.actsize=1234;
            int start=boundaries[si]==65535?(int)n-1:(int)boundaries[si];
            int count=boundaries[ni]==65535?(int)n-start:(int)boundaries[ni];
            int valid=start>=0 && start<(int)n && count>0 && count<=(int)n-start;
            CDBLOCK before=cdb;int result=cal_actual_size();
            if(valid){assert(result==HIRQ_ESEL);size_reply(count*1168);}
            else {assert(!result && (reply[0]>>8)==(STAT_WAIT|cdb.status));assert(!memcmp(&before,&cdb,sizeof cdb));}
            check_owners();
        }
    }
}
static void calculation_matches_fifo(void)
{
    const unsigned lengths[]={2048,2336,2340,2352};
    for(unsigned n=1;n<=200;n++)for(unsigned mode=0;mode<4;mode++) {
        seed(n,0);cdb.sector_size=lengths[mode];
        unsigned sum=0;
        for(unsigned i=0;i<n;i++) {
            BLOCK *b=&cdb.block[i];
            /* Stored lengths/cached tags deliberately differ from read format.
             * Mode1 also has bit5 set: mode must participate in XA detection. */
            b->size=i&1?2048:2352;b->sm=0xff;
            unsigned format=(i+n)%3; /* Rotate first and subsequent formats. */
            b->data[15]=format==0?1:2;b->data[18]=format==1?0:0x20;
            sum+=mode==0 && format==2?2324:lengths[mode];
        }
        cdb.cr2=0;cdb.cr4=0xffff;assert(cal_actual_size()==HIRQ_ESEL);size_reply(sum/2);
        assert(get_sector_data()==HIRQ_DRDY);
        unsigned services=0;
        while(!cdb.trans_finish){assert(++services<=n);trans_handle();}
        assert(fifo_words==sum/2 && cdb.cdwnum==(int)sum && !irq_depth);
        unsigned pos=0;
        for(unsigned i=0;i<n;i++) {
            unsigned bytes=mode==0 && (i+n)%3==2?2324:lengths[mode];
            unsigned offset=mode==0?24:2352-lengths[mode];
            for(unsigned b=0;b<bytes;b+=2) {
                u16 value=cdb.block[i].data[offset+b] | ((u16)cdb.block[i].data[offset+b+1]<<8);
                assert(fifo_output[pos++]==value);
            }
        }
        assert(pos==fifo_words && end_trans()==HIRQ_EHST && !cdb.trans_type);
        assert((((unsigned)(reply[0]&255)<<16)|reply[1])==sum/2);check_owners();
    }
}
static void incomplete_calculation_and_reset(void)
{
    seed(2,0);cdb.sector_size=2048;cdb.cr2=0;cdb.cr4=2;
    cdb.block[0].next=NULL;assert(cal_actual_size()==HIRQ_ESEL);size_reply(0xffffff);
    cdb.block[0].next=&cdb.block[1];check_owners();
    cdb.cr1=1;init_cdblock();assert(!cdb.actsize);size_reply(0);check_owners();
    for(unsigned noreset=0;noreset<2;noreset++) {
        seed(0,0);cdb.actsize=4096;cdb.cr1=noreset?0x80:0;init_cdblock();size_reply(2048);check_owners();
    }
}

static void toc_transfer(int absent)
{
    u32 cached[102];
    if (cdb.TOC) memcpy(cached, cdb.TOC, sizeof cached);
    st_control = 0x0240;
    st_status = 0x1234;
    fifo_rcnt = 0x8888;
    fifo_words = 0;
    check_fifo_publication = 1;
    assert(get_toc_info() == HIRQ_DRDY);
    check_fifo_publication = 0;
    assert(reply[0] == ((cdb.status << 8) | 0x4000));
    assert(reply[1] == 204 && !reply[2] && !reply[3]);
    assert(cdb.trans_type == TRANS_TOC && cdb.trans_size == 408 && !cdb.trans_finish);
    assert(st_status == STIRQ_DAT && st_control == (0x0040 | STIRQ_DAT));
    assert(fifo_words == 204 && !cdb.cdwnum);
    for (unsigned i = 0; i < fifo_words; i++) {
        u16 expected = 0xffff;
        if (!absent) memcpy(&expected, (u8 *)cached + i * sizeof expected, sizeof expected);
        assert(fifo_output[i] == expected);
    }
    if (cdb.TOC) assert(!memcmp(cdb.TOC, cached, sizeof cached));
    trans_handle();
    trans_handle();
    assert(cdb.trans_finish && cdb.cdwnum == 408 && !(st_control & STIRQ_DAT));
    assert(end_trans() == HIRQ_EHST && !cdb.trans_type && reply[1] == 204);
}

static void toc_unavailable(void)
{
    u32 cached[102];
    for (unsigned i = 0; i < 102; i++) cached[i] = 0x41230000 | i * 37;
    for (int status = STAT_OPEN; status <= STAT_NODISC; status++) {
        seed(0, 0);
        cdb.status = status;
        cdb.TOC = cached;
        toc_transfer(1);
        cdb.TOC = NULL;
        toc_transfer(1);
        cdb.status = STAT_PAUSE;
        cdb.TOC = cached;
        toc_transfer(0);
    }
}

static void toc_mounted(void)
{
    u32 cached[102];
    for (unsigned i = 0; i < 102; i++) cached[i] = 0x89abcdef ^ (i * 0x00121111);
    for (int status = 0; status < 256; status++) {
        if (status == STAT_OPEN || status == STAT_NODISC) continue;
        seed(0, 0);
        cdb.status = status;
        cdb.TOC = cached;
        toc_transfer(0);
    }
}

static void toc_busy(void)
{
    const int statuses[] = {STAT_PAUSE, STAT_OPEN, STAT_NODISC};
    for (unsigned s = 0; s < sizeof statuses / sizeof *statuses; s++) {
        for (int type = TRANS_DATA; type <= TRANS_PUT; type++) {
            seed(1, 0);
            cdb.status = statuses[s];
            cdb.trans_type = type;
            cdb.trans_size = 2352;
            cdb.trans_block = cdb.part[0].head;
            st_control = STIRQ_DAT | 0x0200;
            fifo_rcnt = 17;
            st_status = 0x1234;
            CDBLOCK before = cdb;
            assert(!get_toc_info() && reply[0] >> 8 == (STAT_WAIT | cdb.status));
            assert(!fifo_words && st_control == (STIRQ_DAT | 0x0200));
            assert(fifo_rcnt == 17 && st_status == 0x1234 && !memcmp(&before, &cdb, sizeof cdb));
        }
    }
}

/* Adapted from paps-g ba472965. Compare the actual FIFO bytes with the
 * different header and user-data offsets in Sega ST-162 §5.4. */
static void host_put_readback(void)
{
    const unsigned lengths[]={2048,2336,2340,2352};
    for (unsigned put=0;put<4;put++)
    for (unsigned mode=0;mode<4;mode++)
    for (unsigned get=0;get<4;get++)
    for (unsigned moving=0;moving<2;moving++) {
        seed(0,0);
        memset(sector_bytes,0xa5,sizeof sector_bytes);
        u8 expected[3][2352];
        unsigned size=lengths[put],offset=put ? 2352-size : 24;
        fifo_input_count=fifo_input_pos=0;
        for (unsigned n=0;n<3;n++) {
            for (unsigned j=0;j<2352;j++) expected[n][j]=(u8)(j*7+n*29);
            expected[n][15]=mode;
            expected[n][18]&=~0x20; /* Mode 2 Form 1 control. */
            for (unsigned j=offset;j<offset+size;j+=2)
                fifo_input[fifo_input_count++]=expected[n][j] | (u16)expected[n][j+1]<<8;
            memset(expected[n],0,offset);
            memset(expected[n]+offset+size,0xa5,2352-offset-size);
        }
        cdb.put_sector_size=size;cdb.cr3=0;cdb.cr4=3;
        assert(put_sector_data()==HIRQ_DRDY);
        unsigned steps=0;
        while (cdb.trans_bk) { assert(++steps<=3);trans_handle(); }
        assert(end_trans()==HIRQ_EHST && fifo_input_pos==fifo_input_count);
        assert(reply[1]==3*size/2 && !cdb.trans_type && !fifo_words);
        fifo_input_count=fifo_input_pos=0;
        check_owners();

        command(moving,0,1,0,3);
        assert(execute(moving)==HIRQ_ECPY);
        cdb.sector_size=lengths[get];cdb.cr3=1<<8;cdb.cr2=0;cdb.cr4=3;
        assert(cal_actual_size()==HIRQ_ESEL && cdb.actsize==(int)(3*lengths[get]));
        assert(get_sector_data()==HIRQ_DRDY);
        steps=0;
        while (!cdb.trans_finish) { assert(++steps<=3);trans_handle(); }
        assert(fifo_words==3*lengths[get]/2);
        offset=get ? 2352-lengths[get] : (put>=2 && mode==1 ? 16 : 24);
        unsigned pos=0;
        for (unsigned n=0;n<3;n++)
            for (unsigned j=offset;j<offset+lengths[get];j+=2) {
                u16 want=expected[n][j] | (u16)expected[n][j+1]<<8;
                assert(fifo_output[pos++]==want);
            }
        assert(end_trans()==HIRQ_EHST && reply[1]==3*lengths[get]/2);
        check_owners();
    }
}

#ifndef CDC_BOUNDS_MAIN
#define CDC_BOUNDS_MAIN main
#endif
int CDC_BOUNDS_MAIN(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<15);
    if(test==0)invalid_selectors();else if(test==1)valid_filter_registers();
    else if(test==2)sector_positions();else if(test==3)empty_and_broken_info();
    else if(test==4)busy_precedence();else if(test==5)read_delete_controls();
    else if(test==6)valid_file_selectors();else if(test==7)calculated_size_control();
    else if(test==8)calculation_ranges();else if(test==9)calculation_matches_fifo();
    else if(test==10)incomplete_calculation_and_reset();
    else if(test==11)toc_unavailable();else if(test==12)toc_mounted();
    else if(test==13)toc_busy();else host_put_readback();
    puts("pass: actual command selector bounds, positions and valid controls");return 0;
}
