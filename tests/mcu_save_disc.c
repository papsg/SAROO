/* Adapted from paps-g 82125993/cb3abf06; atomic publication follows c6dc9847. Actual save state/ISO/FatFs; scheduling and registers are stubs. */
#include "coupled-harness.inc"
#include "disc-api.h"
#define PLAYTYPE_DIR 3
#define STAT_PAUSE 1
#define STAT_OPEN 6
#define STAT_NODISC 7
#define HIRQ_DCHG 0x20
static int HIRQ;
#include "track-layout.h"
static FIL track_fp[100];
static struct { int status,track_num,has_subrw,play_type,play_wait,pause_request,rdir_state,cdir_pfnum,cdir_drend; TRACK_INFO tracks[100]; TRACK_INFO *play_track; FIL subcode_fp; } cdb;
static int current_index,category_current,total_disc=2,next_disc;
static int disc_path[2]={1,7};
static char path_str[]="\0DISC1\0DISC2";
static char mdisc_str[16];
static int sector_delay_force=-1,play_delay_force=-1,pend_delay_force=-1;
static int sector_delay,play_delay,pend_delay;
static int present=1,identify_result,identifies,discoveries,parse_error,find_error,alloc_error;
static void set_status(u8 state) { assert(cdb.status==state); }
static void init_toc(void) { cdb.status=STAT_PAUSE; }
#define sprintk sprintf
static int find_cue_iso(char *path,char *out)
{ discoveries++;snprintf(out,256,"%s/disc.iso",path);return find_error?find_error:2; }
static int parse_cue(char *name,u8 *ip) { (void)name;(void)ip;assert(0);return -1; }
/* Actual configuration parsing and FatFs share the archive workspace mutex. */
static int lang_id,debug_flags,log_mask,auto_update,sort_mode;
_Alignas(8) static struct { u8 before[32],data[4096],after[32]; } config_info;
#define SYSINFO_ADDR ((uintptr_t)config_info.data)
#define LEDEV_SCFG_ERROR 4
static int config_fault,config_errors;
static unsigned config_reads,config_closed,config_locked_reads;
static FRESULT config_open(FIL *fp,const TCHAR *name,BYTE mode)
{ return config_fault==4?FR_DISK_ERR:f_open(fp,name,mode); }
static FRESULT config_read(FIL *fp,void *out,UINT size,UINT *count)
{
    config_reads++;config_locked_reads+=lock_depth==1;
    return f_read(fp,out,config_fault==1 && size?size-1:size,count);
}
static FRESULT config_close(FIL *fp)
{
    config_closed++;FRESULT result=f_close(fp);
    /* Real release, then an injected reported failure: not media recovery. */
    return result?result:config_fault==2?FR_DISK_ERR:FR_OK;
}
static void config_led(int event) { assert(event==LEDEV_SCFG_ERROR);config_errors++; }
#define f_open config_open
#define f_read config_read
#define f_close config_close
#define led_event config_led
#include "config-disc.inc"
#undef led_event
#undef f_close
#undef f_read
#undef f_open
static int find_disc(char *name) { (void)name;return 0; }
/* Actual worker/helper bodies; notifications/preemption are deterministic stubs. */
static int pause_waits,pause_wakes,pause_acks,pause_allow_ack=1,pause_after=3;
static int buf_fad_start,buf_fad_end,require_quiescence,pause_active;
static int pause_irq_mask,inject_publication,pending_publication,publication_attempts,published_requests;
static void publish_new_request(void)
{ cdb.play_type=1;cdb.pause_request=1;cdb.play_wait=0;published_requests++; }
static void publication_checkpoint(void)
{
    if(!inject_publication)return;
    inject_publication=0;publication_attempts++;
    if(pause_irq_mask)pending_publication=1;else publish_new_request();
}
static int disable_irq(void) { int old=pause_irq_mask;pause_irq_mask=1;return old; }
static void restore_irq(int old)
{
    assert(pause_irq_mask && (old==0 || old==1));pause_irq_mask=old;
    if(!pause_irq_mask && pending_publication) { pending_publication=0;publish_new_request(); }
}
static void set_pause_ok(void) { assert(!pause_irq_mask);pause_acks++; }
#include "pause-ack.inc"
static void pause_worker(void)
{
#define SSLOG(...) ((void)0)
    if(pause_active) {
#include "pause-active-worker.inc"
    } else {
#include "pause-worker.inc"
    }
#undef SSLOG
_restart_wait: return;
}
static void disk_task_wakeup(void) { assert(!pause_irq_mask);pause_wakes++; }
static void wait_pause_ok(void)
{
    assert(!pause_irq_mask);pause_waits++;
    assert(cdb.pause_request && !cdb.play_wait && cdb.track_num==1);
    assert(cdb.tracks[0].fp==&track_fp[0] && track_fp[0].obj.fs);
    /* Early returns represent stale notifications; only the worker retires a request. */
    if(pause_allow_ack && pause_waits>=pause_after)pause_worker();
}
#include "pause-disc.inc"
static FRESULT disc_close(FIL *fp)
{ assert(!pause_irq_mask && (!require_quiescence || !cdb.play_type));return f_close(fp); }
#define parse_iso real_parse_iso
#define f_close disc_close
#include "disc-core.inc"
#undef f_close
#undef parse_iso
static int parse_iso(char *name) { return parse_error?parse_error:real_parse_iso(name); }
static void *disc_alloc(size_t size) { return alloc_error?NULL:malloc(size); }
#define malloc disc_alloc
#include "disc-load.inc"
#undef malloc
static int select_disc(int index,int start_save)
{
#if DISC_STARTS_SAVE
    return load_disc(index,start_save);
#else
    (void)start_save;return load_disc(index);
#endif
}
static struct { u32 AHB3RSTR; } rcc;
static struct { u32 CMD,DCTRL,IDMACTRL,MASK,ICR,CLKCR,DTIMER,POWER; } sdio;
#define RCC (&rcc)
#define SDIO (&sdio)
#define RCC_AHB3RSTR_SDMMC1RST (1u << 16)
static int sd_rca;
static void *cmd_sem=&rcc;
static int expect_reset_lock;
static void osDelay(int ticks) { assert(ticks==10 && (rcc.AHB3RSTR & RCC_AHB3RSTR_SDMMC1RST)); }
#define __DSB() assert(rcc.AHB3RSTR & RCC_AHB3RSTR_SDMMC1RST)
#define sd_clk_set(clock) (sdio.CLKCR=(clock))
#define sd_bus_width(width) assert((width)==1)
#define sd_power_on(on) assert((on)==1)
static int sdcard_identify(void)
{
    if (expect_reset_lock) assert(lock_depth==1);
    identifies++;
    return identify_result;
}
#include "sd-reset.inc"
static int disk_reidentify(void) { return sdio_reset(); }
int sd_card_insert(void) { return present; }
static void swap_step(void)
{
    int retv=0;
#include "swap.inc"
    (void)retv;
}
/* Decoder ownership is exercised by the separate actual-GIF fixture. */
static void gif_decode_exit(void) {}
static int menu_step(int index)
{
    int retv=0;SS_ARG=index;
#include "menu-load.inc"
    return SS_ARG;
}
static void make_iso(const char *dir,const char *id,unsigned size)
{
    assert(strlen(id)==16 && f_mkdir(dir)==0);
    char name[80];snprintf(name,sizeof(name),"%s/disc.iso",dir);
    u8 header[2048]={0};memcpy(header,"SEGA SEGASATURN ",16);memcpy(header+32,id,16);
    FIL fp;UINT count;assert(f_open(&fp,name,FA_CREATE_NEW|FA_WRITE)==0);
    assert(f_write(&fp,header,size,&count)==0 && count==size);assert(f_close(&fp)==0);
}
static void write_config(const void *text,unsigned size)
{
    FIL fp;UINT count;
    assert(!f_open(&fp,"/SAROO/saroocfg.txt",FA_CREATE_ALWAYS|FA_WRITE));
    assert(!f_write(&fp,text,size,&count) && count==size && !f_close(&fp));
}
static void valid_config(void)
{
    const char *text="[global]\nsector_delay=7\nplay_delay=8\npend_delay=9\n";
    write_config(text,strlen(text));
}
static void initialize_disc(void)
{
    assert(f_mkdir("/SAROO")==0);make_archive(0,65536,0);make_archive(1,0x800000,0);
    assert(open_savefile()==0 && load_savefile((char*)test_ip)==0);
    assert(sro_bup_format(0)==0 && sro_bup_format(1)==0);
    assert(f_mkdir("/SAROO/ISO")==0);
    make_iso("/SAROO/ISO/DISC1",(char*)test_ip,2048);
    make_iso("/SAROO/ISO/DISC2","OTHERID000000002",2048);
    assert(select_disc(0,1)==0 && ss_index==1);
    reset_counts();discoveries=identifies=0;
}
static void live_session(int state)
{
    int dev=state==2;fixture(dev,32);
    if (state>=4) {
        FIL fp;UINT count;
        assert(f_open(&fp,"/SAROO/ISO/DISC2/disc.iso",FA_WRITE)==0 && f_lseek(&fp,32)==0);
        assert(f_write(&fp,test_ip,16,&count)==0 && count==16 && f_close(&fp)==0);
    }
    BUPDIR request=target;u8 *retry=NULL;
    if(state==1 || state==2 || state==4) {
        request.data_size=state==2?131073:32;request.date++;
        retry=data_bytes(request.data_size,81);fault="write-error";fault_target=dev;fault_nth=1;armed=1;
        assert(sro_bup_write(dev,&request,retry,0)==BUP_BROKEN);
        assert(phase(dev)==(state==2?1:2));armed=0;
    } else if(state==3) test_shared[4096]^=0x7d;
    u8 snapshot[sizeof test_shared];memcpy(snapshot,test_shared,sizeof snapshot);
    PendingSave saved[2];memcpy(saved,pending_save,sizeof saved);
    int slot=ss_index,header=sm_index0,window=sm_index1;
    reset_counts();next_disc=1;cdb.status=STAT_OPEN;
    swap_step();
    fprintf(stderr,"swap state=%d save_reads=%u read_bytes=%u save_writes=%u write_bytes=%u\n",state,api_reads,read_bytes,api_writes,write_bytes);
    assert(cdb.status==STAT_PAUSE && HIRQ==HIRQ_DCHG);
    assert(!memcmp(snapshot,test_shared,sizeof snapshot) && !memcmp(saved,pending_save,sizeof saved));
    assert(ss_index==slot && sm_index0==header && sm_index1==window && !api_reads && !api_writes);
    if(retry) {
        assert(sro_bup_write(dev,&request,retry,0)==0);reload_buffers();
        check_record(dev,&request,retry);check_record(dev,&neighbor,keep);free(retry);
    }
}
static void swap_failure(int value)
{
    next_disc=1;cdb.status=STAT_OPEN;
    if(value==1) identify_result=19;
    if(value==2) find_error=-4;
    if(value==3) parse_error=-3;
    if(value==5)write_config("[wrong]\n",8);
    if(value==4) {
        FIL fp;assert(f_open(&fp,"/SAROO/ISO/DISC2/disc.iso",FA_WRITE)==0);
        assert(f_lseek(&fp,128)==0 && f_truncate(&fp)==0 && f_close(&fp)==0);
    }
    u8 snapshot[sizeof test_shared];memcpy(snapshot,test_shared,sizeof snapshot);
    swap_step();
    assert(cdb.status==STAT_NODISC && next_disc==1);
    assert(HIRQ==HIRQ_DCHG && !memcmp(snapshot,test_shared,sizeof snapshot));
    if(value==1) assert(!discoveries);
    unsigned calls=discoveries+identifies;
    for(int i=0;i<100;i++) swap_step();
    assert(discoveries+identifies==calls);
    identify_result=find_error=parse_error=0;
    if(value==5)valid_config();
    if(value==4) {
        assert(f_unlink("/SAROO/ISO/DISC2/disc.iso")==0);
        /* Directory already exists; write a complete replacement fixture. */
        FIL fp;UINT count;u8 bytes[2048]={0};memcpy(bytes+32,"OTHERID000000002",16);
        memcpy(bytes,"SEGA SEGASATURN ",16);
        assert(f_open(&fp,"/SAROO/ISO/DISC2/disc.iso",FA_CREATE_NEW|FA_WRITE)==0);
        assert(f_write(&fp,bytes,sizeof bytes,&count)==0 && count==sizeof bytes && f_close(&fp)==0);
    }
    present=0;swap_step();assert(cdb.status==STAT_OPEN);
    present=1;swap_step();assert(cdb.status==STAT_PAUSE);
    assert(!memcmp(snapshot,test_shared,sizeof snapshot));
}
static void quiesce(int value)
{
    u8 retry[32]={0};
    if(value==2 || value==3) {
        fixture(0,32);BUPDIR request=target;request.date++;
        fault="write-error";fault_target=0;fault_nth=1;armed=1;
        assert(sro_bup_write(0,&request,retry,0)==BUP_BROKEN && phase(0)==2);
        armed=0;reset_counts();
    }
    PendingSave saved[2];memcpy(saved,pending_save,sizeof saved);
    u8 snapshot[sizeof test_shared];memcpy(snapshot,test_shared,sizeof snapshot);
    int idle=value==4 || value==7, load=value==1 || value==3 || value==6 || value==7;
    pause_active=value==5 || value==6;
    cdb.play_type=idle?0:1;cdb.play_wait=1;buf_fad_start=150;buf_fad_end=160;
    require_quiescence=1;pause_allow_ack=value!=2 && value!=3;
    int result=menu_step(load?1:0xffff);
    if(value==2 || value==3) {
        assert(result && pause_waits==500 && pause_wakes==1 && !pause_acks);
        assert(cdb.track_num==1 && cdb.play_type==1 && cdb.pause_request && track_fp[0].obj.fs);
        assert(!memcmp(snapshot,test_shared,sizeof snapshot) && ss_index==1 && !api_reads && !api_writes);
        assert(!memcmp(saved,pending_save,sizeof saved));
        assert(buf_fad_start==150 && buf_fad_end==160);
        /* The native worker can still acknowledge later; timeout released no reader. */
        pause_worker();
    } else {
        assert(!result && !cdb.play_type && !cdb.pause_request);
        assert(pause_waits==(idle?0:3) && pause_wakes==(idle?0:1) && pause_acks==(idle?0:1));
        assert(!buf_fad_start && !buf_fad_end);
        if(load)assert(cdb.track_num==1 && ss_index==2 && api_reads);
        else assert(!cdb.track_num && cdb.status==STAT_NODISC && !api_reads && !api_writes && !memcmp(snapshot,test_shared,sizeof snapshot));
    }
}
static void publication(int value)
{
    /* Modeled new command publication at the idle/request boundary. No new
     * command implementation, device scheduling or retained-PLAY protocol. */
    pause_active=value%2;cdb.play_type=value/2+1;cdb.pause_request=1;
    buf_fad_start=150;buf_fad_end=160;inject_publication=1;
    pause_worker();
    assert(publication_attempts==1 && published_requests==1 && !pending_publication);
    assert(cdb.play_type==1 && cdb.pause_request==1 && pause_acks==1);
    assert(buf_fad_start==150 && buf_fad_end==160 && !pause_irq_mask);
}
static void automatic_no_wait(void)
{
    cdb.play_type=1;cdb.play_wait=1;pause_allow_ack=0;
    next_disc=1;cdb.status=STAT_OPEN;swap_step();
    assert(cdb.track_num==1 && cdb.status==STAT_PAUSE && ss_index==1);
    assert(!pause_waits && !pause_wakes && !api_reads && !api_writes);
}
static void reidentify_under_lock(int fail)
{
    expect_reset_lock=1;
    next_disc=1;
    cdb.status=STAT_OPEN;
    identify_result=fail ? -17 : 0;
    swap_step();
    assert(!lock_depth && identifies==1 && ss_index==1);
    assert(cdb.status==(fail ? STAT_NODISC : STAT_PAUSE));
    assert(discoveries==!fail && !api_reads && !api_writes);
}
static void configuration_failure(int value)
{
    fixture(0,32);
    sector_delay=45;play_delay=46;pend_delay=47;strcpy(mdisc_str,"old-disc");
    memset(&config_info,0xa5,sizeof(config_info));
    u8 snapshot[sizeof test_shared],settings[sizeof config_info];
    memcpy(snapshot,test_shared,sizeof snapshot);memcpy(settings,&config_info,sizeof settings);
    PendingSave pending[2];memcpy(pending,pending_save,sizeof pending);
    int slot=ss_index,header=sm_index0,window=sm_index1;
    u8 *retry=NULL;BUPDIR request=target;
    if(value==5) {
        request.date++;retry=data_bytes(32,81);
        fault="write-error";fault_target=0;fault_nth=1;armed=1;
        assert(sro_bup_write(0,&request,retry,0)==BUP_BROKEN && phase(0)==2);armed=0;
        memcpy(snapshot,test_shared,sizeof snapshot);memcpy(pending,pending_save,sizeof pending);
    }
    const char *text=value==0 || value==5?"[global]\nplay_delay=bad\n":"[global]\nplay_delay=7\n";
    write_config(text,strlen(text));
    if(value==3) {
        char *large=malloc(32768);assert(large);memset(large,'#',32768);
        write_config(large,32768);free(large);
    }
    config_fault=value==1?1:value==2?2:value==4?4:0;
    config_reads=config_closed=config_locked_reads=0;reset_counts();
    int expected=value==0 || value==5?FR_INVALID_PARAMETER:value==3?FR_INVALID_OBJECT:FR_DISK_ERR;
    assert(menu_step(1)==(u16)-expected);
    assert(!cdb.track_num && !cdb.has_subrw && !track_fp[0].obj.fs);
    assert(ss_index==slot && sm_index0==header && sm_index1==window);
    assert(!api_reads && !api_writes && !memcmp(snapshot,test_shared,sizeof snapshot));
    assert(!memcmp(pending,pending_save,sizeof pending));
    assert(sector_delay==45 && play_delay==46 && pend_delay==47 && !strcmp(mdisc_str,"old-disc"));
    assert(!memcmp(settings,&config_info,sizeof settings));
    assert(config_closed==(value!=4) && config_reads==(value!=3 && value!=4));
    assert(config_locked_reads==config_reads && config_errors);
    config_fault=0;valid_config();
    if(retry) {
        assert(!sro_bup_write(0,&request,retry,0));check_record(0,&request,retry);
        check_record(0,&neighbor,keep);free(retry);
    }
    assert(!menu_step(1) && ss_index==2);
    assert(sector_delay==7 && play_delay==8 && pend_delay==9 && !mdisc_str[0]);
}
static void configuration_defaults(void)
{
    valid_config();assert(!menu_step(0));
    assert(sector_delay==7 && play_delay==8 && pend_delay==9);
    write_config("[global]\n",9);assert(!menu_step(1));
    assert(!sector_delay && !play_delay && !pend_delay && !mdisc_str[0]);
}

static void nongame_session(int pending)
{
    fixture(0,32);
    BUPDIR request=target;
    u8 *retry=NULL;
    if (pending) {
        request.date++;
        retry=data_bytes(32,81);
        fault="write-error";fault_target=0;fault_nth=1;armed=1;
        assert(sro_bup_write(0,&request,retry,0)==BUP_BROKEN && phase(0)==2);
        armed=0;
    }
    FIL image;
    UINT count;
    const u8 signature[16]={0};
    assert(f_open(&image,"/SAROO/ISO/DISC2/disc.iso",FA_WRITE)==FR_OK);
    assert(f_write(&image,signature,sizeof(signature),&count)==FR_OK && count==sizeof(signature));
    assert(f_close(&image)==FR_OK);
    const char config[]="[global]\nplay_delay=7\n[]\nplay_delay=42\n"
                        "[OTHERID000000002]\nplay_delay=93\n";
    write_config(config,sizeof(config)-1);
    int slot=ss_index,header=sm_index0,window=sm_index1;
    FSIZE_t archive_size=f_size(&ss_fp);
    u8 snapshot[sizeof(test_shared)];
    memcpy(snapshot,test_shared,sizeof(snapshot));
    PendingSave saved[2];
    memcpy(saved,pending_save,sizeof(saved));
    config_reads=config_locked_reads=0;
    reset_counts();
    assert(!menu_step(1) && cdb.track_num==1 && play_delay==7);
    assert(config_reads==1 && config_locked_reads==1);
    assert(ss_index==slot && sm_index0==header && sm_index1==window);
    assert(!memcmp(snapshot,test_shared,sizeof(snapshot)) && !memcmp(saved,pending_save,sizeof(saved)));
    assert(!api_reads && !api_writes && !api_syncs && f_size(&ss_fp)==archive_size);
    if (retry) {
        assert(sro_bup_write(0,&request,retry,0)==0);
        check_record(0,&request,retry);
        check_record(0,&neighbor,keep);
        free(retry);
    }
}
int main(int argc,char **argv)
{
    assert(argc==4);const char *mode=argv[2];int value=atoi(argv[3]);
    disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(f_mount(&fs,"0:",1)==0);
    fault="none";fault_nth=1;initialize_disc();
    if(!strcmp(mode,"configuration"))configuration_failure(value);
    else if(!strcmp(mode,"configuration-defaults"))configuration_defaults();
    else if(!strcmp(mode,"nongame"))nongame_session(value);
    else if(!strcmp(mode,"session")) live_session(value);
    else if(!strcmp(mode,"menu")) {
        assert(menu_step(value)==0 && ss_index==(value?2:1));
        assert(api_reads && (value?!memcmp(test_shared,(u8[65536]){0},65536):1));
    } else if(!strcmp(mode,"quiesce")) quiesce(value);
    else if(!strcmp(mode,"publication")) publication(value);
    else if(!strcmp(mode,"automatic-no-wait")) automatic_no_wait();
    else if(!strcmp(mode,"reset-lock")) reidentify_under_lock(value);
    else if(!strcmp(mode,"stop")) {
        u8 snapshot[sizeof test_shared];memcpy(snapshot,test_shared,sizeof snapshot);
        assert(!menu_step(0xffff) && cdb.status==STAT_NODISC && !cdb.track_num);
        assert(!memcmp(snapshot,test_shared,sizeof snapshot) && ss_index==1);
        assert(!api_reads && !api_writes && !discoveries && !identifies);
        assert(!menu_step(0xffff) && cdb.status==STAT_NODISC && !cdb.track_num);
    } else if(!strcmp(mode,"swap-failure")) swap_failure(value);
    else if(!strcmp(mode,"reset")) { identify_result=value;assert(sdio_reset()==value && identifies==1); }
    else {
        int preview=!strncmp(mode,"preview-",8);u8 ip[256];
        alloc_error=strstr(mode,"allocation")!=NULL;
        int result=preview?get_disc_ip(value,ip):select_disc(value,1);
        assert(result==(alloc_error?(preview?-FR_NOT_ENOUGH_CORE:FR_NOT_ENOUGH_CORE):-1));
        assert(!discoveries && !api_reads && !api_writes && ss_index==1);
    }
    assert(!lock_depth);printf("disc %s value=%d reads=%u writes=%u identifies=%d\n",mode,value,api_reads,api_writes,identifies);
    unload_disc();assert(f_close(&ss_fp)==0 && f_close(&sm_fp)==0);
    free(original);free(keep);assert(!fclose(disk));return 0;
}
