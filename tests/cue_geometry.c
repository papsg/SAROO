/* Adapted from paps-g659fe4ee; actual public parser/reader and synthetic media. */
#define MCU_CUE_TEST_ENTRY cue_fixture_main
#include "mcu_save_cue.c"
#define STAT_ERROR 9
/* Production functions use only these BLOCK fields. Buffer ownership is
 * exercised separately by the actual CDBLOCK/filter fixture. */
typedef struct {u8 *data;int size,fad,fn,cn,sm,ci;} BLOCK;
static u8 buffer[0x8000],subcode[1536],subring[1536];
static u8 *sector_buffer=buffer;
static u32 buf_fad_start,buf_fad_end,buf_fad_offset,buf_fad_size;
static u8 buf_subcode_mode;
static FSIZE_t large_position;
static int large_io,large_clip,mode_change_read;
static u8 mode_after_read;
static FRESULT reader_seek(FIL *fp,FSIZE_t offset) {
    if(!large_io)return cue_seek(fp,offset);
    assert(lock_depth==1);large_position=offset;fp->fptr=large_clip?0:offset;return FR_OK;
}
static FRESULT reader_read(FIL *fp,void *out,UINT size,UINT *count) {
    if(!large_io) {
        FRESULT result=cue_read(fp,out,size,count);
        if(mode_change_read && !--mode_change_read)cdb.iflag=mode_after_read;
        return result;
    }
    assert(lock_depth==1 && fp->fptr==large_position);
    memset(out,0x73,size);*count=size;return FR_OK;
}
#define f_lseek reader_seek
#define f_read reader_read
#include "reader.inc"
#undef f_lseek
#undef f_read

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
static void boundary(int value) {
    unsigned a=value,b=(value+1)%6;FIL fp;start_file(&fp,"/D/a.bin");
    append_frames(&fp,width[a],7,1);append_frames(&fp,width[b],5,8);append_frames(&fp,2352,4,13);assert(!f_close(&fp));
    assert(!cue_open(&track_fp[0],"/D/a.bin",FA_READ));
    cdb.track_num=3;unsigned first=150,offset=0;
    const unsigned sizes[]={width[a],width[b],2352},lengths[]={7,5,4};
    for(unsigned t=0;t<3;t++) {
        TRACK_INFO *track=&cdb.tracks[t];
        *track=(TRACK_INFO){.fp=&track_fp[0],.fad_0=first,.fad_start=first,.fad_end=first+lengths[t]-1,.file_offset=offset,.max_index=1};
        test_track_size(track,sizes[t]);test_track_mode(track,t==2?3:(t?b:a)==1?3:(t?b:a)==0||(t?b:a)==4?1:2);
        first+=lengths[t];offset+=lengths[t]*sizes[t];
    }
    static int index=164;cdb.tracks[2].max_index=2;test_track_indices(&cdb.tracks[2],&index,0);
    cdb.play_track=NULL;cdb.play_type=PLAYTYPE_SECTOR;cdb.subcode_buf=subcode;cdb.subrw_buf=subring;
    for(unsigned pass=0;pass<3;pass++)for(unsigned i=0;i<16;i++) {
        unsigned n=pass==0?i:pass==1?15-i:(i*7)%16;
        expect(150+n,n<7?width[a]:n<12?width[b]:2352,n+1,n<7?1:n<12?2:3,n>=14?2:1);
    }
    assert(!unload_disc());
}
static void reports(void) {
    TRACK_INFO *track=&cdb.tracks[0];static int indices[98];
    for(unsigned i=0;i<98;i++)indices[i]=151+i;
    *track=(TRACK_INFO){.fad_0=147,.fad_start=150,.fad_end=260,.max_index=99};
    test_track_size(track,2352);test_track_mode(track,3);test_track_indices(track,indices,0);
    cdb.track_num=1;cdb.play_track=NULL;
    for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<114;i++) {
        unsigned fad=pass?260-i:147+i;
        assert(!get_sector(fad,NULL));
        unsigned expected=fad<150?0:fad<249?fad-149:99;
        assert(cdb.track==1 && cdb.index==(int)expected && cdb.ctrladdr==1 && !reads && !seeks);
    }
}
static void large(void) {
    FIL fp={0};cdb.track_num=1;cdb.tracks[0]=(TRACK_INFO){.fp=&fp,.fad_0=150,.fad_start=150,.fad_end=151,.file_offset=0x80000211u};test_track_size(&cdb.tracks[0],2352);test_track_mode(&cdb.tracks[0],3);
    large_io=1;BLOCK b={0};assert(!get_sector(150,&b) && large_position==0x80000200u && b.data==buffer+17);
    for(unsigned i=0;i<2352;i++)assert(b.data[i]==0x73);
    buf_fad_end=0;large_clip=1;b.data=NULL;assert(get_sector(151,&b)<0 && !buf_fad_end && !b.data);
}
static void generated_gaps(void) {
#ifdef TRACK_INDEX
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2352,8,1);assert(!f_close(&fp));
    assert(!cue_open(&track_fp[0],"/D/a.bin",FA_READ));cdb.track_num=1;
    cdb.tracks[0]=(TRACK_INFO){.fp=&track_fp[0],.fad_0=150,.fad_start=155,.fad_end=162,.file_offset=2*2352,.pregap=3,.postgap=2,.max_index=1};
    test_track_size(&cdb.tracks[0],2352);test_track_mode(&cdb.tracks[0],3);
    cdb.play_type=PLAYTYPE_SECTOR;cdb.play_track=NULL;
    for(unsigned pass=0;pass<3;pass++)for(unsigned i=0;i<13;i++) {
        unsigned n=pass==0?i:pass==1?12-i:(i*5)%13;
        unsigned before=reads;expect(150+n,2352,n<3||n>10?-1:(int)n-2,1,n<5?0:1);
        if(n<3||n>10)assert(reads==before);
    }
    assert(!unload_disc());
#else
    assert(!"Generated gaps require the new bounded metadata representation");
#endif
}
static void unchanged_control(void) {
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2352,8,1);assert(!f_close(&fp));
    assert(!cue_open(&track_fp[0],"/D/a.bin",FA_READ));cdb.track_num=1;
    cdb.tracks[0]=(TRACK_INFO){.fp=&track_fp[0],.fad_0=150,.fad_start=150,.fad_end=157,.max_index=1};
    test_track_size(&cdb.tracks[0],2352);test_track_mode(&cdb.tracks[0],3);
    cdb.play_type=PLAYTYPE_SECTOR;cdb.play_track=NULL;
    for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<8;i++) {
        unsigned n=pass?7-i:i;expect(150+n,2352,n+1,1,1);
    }
    assert(reads==1 && !unload_disc());
}
int main(int argc,char **argv) {
    assert(argc==4);disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(!f_mount(&fs,"0:",1));assert(!f_mkdir("/D"));
    memset(&text_space,0xa5,sizeof text_space);memset(&paths,0xa5,sizeof paths);
    int value=atoi(argv[3]);const char *name=argv[2];
    if(!strcmp(name,"boundary"))boundary(value);else if(!strcmp(name,"reports"))reports();
    else if(!strcmp(name,"large"))large();else if(!strcmp(name,"control"))unchanged_control();else {assert(!strcmp(name,"generated"));generated_gaps();}
    guards();assert(!lock_depth && !fclose(disk));puts("pass: actual reader, independent metadata/bytes and cache boundaries");return 0;
}
