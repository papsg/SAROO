/* Adapted from paps-g 60d5df01; actual public C/FatFs with generated input. */
/* Synthetic sector bytes and native addresses; no physical CD task runs here. */
#define main startup_test_main
#include "mcu_save_startup.c"
#undef main
#include <stdarg.h>
typedef uint16_t u16;
#include "track-layout.h"
#include "track_fixture.h"
#include "track-limit.h"
static FIL track_fp[100];
TEST_INDEX_TABLE;
static int current_index;
#define MAX_BLOCKS 200
#define STAT_NODISC 7
#define STAT_PAUSE 1
#define MSF_TO_FAD(m,s,f) ((m)*4500+(s)*75+(f))
#define SSLOG(...) ((void)0)
static inline int disable_irq(void) { return 0; }
static inline void restore_irq(int key) { assert(!key); }
_Alignas(8) static struct { u8 before[32],data[MAX_BLOCKS*2352-4096],after[32]; } text_space;
_Alignas(8) static struct { u8 before[32],dir[512],between[32],full[512],after[32]; } paths;
#define TEST_CUE_ADDR ((uintptr_t)text_space.data)
#define TEST_DIR_PATH ((uintptr_t)paths.dir)
#define TEST_FULL_PATH ((uintptr_t)paths.full)
static struct {
    int status,track_num,has_subrw,track,index,ctrladdr,play_type,iflag,subrw_wp,subrw_rp;
    u8 *subcode_buf,*subrw_buf;
    TRACK_INFO tracks[100];
    TRACK_INFO *play_track;
    FIL subcode_fp;
    u32 TOC[102];
} cdb;
static int sector_delay;
static void set_status(u8 status) { assert(cdb.status==status); }
static FIL *handles[103];
static unsigned handles_count,opens,reads,closes,seeks,allocations,frees;
static int inject_seek,clip_seek,fail_alloc;
static int inject_open,inject_read,inject_close,short_read,partial_error;
static FSIZE_t virtual_raw_size;
static FSIZE_t virtual_track_size;
static FRESULT cue_open(FIL *fp,const char *name,BYTE mode)
{
    assert(mode==FA_READ);opens++;
    if(inject_open==(int)opens)return FR_DISK_ERR;
    FRESULT r=f_open(fp,name,mode);
    if(!r){for(unsigned i=0;i<handles_count;i++)assert(handles[i]!=fp);handles[handles_count++]=fp;
        if(virtual_raw_size && !strcmp(name,"/D/raw.bin"))fp->obj.objsize=virtual_raw_size;
        if(virtual_track_size && !strcmp(name,"/D/a.bin"))fp->obj.objsize=virtual_track_size;}
    return r;
}
static FRESULT cue_close(FIL *fp)
{
    closes++;unsigned i=0;while(i<handles_count && handles[i]!=fp)i++;
    assert(i<handles_count);FRESULT r=f_close(fp);assert(!r);
    handles[i]=handles[--handles_count];return inject_close==(int)closes?FR_DISK_ERR:r;
}
static FRESULT cue_read(FIL *fp,void *out,UINT size,UINT *count)
{
    reads++;if(inject_read==(int)reads){*count=0;return FR_DISK_ERR;}
    if(partial_error==(int)reads){assert(!f_read(fp,out,size/2,count));return FR_DISK_ERR;}
    if(short_read==(int)reads)size--;
    return f_read(fp,out,size,count);
}
static FRESULT cue_seek(FIL *fp,FSIZE_t offset)
{
    seeks++;
    if(inject_seek==(int)seeks)return FR_DISK_ERR;
    return f_lseek(fp,clip_seek==(int)seeks && offset?offset-1:offset);
}
static void *cue_alloc(size_t size)
{ allocations++;return fail_alloc?NULL:malloc(size); }
static void cue_free(void *ptr) { if(ptr)frees++;free(ptr); }
static int sprintk(char *out,const char *format,...)
{ va_list args;va_start(args,format);int n=vsprintf(out,format,args);va_end(args);return n; }
#include "token.inc"
#include "toc.inc"
#define f_open cue_open
#define f_close cue_close
#define f_read cue_read
#define f_lseek cue_seek
#include "cue.inc"
#undef f_lseek
#undef f_open
#undef f_close
#undef f_read
/* Keep the actual loader/TOC/readers. Configuration and discovery are fixtures. */
static int total_disc=1,disc_path[1]={1},category_num,category_current,next_disc;
static char path_str[]="\0D",mdisc_str[16];
static int sector_delay_force=-1,play_delay_force=-1,pend_delay_force=-1;
static int play_delay,pend_delay,selected_format=1;
static int media_case,media_pauses,media_config_calls,media_save_calls;
static char media_id[17];
static char *get_category(int index) { (void)index;assert(0);return NULL; }
static int find_cue_iso(char *dir,char *out)
{ assert(!strcmp(dir,"/SAROO/ISO/D"));strcpy(out,selected_format==1?"/D/Game.cue":"/D/raw.iso");return selected_format; }
static int parse_config(char *path,char *id)
{
    assert(!strcmp(path,"/SAROO/saroocfg.txt"));
    if (media_case) {
        media_config_calls++;
        assert(id);
        snprintf(media_id,sizeof(media_id),"%s",id);
    }
    return 0;
}
static int find_disc(char *name) { (void)name;assert(0);return -1; }
static int select_save(char *id)
{
    assert(media_case && !strcmp(id,media_id));
    media_save_calls++;
    return *id ? FR_OK : FR_INVALID_PARAMETER;
}
static int pause_disc(void) { assert(media_case);media_pauses++;return 0; }
#define load_savefile select_save
#define f_open cue_open
#define malloc cue_alloc
#define free cue_free
#include "load.inc"
#define f_read cue_read
#define f_close cue_close
#define f_lseek cue_seek
#include "preview.inc"
#undef f_lseek
#undef f_read
#undef f_close
#undef malloc
#undef free
#undef f_open
#undef load_savefile
static void create(const char *name,const void *data,unsigned size)
{
    FIL fp;UINT count;assert(f_open(&fp,name,FA_CREATE_ALWAYS|FA_WRITE)==0);
    assert(f_write(&fp,data,size,&count)==0 && count==size && f_close(&fp)==0);
}
static void guards(void)
{
    for(unsigned i=0;i<32;i++) {
        assert(text_space.before[i]==0xa5 && text_space.after[i]==0xa5);
        assert(paths.before[i]==0xa5 && paths.between[i]==0xa5 && paths.after[i]==0xa5);
    }
}
static const char *cue_name="/D/Game.cue";
static const char *single="FILE \"a.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n";
static void valid(int value)
{
    char cue[16384];strcpy(cue,single);
    if(value==1)strcat(cue,"TRACK 02 AUDIO\nINDEX 00 00:01:00\nINDEX 01 00:02:00\n");
    if(value==2)strcat(cue,"FILE \"b.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 00 00:00:00\nINDEX 01 00:02:00\n");
    if(value==3)strcat(cue,"INDEX 02 00:00:10\nINDEX 03 00:00:20\n");
    if(value==4)strcpy(cue,"\xef\xbb\xbf" "FILE\t\"a.bin\"\tBINARY\r\n\tTRACK\t01\tMODE1/2048\r\nINDEX\t01\t00:00:00");
    if(value==5){cue[0]=0;for(int i=1;i<=99;i++){char row[128];snprintf(row,sizeof(row),"FILE \"a.bin\" BINARY\nTRACK %02d AUDIO\nINDEX 01 00:00:00\n",i);strcat(cue,row);}}
    if(value>=6){const unsigned sizes[]={2048,2324,2336,2352};snprintf(cue,sizeof(cue),"FILE \"a.bin\" BINARY\nTRACK 01 MODE2/%u\nINDEX 01 00:00:00\n",sizes[value-6]);}
    create(cue_name,cue,strlen(cue));int r=parse_cue((char*)cue_name,NULL);
    assert(!r && cdb.status==STAT_PAUSE && cdb.track_num==(value==5?99:value==1||value==2?2:1));
    assert(handles_count==(unsigned)(value==5?99:value==2?2:1));
    assert(cdb.tracks[0].fad_start==150);
    if(value==1)assert(cdb.tracks[0].fad_end==224 && cdb.tracks[1].fad_0==225 && cdb.tracks[1].fad_start==300 && cdb.tracks[1].fad_end==449 && cdb.tracks[1].file_offset==150*2352);
    if(value==2)assert(cdb.tracks[0].fad_end==449 && cdb.tracks[1].fad_0==450 && cdb.tracks[1].fad_start==600 && cdb.tracks[1].fad_end==749);
    if(value==3)assert(current_index==2 && cdb.tracks[0].max_index==3 && TEST_INDEX_READ(0)==160 && TEST_INDEX_READ(1)==170);
    if(value==5)assert(cdb.tracks[98].fad_start==29550 && cdb.tracks[98].fad_end==29849);
    assert(bswap32(cdb.TOC[99])==((value==5?0x01u:0x41u)<<24|0x010000u));
    assert((bswap32(cdb.TOC[100])&0x00ff0000)==(unsigned)cdb.track_num<<16);
    assert((bswap32(cdb.TOC[101])&0x00ffffff)==cdb.tracks[cdb.track_num-1].fad_end+1);
    assert(!unload_disc() && !handles_count && !cdb.track_num && !current_index);
    unsigned n=closes;assert(!unload_disc() && closes==n);guards();
}
static void syntax(int value)
{
    const char *bad[]={"","\n \t\n","FILE\n","FILE \"a.bin\"\n","FILE \"a.bin BINARY\n","FILE \"a.bin\" WAVE\n", "TRACK 01 AUDIO\n",
        "FILE \"a.bin\" BINARY\nTRACK\n","FILE \"a.bin\" BINARY\nTRACK 01\n","FILE \"a.bin\" BINARY\nTRACK 01 MODE1/0\n",
        "FILE \"a.bin\" BINARY\nTRACK 01 MODE1/999999999999999999\n","FILE \"a.bin\" BINARY\nTRACK 01 AUDIO\nINDEX 01\n",
        "FILE \"a.bin\" BINARY\nTRACK 01 AUDIO\nINDEX 01 0\n","FILE \"a.bin\" BINARY\nINDEX 00 00:00:00\n",
        "FILE \"a.bin\" BINARY\nTRACK 01 AUDIO\n","FILE \"a.bin\" BINARY\nFILE \"b.bin\" BINARY\n"};
    char cue[16384];
    if(value<16)strcpy(cue,bad[value]);
    else {
        strcpy(cue,single);
        const char *tail[]={"FILE \"missing.bin\" BINARY\n","TRACK 03 AUDIO\n","INDEX 01 00:00:00\n",
            "TRACK 02 AUDIO\nINDEX 02 00:00:10\n","INDEX 100 00:00:10\n","INDEX 02 00:60:00\n",
            "INDEX 02 00:00:75\n","TRACK 02 AUDIO\nINDEX 01 99:00:00\n","INDEX 03 00:00:10\n"};
        if(value<25)strcat(cue,tail[value-16]);
        else {cue[0]=0;for(int i=1;i<=100;i++){char row[128];snprintf(row,sizeof(row),"FILE \"a.bin\" BINARY\nTRACK %02d AUDIO\nINDEX 01 00:00:00\n",i);strcat(cue,row);}}
    }
    create(cue_name,cue,strlen(cue));assert(parse_cue((char*)cue_name,NULL)!=0);
    assert(!handles_count && !cdb.track_num && !current_index);guards();
    create(cue_name,single,strlen(single));assert(!parse_cue((char*)cue_name,NULL) && cdb.track_num==1);
    assert(!unload_disc() && !handles_count && !cdb.track_num);guards();
}
static void preview(int value)
{
    create(cue_name,single,strlen(single));
    memset(&cdb,0x5a,sizeof(cdb));cdb.track_num=7;current_index=77;
    u8 snapshot[sizeof(cdb)],out[256];memcpy(snapshot,&cdb,sizeof(cdb));
    if(value==1)inject_open=2;
    if(value==2)inject_read=2;
    if(value==3)short_read=2;
    if(value==4)inject_close=2;
    int r=parse_cue((char*)cue_name,out);assert((r==0)==(value==0));
    assert(!handles_count && !memcmp(snapshot,&cdb,sizeof(cdb)) && current_index==77);guards();
    if(!value)for(unsigned i=0;i<sizeof(out);i++)assert(out[i]==(u8)(i+16));
}
static void retry(void)
{
    assert(!handles_count && !cdb.track_num && !current_index);guards();
    inject_open=inject_read=inject_close=short_read=partial_error=0;
    create(cue_name,single,strlen(single));assert(!parse_cue((char*)cue_name,NULL));
    assert(!unload_disc() && !handles_count);guards();
}
static void input(int value)
{
    unsigned bytes=value<3?sizeof(text_space.data)-1+value:strlen(single);
    char *cue=malloc(bytes);assert(cue);memset(cue,' ',bytes);memcpy(cue,single,strlen(single));
    if(value==3)cue[10]=0;
    if(value==9)bytes=0;
    create(cue_name,cue,bytes);free(cue);
    if(value==4)inject_open=1;
    if(value==5)inject_read=1;
    if(value==8)partial_error=1;
    if(value==6)short_read=1;
    if(value==7)inject_close=1;
    int r=parse_cue((char*)cue_name,NULL);assert((r==0)==(value==0));guards();
    if(!r)assert(!unload_disc());retry();
}
static void indices(int count)
{
    char *cue=malloc(65536);assert(cue);unsigned used=0;int remaining=count;
    for(int track=1;remaining;track++) {
        used+=snprintf(cue+used,65536-used,"FILE \"a.bin\" BINARY\nTRACK %02d AUDIO\nINDEX 01 00:00:00\n",track);
        for(int index=2;index<=99 && remaining;index++,remaining--)
            used+=snprintf(cue+used,65536-used,"INDEX %02d 00:%02d:%02d\n",index,(index-1)/75,(index-1)%75);
    }
    create(cue_name,cue,used);free(cue);int r=parse_cue((char*)cue_name,NULL);
    assert((r==0)==(count==2048));
    if(!r){assert(current_index==2048 && cdb.track_num==21);assert(!unload_disc());}
    retry();
}
static void make_parents(char *path)
{
    for(char *p=path+1;*p;p++)if(*p=='/') {
        *p=0;FRESULT r=f_mkdir(path);assert(!r || r==FR_EXIST);*p='/';
    }
}
static void long_path(char *out,unsigned length)
{
    memset(out,'p',length);memcpy(out,"/D/",3);
    for(unsigned i=103;i<length-10;i+=101)out[i]='/';
    memcpy(out+length-4,".bin",4);out[length]=0;make_parents(out);
}
static void path_test(int value)
{
    char cue[2048],name[520],file[520];const char *load=cue_name;strcpy(cue,single);
    u8 data[2352]={0};
    if(value==0){assert(!f_chdir("/D"));load="Game.cue";}
    if(value==1){load="/Game.cue";create("/a.bin",data,sizeof(data));}
    if(value==2){create("/D/a b.bin",data,sizeof(data));strcpy(cue,"FILE \"a b.bin\" BINARY\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n");}
    if(value==3 || value==4) {
        long_path(name,508+value);load=name;
        strcpy(file,name);strcpy(strrchr(file,'/')+1,"a.bin");create(file,data,sizeof(data));
    }
    if(value==5 || value==6) {
        long_path(file,506+value);create(file,data,sizeof(data));
        snprintf(cue,sizeof(cue),"FILE \"%s\" BINARY\nTRACK 01 AUDIO\nINDEX 01 00:00:00\n",file+3);
    }
    create(load,cue,strlen(cue));int r=parse_cue((char*)load,NULL);
    assert((r==0)==(value!=4 && value!=6));
    if(!r)assert(!unload_disc());
    assert(!f_chdir("/"));retry();
}
static void raw(int value)
{
    unsigned bytes=value==0?2048:value==1?2047:value==2?0:value==4?2351:2352;
    u8 data[2352]={0};create("/D/raw.bin",data,bytes);
    if(value==5 || value==6)virtual_raw_size=(FSIZE_t)1<<32;
    if(value==7)inject_open=1;
    if(value>=8){create(cue_name,single,strlen(single));assert(!parse_cue((char*)cue_name,NULL));}
    unsigned old_opens=opens;
    int r=value==3 || value==4 || value==6 || value==9?load_pcm("/D/raw.bin"):parse_iso("/D/raw.bin");
    assert((r==0)==(value==0 || value==3));
    if(value>=8)assert(opens==old_opens && handles_count==1 && cdb.track_num==1);
    if(!r){assert(cdb.tracks[0].fad_start==150 && cdb.tracks[0].fad_end==150 && !cdb.tracks[0].max_index);}
    if(!r || value>=8)assert(!unload_disc());retry();
}
static void cleanup(int value)
{
    char cue[1024];strcpy(cue,single);
    if(value<=2) {
        strcat(cue,"FILE \"b.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n");
        if(value==0)strcat(cue,"INDEX 03 00:00:10\n");
        if(value==1)inject_open=3;
        if(value==2)strcat(cue,"FILE \"missing.bin\" BINARY\nTRACK 03 AUDIO\nINDEX 01 00:00:00\n");
    } else strcat(cue,"TRACK 02 AUDIO\nINDEX 01 00:02:00\nFILE \"b.bin\" BINARY\nTRACK 03 AUDIO\nINDEX 01 00:00:00\n");
    create(cue_name,cue,strlen(cue));int r=parse_cue((char*)cue_name,NULL);
    if(value<=2)assert(r);
    else {
        assert(!r && handles_count==2);
        cdb.play_track=&cdb.tracks[2];
        assert(!cue_open(&cdb.subcode_fp,"/D/b.bin",FA_READ));cdb.has_subrw=1;
        unsigned old_closes=closes;
        if(value==4)inject_close=closes+1;
        if(value==5)inject_close=closes+3;
        if(value==6){assert(!cue_close(cdb.tracks[0].fp) && !cue_close(cdb.tracks[2].fp));
            memset(cdb.tracks,0,sizeof(cdb.tracks));cdb.track_num=0;old_closes=closes;}
        r=unload_disc();assert((r==0)==(value!=4 && value!=5));
        assert(closes==old_closes+(value==6?1:3) && !cdb.play_track && !cdb.has_subrw);
        old_closes=closes;assert(!unload_disc() && closes==old_closes);
    }
    retry();
}
static void track_extent(int value)
{
    u8 data[2352]={0};create("/D/a.bin",data,value==0?0:value==1?2351:2352);
    if(value>=3)virtual_track_size=(FSIZE_t)0xffffffffu+(value==4);
    create(cue_name,single,strlen(single));int r=parse_cue((char*)cue_name,NULL);
    assert((r==0)==(value==2 || value==3));
    if(!r)assert(!unload_disc());
    virtual_track_size=0;create("/D/a.bin",data,sizeof(data));retry();
}
static void indexed_sector(int value)
{
    const char *modes[]={"MODE1/2048","MODE1/2352","MODE2/2048",
                         "MODE2/2336","MODE2/2324","MODE2/2352","AUDIO"};
    const unsigned widths[]={2048,2352,2048,2336,2324,2352,2352};
    unsigned mode=value/6,width=widths[mode],previewing=value%2;
    int tail=value/2%3-1;
    u8 data[4705],output[256];
    for(unsigned i=0;i<sizeof(data);i++)data[i]=(u8)(i*29+i/256);
    create("/D/a.bin",data,2*width+tail);
    char cue[128];snprintf(cue,sizeof(cue),
        "FILE \"a.bin\" BINARY\nTRACK 01 %s\nINDEX 01 00:00:01\n",modes[mode]);
    create(cue_name,cue,strlen(cue));
    memset(output,0xd3,sizeof(output));
    int result=parse_cue((char*)cue_name,previewing?output:NULL);
    assert((result==0)==(tail>=0));
    if(!result && previewing) {
        unsigned prefix=width==2352?(mode==5?24:16):width==2336?8:0;
        assert(!memcmp(output,data+width+prefix,sizeof(output)));
    } else if(!result) {
        assert(cdb.tracks[0].file_offset==width && cdb.tracks[0].fad_start==150);
        assert(cdb.tracks[0].fad_end==150 && !unload_disc());
    } else {
        for(unsigned i=0;i<sizeof(output);i++)assert(output[i]==0xd3);
    }
    assert(!handles_count && !cdb.track_num && !current_index);guards();
}
static void mode_prefix(void)
{
    const char *invalid[]={"", "M", "MO", "MOD", "MODE", "MODE1", "MODE2",
        "MODE1/", "MODE2/", "MODE0/2048", "MODE3/2048", "MODE11/2048",
        "MODE12/2048", "MODE1:2048", "MODE2:2352", "MODE/2048", "MODE /2048",
        "mode1/2048", "MODE1/2324", "MODE1/2336", "MODE2/2353", "MODE2/-2048",
        "MODE2/2048x", "MODE2/2048/", "MODE2/4294967296"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        char cue[160];snprintf(cue,sizeof(cue),
            "FILE \"a.bin\" BINARY\nTRACK 01 %s\nINDEX 01 00:00:00\n",invalid[i]);
        create(cue_name,cue,strlen(cue));
        assert(parse_cue((char*)cue_name,NULL)!=0);
        assert(!handles_count && !cdb.track_num && !current_index);guards();
    }
}
static void dispatch(int value)
{
    create(cue_name,single,strlen(single));u8 data[2048]={0};create("/D/raw.iso",data,sizeof(data));
    selected_format=value==1?2:1;
    if(value==2){assert(!parse_cue((char*)cue_name,NULL));inject_close=closes+1;}
    unsigned old_opens=opens;int r=load_disc(0,0);
    assert((r==0)==(value!=2));
    if(!r) {
        assert(cdb.track_num==1 && cdb.tracks[0].sector_size==(value==1?2048:2352));
        assert(cdb.tracks[0].fad_end==(value==1?150:449));assert(!unload_disc());
    } else assert(opens==old_opens && r==-FR_DISK_ERR);
    retry();
}

/* Adapted from paps-g ec0c0e93. Load generated CUE/ISO through actual FatFs. */
static void classify_media(int value)
{
    u8 bytes[4*2352];
    memset(bytes,value==0 ? 0 : 0x59,sizeof(bytes));
    if (value==2 || value>=5) {
        memcpy(bytes+16,"SEGA SEGASATURN ",16);
        memcpy(bytes+48,"MEDIAGAME0000001",16);
    }
    if (value==6) bytes[48]=0;
    create("/D/a.bin",bytes,sizeof(bytes));
    char cue[256];
    snprintf(cue,sizeof(cue),"FILE \"a.bin\" BINARY\nTRACK 01 %s\nINDEX 01 00:00:00\n",
             value<4 ? "AUDIO" : "MODE1/2352");
    if (value==3 || value==5) {
        strcat(cue,value==3 ? "TRACK 02 MODE1/2352\nINDEX 01 00:00:02\n" :
                              "TRACK 02 AUDIO\nINDEX 01 00:00:02\n");
    }
    create(cue_name,cue,strlen(cue));
    if (value==4) {
        memcpy(bytes+32,"FAKEGAME00000001",16);
        create("/D/raw.iso",bytes,2048);
        selected_format=2;
    }
    media_case=1;
    int result=load_disc(0,1);
    assert(media_pauses==1 && media_config_calls==1);
    assert(cdb.track_num==(value==3 || value==5 ? 2 : 1));
    if (value<5) {
        assert(!result && !media_id[0] && !media_save_calls);
        assert(reads==1); /* Only CUE text or the foreign data header. */
    } else {
        assert(media_save_calls==1 && reads==2);
        if (value==5) assert(!result && !strcmp(media_id,"MEDIAGAME0000001"));
        else assert(result==FR_INVALID_PARAMETER && !media_id[0]);
    }
    assert(!unload_disc() && !handles_count);
    guards();
}
#ifndef MCU_CUE_TEST_ENTRY
#define MCU_CUE_TEST_ENTRY main
#endif
int MCU_CUE_TEST_ENTRY(int argc,char **argv)
{
    assert(argc==4);disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(f_mount(&fs,"0:",1)==0);
    assert(f_mkdir("/D")==0);unsigned size=2352*300;u8 *data=malloc(size);assert(data);
    for(unsigned i=0;i<size;i++)data[i]=i;
    create("/D/a.bin",data,size);create("/D/b.bin",data,size);free(data);
    memset(&text_space,0xa5,sizeof(text_space));memset(&paths,0xa5,sizeof(paths));
    int value=atoi(argv[3]);
    if(!strcmp(argv[2],"valid"))valid(value);
    else if(!strcmp(argv[2],"syntax"))syntax(value);
    else if(!strcmp(argv[2],"input"))input(value);
    else if(!strcmp(argv[2],"indices"))indices(value);
    else if(!strcmp(argv[2],"path"))path_test(value);
    else if(!strcmp(argv[2],"raw"))raw(value);
    else if(!strcmp(argv[2],"track-extent"))track_extent(value);
    else if(!strcmp(argv[2],"indexed-sector"))indexed_sector(value);
    else if(!strcmp(argv[2],"mode-prefix"))mode_prefix();
    else if(!strcmp(argv[2],"cleanup"))cleanup(value);
    else if(!strcmp(argv[2],"dispatch"))dispatch(value);
    else if(!strcmp(argv[2],"media"))classify_media(value);
    else {assert(!strcmp(argv[2],"preview"));preview(value);}
    printf("cue %s %d reads=%u opens=%u closes=%u handles=%u\n",argv[2],value,reads,opens,closes,handles_count);
    assert(!fclose(disk));return 0;
}
