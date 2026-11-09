/* Adapted from paps-g 82125993. Actual save state/ISO/FatFs; scheduling and registers are stubs. */
#include "coupled-harness.inc"
#include "disc-api.h"
#define STAT_PAUSE 1
#define STAT_OPEN 6
#define STAT_NODISC 7
#define HIRQ_DCHG 0x20
static int HIRQ;
typedef struct {
    FIL *fp; u32 fad_0,fad_start; int *index,max_index;
    u32 fad_end,file_offset; u16 sector_size; u8 ctrl_addr,mode;
} TRACK_INFO;
static FIL track_fp[100];
static struct { int status,track_num,has_subrw; TRACK_INFO tracks[100]; TRACK_INFO *play_track; FIL subcode_fp; } cdb;
static int current_index,category_num,category_current,total_disc=2,next_disc;
static int disc_path[2]={1,7};
static char path_str[]="\0DISC1\0DISC2";
static char mdisc_str[16];
static int sector_delay_force=-1,play_delay_force=-1,pend_delay_force=-1;
static int sector_delay,play_delay,pend_delay;
static int present=1,identify_result,identifies,discoveries,parse_error,find_error,alloc_error;
static void set_status(u8 state) { assert(cdb.status==state); }
static void init_toc(void) { cdb.status=STAT_PAUSE; }
static char *get_category(int id) { assert(!id);return "Category"; }
#define sprintk sprintf
static int find_cue_iso(char *path,char *out)
{ discoveries++;snprintf(out,256,"%s/disc.iso",path);return find_error?find_error:2; }
static int parse_cue(char *name,u8 *ip) { (void)name;(void)ip;assert(0);return -1; }
static int parse_config(char *path,char *id) { (void)path;(void)id;return 0; }
static int find_disc(char *name) { (void)name;return 0; }
#define parse_iso real_parse_iso
#include "disc-core.inc"
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
static struct { u32 CMD,DCTRL,IDMACTRL,MASK,ICR; } sdio;
#define RCC (&rcc)
#define SDIO (&sdio)
#define SDCLK_LOW 0
static int sd_rca;
static void osDelay(int ticks) { assert(ticks==10); }
static void sd_clk_set(int clock) { assert(clock==SDCLK_LOW); }
static void sd_bus_width(int width) { assert(width==1); }
static void sd_power_on(int on) { assert(on==1); }
static int sdcard_identify(void) { identifies++;return identify_result; }
#include "sd-reset.inc"
int sd_card_insert(void) { return present; }
static void swap_step(void)
{
    int retv=0;
#include "swap.inc"
    (void)retv;
}
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
    u8 header[2048]={0};memcpy(header+32,id,16);
    FIL fp;UINT count;assert(f_open(&fp,name,FA_CREATE_NEW|FA_WRITE)==0);
    assert(f_write(&fp,header,size,&count)==0 && count==size);assert(f_close(&fp)==0);
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
    if(value==4) {
        assert(f_unlink("/SAROO/ISO/DISC2/disc.iso")==0);
        /* Directory already exists; write a complete replacement fixture. */
        FIL fp;UINT count;u8 bytes[2048]={0};memcpy(bytes+32,"OTHERID000000002",16);
        assert(f_open(&fp,"/SAROO/ISO/DISC2/disc.iso",FA_CREATE_NEW|FA_WRITE)==0);
        assert(f_write(&fp,bytes,sizeof bytes,&count)==0 && count==sizeof bytes && f_close(&fp)==0);
    }
    present=0;swap_step();assert(cdb.status==STAT_OPEN);
    present=1;swap_step();assert(cdb.status==STAT_PAUSE);
    assert(!memcmp(snapshot,test_shared,sizeof snapshot));
}
int main(int argc,char **argv)
{
    assert(argc==4);const char *mode=argv[2];int value=atoi(argv[3]);
    disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(f_mount(&fs,"0:",1)==0);
    fault="none";fault_nth=1;initialize_disc();
    if(!strcmp(mode,"session")) live_session(value);
    else if(!strcmp(mode,"menu")) {
        assert(menu_step(value)==0 && ss_index==(value?2:1));
        assert(api_reads && (value?!memcmp(test_shared,(u8[65536]){0},65536):1));
    } else if(!strcmp(mode,"swap-failure")) swap_failure(value);
    else if(!strcmp(mode,"reset")) { identify_result=value;assert(sdio_reset()==value && identifies==1); }
    else {
        int preview=!strncmp(mode,"preview-",8);u8 ip[256];
        alloc_error=strstr(mode,"allocation")!=NULL;
        int result=preview?get_disc_ip(value,ip):select_disc(value,1);
        assert(result==(alloc_error?FR_NOT_ENOUGH_CORE:-1));
        assert(!discoveries && !api_reads && !api_writes && ss_index==1);
    }
    assert(!lock_depth);printf("disc %s value=%d reads=%u writes=%u identifies=%d\n",mode,value,api_reads,api_writes,identifies);
    unload_disc();assert(f_close(&ss_fp)==0 && f_close(&sm_fp)==0);
    free(original);free(keep);assert(!fclose(disk));return 0;
}
