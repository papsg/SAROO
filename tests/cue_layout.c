/* paps-g659fe4ee CUE geometry, adapted to the actual public parser/reader. */
#define MCU_CUE_TEST_ENTRY cue_fixture_main
#include "mcu_save_cue.c"
#define STAT_ERROR 9
typedef struct {u8 *data;int size,fad,fn,cn,sm,ci;} BLOCK;
static u8 buffer[0x8000],subcode[1536],subring[1536];
static u8 *sector_buffer=buffer;
static u32 buf_fad_start,buf_fad_end,buf_fad_offset,buf_fad_size;
static u8 buf_subcode_mode;
#include "reader.inc"
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
static void mixed(int value) {
    unsigned a=value,b=(value+1)%6;FIL fp;start_file(&fp,"/D/a.bin");
    append_frames(&fp,width[a],7,1);append_frames(&fp,width[b],5,8);append_frames(&fp,2352,4,13);assert(!f_close(&fp));
    char cue[512];snprintf(cue,sizeof cue,"FILE \"a.bin\" BINARY\nTRACK 01 %s\nINDEX 01 00:00:00\nTRACK 02 %s\nINDEX 01 00:00:07\nTRACK 03 AUDIO\nINDEX 01 00:00:12\nINDEX 02 00:00:14\n",mode[a],mode[b]);load(cue);
    assert(cdb.tracks[1].file_offset==7*width[a] && cdb.tracks[2].file_offset==7*width[a]+5*width[b]);
    assert(cdb.tracks[0].fad_end==156 && cdb.tracks[1].fad_end==161 && cdb.tracks[2].fad_end==165);
    for(unsigned pass=0;pass<3;pass++)for(unsigned i=0;i<16;i++) {
        unsigned n=pass==0?i:pass==1?15-i:(i*7)%16;
        expect(150+n,n<7?width[a]:n<12?width[b]:2352,n+1,n<7?1:n<12?2:3,n>=14?2:1);
    }
    assert(!unload_disc());
}
static void gaps(void) {
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2048,4,1);append_frames(&fp,2352,7,5);assert(!f_close(&fp));
    start_file(&fp,"/D/b.bin");append_frames(&fp,2352,6,21);assert(!f_close(&fp));
    load("FILE \"a.bin\" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\nPOSTGAP 00:00:02\nTRACK 02 AUDIO\nPREGAP 00:00:03\nINDEX 00 00:00:04\nINDEX 01 00:00:06\nINDEX 02 00:00:07\nFILE \"b.bin\" BINARY\nTRACK 03 MODE1/2352\nPREGAP 00:00:01\nINDEX 01 00:00:02\nPOSTGAP 00:00:03\n");
    assert(cdb.tracks[0].fad_end==155 && cdb.tracks[1].fad_0==156 && cdb.tracks[1].fad_start==161);
    assert(cdb.tracks[1].fad_end==165 && cdb.tracks[2].fad_0==166 && cdb.tracks[2].fad_start==167 && cdb.tracks[2].fad_end==173);
    assert((bswap32(cdb.TOC[101])&0xffffff)==174);
    for(unsigned pass=0;pass<3;pass++)for(unsigned i=0;i<24;i++) {
        unsigned n=pass==0?i:pass==1?23-i:(i*7)%24;
        int tag=n<4?(int)n+1:n<9?-1:n<16?(int)n-4:n==16?-1:n<21?(int)n+6:-1;
        unsigned size=n<6?2048:n<16?2352:n==16||n>20?2048:2352;
        expect(150+n,size,tag,n<6?1:n<16?2:3,n>=6&&n<11?0:n==16?0:n>=12&&n<16?2:1);
    }
    assert(!unload_disc());
}
static void first(int value) {
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2352,value+4,1);assert(!f_close(&fp));
    char cue[256];snprintf(cue,sizeof cue,"FILE \"a.bin\" BINARY\nTRACK 01 AUDIO\nPREGAP 00:00:03\nINDEX 00 00:00:00\nINDEX 01 %02d:%02d:%02d\n",value/4500,value/75%60,value%75);load(cue);
    assert(cdb.tracks[0].fad_start==150 && cdb.tracks[0].fad_end==153 && cdb.tracks[0].file_offset==(unsigned)value*2352);
    for(unsigned f=150;f<154;f++)expect(f,2352,value+1+f-150,1,1);
    if(value)expect(149,2352,value,1,0);
    if(value<150)expect(149-value,2352,-1,1,0);
    assert(!unload_disc());
}
static void tracks(void) {
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2352,99,1);assert(!f_close(&fp));
    char text[8192]="FILE \"a.bin\" BINARY\n",line[80];
    for(unsigned i=0;i<99;i++){snprintf(line,sizeof line,"TRACK %02u AUDIO\nINDEX 01 00:%02u:%02u\n",i+1,i/75,i%75);strcat(text,line);}load(text);
    for(unsigned i=0;i<99;i++) {
        assert(get_track_info((i+1)<<8)==&cdb.tracks[i]);assert(track_to_fad(((i+1)<<8)|1)==150+i);
        expect(150+i,2352,i+1,i+1,1);
    }
    assert(!get_track_info(100<<8) && get_track_info(0xffff)==&cdb.tracks[98]);assert(!unload_disc());
}
static void bad_gap(int value) {
    const char *tail[]={"PREGAP 00:00:01\nINDEX 01 00:00:00\nPREGAP 00:00:02\n",
      "POSTGAP 00:00:01\nINDEX 01 00:00:00\n","PREGAP 00:00:01\nPREGAP 00:00:01\nINDEX 01 00:00:00\n",
      "INDEX 01 00:00:00\nPOSTGAP 00:00:01\nPOSTGAP 00:00:01\n", "INDEX 01 00:00:00\nPOSTGAP 00:00:01\nINDEX 02 00:00:01\n",
      "PREGAP -1:00:00\nINDEX 01 00:00:00\n", "PREGAP 00:60:00\nINDEX 01 00:00:00\n", "INDEX 01 00:00:00\nPOSTGAP 999999:00:00\n",
      "INDEX 01 00:00:00\nPOSTGAP 3728:16:00\n", "INDEX 00 00:00:00\nINDEX 00 00:00:00\nINDEX 01 00:00:01\n",
      "INDEX 01 00:00:00\nPOSTGAP 3728:59:74\n", "PREGAP 3728:59:74\nINDEX 01 00:00:00\n",
      "INDEX 01 00:00:00\nINDEX 02 00:00:03\nTRACK 02 AUDIO\nINDEX 01 00:00:02\n"};
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2352,4,1);assert(!f_close(&fp));
    char text[512]="FILE \"a.bin\" BINARY\nTRACK 01 AUDIO\n";strcat(text,tail[value]);
    create(cue_name,text,strlen(text));assert(parse_cue((char*)cue_name,NULL)<0 && !cdb.track_num && !handles_count);
}
static void gap_pool(int value) {
    FIL fp;start_file(&fp,"/D/a.bin");append_frames(&fp,2352,100,1);assert(!f_close(&fp));
    if(value==4) {
        const char *cue="FILE \"a.bin\" BINARY\nTRACK 01 AUDIO\nPREGAP 00:00:03\nINDEX 00 00:00:00\nINDEX 01 00:00:02\n";
        create(cue_name,cue,strlen(cue));current_index=2047;
        for(unsigned i=0;i<2048;i++)index_table[i]=i*13;
        u8 out[256];assert(!parse_cue((char*)cue_name,out) && current_index==2047 && !handles_count);
        for(unsigned i=0;i<2048;i++)assert(index_table[i]==(int)i*13);
        for(unsigned i=0;i<256;i++)assert(out[i]==byte_at(3,i+16));
        return;
    }
    char *cue=malloc(60000);assert(cue);strcpy(cue,"FILE \"a.bin\" BINARY\n");char row[100];
    unsigned remaining=2046+value,tracks=0;
    while(remaining) {
        unsigned sub=remaining>98?98:remaining,base=tracks*100;tracks++;
        snprintf(row,sizeof row,"TRACK %02u AUDIO\n",tracks);strcat(cue,row);
        for(unsigned i=0;i<=sub;i++) {
            unsigned f=base+i;snprintf(row,sizeof row,"INDEX %02u %02u:%02u:%02u\n",i+1,f/4500,f/75%60,f%75);strcat(cue,row);
        }
        remaining-=sub;
    }
    strcat(cue,"POSTGAP 00:00:03\n");virtual_track_size=tracks*100*2352;
    create(cue_name,cue,strlen(cue));free(cue);int result=parse_cue((char*)cue_name,NULL);
    if(value==3)assert(result<0 && !current_index && !handles_count && !cdb.track_num);
    else {
        assert(!result && current_index==2046+value && cdb.track_num==(int)tracks);
        assert(cdb.tracks[tracks-1].fad_end==152+tracks*100);
        for(unsigned t=0;t<tracks;t++) for(unsigned n=100;n--;) {
            unsigned count=(2046+value)-t*98;if(count>98)count=98;
            unsigned expected=n<=count?n+1:count+1;
            assert(!get_sector(150+t*100+n,NULL));assert(cdb.index==(int)expected);
            if(expected>1)assert(track_to_fad(((t+1)<<8)|expected)==150+t*100+expected-1);
        }
        assert(!unload_disc());
    }
}
int main(int argc,char **argv) {
    assert(argc==4);disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(!f_mount(&fs,"0:",1));assert(!f_mkdir("/D"));
    memset(&text_space,0xa5,sizeof text_space);memset(&paths,0xa5,sizeof paths);
    int value=atoi(argv[3]);const char *test=argv[2];
    if(!strcmp(test,"mixed"))mixed(value);else if(!strcmp(test,"gaps"))gaps();else if(!strcmp(test,"first"))first(value);
    else if(!strcmp(test,"tracks"))tracks();else if(!strcmp(test,"bad-gap"))bad_gap(value);else {assert(!strcmp(test,"gap-pool"));gap_pool(value);}
    guards();assert(!lock_depth && !fclose(disk));puts("pass: actual CUE parser, reader, TOC and full index pool");return 0;
}
