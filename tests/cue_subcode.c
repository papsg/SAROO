/* paps-g659fe4ee optional sidecode regression, actual public I/O/converter. */
#define MCU_CUE_TEST_ENTRY cue_fixture_main
#include "mcu_save_cue.c"
#define STAT_ERROR 9
typedef struct {u8 *data;int size,fad,fn,cn,sm,ci;} BLOCK;
static u8 buffer[0x8000],subcode[1536],subring[1536];
static u8 *sector_buffer=buffer;
static u32 buf_fad_start,buf_fad_end,buf_fad_offset,buf_fad_size;
static u8 buf_subcode_mode;
static int mode_change_read;
static u8 mode_after_read;
static FRESULT sidecode_read(FIL *fp,void *out,UINT size,UINT *count) {
    FRESULT result=cue_read(fp,out,size,count);
    if(mode_change_read && !--mode_change_read)cdb.iflag=mode_after_read;
    return result;
}
#define f_lseek cue_seek
#define f_read sidecode_read
#include "reader.inc"
#undef f_read
#undef f_lseek
static unsigned width[]={2048,2352,2324,2336,2352,2352};
static const char *mode[]={"MODE1/2048","AUDIO","MODE2/2324","MODE2/2336","MODE1/2352","MODE2/2352"};
static u8 byte_at(unsigned tag,unsigned byte) {return (u8)(tag*29+byte*13+(byte>>8));}
static void append_frames(FIL *fp,unsigned size,unsigned count,unsigned tag) {
    u8 frame[2352];UINT n;
    for(unsigned f=0;f<count;f++) {
        for(unsigned b=0;b<size;b++)frame[b]=byte_at(tag+f,b);
        assert(!f_write(fp,frame,size,&n) && n==size);
    }
}
static void start_file(FIL *fp,const char *path) {assert(!f_open(fp,path,FA_CREATE_ALWAYS|FA_WRITE));}
static void load(const char *text) {
    create(cue_name,text,strlen(text));assert(!parse_cue((char*)cue_name,NULL));
    buf_fad_start=buf_fad_end=0;cdb.play_track=NULL;cdb.play_type=PLAYTYPE_SECTOR;
    cdb.subcode_buf=subcode;cdb.subrw_buf=subring;
}
static void expect(unsigned fad,unsigned size,int tag,unsigned track,unsigned index) {
    BLOCK b={0};assert(!get_sector(fad,&b));
    assert(b.fad==(int)fad && b.size==(int)size && cdb.track==(int)track && cdb.index==(int)index);
    assert(cdb.ctrladdr==(int)TRACK_CTRL(&cdb.tracks[track-1]));
    for(unsigned i=0;i<size;i++) {
        u8 want=tag<0?0:byte_at(tag,i);
        if(b.data[i]!=want)fprintf(stderr,"fad=%u byte=%u actual=%u expected=%u\n",fad,i,b.data[i],want);
        assert(b.data[i]==want);
    }
    unsigned old=reads;assert(!get_sector(fad,&b));assert(old==reads);
}
static void subcodes(int kind,int fault) {
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2048,18,1);assert(!f_close(&fp));
    u8 input[18*96];for(unsigned i=0;i<sizeof input;i++)input[i]=(i*17+(i/96)*11)&63;create("/D/a.sub",input,sizeof input);
    load("FILE \"a.bin\" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\n");
    if(fault==-2)expect(150,2048,1,1,1);
    assert(!cue_open(&cdb.subcode_fp,"/D/a.sub",FA_READ));cdb.has_subrw=kind;cdb.iflag=2;
    memset(subcode,0xa5,sizeof subcode);memset(subring,0x5a,sizeof subring);
    if(fault>=0) {
        if(fault==0)inject_seek=seeks+1;
        if(fault==1)clip_seek=seeks+1;
        if(fault==2)inject_read=reads+1;
        if(fault==3)short_read=reads+1;
        if(fault==4)partial_error=reads+1;
        BLOCK b={0};assert(get_sector(151,&b)<0 && !b.data && !buf_fad_end && cdb.status==STAT_ERROR && !cdb.subrw_wp);
        for(unsigned i=0;i<sizeof subring;i++)assert(subring[i]==0x5a);
    } else {
        for(unsigned f=0;f<18;f++) {
            expect(150+f,2048,f+1,1,1);
            if(!f)assert(buf_fad_end==(unsigned)(kind==1?164:166));
            /* Independent planar-to-RW conversion, then packet permutation. */
            static const unsigned perm[24]={0,66,125,191,100,50,150,175,8,33,58,83,108,133,158,183,16,41,25,91,116,141,166,75};
            for(unsigned j=0;j<96;j++) {
                unsigned offset=(150+f-buf_fad_start)*96+j;u8 want=input[f*96+j];
                if(kind==1) {
                    unsigned bit=(j/24)*24+perm[j%24],frame=f+bit/96,within=bit%96;want=0;
                    for(unsigned ch=2;ch<8;ch++)if(frame<18)want|=((input[frame*96+ch*12+within/8]>>(7-within%8))&1)<<(7-ch);
                }
                assert(subcode[offset]==want);
            }
        }
    }
    assert(!unload_disc() && !handles_count);
}
static void subcode_mode_change(int value) {
    int kind=value/3+1,point=value%3;
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2048,18,1);assert(!f_close(&fp));
    u8 input[18*96];for(unsigned i=0;i<sizeof input;i++)input[i]=(i*17+(i/96)*11)&63;create("/D/a.sub",input,sizeof input);
    load("FILE \"a.bin\" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\n");
    assert(!cue_open(&cdb.subcode_fp,"/D/a.sub",FA_READ));cdb.has_subrw=kind;cdb.iflag=point?2:0;
    memset(subcode,0xa5,sizeof subcode);memset(subring,0x5a,sizeof subring);
    unsigned before=reads;mode_change_read=point==2?2:1;mode_after_read=point?0:2;
    BLOCK sector={0};assert(!get_sector(150,&sector) && sector.size==2048 && cdb.index==1);
    for(unsigned i=0;i<2048;i++)assert(sector.data[i]==byte_at(1,i));
    assert(!mode_change_read && cdb.iflag==mode_after_read);
    assert(reads==before+(point?2:1) && cdb.subrw_wp==(point?4:0));
    if(!point)for(unsigned i=0;i<sizeof subring;i++)assert(subring[i]==0x5a);
    assert(!get_sector(151,&sector) && sector.size==2048 && cdb.index==1);
    for(unsigned i=0;i<2048;i++)assert(sector.data[i]==byte_at(2,i));
    assert(reads==before+3 && cdb.subrw_wp==4);
    unsigned frame=point?0:1;
    static const unsigned perm[24]={0,66,125,191,100,50,150,175,8,33,58,83,108,133,158,183,16,41,25,91,116,141,166,75};
    for(unsigned j=0;j<96;j++) {
        u8 want=input[frame*96+j];
        if(kind==1) {
            unsigned bit=(j/24)*24+perm[j%24],f=frame+bit/96,within=bit%96;want=0;
            for(unsigned ch=2;ch<8;ch++)want|=((input[f*96+ch*12+within/8]>>(7-within%8))&1)<<(7-ch);
        }
        assert(subring[j]==want);
    }
    assert(!unload_disc() && !handles_count);
}


static void payload_failure_control(int kind) {
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2048,18,1);assert(!f_close(&fp));
    u8 input[18*96];for(unsigned i=0;i<sizeof input;i++)input[i]=(i*17+(i/96)*11)&63;
    create("/D/a.sub",input,sizeof input);
    load("FILE \"a.bin\" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\n");
    assert(!cue_open(&cdb.subcode_fp,"/D/a.sub",FA_READ));cdb.has_subrw=kind;cdb.iflag=2;
    memset(subring,0x5a,sizeof subring);memset(subcode,0xa5,sizeof subcode);
    inject_read=reads+2;BLOCK block,before;memset(&block,0xa5,sizeof block);memcpy(&before,&block,sizeof block);
    assert(get_sector(150,&block)<0 && !memcmp(&block,&before,sizeof block));
    assert(!buf_fad_end && !cdb.subrw_wp && cdb.status==STAT_ERROR);
    for(unsigned i=0;i<sizeof subring;i++)assert(subring[i]==0x5a);
    inject_read=0;cdb.play_type=PLAYTYPE_SECTOR;
    assert(!get_sector(150,&block) && block.fad==150 && block.size==2048 && cdb.subrw_wp==4);
    for(unsigned i=0;i<2048;i++)assert(block.data[i]==byte_at(1,i));
    assert(!unload_disc() && !handles_count);
}
int main(int argc,char **argv) {
    assert(argc==4);disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(!f_mount(&fs,"0:",1));assert(!f_mkdir("/D"));
    memset(&text_space,0xa5,sizeof text_space);memset(&paths,0xa5,sizeof paths);
    const char *test=argv[2];int value=atoi(argv[3]);
    if(!strcmp(test,"subcode"))subcodes(value,-1);else if(!strcmp(test,"toggle"))subcodes(1,-2);
    else if(!strcmp(test,"mode-change"))subcode_mode_change(value);else if(!strcmp(test,"payload-fault"))payload_failure_control(value);else {assert(!strcmp(test,"fault"));subcodes(1,value);}
    guards();assert(!lock_depth && !fclose(disk));puts("pass: actual optional subcode I/O, mode snapshot and ring bytes");return 0;
}
