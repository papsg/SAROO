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
static void (*drive_wait_hook)(void), (*drive_ack_hook)(void);
static int hw_delay(int delay) { (void)delay; return 0; }
static void disk_task_wakeup(void) { assert(!irq_depth);wakeups++; }
static int pause_active;
static unsigned pause_signals;
static const CDBLOCK *pause_state;
static u16 pause_control,pause_hirq,pause_clear;
static void set_pause_ok(void)
{
    assert(!irq_depth);
    pause_signals++;
    if (drive_ack_hook) drive_ack_hook();
}
#include "pause-worker.inc"
static void wait_pause_ok(void)
{
    waits++;assert(waits <= 501 && !irq_depth);
    if (drive_wait_hook) { drive_wait_hook(); return; }
    if(pause_state) {
        assert(!memcmp(pause_state,&cdb,sizeof cdb));
        assert(st_control==pause_control && hirq==pause_hirq && hirq_clear==pause_clear);
    }
    if (acknowledge_after >= 0 && waits >= (unsigned)acknowledge_after) {
        if(repeat_on_ack){cdb.repcnt++;repeat_on_ack=0;}
        worker_pause_step();
    }
}
static unsigned subcode_starts;
static void trans_start(void) { subcode_starts++; }
#include "drive.inc"
static u16 ss_control;
static u16 *const drive_hirq = &hirq;
#undef HIRQ
#define HIRQ (*drive_hirq)
#undef SS_CTRL
#define SS_CTRL ss_control
#include "drive-dispatch.inc"
#include "file-start.inc"
#include "reset.inc"
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
    drive_wait_hook=drive_ack_hook=NULL;
    hirq=hirq_clear=0;
    ss_control=0x8000;
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
        if(pos){
            u32 target=expected_track(t,i,0);
            cdb.play_type=0;request(pos,0,0);
            assert(seek_cd()==(target>=expected_track(t,i,1)?HIRQ_PEND:0) && cdb.fad==target);
        }
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
        cdb.play_type=0;request(0x800000|starts[s],0,0);
        assert(seek_cd()==(start==end?HIRQ_PEND:0) && cdb.fad==start);
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
        assert(cdb.play_fad_start==150 && cdb.play_fad_end==700);
        request(0xffffff,0xffffff,255);assert(!play_cd());
        assert(cdb.fad==(audio?400u:200u) && cdb.play_fad_start==150 && cdb.play_fad_end==700);
    }
    ready();cdb.fad=250;request(0x101,0x101,128);assert(!play_cd());assert(cdb.fad==250);
    ready();cdb.fad=450;request(0x201,0x201,128);assert(!play_cd());assert(cdb.fad==450);
}

static void seek_retained_forms(void)
{
    const u32 starts[] = {0, 0x201, 0x80015e};
    const u32 ends[] = {0, 0x301, 0x800064};
    const u32 positions[] = {0, 0xffffff, 0x101, 0x202, 0x303,
                            0x800096, 0x800190, 0x800258, 0xfffffe};
    for (unsigned start = 0; start < 3; start++) {
        for (unsigned end = 0; end < 3; end++) {
            if ((start | end) == 3) continue;
            for (unsigned i = 0; i < sizeof positions / sizeof *positions; i++) {
                ready();
                request(starts[start], ends[end], 14);
                assert(!play_cd());
                cdb.repcnt = 12;
                u32 first = cdb.play_fad_start, last = cdb.play_fad_end;
                u8 types = cdb.play_pos_types;
                request(positions[i], 0, 0);
                assert(!seek_cd());
                assert(cdb.play_fad_start == first && cdb.play_fad_end == last);
                assert(cdb.play_pos_types == types && cdb.repcnt == 12 && cdb.max_repeat == 14);
                assert(!cdb.play_type && !cdb.pause_request);
            }
        }
    }
}

static void seek_resume_ranges(void)
{
    const u32 ranges[][2] = {{175, 225}, {375, 425}, {375, 625}};
    const unsigned repeats[] = {0, 1, 15}, counts[] = {0, 1, 14};
    for (unsigned r = 0; r < sizeof ranges / sizeof *ranges; r++) {
        u32 first = ranges[r][0], last = ranges[r][1];
        const u32 positions[] = {first - 1, first, first + 1, last - 1, last, last + 1};
        for (unsigned p = 0; p < sizeof positions / sizeof *positions; p++) {
            for (unsigned m = 0; m < sizeof repeats / sizeof *repeats; m++) {
                for (unsigned c = 0; c < sizeof counts / sizeof *counts; c++) {
                    ready();
                    u32 target = positions[p];
                    request(0x800000 | first, 0x800000 | (last - first), repeats[m]);
                    assert(!play_cd());
                    cdb.repcnt = counts[c];
                    request(0x800000 | target, 0, 0);
                    int expected = (p == 0 || p >= 4) && counts[c] >= repeats[m] ? HIRQ_PEND : 0;
                    assert(seek_cd() == expected);
                    assert(cdb.fad == target && cdb.play_fad_start == first && cdb.play_fad_end == last);
                    request(0xffffff, 0xffffff, 0xff);
                    assert(!play_cd());
                    assert(cdb.fad == target && cdb.repcnt == counts[c] && cdb.max_repeat == (int)repeats[m]);
                    unsigned delivered = 0;
                    int state;
                    while ((state = worker_step()) == 1) {
                        assert(cdb.fad == target + delivered && cdb.fad >= first && cdb.fad < last);
                        delivered++;
                        cdb.fad++;
                    }
                    assert(delivered == (target >= first && target < last ? last - target : 0));
                    assert(read_calls == delivered && cdb.play_fad_start == first && cdb.play_fad_end == last);
                    if (counts[c] < repeats[m]) {
                        assert(state == 2 && cdb.fad == first && cdb.repcnt == (counts[c] < 14 ? counts[c] + 1 : 14));
                        assert(!hirq && cdb.play_type == PLAYTYPE_SECTOR);
                    } else {
                        assert(state == 0 && hirq == HIRQ_PEND && !cdb.play_type && cdb.repcnt == counts[c]);
                    }
                }
            }
        }
    }
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
    assert(reply[0]==((STAT_BUSY<<8)|0x80) && reply[3]==150);check_owners();
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
            assert(reply[0]==((STAT_BUSY<<8)|255));
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
            } else {
                assert((reply[0]>>8)==STAT_BUSY && waits==1 && wakeups==1);
                assert(cdb.play_type==PLAYTYPE_SECTOR && cdb.fad==before.fad && !cdb.pause_request);
            }
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
    start_file_play(0,150,1,PLAYTYPE_FILE);assert(cdb.play_pos_types==PLAY_RANGE_FILE);
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
    const unsigned flags[]={0,2,1,0x7f,0x80,0x83};
    for(unsigned type=1;type<=3;type++)for(unsigned branch=0;branch<2;branch++)
    for(unsigned f=0;f<6;f++) {
        init_request(flags[f],type,branch);CDBLOCK before;expect_pause_state(&before);
        acknowledge_after=-1;
        assert(!init_cdblock());
        assert(waits==500 && wakeups==1 && reply[0]==(((STAT_WAIT|STAT_PLAY)<<8)|0xa5));
        assert(reply[1]==0xb2c3 && reply[2]==0xd4e5 && reply[3]==0xf678);
        assert(!memcmp(&before,&cdb,sizeof cdb) && copy_move_error==1);
        assert(buf_fad_start==123 && buf_fad_end==456 && !pause_signals);
        check_owners();
        waits=wakeups=0;acknowledge_after=3;
        int reset=!(flags[f]&0x80) && (flags[f]&1);
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
            assert(cdb.iflag==(flags[f]&0x80?0x14:flags[f]) && cdb.block_free==198 && cdb.trans_type==TRANS_DATA);
            assert(st_control==0x221 && hirq==0x55 && hirq_clear==0x66 && copy_move_error==1);
        }
        if(type==PLAYTYPE_DIR || reset)assert(!cdb.rdir_state && !cdb.cdir_pfnum);
        check_owners();
    }
}
static void init_flags(void)
{
    for(unsigned flags=0;flags<256;flags++)for(unsigned active=0;active<2;active++)
    for(unsigned branch=0;branch<2;branch++) {
        init_request(flags,active?PLAYTYPE_SECTOR:0,branch);
        st_control|=STIRQ_DAT;
        if(!active)cdb.status=STAT_PAUSE;
        CDBLOCK before;expect_pause_state(&before);
        int reset=!(flags&0x80) && (flags&1);
        assert(init_cdblock()==(reset?(HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST):0));
        pause_state=NULL;
        assert(waits==active && wakeups==active && pause_signals==active);
        assert(!cdb.play_type && !cdb.pause_request && cdb.status==STAT_PAUSE);
        assert(cdb.iflag==(flags&0x80?0x14:reset?0:flags));
        assert(reply[0]==((STAT_PAUSE<<8)|0x84) && reply[1]==0x4101);
        assert(reply[2]==0x100 && reply[3]==180 && !irq_depth);
        assert(cdb.play_pos_types==5 && cdb.max_repeat==9 && cdb.repcnt==4);
        assert(cdb.fad==180 && cdb.play_fad_start==150 && cdb.play_fad_end==700);
        assert(buf_fad_start==123 && buf_fad_end==456);
        if(reset) {
            assert(cdb.block_free==200 && cdb.sector_size==2048 && cdb.put_sector_size==2048);
            assert(!cdb.trans_type && !cdb.trans_block && !cdb.trans_bk && !cdb.cdwnum && !cdb.actsize);
            assert(!cdb.subrw_rp && !cdb.subrw_wp && !copy_move_error);
            assert(!cdb.rdir_state && !cdb.cdir_pfnum);
            assert(st_control==0x21 && hirq==0x55 && hirq_clear==(HIRQ_PEND|HIRQ_DRDY|HIRQ_BFUL));
            for(unsigned i=0;i<MAX_SELECTORS;i++) {
                assert(cdb.part[i].size==-1 && !cdb.part[i].head && !cdb.part[i].tail);
                FILTER expected;memset(&expected,0,sizeof expected);
                expected.c_true=i;expected.c_false=0xff;
                assert(!memcmp(&expected,&cdb.filter[i],sizeof expected));
            }
        } else {
            /* Only the requested policy and acknowledged idle publication may
             * differ; reset-only state and register contents remain exact. */
            before.iflag=flags&0x80?0x14:flags;before.pause_request=0;
            before.play_type=0;before.status=STAT_PAUSE;
            if(!active)before.play_wait=1;
            assert(!memcmp(&before,&cdb,sizeof cdb));
            assert(cdb.block_free==198 && copy_move_error==1 && st_control==(0x221|STIRQ_DAT));
            assert(hirq==0x55 && hirq_clear==0x66);
        }
        check_owners();
    }
}
static void init_no_media(void)
{
    const unsigned states[]={STAT_OPEN,STAT_NODISC};
    for(unsigned state=0;state<2;state++)for(unsigned flags=0;flags<256;flags++) {
        init_request(flags,0,0);cdb.tracks=NULL;cdb.track_num=0;cdb.play_track=NULL;
        cdb.status=states[state];int reset=!(flags&0x80) && (flags&1);
        assert(init_cdblock()==(reset?(HIRQ_EFLS|HIRQ_ECPY|HIRQ_ESEL|HIRQ_EHST):0));
        assert(cdb.status==(int)states[state] && reply[0]==((states[state]<<8)|0x84));
        assert(reply[1]==0x4101 && reply[2]==0x100 && reply[3]==180);
        assert(!waits && !wakeups && !cdb.play_type && !cdb.pause_request);
        assert(cdb.iflag==(flags&0x80?0x14:reset?0:flags));
        assert(cdb.block_free==(reset?200:198));check_owners();
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
static void selector_handoff(void)
{
    const unsigned modes[]={0,4,0xfc};
    for(unsigned type=1;type<=3;type++)for(unsigned branch=0;branch<2;branch++)
    for(unsigned m=0;m<3;m++) {
        init_request(2,type,branch);cdb.cddev_filter=0;
        cdb.cr1=0x4800|modes[m];cdb.cr3=0;st_control=STIRQ_DAT;
        CDBLOCK before;expect_pause_state(&before);acknowledge_after=-1;
        assert(!reset_filter() && waits==500 && wakeups==1);
        assert(reply[0]==(((STAT_WAIT|STAT_PLAY)<<8)|0xa5));
        assert(reply[1]==0xb2c3 && reply[2]==0xd4e5 && reply[3]==0xf678);
        assert(!memcmp(&before,&cdb,sizeof cdb) && st_control==STIRQ_DAT);
        assert(hirq==0x55 && hirq_clear==0x66 && copy_move_error==1);
        assert(buf_fad_start==123 && buf_fad_end==456);check_owners();
        waits=wakeups=0;acknowledge_after=2;
        assert(reset_filter()==HIRQ_ESEL && waits==2 && wakeups==1);
        pause_state=NULL;
        assert(!cdb.play_type && !cdb.pause_request && pause_signals==1);
        assert(!cdb.part[0].numblocks && cdb.block_free==200);
        assert(cdb.trans_part_index==0xff && cdb.trans_finish && !(st_control&STIRQ_DAT));
        assert(hirq==HIRQ_EHST && hirq_clear==0x66 && copy_move_error==1);
        assert(cdb.iflag==0x14 && cdb.fad==180 && cdb.play_fad_start==150 && cdb.play_fad_end==700);
        assert(cdb.play_pos_types==5 && cdb.max_repeat==9 && cdb.repcnt==4);
        if(type==PLAYTYPE_DIR)assert(!cdb.rdir_state && !cdb.cdir_pfnum);
        assert(buf_fad_start==123 && buf_fad_end==456);check_owners();
    }
}
static void selector_scope(void)
{
    /* Explicit own/other/disconnected input and audio/data/idle cases retain
     * the public pause predicate, rather than inventing routing-wide policy. */
    for(unsigned mode=0;mode<256;mode++)for(unsigned kind=0;kind<5;kind++) {
        ready();cdb.cr1=0x4800|mode;cdb.cr3=0;
        cdb.cddev_filter=kind==2?1:kind==3?0xff:0;
        cdb.play_type=kind==4?0:PLAYTYPE_SECTOR;
        if(kind==1)media[0].mode=3;
        for(unsigned i=0;i<MAX_SELECTORS;i++) {
            FILTER *f=&cdb.filter[i];f->c_true=(i+2)%24;f->c_false=(i+1)%24;
            f->mode=0x4f;f->fad=150;f->range=100;f->fid=8;f->chan=3;
            f->smmask=9;f->smval=8;f->cimask=5;f->cival=4;
        }
        unsigned pause=((mode&4) || (!mode && kind==0)) && (kind==0 || kind==2);
        assert(reset_filter()==HIRQ_ESEL && waits==pause && wakeups==pause);
        assert(cdb.play_type==(pause || kind==4?0:PLAYTYPE_SECTOR));
        assert(cdb.cddev_filter==((mode&0x20) || kind==3?0xff:kind==2?1:0));
        unsigned cleared=(mode&4) || !mode;
        assert(cdb.part[0].numblocks==(cleared?0:2) && cdb.block_free==(cleared?200:198));
        for(unsigned i=0;i<MAX_SELECTORS;i++) {
            FILTER *f=&cdb.filter[i];int zero=!!(mode&0x10);
            assert(f->c_true==(mode&0x40?i:(i+2)%24));
            assert(f->c_false==(mode&(0x80|0x20)?0xff:(i+1)%24));
            assert(f->mode==(zero?0:0x4f) && f->fad==(zero?0:150) && f->range==(zero?0:100));
            assert(f->fid==(zero?0:8) && f->chan==(zero?0:3));
            assert(f->smmask==(zero?0:9) && f->smval==(zero?0:8));
            assert(f->cimask==(zero?0:5) && f->cival==(zero?0:4));
        }
        assert(buf_fad_start==123 && buf_fad_end==456 && !irq_depth);check_owners();
    }
}
static void selector_logs(void)
{
    ready();cdb.play_type=PLAYTYPE_SECTOR;cdb.cddev_filter=0;cdb.cr1=4;cdb.cr3=0;
    fixture_log_mask=_FILTER|_CDRV;
    assert(reset_filter()==HIRQ_ESEL && waits==1 && wakeups==1);
    assert(!strcmp(fixture_log_text,"reset_filter: mode=04 fid=0\nreset_filter: Send PAUSE request!\n"));
}

/* Subcode cases adapted from paps-g 555eb231. Only FIFO start is modeled. */
static u8 subcode_payload[10];

static void prepare_subcode(void)
{
    ready();
    memset(subcode_payload, 0xa5, sizeof subcode_payload);
    cdb.SUBH = subcode_payload;
    subcode_starts = 0;
}

static u32 read_be24(const u8 *bytes)
{
    return (u32)bytes[0] << 16 | (u32)bytes[1] << 8 | bytes[2];
}

static void check_subcode_q(u32 relative_fad)
{
    CDBLOCK before;
    memcpy(&before, &cdb, sizeof before);
    unsigned starts = subcode_starts;
    assert(get_subcode() == HIRQ_DRDY);
    assert(subcode_starts == starts + 1 && cdb.trans_type == TRANS_SUBQ);
    assert(reply[0] == (0x4000 | (cdb.status << 8)) && reply[1] == 5);
    assert(!reply[2] && !reply[3]);
    assert(subcode_payload[0] == cdb.ctrladdr && subcode_payload[1] == cdb.track);
    assert(subcode_payload[2] == cdb.index && !subcode_payload[6]);
    assert(read_be24(subcode_payload + 3) == (relative_fad & 0xffffff));
    assert(read_be24(subcode_payload + 7) == (cdb.fad & 0xffffff));
    before.trans_type = TRANS_SUBQ;
    assert(!memcmp(&before, &cdb, sizeof cdb));
    cdb.trans_type = 0;
}

static void subcode_transitions(void)
{
    prepare_subcode();
    request(0, 0, 0);
    assert(!seek_cd() && cdb.status == STAT_STANDBY);
    assert(cdb.track == 255 && cdb.fad == 0xffffffffu);
    cdb.cr1 = 0;
    check_subcode_q(0xffffff);
    for (unsigned i = 0; i < sizeof subcode_payload; i++)
        assert(subcode_payload[i] == (i == 6 ? 0 : 255));

    request(0xffffff, 0, 0);
    assert(!seek_cd());
    cdb.cr1 = 0;
    check_subcode_q(0);
    request(0x800180, 0, 0);
    assert(!seek_cd());
    cdb.cr1 = 0;
    check_subcode_q(0x180 - 350);

    cdb.tracks = NULL;
    cdb.track_num = 0;
    cdb.track = cdb.index = cdb.ctrladdr = 255;
    cdb.fad = 0xffffffffu;
    cdb.status = STAT_NODISC;
    check_subcode_q(0xffffff);
    cdb.status = STAT_OPEN;
    check_subcode_q(0xffffff);
}

static void subcode_track_bounds(void)
{
    prepare_subcode();
    for (int count = 1; count <= MAX_TRACKS; count++) {
        free(media);
        media = calloc(count, sizeof *media);
        assert(media);
        for (int i = 0; i < count; i++)
            media[i].fad_start = 150 + i * 100;
        cdb.tracks = media;
        cdb.track_num = count;
        cdb.fad = 0x800123;
        for (unsigned track = 0; track < 256; track++) {
            cdb.track = track;
            u32 expected = track && track <= (unsigned)count ?
                cdb.fad - (150 + (track - 1) * 100) : 0xffffff;
            check_subcode_q(expected);
        }
    }

    cdb.track = 1;
    cdb.track_num = MAX_TRACKS + 1;
    check_subcode_q(0xffffff);
    cdb.track_num = 0;
    check_subcode_q(0xffffff);
    cdb.track_num = -1;
    check_subcode_q(0xffffff);
    cdb.track_num = 1;
    cdb.tracks = NULL;
    check_subcode_q(0xffffff);
}

static void subcode_admission(void)
{
    prepare_subcode();
    for (unsigned type = 0; type < 256; type++) {
        for (unsigned state = 0; state < 4; state++) {
            int busy = state & 1;
            int empty = state & 2;
            cdb.cr1 = 0x2000 | type;
            cdb.trans_type = busy ? TRANS_DATA : 0;
            cdb.subrw_rp = 7;
            cdb.subrw_wp = empty ? 7 : 8;
            memset(subcode_payload, 0xa5, sizeof subcode_payload);
            subcode_starts = 0;
            CDBLOCK before;
            memcpy(&before, &cdb, sizeof before);
            int waiting = busy || (type == 1 && empty);
            int result = get_subcode();
            if (waiting || type > 1) {
                assert(!result && !subcode_starts && !memcmp(&before, &cdb, sizeof cdb));
                assert(reply[0] >> 8 == (waiting ? STAT_WAIT | cdb.status : STAT_REJECT));
                for (unsigned i = 0; i < sizeof subcode_payload; i++)
                    assert(subcode_payload[i] == 0xa5);
            } else {
                assert(result == HIRQ_DRDY && subcode_starts == 1);
                assert(cdb.trans_type == (type ? TRANS_SUBRW : TRANS_SUBQ));
                assert(reply[1] == (type ? 12 : 5));
            }
        }
    }
}

static void subcode_valid_payloads(void)
{
    prepare_subcode();
    const u32 frames[] = {0, 149, 150, 151, 0xffff, 0x10000, 0x7fffff, 0xffffff};
    for (unsigned track = 1; track <= 3; track++) {
        for (unsigned index = 0; index < 256; index++) {
            cdb.track = track;
            cdb.index = index;
            cdb.ctrladdr = index ^ 0x5a;
            for (unsigned i = 0; i < sizeof frames / sizeof *frames; i++) {
                cdb.fad = frames[i];
                check_subcode_q(cdb.fad - media[track - 1].fad_start);
            }
        }
    }

    cdb.cr1 = 0x2001;
    cdb.subrw_rp = 63;
    cdb.subrw_wp = 0;
    memset(subcode_payload, 0xa5, sizeof subcode_payload);
    subcode_starts = 0;
    assert(get_subcode() == HIRQ_DRDY && subcode_starts == 1);
    assert(cdb.trans_type == TRANS_SUBRW && reply[1] == 12 && !reply[2] && !reply[3]);
    for (unsigned i = 0; i < sizeof subcode_payload; i++)
        assert(subcode_payload[i] == 0xa5);
}

static void seek_completion_bounds(void)
{
    const u32 ranges[][2] = {{175, 225}, {375, 425}, {375, 625}, {400, 400}};
    for (unsigned r = 0; r < sizeof ranges / sizeof *ranges; r++) {
        u32 first = ranges[r][0], last = ranges[r][1];
        const u32 targets[] = {first - 1, first, first + 1, last - 1, last, last + 1, 700};
        for (unsigned p = 0; p < sizeof targets / sizeof *targets; p++) {
            for (unsigned limit = 0; limit < 16; limit++) {
                for (unsigned count = 0; count < 15; count++) {
                    ready();
                    request(0x800000 | first, 0x800000 | (last - first), limit);
                    assert(!play_cd());
                    cdb.repcnt = count;
                    hirq = HIRQ_EHST | HIRQ_EFLS;
                    hirq_clear = 0;
                    int outside = r == 3 || p == 0 || p >= 4;
                    request(0x800000 | targets[p], 0, 0);
                    assert(seek_cd() == (outside && count >= limit ? HIRQ_PEND : 0));
                    assert(!hirq_clear && hirq == (HIRQ_EHST | HIRQ_EFLS));
                    assert(cdb.fad == targets[p] && cdb.status == STAT_PAUSE && !cdb.play_type);
                    assert(cdb.play_fad_start == first && cdb.play_fad_end == last);
                    assert(cdb.repcnt == count && cdb.max_repeat == (int)limit);
                }
            }
        }
    }
    ready();
    cdb.play_fad_start = 550;
    cdb.play_fad_end = 350;
    request(0x800190, 0, 0);
    assert(seek_cd() == HIRQ_PEND);
}

static void dispatch_seek(u32 target)
{
    request(target, 0, 0);
    cdb.cr1 |= 0x1100;
    cdc_cmd_process();
}

static void seek_handoff_event(void)
{
    for (unsigned active = 0; active < 2; active++) {
        for (unsigned buffered = 0; buffered < 2; buffered++) {
            for (unsigned pending = 0; pending < 2; pending++) {
                ready();
                pause_active = active;
                cdb.block_free = MAX_BLOCKS - buffered;
                cdb.play_type = PLAYTYPE_SECTOR;
                cdb.play_fad_start = 175;
                cdb.play_fad_end = 225;
                cdb.max_repeat = 1;
                repeat_on_ack = 1;
                u16 prior = HIRQ_EFLS | (pending ? HIRQ_PEND : 0);
                u16 common = HIRQ_CMOK | (buffered ? HIRQ_SCDQ : 0);
                hirq = prior;
                acknowledge_after = -1;
                dispatch_seek(0x80012c);
                assert(hirq == common && !hirq_clear && waits == 500);
                assert(cdb.fad == 180 && !cdb.repcnt && reply[0] >> 8 == (STAT_WAIT | STAT_PAUSE));
                hirq = prior;
                waits = 0;
                acknowledge_after = 1;
                dispatch_seek(0x80012c);
                assert(hirq == (common | HIRQ_PEND) && !hirq_clear && waits == 1);
                assert(cdb.fad == 300 && cdb.repcnt == 1 && !cdb.play_type);
                assert(buf_fad_start == 123 && buf_fad_end == 456);
            }
        }
    }
}

static void seek_status_reply(void)
{
    const u32 positions[] = {0, 0xffffff, 0x101, 0x202, 0x303, 0x800096, 0x800190, 0xfffffe};
    const unsigned states[] = {STAT_PAUSE, STAT_PLAY, STAT_STANDBY, STAT_SEEK, STAT_SCAN, STAT_RETRY, STAT_ERROR};
    for (unsigned s = 0; s < sizeof states / sizeof *states; s++) {
        for (unsigned p = 0; p < sizeof positions / sizeof *positions; p++) {
            ready();
            cdb.status = states[s];
            if (states[s] == STAT_STANDBY) cdb.fad = 0xffffffff;
            dispatch_seek(positions[p]);
            assert(reply[0] >> 8 == STAT_BUSY);
            int settled = positions[p] ? STAT_PAUSE : STAT_STANDBY;
            assert(cdb.status == settled);
            u16 low = reply[0] & 255, fields[] = {reply[1], reply[2], reply[3]};
            request(0, 0, 0);
            cdc_cmd_process();
            assert(reply[0] == ((settled << 8) | low));
            assert(reply[1] == fields[0] && reply[2] == fields[1] && reply[3] == fields[2]);
        }
    }
}

static void seek_event_controls(void)
{
    for (unsigned kind = 0; kind < 5; kind++) {
        const u32 targets[] = {0x8000c8, 0x80012c, 0, 0xffffff, 0x80012c};
        ready();
        cdb.play_fad_start = 175;
        cdb.play_fad_end = 225;
        cdb.block_free = MAX_BLOCKS;
        if (kind == 1) cdb.max_repeat = 15;
        if (kind == 4) cdb.track_num = 0;
        hirq = HIRQ_EHST | HIRQ_PEND;
        u16 prior_low = reply[0] & 255;
        dispatch_seek(targets[kind]);
        assert(hirq == HIRQ_CMOK);
        assert(!hirq_clear);
        if (kind == 4) assert(reply[0] == ((STAT_REJECT << 8) | prior_low));
    }
}

static void seed_abort(void)
{
    cdb.cr1 = 0x7519;
    reply[0] = 0x7519;
    reply[1] = 0x1234;
    reply[2] = 0x5678;
    reply[3] = 0x9abc;
}

static void abort_reply(void)
{
    const unsigned states[] = {STAT_PAUSE, STAT_PLAY, STAT_STANDBY, STAT_SEEK,
                               STAT_SCAN, STAT_RETRY, STAT_ERROR, STAT_OPEN, STAT_NODISC};
    for (unsigned s = 0; s < sizeof states / sizeof *states; s++) {
        for (unsigned kind = 0; kind <= PLAYTYPE_DIR; kind++) {
            for (unsigned buffered = 0; buffered < 2; buffered++) {
                for (unsigned active = 0; active < 2; active++) {
                    ready();
                    if (!buffered) free_partition(&cdb.part[0]);
                    cdb.status = states[s];
                    cdb.play_type = kind;
                    cdb.ctrladdr = 0x41;
                    cdb.track = 2;
                    cdb.index = 3;
                    cdb.fad = 0x123456;
                    cdb.options = 8;
                    cdb.repcnt = 7;
                    pause_active = active;
                    acknowledge_after = active ? 500 : 1;
                    seed_abort();
                    cdc_cmd_process();
                    int aborts_reader = kind == PLAYTYPE_FILE || kind == PLAYTYPE_DIR;
                    unsigned status = aborts_reader ? STAT_PAUSE : states[s];
                    assert(reply[0] == ((status << 8) | 0x87));
                    assert(reply[1] == 0x4102 && reply[2] == 0x0312 && reply[3] == 0x3456);
                    assert(hirq == (HIRQ_CMOK | HIRQ_EFLS | (buffered ? HIRQ_SCDQ : 0)));
                    assert(!hirq_clear && !cdb.pause_request && cdb.status == (int)status);
                    assert(waits == (aborts_reader ? (unsigned)acknowledge_after : 0));
                    assert(cdb.play_type == (aborts_reader ? 0 : (int)kind));
                }
            }
        }
    }
}

static void abort_cache(void)
{
    for (unsigned kind = PLAYTYPE_FILE; kind <= PLAYTYPE_DIR; kind++) {
        for (unsigned active = 0; active < 2; active++) {
            ready();
            pause_active = active;
            cdb.play_type = kind;
            cdb.rdir_state = 3;
            cdb.cdir_pfnum = 9;
            cdb.cdir_drend = 1;
            cdb.play_pos_types = 5;
            cdb.max_repeat = 5;
            CDBLOCK before = cdb;
            seed_abort();
            cdc_cmd_process();
            assert(buf_fad_start == 123 && buf_fad_end == 456);
            assert(!memcmp(cdb.part, before.part, sizeof cdb.part));
            assert(!memcmp(cdb.block, before.block, sizeof cdb.block));
            assert(!memcmp(cdb.filter, before.filter, sizeof cdb.filter));
            assert(cdb.block_free == before.block_free && cdb.cddev_filter == before.cddev_filter);
            assert(cdb.play_fad_start == before.play_fad_start && cdb.play_fad_end == before.play_fad_end);
            assert(cdb.max_repeat == before.max_repeat && cdb.play_pos_types == before.play_pos_types);
            assert(cdb.rdir_state == (kind == PLAYTYPE_DIR ? 0 : 3));
            assert(cdb.cdir_pfnum == (kind == PLAYTYPE_DIR ? 0 : 9));
            assert(cdb.cdir_drend == (kind == PLAYTYPE_DIR ? 0 : 1));
        }
    }
}

static void abort_timeout(void)
{
    for (unsigned kind = PLAYTYPE_FILE; kind <= PLAYTYPE_DIR; kind++) {
        ready();
        cdb.status = STAT_PLAY;
        cdb.play_type = kind;
        cdb.rdir_state = 3;
        cdb.cdir_pfnum = 9;
        acknowledge_after = -1;
        seed_abort();
        cdc_cmd_process();
        assert(waits == 500 && cdb.play_type == (int)kind && cdb.pause_request);
        assert(reply[0] == (((STAT_WAIT | STAT_PLAY) << 8) | 0x19));
        assert(reply[1] == 0x1234 && reply[2] == 0x5678 && reply[3] == 0x9abc);
        assert(hirq == (HIRQ_CMOK | HIRQ_SCDQ) && !hirq_clear);
        assert(cdb.rdir_state == 3 && cdb.cdir_pfnum == 9);
        assert(buf_fad_start == 123 && buf_fad_end == 456);
        u16 old_reply[4];
        memcpy(old_reply, reply, sizeof old_reply);
        worker_pause_step();
        assert(!cdb.play_type && !cdb.pause_request && cdb.status == STAT_PAUSE);
        assert(hirq == (HIRQ_CMOK | HIRQ_SCDQ) && !memcmp(reply, old_reply, sizeof reply));
    }
}

static void begin_file_work(int kind)
{
    ready();
    pause_active = 1;
    start_file_play(2, 250, 10, kind);
    cdb.rdir_state = 2;
    cdb.cdir_pfnum = 9;
    assert(cdb.fad == 250 && cdb.play_fad_start == 250 && cdb.play_fad_end == 260);
    assert(cdb.filter[2].fad == 250 && cdb.filter[2].range == 10);
}

static void file_transition(unsigned stage, int kind)
{
    if (stage == 1) {
        cdb.fad = cdb.play_fad_end;
        if (kind == PLAYTYPE_DIR) finish_directory(1);
        else assert(!worker_step());
        assert(!cdb.play_type && !read_calls);
    } else if (stage == 2) {
        assert(abort_file() == HIRQ_EFLS);
    } else if (stage >= 3) {
        cdb.cr1 = (u16[]){0, 1, 0x80}[stage - 3];
        int events = cdb.cr1 == 1 ? HIRQ_EFLS | HIRQ_ECPY | HIRQ_ESEL | HIRQ_EHST : 0;
        assert(init_cdblock() == events);
    }
}

static void file_nochg_play(void)
{
    const u32 positions[][4] = {
        {0xffffff, 0xffffff, 150, 700}, {0xffffff, 0x201, 150, 400},
        {0xffffff, 0x800020, 150, 182}, {0x800258, 0xffffff, 600, 700},
        {0x201, 0xffffff, 350, 700}, {0, 0xffffff, 150, 700},
        {0xffffff, 0, 150, 700}
    };
    for (int kind = PLAYTYPE_FILE; kind <= PLAYTYPE_DIR; kind++) {
        for (unsigned stage = 0; stage < 6; stage++) {
            for (unsigned mode = 0; mode <= 0x80; mode += 0x80) {
                for (unsigned p = 0; p < sizeof positions / sizeof *positions; p++) {
                    begin_file_work(kind);
                    file_transition(stage, kind);
                    FILTER filters[MAX_SELECTORS];
                    memcpy(filters, cdb.filter, sizeof filters);
                    request(positions[p][0], positions[p][1], mode);
                    assert(!play_cd());
                    assert(cdb.play_fad_start == positions[p][2] && cdb.play_fad_end == positions[p][3]);
                    assert(cdb.play_type == PLAYTYPE_SECTOR && !cdb.pause_request && cdb.play_pos_types < 16);
                    assert(!memcmp(filters, cdb.filter, sizeof filters));
                    request(0xffffff, 0xffffff, 0x80);
                    assert(!play_cd() && cdb.play_fad_start == positions[p][2] && cdb.play_fad_end == positions[p][3]);
                }
            }
        }
    }
}

static void file_range_seek(void)
{
    const u32 positions[] = {0x800000, 0x800001, 0x8000c7, 0x8000fa, 0x800103,
                             0x800104, 0x800258, 0x8002bc, 0x8fffff, 0, 0xffffff};
    for (int kind = PLAYTYPE_FILE; kind <= PLAYTYPE_DIR; kind++) {
        for (unsigned stage = 0; stage < 6; stage++) {
            for (unsigned p = 0; p < sizeof positions / sizeof *positions; p++) {
                begin_file_work(kind);
                file_transition(stage, kind);
                u8 saved = cdb.play_pos_types;
                request(positions[p], 0, 0);
                assert(seek_cd() == (p == 7 || p == 8 ? HIRQ_PEND : 0));
                assert(cdb.play_fad_start == 250 && cdb.play_fad_end == 260 && cdb.play_pos_types == saved);
                request(0xffffff, 0xffffff, 0);
                assert(!play_cd() && cdb.play_fad_start == 150 && cdb.play_fad_end == 700);
            }
        }
    }
}

static void file_range_wait(void)
{
    for (int kind = PLAYTYPE_FILE; kind <= PLAYTYPE_DIR; kind++) {
        for (unsigned seeking = 0; seeking < 2; seeking++) {
            begin_file_work(kind);
            u8 saved = cdb.play_pos_types;
            waits = 0;
            request(0x8000c8, 0x201, 0);
            assert(!play_cd() && reply[0] >> 8 == STAT_REJECT && !waits && cdb.play_type == kind);
            acknowledge_after = -1;
            request(seeking ? 0x8000c8 : 0xffffff, 0xffffff, 0);
            assert(!(seeking ? seek_cd() : play_cd()));
            assert(waits == 500 && reply[0] >> 8 == (STAT_WAIT | cdb.status));
            assert(cdb.play_type == kind && cdb.play_fad_start == 250 && cdb.play_fad_end == 260);
            assert(cdb.play_pos_types == saved);
            worker_pause_step();
            assert(!cdb.play_type && !cdb.pause_request);
            assert(cdb.play_fad_start == 250 && cdb.play_fad_end == 260 && cdb.play_pos_types == saved);
        }
    }
}

static void file_range_media(void)
{
    begin_file_work(PLAYTYPE_FILE);
    file_transition(2, PLAYTYPE_FILE);
    cdb.track_num = 0;
    request(0xffffff, 0xffffff, 0);
    assert(!play_cd() && reply[0] >> 8 == STAT_REJECT);
    assert(cdb.play_fad_start == 250 && cdb.play_fad_end == 260);
    request(0, 0, 0);
    assert(!seek_cd() && cdb.status == STAT_STANDBY);
    cdb.track_num = 1;
    media[0].fad_end = 899;
    request(0xffffff, 0xffffff, 0);
    assert(!play_cd() && cdb.play_fad_start == 150 && cdb.play_fad_end == 900);
}

static void file_range_controls(void)
{
    for (int kind = PLAYTYPE_FILE; kind <= PLAYTYPE_DIR; kind++) {
        begin_file_work(kind);
        get_cd_status();
        assert(cdb.play_type == kind && cdb.play_fad_start == 250 && cdb.play_fad_end == 260);
        file_transition(1, kind);
        assert(cdb.play_fad_start == 250 && cdb.play_fad_end == 260);
        background_start(0);
        request(0xffffff, 0xffffff, 0);
        assert(!play_cd() && cdb.play_fad_start == 150 && cdb.play_fad_end == 300);
    }
}

/* Actual dispatch and register mask, adapted from paps-g 4e024188. */
static void original_drive_commands(void)
{
    const u16 disabled[] = {0, 0x0100, 0x3000, 0x7fff};
    for (unsigned control = 0; control < sizeof disabled / sizeof *disabled; control++) {
        for (unsigned opcode = 0; opcode < 256; opcode++) {
            ready();
            ss_control = disabled[control];
            cdb.cr1 = (opcode << 8) | 0x5a;
            cdb.cr2 = 0x1234;
            cdb.cr3 = 0x5678;
            cdb.cr4 = 0x9abc;
            cdb.play_type = PLAYTYPE_SECTOR;
            cdb.block_free = MAX_BLOCKS - 1;
            CDBLOCK before = cdb;
            u16 response[4];
            memcpy(response, reply, sizeof response);
            cdc_cmd_process();
            assert(hirq == HIRQ_CMOK && !hirq_clear);
            assert(!memcmp(&before, &cdb, sizeof cdb) && !memcmp(response, reply, sizeof response));
            assert(!waits && !wakeups && buf_fad_start == 123 && buf_fad_end == 456);
        }
    }

    ss_control = 0x8000;
    cdb.cr1 = 0;
    cdc_cmd_process();
    assert(hirq == (HIRQ_CMOK | HIRQ_SCDQ) && (reply[0] >> 8) == cdb.status);
}

static void original_drive_mask(void)
{
    ready();
    ss_control = 0x0100;
    cdb.cr1 = 0x0100;
    cdc_cmd_process();
    printf("shadow mask=%u\n", (unsigned)hirq);
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
    case 21:init_flags();break;case 22:init_no_media();break;
    case 23:selector_handoff();break;case 24:selector_scope();break;case 25:selector_logs();break;
    case 26:subcode_transitions();break;
    case 27:subcode_track_bounds();break;
    case 28:subcode_admission();break;
    case 29:subcode_valid_payloads();break;
    case 30:seek_retained_forms();break;
    case 31:seek_resume_ranges();break;
    case 32:seek_completion_bounds();break;
    case 33:seek_handoff_event();break;
    case 34:seek_status_reply();break;
    case 35:seek_event_controls();break;
    case 36:abort_reply();break;
    case 37:abort_cache();break;
    case 38:abort_timeout();break;
    case 39:file_nochg_play();break;
    case 40:file_range_seek();break;
    case 41:file_range_wait();break;
    case 42:file_range_media();break;
    case 43:file_range_controls();break;
    case 44:original_drive_commands();break;
    case 45:original_drive_mask();break;
    }
    free(media);puts("pass: actual drive commands, helpers and worker fragments");return 0;
}
