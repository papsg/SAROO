/* Adapted from paps-g cb3abf06; actual public parser/TOC/FatFs. */
#define MCU_CUE_TEST_ENTRY cue_fixture_main
#include "mcu_save_cue.c"
static const struct { const char *name;unsigned size,prefix; } formats[]={
    {"MODE1/2048",2048,0},{"MODE1/2352",2352,16},{"MODE2/2048",2048,0},
    {"MODE2/2324",2324,0},{"MODE2/2336",2336,8},{"MODE2/2352",2352,24}
};
static u8 expected[256];
static unsigned file_offset,prefix;
static void make_header(int format,int offset)
{
    unsigned size=formats[format].size;prefix=formats[format].prefix;file_offset=offset?2*size:0;
    for(unsigned i=0;i<256;i++)expected[i]=i*13+7;
    memcpy(expected,"SEGA SEGASATURN ",16);memcpy(expected+32,"OFFSETID00000001",16);
    u8 *data=malloc(6*size);assert(data);memset(data,0x5c,6*size);
    memcpy(data+prefix+32,"WRONGKEY00000001",16);memcpy(data+file_offset+prefix,expected,256);
    create("/D/a.bin",data,6*size);free(data);
    char cue[256];snprintf(cue,sizeof(cue),"FILE \"a.bin\" BINARY\nTRACK 01 %s\nINDEX 00 00:00:00\nINDEX 01 00:00:%02d\n",formats[format].name,offset?2:0);
    create(cue_name,cue,strlen(cue));
    u8 iso[2048]={0};memcpy(iso,expected,sizeof(expected));create("/D/raw.iso",iso,sizeof(iso));
}
static void full_header(int value)
{
    make_header(value/2,value%2);assert(!parse_cue((char*)cue_name,NULL));
    /* The identity must not depend on where an earlier operation left the FIL. */
    assert(!f_lseek(cdb.tracks[0].fp,997));
    u8 snapshot[sizeof(cdb)];memcpy(snapshot,&cdb,sizeof(cdb));
    char id[17];memset(id,0xcc,sizeof(id));unsigned old_reads=reads;
    assert(!get_gameid(id) && !memcmp(id,expected+32,16) && !id[16]);
    assert(reads==old_reads+1 && handles_count==1);
    assert(!memcmp(snapshot,&cdb,sizeof(cdb)));assert(!unload_disc() && !handles_count);guards();
}
static void preview_header(int value)
{
    make_header(value/2,value%2);memset(&cdb,0x5a,sizeof(cdb));cdb.track_num=7;current_index=77;
    u8 snapshot[sizeof(cdb)],out[256];memcpy(snapshot,&cdb,sizeof(cdb));memset(out,0xcc,256);
    assert(!get_disc_ip(0,out) && !memcmp(out,expected,256));
    assert(!memcmp(snapshot,&cdb,sizeof(cdb)) && current_index==77 && !handles_count);
    assert(allocations==1 && frees==1 && reads==2);guards();
}
static void full_fault(int value)
{
    make_header(1,1);assert(!parse_cue((char*)cue_name,NULL));
    FIL *fp=cdb.tracks[0].fp;assert(!f_lseek(fp,file_offset));
    if(value==1)inject_seek=seeks+1;
    if(value==2)clip_seek=seeks+1;
    if(value==3)inject_read=reads+1;
    if(value==4)partial_error=reads+1;
    if(value==5)short_read=reads+1;
    if(value==6)fp->obj.objsize=file_offset+prefix+255;
    if(value==7)disk_read_fault=1;
    if(value==8)cdb.tracks[0].file_offset=0xfffffff8u;
    if(value==9)cdb.tracks[0].file_offset=f_size(fp)+1;
    u8 snapshot[sizeof(cdb)];memcpy(snapshot,&cdb,sizeof(cdb));char id[17];memset(id,0xcc,17);
    int r=get_gameid(id);assert((r==0)==(value==0));
    if(r)for(unsigned i=0;i<17;i++)assert((u8)id[i]==0xcc);
    else assert(!memcmp(id,expected+32,16) && !id[16]);
    assert(!memcmp(snapshot,&cdb,sizeof(cdb)) && handles_count==1 && !disk_read_fault);
    assert(!unload_disc() && !handles_count);guards();
}
static void full_end_boundary(void)
{
    _Static_assert(sizeof(FSIZE_t) == (FF_FS_EXFAT ? 8 : 4), "FatFs file-offset width");
    make_header(1,1);
    u8 data[3*2352];memset(data,0x5c,sizeof(data));
    memcpy(data+file_offset+prefix,expected,sizeof(expected));
    create("/D/a.bin",data,sizeof(data));
    assert(!parse_cue((char*)cue_name,NULL));
    assert(cdb.tracks[0].file_offset==2*2352 && f_size(cdb.tracks[0].fp)==3*2352);
    char id[17];memset(id,0xcc,sizeof(id));
    assert(!get_gameid(id) && !memcmp(id,expected+32,16) && !id[16]);
    assert(!unload_disc() && !handles_count);guards();
}
static void preview_fault(int value,int iso)
{
    make_header(1,1);selected_format=iso?2:1;
    if(value==1)fail_alloc=1;
    if(value==2)inject_open=1;
    if(value==3)inject_read=1;
    if(value==4)short_read=1;
    if(value==5)inject_close=1;
    if(value==6)inject_open=2;
    if(value==7)inject_seek=1;
    if(value==8)clip_seek=1;
    if(value==9)inject_read=iso?1:2;
    if(value==10)short_read=iso?1:2;
    if(value==11)partial_error=iso?1:2;
    if(value==12)inject_close=iso?1:2;
    if(value==13){u8 data[255]={0};create(iso?"/D/raw.iso":"/D/a.bin",data,sizeof(data));}
    if(value==14)selected_format=0;
    if(value==15){const char *bad="FILE \"a.bin\" BINARY\nTRACK 01 MODE1/2352\n";create(cue_name,bad,strlen(bad));}
    memset(&cdb,0x5a,sizeof(cdb));cdb.track_num=7;current_index=77;
    u8 snapshot[sizeof(cdb)],out[256];memcpy(snapshot,&cdb,sizeof(cdb));memset(out,0xcc,256);
    int r=get_disc_ip(0,out);assert(value?r<0:r==0);
    if(!value)assert(!memcmp(out,expected,256));
    assert(!memcmp(snapshot,&cdb,sizeof(cdb)) && current_index==77 && !handles_count);
    assert(allocations==1 && frees==(unsigned)(value!=1));guards();
}
static void load_fault(int value)
{
    make_header(1,1);selected_format=value==1 || value==3?2:1;
    if(value<2)inject_read=selected_format==1?2:1;
    if(value==2)partial_error=2;
    if(value==3)short_read=1;
    if(value==4)clip_seek=1;
    assert(load_disc(0,0)<0 && !handles_count && !cdb.track_num && !current_index && cdb.status==STAT_NODISC);
    assert(allocations==1 && frees==1);guards();
}
int main(int argc,char **argv)
{
    assert(argc==4);disk=fopen(argv[1],"r+b");assert(disk);FATFS fs;assert(!f_mount(&fs,"0:",1) && !f_mkdir("/D"));
    memset(&text_space,0xa5,sizeof(text_space));memset(&paths,0xa5,sizeof(paths));int value=atoi(argv[3]);
    if(!strcmp(argv[2],"header-full"))full_header(value);
    else if(!strcmp(argv[2],"header-boundary"))full_end_boundary();
    else if(!strcmp(argv[2],"header-preview"))preview_header(value);
    else if(!strcmp(argv[2],"header-fault"))full_fault(value);
    else if(!strcmp(argv[2],"cue-preview-fault"))preview_fault(value,0);
    else if(!strcmp(argv[2],"load-header-fault"))load_fault(value);
    else {assert(!strcmp(argv[2],"iso-preview-fault"));preview_fault(value,1);}
    printf("header %s %d reads=%u seeks=%u opens=%u closes=%u allocations=%u frees=%u\n",argv[2],value,reads,seeks,opens,closes,allocations,frees);
    assert(!fclose(disk));return 0;
}
