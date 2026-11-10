/* MCU-CDC-014/017/022: paps-g 47ec46a4/cd83b371/3772326a, actual-source fixtures. */
#define CDC_OWNERSHIP_HOOKS
#define main copy_fixture_main
#define disk_task_wakeup copy_stub_wakeup
#define wait_pause_ok copy_stub_wait
#define set_report copy_stub_report
#define init_cdblock copy_stub_init_cdblock
#define pause_drive copy_stub_pause_drive
#include "cdc_copy.c"
#undef main
#undef disk_task_wakeup
#undef wait_pause_ok
#undef set_report
#undef init_cdblock
#undef pause_drive
static u32 buf_fad_start, buf_fad_end;
static unsigned wakeups, waits, read_calls;
static int acknowledge_after;
static int repeat_on_ack;
static int pend_delay;
static void hw_delay(int delay) { (void)delay; }
static void disk_task_wakeup(void) { assert(!irq_depth);wakeups++; }
static int pause_active;
static unsigned pause_signals;
static const CDBLOCK *pause_state;
static u16 pause_control,pause_hirq,pause_clear;
static void set_pause_ok(void) { assert(!irq_depth);pause_signals++; }
#include "pause-worker.inc"
static void wait_pause_ok(void)
{
    waits++;assert(waits <= 501 && !irq_depth);
    if(pause_state) {
        assert(!memcmp(pause_state,&cdb,sizeof cdb));
        assert(st_control==pause_control && hirq==pause_hirq && hirq_clear==pause_clear);
    }
    if (acknowledge_after >= 0 && waits >= (unsigned)acknowledge_after) {
        if(repeat_on_ack){cdb.repcnt++;repeat_on_ack=0;}
        worker_pause_step();
    }
}
#include "drive.inc"
#include "file-start.inc"
#include "phase.inc"
#include "background.inc"

int index_table[2048];
#include "track_fixture.h"
static TRACK_INFO *media;
static int indices[3][2] = {{200,250},{400,450},{600,650}};
static void ready(void)
{
    free(media); media=calloc(3,sizeof *media); assert(media);
    setup(2,198); cdb.tracks=media; cdb.track_num=3;
    for(unsigned i=0;i<3;i++) {
        media[i]=(TRACK_INFO){.fad_0=i?150+i*200-50:150,.fad_start=150+i*200,
            .fad_end=299+i*200,.max_index=3,.mode=i?3:1};
        test_track_indices(&media[i],indices[i],i*2);
    }
    cdb.play_fad_start=150; cdb.play_fad_end=700;
    cdb.fad=180; cdb.track=1; cdb.index=1; cdb.play_track=&media[0];
    cdb.options=8; cdb.ctrladdr=0x41;cdb.max_repeat=0; cdb.repcnt=0;
    buf_fad_start=123; buf_fad_end=456;
    wakeups=waits=read_calls=0; acknowledge_after=1;
    pause_active=0;pause_signals=0;repeat_on_ack=0;
    pause_state=NULL;
    hirq=hirq_clear=0;
}
static void request(u32 start,u32 end,unsigned mode)
{
    cdb.cr1=start>>16; cdb.cr2=start; cdb.cr3=(mode<<8)|(end>>16); cdb.cr4=end;
}
static void helpers(void)
{
    ready();
    for(int t=-256;t<65536;t++) {
        int n=t==65535?3:(t>>8);
        TRACK_INFO *p=get_track_info(t);
        assert(p==(n>=1 && n<=3?&media[n-1]:NULL));
    }
    assert(track_to_fad(0)==0 && track_to_fad(0xffff)==699);
    for(unsigned t=0;t<256;t++)for(unsigned i=0;i<256;i++) {
        u32 result=track_to_fad((t<<8)|i);
        if(!t && !i)assert(!result);
        else if(t==255 && i==255)assert(result==699);
        else if(t<1 || t>3)assert(result==0xffffffffu);
        else assert(result==(i<=1?media[t-1].fad_start:i>3?media[t-1].fad_end:(u32)indices[t-1][i-2]));
    }
    test_track_indices(&media[0],NULL,0);assert(track_to_fad(0x102)==0xffffffffu);
    cdb.track_num=0;assert(!get_track_info(0xffff) && track_to_fad(0xffff)==0xffffffffu);
    cdb.track_num=100;assert(!get_track_info(0x100));
    cdb.track_num=3;cdb.tracks=NULL;assert(!get_track_info(0x100));
}
/* Expected positions come from the three authored track intervals, independently
 * of the production lookup helper. Every possible track and index byte is used. */
static u32 expected_track(unsigned track,unsigned index,int ending)
{
    if(!track && !index)return ending?700:150;
    if(!track)track=ending?3:1;
    else if(track>3){track=3;index=0;}
    u32 beginning=150+200*(track-1);
    if(ending){
        if(!index || index>=3)return beginning+150;
        return beginning+(index==1?50:100);
    }
    if(index<=1)return beginning;
    if(index<=3)return beginning+(index==2?50:100);
    return track<3?beginning+200:700;
}
static void track_bytes(void)
{
    ready();
    for(unsigned t=0;t<256;t++)for(unsigned i=0;i<256;i++) {
        unsigned pos=t<<8|i;
        cdb.play_type=0;request(pos,0,0);assert(!play_cd());
        assert(cdb.play_fad_start==expected_track(t,i,0) && cdb.play_fad_end==700);
        cdb.play_type=0;request(0,pos,0);assert(!play_cd());
        assert(cdb.play_fad_start==150 && cdb.play_fad_end==expected_track(t,i,1));
        if(pos){cdb.play_type=0;request(pos,0,0);assert(!seek_cd());assert(cdb.fad==expected_track(t,i,0));}
        assert(cdb.play_track==NULL || (cdb.play_track>=media && cdb.play_track<media+3));
    }
}
static void fad_parameters(void)
{
    ready();
    const u32 starts[]={0,1,149,150,151,299,300,699,700,701,0x100096,0x7ffffe};
    const u32 counts[]={0,1,2,149,150,549,550,551,0x100001,0x7ffffe};
    for(unsigned s=0;s<12;s++)for(unsigned n=0;n<10;n++) {
        u32 start=starts[s]<150?150:starts[s]>700?700:starts[s];
        uint64_t end=(uint64_t)start+counts[n];if(end>700)end=700;
        cdb.play_type=0;request(0x800000|starts[s],0x800000|counts[n],0);
        assert(!play_cd() && cdb.fad==start && cdb.play_fad_start==start && cdb.play_fad_end==end);
        cdb.play_type=0;request(0x800000|starts[s],0,0);assert(!seek_cd() && cdb.fad==start);
    }
    /* Retain all 23 FAD bits even when a synthetic large extent covers them. */
    media[2].fad_end=0x200000;cdb.play_type=0;
    request(0x900096,0x900001,0);assert(!play_cd());
    assert(cdb.fad==0x100096 && cdb.play_fad_end==0x200001);
}
static void mixed_forms(void)
{
    for(unsigned dir=0;dir<2;dir++) {
        ready();cdb.play_type=PLAYTYPE_DIR;cdb.rdir_state=2;cdb.cdir_pfnum=7;
        request(dir?0x101:0x800096,dir?0x800001:0x201,0);
        CDBLOCK before=cdb;
        assert(!play_cd() && reply[0]==STAT_REJECT<<8);
        assert(!memcmp(&before,&cdb,sizeof cdb) && !waits && !wakeups && !hirq_clear);
    }
}
static void failed_pause(void)
{
    for(unsigned seek=0;seek<2;seek++)for(unsigned type=1;type<=3;type++) {
        ready();cdb.play_type=type;cdb.play_wait=1;cdb.rdir_state=2;cdb.cdir_pfnum=7;
        request(0x201,0x300,0);acknowledge_after=-1;
        assert(!(seek?seek_cd():play_cd()));
        assert(waits==500 && wakeups==1 && (reply[0]>>8)==(STAT_WAIT|STAT_PAUSE));
        assert(cdb.play_type==(int)type && cdb.pause_request && !cdb.play_wait);
        assert(cdb.fad==180 && cdb.play_fad_start==150 && cdb.play_fad_end==700);
        assert(cdb.rdir_state==2 && cdb.cdir_pfnum==7 && cdb.part[0].numblocks==2 && !hirq_clear);
        waits=wakeups=0;acknowledge_after=1;
        assert(!(seek?seek_cd():play_cd()));
        assert(waits==1 && wakeups==(seek?1u:2u) && !cdb.pause_request && cdb.fad==350);
        assert(type!=PLAYTYPE_DIR || (!cdb.rdir_state && !cdb.cdir_pfnum));
    }
}
static void worker_boundaries(void)
{
    for(unsigned repeats=0;repeats<16;repeats++)for(unsigned kind=0;kind<5;kind++) {
        ready();request(0x8000fa,0x800032,repeats);assert(!play_cd());
        cdb.fad=(u32[]){249,250,299,300,700}[kind];
        u32 original=cdb.fad;int result=worker_step();
        if(kind==1 || kind==2)assert(result==1 && read_calls==1 && cdb.fad==original);
        else if(repeats)assert(result==2 && !read_calls && cdb.fad==250 && cdb.repcnt==1);
        else assert(result==0 && !read_calls && !cdb.play_type && hirq==HIRQ_PEND && cdb.fad==original);
    }
    for(unsigned repeats=0;repeats<16;repeats++)for(unsigned kind=0;kind<3;kind++) {
        ready();request(0x8002bc,0x800000,repeats);assert(!play_cd());
        if(kind==1)cdb.play_fad_end=600;if(kind==2)cdb.fad=150;
        assert(worker_step()==0 && !read_calls && !cdb.play_type && hirq==HIRQ_PEND);
    }
    ready();request(0x100,0x100,0);assert(!play_cd());
    assert(cdb.play_fad_start==150 && cdb.play_fad_end==300);
    unsigned delivered=0;
    while(worker_step()==1){delivered++;cdb.fad++;assert(delivered<=150);}
    assert(delivered==150 && cdb.fad==300);
}
static void seek_resume(void)
{
    ready();cdb.play_type=PLAYTYPE_SECTOR;request(0xffffff,0,0);assert(!seek_cd());
    assert(cdb.fad==180 && cdb.play_fad_start==150 && cdb.play_fad_end==700 && !cdb.play_type);
    request(0,0,0);assert(!seek_cd() && cdb.status==STAT_STANDBY && cdb.fad==0xffffffffu && !cdb.play_track);
    ready();request(0x202,0,0);assert(!seek_cd());assert(cdb.fad==400 && cdb.index==2 && cdb.ctrladdr==1);
    request(0xffffff,0xffffff,255);assert(!play_cd());assert(cdb.fad==400 && cdb.play_fad_end==700);
    for(unsigned audio=0;audio<2;audio++) {
        ready();request(0x800000|(audio?400:200),0,0);assert(!seek_cd());
        assert(cdb.play_fad_end==(audio?500u:200u));
        request(0xffffff,0xffffff,255);assert(!play_cd());
        assert(cdb.fad==(audio?400u:200u) && cdb.play_fad_end==(audio?500u:200u));
    }
    ready();cdb.fad=250;request(0x101,0x101,128);assert(!play_cd());assert(cdb.fad==150);
    ready();cdb.fad=450;request(0x201,0x201,128);assert(!play_cd());assert(cdb.fad==450);
}
static void no_media(void)
{
    for(unsigned bad=0;bad<3;bad++)for(unsigned seek=0;seek<2;seek++) {
        ready();if(bad==0)cdb.track_num=0;if(bad==1)cdb.track_num=100;if(bad==2)cdb.tracks=NULL;
        cdb.play_type=PLAYTYPE_FILE;request(0x800096,0,0);CDBLOCK before=cdb;
        assert(!(seek?seek_cd():play_cd()) && reply[0]==STAT_REJECT<<8);
        assert(!memcmp(&before,&cdb,sizeof cdb) && !waits && !wakeups);
    }
}
static void background_audio(void)
{
    for(unsigned repeat=0;repeat<2;repeat++) {
        ready();cdb.track_num=1;media[0].mode=3;background_start(repeat);
        assert(cdb.play_fad_start==150 && cdb.play_fad_end==300 && cdb.fad==150);
        unsigned delivered=0;
        while(worker_step()==1){delivered++;cdb.fad++;assert(delivered<=150);}
        assert(delivered==150 && (repeat?cdb.fad==150:cdb.fad==300));
    }
}
static void cached_reader(void)
{
    /* Execute both actual worker acknowledgement branches. Only notifications
     * and thread preemption are modeled; no fake cache acknowledgement. */
    for(unsigned active=0;active<2;active++)
    for (unsigned seek=0;seek<2;seek++) for (unsigned type=0;type<=3;type++) {
        ready();pause_active=active;cdb.play_type=type;request(0x201,0x300,0);
        assert(!(seek?seek_cd():play_cd()));
        assert(buf_fad_start==123 && buf_fad_end==456);
        assert(waits==(type?1u:0u));
    }
    for (unsigned seek=0;seek<2;seek++) {
        ready();cdb.play_type=PLAYTYPE_SECTOR;acknowledge_after=-1;
        request(0x201,0x300,0);assert(!(seek?seek_cd():play_cd()));
        assert(waits==500 && buf_fad_start==123 && buf_fad_end==456);
        assert(cdb.fad==180 && cdb.play_fad_start==150 && cdb.play_fad_end==700);
    }
    for (unsigned type=0;type<=3;type++) {
        ready();cdb.play_type=type;assert(!pause_disc());
        assert(!buf_fad_start && !buf_fad_end && waits==(type?1u:0u));
        if (type) {
            ready();cdb.play_type=type;acknowledge_after=-1;
            assert(pause_disc()==-1 && waits==500);
            assert(buf_fad_start==123 && buf_fad_end==456);
        }
    }
}
static void valid_log_control(void)
{
    ready();request(0x800096,0x800014,0);fixture_log_mask=_CDRV;
    assert(!play_cd() && cdb.fad==150 && cdb.play_fad_start==150 && cdb.play_fad_end==170);
    assert(cdb.play_type==PLAYTYPE_SECTOR && wakeups==1 && !waits && cdb.play_track==media);
    assert(reply[0]==((STAT_BUSY<<8)|0x80) && reply[1]==0x4101 && reply[2]==0x100 && reply[3]==150);
    assert(!strcmp(fixture_log_text,"play_cd: start=00800096 end=00800014 mode=00\n"));
    cdb.play_type=0;request(0xffffff,0,0);fixture_log_used=0;fixture_log_text[0]=0;
    assert(!seek_cd() && cdb.fad==150 && cdb.play_fad_start==150 && cdb.play_fad_end==170);
    assert(!strcmp(fixture_log_text,"\nseek_cd: pos=00ffffff\n       : PAUSE cmd\n       : fad=00000096 track=1 index=1 ctladr=41 opt=08\n"));
    assert(reply[0]==((STAT_PAUSE<<8)|0x80) && reply[3]==150);check_owners();
}
static void full_track_capacity(void)
{
    ready();free(media);media=calloc(99,sizeof *media);assert(media);cdb.tracks=media;cdb.track_num=99;
    for(unsigned i=0;i<99;i++)media[i]=(TRACK_INFO){.fad_0=150+i*10,.fad_start=150+i*10,
        .fad_end=159+i*10,.max_index=1,.mode=1};
    for(unsigned i=0;i<99;i++) {
        assert(get_track_info((i+1)<<8)==media+i && track_to_fad((i+1)<<8)==150+i*10);
        cdb.play_type=0;request((i+1)<<8,0,0);assert(!play_cd());
        assert(cdb.fad==150+i*10 && cdb.play_fad_end==1140 && cdb.track==i+1);
    }
    cdb.track_num=100;assert(!get_track_info(0x100));
}
static void repeat_preservation(void)
{
    /* Equivalent FAD, track-default and NOCHG requests do not start a new
     * repeat budget, even with an explicitly specified (unchanged) limit. */
    for (unsigned count=0;count<=14;count++) for (unsigned limit=0;limit<=15;limit++)
    for (unsigned form=0;form<3;form++) for (unsigned mode=0;mode<4;mode++) {
        ready();cdb.repcnt=count;cdb.max_repeat=limit;cdb.play_type=PLAYTYPE_SECTOR;
        request((u32[]){0xffffff,0x800096,0}[form],
                (u32[]){0xffffff,0x800226,0}[form],
                (unsigned[]){0xff,0x7f,limit,limit|0x80}[mode]);
        assert(!play_cd() && cdb.repcnt==count && cdb.max_repeat==(int)limit);
        assert((reply[0]&15)==count && cdb.play_fad_start==150 && cdb.play_fad_end==700);
        cdb.fad=700;hirq=0;
        if (count>=limit) assert(worker_step()==0 && hirq==HIRQ_PEND && !cdb.play_type);
        else assert(worker_step()==2 && !hirq && cdb.repcnt==(count<14?count+1:14));
    }
    ready();cdb.repcnt=5;cdb.max_repeat=9;cdb.play_type=PLAYTYPE_SECTOR;repeat_on_ack=1;
    request(0xffffff,0xffffff,0xff);assert(!play_cd() && cdb.repcnt==6);
    /* Real completion blocks and pause/resume commands must exhaust a finite
     * budget; repeated resumes previously replenished it indefinitely. */
    ready();request(0x800096,0x800001,2);assert(!play_cd());
    for (unsigned expected=1;expected<=2;expected++) {
        cdb.fad=151;assert(worker_step()==2 && cdb.repcnt==expected);
        request(0xffffff,0,0);assert(!seek_cd());
        request(0xffffff,0xffffff,0xff);assert(!play_cd() && cdb.repcnt==expected);
    }
    cdb.fad=151;assert(worker_step()==0 && !cdb.play_type && hirq==HIRQ_PEND);
}
static void repeat_changes(void)
{
    for (unsigned which=0;which<4;which++) for (unsigned failing=0;failing<2;failing++) {
        ready();cdb.repcnt=5;cdb.max_repeat=9;cdb.play_type=PLAYTYPE_SECTOR;
        request((u32[]){0x800097,0xffffff,0xffffff,0xffffff}[which],
                (u32[]){0xffffff,0x800001,0xffffff,0xffffff}[which],
                (unsigned[]){0xff,0xff,0x88,0x80}[which]);
        repeat_on_ack=1;if(failing)acknowledge_after=-1;
        assert(!play_cd());
        if (failing) {
            assert(waits==500 && cdb.repcnt==5 && cdb.max_repeat==9);
            assert(cdb.play_fad_start==150 && cdb.play_fad_end==700);
            waits=0;acknowledge_after=1;assert(!play_cd());
        }
        assert(!cdb.repcnt && cdb.max_repeat==(which<2?9:which==2?8:0));
        assert(cdb.play_fad_start==(which==0?151u:150u));
        assert(cdb.play_fad_end==(which==1?151u:700u));
    }
}
static void repeat_seek_stop(void)
{
    for (unsigned count=0;count<=14;count++) for (unsigned kind=0;kind<4;kind++) {
        ready();cdb.repcnt=count;cdb.max_repeat=15;cdb.play_type=PLAYTYPE_SECTOR;
        request((u32[]){0xffffff,0,0x201,0x800190}[kind],0,0);assert(!seek_cd());
        assert(cdb.repcnt==count && cdb.max_repeat==15);
        if (kind==1) {
            assert(reply[0]==((STAT_STANDBY<<8)|255));
            assert(reply[1]==65535 && reply[2]==65535 && reply[3]==65535);
            assert(cdb.play_fad_start==150 && cdb.play_fad_end==700);
        } else assert((reply[0]&15)==count);
    }
    for (unsigned kind=0;kind<4;kind++) {
        ready();cdb.repcnt=7;cdb.max_repeat=11;cdb.play_type=PLAYTYPE_SECTOR;
        acknowledge_after=-1;request((u32[]){0xffffff,0,0x201,0x800190}[kind],0,0);
        assert(!seek_cd() && waits==500 && cdb.repcnt==7 && cdb.max_repeat==11);
        assert(cdb.fad==180 && cdb.play_fad_start==150 && cdb.play_fad_end==700);
    }
}
static void home_resume(void)
{
    for (unsigned pause_first=0;pause_first<2;pause_first++) {
        ready();cdb.repcnt=2;cdb.max_repeat=4;cdb.play_fad_start=250;cdb.play_fad_end=300;
        request(0,0,0);assert(!seek_cd());
        if (pause_first) {
            request(0xffffff,0,0);assert(!seek_cd() && cdb.status==STAT_PAUSE);
            assert(cdb.fad==150 && cdb.track==1 && cdb.index==1 && cdb.ctrladdr==0x41);
            assert((reply[0]&255)==2 && reply[1]==0x4101 && reply[2]==0x100 && reply[3]==150);
        }
        request(0xffffff,0xffffff,0xff);assert(!play_cd());
        assert(cdb.fad==150 && cdb.play_fad_start==250 && cdb.play_fad_end==300);
        assert(cdb.repcnt==2 && cdb.max_repeat==4 && cdb.play_track==media);
        assert(worker_step()==2 && cdb.fad==250 && cdb.repcnt==3);
    }
    ready();request(0,0,0);assert(!seek_cd());
    cdb.track_num=0;request(0xffffff,0,0);CDBLOCK before=cdb;
    assert(!seek_cd() && (reply[0]>>8)==STAT_REJECT && !memcmp(&before,&cdb,sizeof cdb));
}
static void retained_forms(void)
{
    const u32 starts[]={0,0x101,0x800096,0xffffff};
    const u32 ends[]={0,0x201,0x800032,0xffffff};
    for(unsigned old_start=0;old_start<3;old_start++)
    for(unsigned old_end=0;old_end<3;old_end++) {
        if((old_start|old_end)==3)continue;
        for(unsigned new_start=0;new_start<4;new_start++)
        for(unsigned new_end=0;new_end<4;new_end++) {
            ready();request(starts[old_start],ends[old_end],0);assert(!play_cd());
            unsigned expected_start=new_start==3?old_start:new_start;
            unsigned expected_end=new_end==3?old_end:new_end;
            request(starts[new_start],ends[new_end],0xff);
            CDBLOCK before=cdb;waits=wakeups=0;hirq_clear=0;
            assert(!play_cd());
            if((expected_start|expected_end)==3) {
                assert((reply[0]>>8)==STAT_REJECT && !waits && !wakeups && !hirq_clear);
                assert(!memcmp(&before,&cdb,sizeof cdb));
            } else assert((reply[0]>>8)!=STAT_REJECT && waits==1 && wakeups==2);
        }
    }
}

static void retained_form_failure(void)
{
    for(unsigned end=0;end<2;end++) {
        ready();request(0x101,0x201,0);assert(!play_cd());
        request(0x800096,0x800032,0);acknowledge_after=-1;
        assert(!play_cd() && (reply[0]>>8)==(STAT_WAIT|cdb.status) && waits==500);
        request(end?0xffffff:0x800096,end?0x800032:0xffffff,0);
        CDBLOCK before=cdb;waits=wakeups=0;hirq_clear=0;
        assert(!play_cd() && (reply[0]>>8)==STAT_REJECT);
        assert(!waits && !wakeups && !hirq_clear && !memcmp(&before,&cdb,sizeof cdb));
    }
    /* Replacing a track endpoint by default removes only that constraint. */
    ready();request(0x101,0,0);assert(!play_cd());
    request(0,0xffffff,0);assert(!play_cd());
    request(0xffffff,0x800032,0);assert(!play_cd() && (reply[0]>>8)!=STAT_REJECT);
    ready();request(0,0x201,0);assert(!play_cd());
    request(0xffffff,0,0);assert(!play_cd());
    request(0x800096,0xffffff,0);assert(!play_cd() && (reply[0]>>8)!=STAT_REJECT);
}

static void replacement_types(void)
{
    ready();cdb.play_pos_types=5;
    start_file_play(0,150,1,PLAYTYPE_FILE);assert(!cdb.play_pos_types);
    request(0xffffff,0x800001,0);assert(!play_cd() && (reply[0]>>8)!=STAT_REJECT);
    assert(cdb.play_pos_types==8);check_owners();
    ready();cdb.play_pos_types=5;background_start(0);assert(!cdb.play_pos_types);
    request(0xffffff,0x800001,0);assert(!play_cd() && (reply[0]>>8)!=STAT_REJECT);
    assert(cdb.play_pos_types==8);check_owners();
}

/* The complete command state, hardware-register model and block ownership must
 * stay unchanged while the producer still owns its request. */
static void init_request(unsigned flags,unsigned type,unsigned branch)
{
    ready();cdb.play_type=type;cdb.play_wait=1;cdb.status=STAT_PLAY;
    cdb.iflag=0x14;cdb.cr1=0x400|flags;cdb.cr2=900;cdb.cr4=0x540;
    cdb.trans_type=TRANS_DATA;cdb.trans_part_index=0;
    cdb.trans_block=cdb.part[0].head;cdb.trans_bk=1;cdb.cdwnum=600;
    cdb.subrw_rp=3;cdb.subrw_wp=7;cdb.rdir_state=2;cdb.cdir_pfnum=7;
    cdb.play_pos_types=5;cdb.max_repeat=9;cdb.repcnt=4;
    st_control=0x221;hirq=0x55;hirq_clear=0x66;copy_move_error=1;
    reply[0]=0x5aa5;reply[1]=0xb2c3;reply[2]=0xd4e5;reply[3]=0xf678;
    pause_active=branch;
}
static void expect_pause_state(CDBLOCK *before)
{
    memcpy(before,&cdb,sizeof cdb);before->pause_request=1;before->play_wait=0;
    pause_state=before;pause_control=st_control;pause_hirq=hirq;pause_clear=hirq_clear;
}
static void init_handoff(void)
{
    const unsigned flags[]={0,2,1,0x7f};
    for(unsigned type=1;type<=3;type++)for(unsigned branch=0;branch<2;branch++)
    for(unsigned f=0;f<4;f++) {
        init_request(flags[f],type,branch);CDBLOCK before;expect_pause_state(&before);
        acknowledge_after=-1;
        assert(!init_cdblock());
        assert(waits==500 && wakeups==1 && reply[0]==(((STAT_WAIT|STAT_PLAY)<<8)|0xa5));
        assert(reply[1]==0xb2c3 && reply[2]==0xd4e5 && reply[3]==0xf678);
        assert(!memcmp(&before,&cdb,sizeof cdb) && copy_move_error==1);
        assert(buf_fad_start==123 && buf_fad_end==456 && !pause_signals);
        check_owners();
        waits=wakeups=0;acknowledge_after=3;
        int reset=flags[f]&1;
        assert(init_cdblock()==(reset?(HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST):0));
        pause_state=NULL;
        assert(waits==3 && wakeups==1 && pause_signals==1);
        assert(!cdb.play_type && !cdb.pause_request && cdb.status==STAT_PAUSE);
        assert(cdb.fad==180 && cdb.play_fad_start==150 && cdb.play_fad_end==700);
        assert(cdb.play_pos_types==5 && cdb.max_repeat==9 && cdb.repcnt==4);
        assert(buf_fad_start==123 && buf_fad_end==456);
        if(reset) {
            assert(cdb.block_free==200 && !cdb.trans_type && !copy_move_error);
            assert(hirq_clear==(HIRQ_PEND|HIRQ_DRDY|HIRQ_BFUL));
        } else {
            assert(cdb.iflag==flags[f] && cdb.block_free==198 && cdb.trans_type==TRANS_DATA);
            assert(st_control==0x221 && hirq==0x55 && hirq_clear==0x66 && copy_move_error==1);
        }
        if(type==PLAYTYPE_DIR || reset)assert(!cdb.rdir_state && !cdb.cdir_pfnum);
        check_owners();
    }
}
static void init_late_ack(void)
{
    for(unsigned branch=0;branch<2;branch++)for(unsigned late=0;late<2;late++) {
        init_request(2,PLAYTYPE_DIR,branch);CDBLOCK before;expect_pause_state(&before);
        acknowledge_after=late?-1:500;
        assert(!init_cdblock() && waits==500 && wakeups==1);
        if(late) {
            assert(cdb.pause_request && (reply[0]>>8)==(STAT_WAIT|STAT_PLAY));
            worker_pause_step();assert(!cdb.pause_request && !cdb.play_type);
            pause_state=NULL;waits=wakeups=0;
            assert(!init_cdblock() && !waits && !wakeups);
        }
        pause_state=NULL;
        assert(!cdb.play_type && !cdb.pause_request && pause_signals==1 && cdb.iflag==2);
        assert(!cdb.rdir_state && !cdb.cdir_pfnum && cdb.block_free==198);
        assert(buf_fad_start==123 && buf_fad_end==456);check_owners();
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);
    switch(atoi(argv[1])) {
    case 0:helpers();break;case 1:track_bytes();break;case 2:fad_parameters();break;
    case 3:mixed_forms();break;case 4:failed_pause();break;case 5:worker_boundaries();break;
    case 6:seek_resume();break;case 7:no_media();break;default:abort();
    case 8:background_audio();break;
    case 9:cached_reader();break;
    case 10:valid_log_control();break;case 11:full_track_capacity();break;
    case 12:repeat_preservation();break;case 13:repeat_changes();break;
    case 14:repeat_seek_stop();break;case 15:home_resume();break;
    case 16:retained_forms();break;case 17:retained_form_failure();break;case 18:replacement_types();break;
    case 19:init_handoff();break;case 20:init_late_ack();break;
    }
    free(media);puts("pass: actual drive commands, helpers and worker fragments");return 0;
}
