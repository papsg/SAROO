/* MCU-CDC-008: adapted from paps-g 89e644f6; independent command/list oracles. */
#define main copy_fixture_main
#include "cdc_copy.c"
#undef main

static int old_numblocks,min_num;
static u16 fifo_status,fifo_rcnt,st_status;
static u16 fifo_output[MAX_BLOCKS*1176];
static unsigned fifo_words,wake_count,track_count;
static u16 *fifo_slot(void)
{
    assert(!irq_depth && fifo_words<sizeof fifo_output/sizeof *fifo_output);
    return &fifo_output[fifo_words++];
}
#undef FIFO_DATA
#undef FIFO_STAT
#undef FIFO_RCNT
#undef ST_STAT
#undef printk
#define FIFO_DATA (*fifo_slot())
#define FIFO_STAT fifo_status
#define FIFO_RCNT fifo_rcnt
#define ST_STAT st_status
#define printk(...) ((void)0)
static int fad_to_track(int fad) { assert(fad>=150);track_count++;return 1; }
static void cdc_delay(int ticks) { (void)ticks;assert(0); }
static void bounds_wakeup(void) { wake_count++; }
#define disk_task_wakeup bounds_wakeup
#include "bounds.inc"
#undef disk_task_wakeup

static void seed(unsigned count,unsigned selector)
{
    setup(count,200-count);
    if(selector){cdb.part[selector]=cdb.part[0];cdb.part[0]=(PARTITION){.size=-1};}
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
            assert(get_sector_data()==(HIRQ_DRDY|HIRQ_EHST));
            trans_handle();assert(cdb.trans_finish && fifo_words==1176);
            assert(end_trans()==HIRQ_EHST);check_owners();
        }
    }
}
static void valid_file_selectors(void)
{
    for(unsigned selector=0;selector<24;selector++) {
        seed(0,0);cdb.cr3=(selector<<8)|0xff;cdb.cr4=0xffff;
        cdb.root_lba=20;cdb.root_size=3;
        assert(!change_dir() && cdb.dir_filter==(int)selector && cdb.cddev_filter==(int)selector);
        assert(cdb.filter[selector].mode==0x40 && cdb.filter[selector].fad==170 && cdb.filter[selector].range==3);
        assert(wake_count==1 && track_count==1);
        seed(0,0);cdb.cr3=selector<<8;cdb.cr4=7;cdb.cdir_lba=20;cdb.cdir_size=3;
        assert(!read_dir() && cdb.cdir_pfid==7 && cdb.cddev_filter==(int)selector);
        assert(cdb.filter[selector].fad==170 && wake_count==1 && track_count==1);
        seed(0,0);cdb.cr3=selector<<8;cdb.cr4=2;cdb.cdir_pfid=2;cdb.cdir_pfnum=1;
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
int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<8);
    if(test==0)invalid_selectors();else if(test==1)valid_filter_registers();
    else if(test==2)sector_positions();else if(test==3)empty_and_broken_info();
    else if(test==4)busy_precedence();else if(test==5)read_delete_controls();
    else if(test==6)valid_file_selectors();else calculated_size_control();
    puts("pass: actual command selector bounds, positions and valid controls");return 0;
}
