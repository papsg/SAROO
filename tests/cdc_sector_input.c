/* MCU-CDIO-001: paps-g selected8e465233; independent seek/read/cache oracles. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"
static CDBLOCK cdb;
_Alignas(8) static u8 cache[32768],image[65536];
static u8 *sector_buffer=cache;
static u32 buf_fad_start,buf_fad_end,buf_fad_offset,buf_fad_size;
static u8 buf_subcode_mode;
int index_table[2048];
static FIL file;
static TRACK_INFO tracks[2];
static FRESULT seek_result,read_result;
static unsigned supplied,seeks,reads,locks,unlocks,lookups;
static FSIZE_t sought;
static int locked,lookup_result;
#define SSLOG(...) ((void)0)
static int fad_to_track(u32 fad) { assert(fad>=150);lookups++;return lookup_result; }
static void subcode_convert(void) { assert(0); }
static void fs_lock(void) { assert(!locked);locked=1;locks++; }
static void fs_unlock(void) { assert(locked);locked=0;unlocks++; }
FRESULT f_lseek(FIL *fp,FSIZE_t offset)
{
    assert(locked && fp==&file);seeks++;sought=offset;if(!seek_result)fp->fptr=offset;return seek_result;
}
FRESULT f_read(FIL *fp,void *data,UINT count,UINT *actual)
{
    assert(locked && fp==&file && data==cache && count==sizeof cache && supplied<=count);
    assert(sought+supplied<=sizeof image);
    reads++;memcpy(data,image+(size_t)sought,supplied);*actual=supplied;return read_result;
}
#include "sector-input.inc"

static void setup(unsigned size,unsigned offset,unsigned mode)
{
    memset(&cdb,0,sizeof cdb);memset(cache,0xa5,sizeof cache);
    for(unsigned i=0;i<sizeof image;i++)image[i]=(u8)(i*13+offset);
    image[offset+15]=2;image[offset+16]=7;image[offset+17]=9;
    image[offset+18]=0x28;image[offset+19]=0x11;
    if(size==2336) { image[offset]=7;image[offset+1]=9;image[offset+2]=0x28;image[offset+3]=0x11; }
    tracks[0]=(TRACK_INFO){.fp=&file,.mode=mode,.sector_size=size,.fad_0=150,.fad_start=150,.fad_end=400,.file_offset=offset,.max_index=1};
    cdb.play_track=&tracks[0];cdb.tracks=tracks;cdb.track_num=1;
    cdb.play_type=PLAYTYPE_FILE;cdb.status=STAT_PLAY;
    seek_result=read_result=FR_OK;supplied=sizeof cache;
    seeks=reads=locks=unlocks=lookups=0;locked=0;lookup_result=255;sought=0;
    buf_fad_start=buf_fad_end=buf_fad_offset=buf_fad_size=0;
}
static void failed(BLOCK *block,const BLOCK *before)
{
    assert(!memcmp(block,before,sizeof *block) && !buf_fad_end && cdb.status==STAT_ERROR);
    assert(!cdb.play_type && !locked && locks==unlocks && locks==1);
}
static void seek_errors(void)
{
    const unsigned sizes[]={2048,2324,2336,2352},offsets[]={0,1,256,511};
    for(unsigned size=0;size<4;size++)for(unsigned offset=0;offset<4;offset++)
    for(unsigned error=1;error<=FR_INVALID_PARAMETER;error++) {
        setup(sizes[size],offsets[offset],size==0?1:2);seek_result=(FRESULT)error;
        BLOCK block,before;memset(&block,0x5a,sizeof block);memcpy(&before,&block,sizeof before);
        assert(get_sector(150,&block)==-2 && seeks==1 && !reads);failed(&block,&before);
    }
}
static void short_reads(void)
{
    const unsigned sizes[]={2048,2324,2336,2352};
    for(unsigned size=0;size<4;size++)for(unsigned offset=0;offset<512;offset++)
    for(unsigned edge=0;edge<4;edge++) {
        setup(sizes[size],offset,size==0?1:2);
        supplied=edge==0?0:edge==1?(offset?offset-1:0):edge==2?offset:offset+sizes[size]-1;
        BLOCK block,before;memset(&block,0x5a,sizeof block);memcpy(&before,&block,sizeof before);
        assert(get_sector(150,&block)==-2 && seeks==1 && reads==1);failed(&block,&before);
    }
}
static void exact_and_full_reads(void)
{
    const unsigned sizes[]={2048,2324,2336,2352};
    for(unsigned size=0;size<4;size++)for(unsigned offset=0;offset<512;offset++)
    for(unsigned edge=0;edge<3;edge++) {
        setup(sizes[size],offset,size==0?1:2);cdb.play_type=edge==2?PLAYTYPE_DIR:PLAYTYPE_FILE;
        supplied=edge==0?offset+sizes[size]:edge==1?offset+sizes[size]+1:sizeof cache;
        BLOCK block;memset(&block,0x5a,sizeof block);
        assert(!get_sector(150,&block) && seeks==1 && reads==1 && locks==1 && unlocks==1 && !locked);
        assert(sought==0 && block.data==cache+offset && block.size==(int)sizes[size] && block.fad==150);
        assert(buf_fad_start==150 && buf_fad_end==150+(supplied-offset)/sizes[size]);
        assert(!memcmp(block.data,image+offset,sizes[size]));
        for(unsigned i=supplied;i<sizeof cache;i++)assert(cache[i]==0xa5);
        assert(cdb.play_type==(edge==2?PLAYTYPE_DIR:PLAYTYPE_FILE) && cdb.status==STAT_PLAY);
    }
}
static void cache_boundaries_and_recovery(void)
{
    const unsigned sizes[]={2048,2324,2336,2352};
    for(unsigned size=0;size<4;size++)for(unsigned offset=0;offset<512;offset+=511) {
        setup(sizes[size],offset,size==0?1:2);BLOCK block;
        assert(!get_sector(150,&block));unsigned end=buf_fad_end;
        for(unsigned fad=150;fad<end;fad++) {
            assert(!get_sector(fad,&block) && seeks==1 && reads==1);
            assert(block.data==cache+offset+(fad-150)*sizes[size] && block.fad==fad);
            assert(!memcmp(block.data,image+offset+(fad-150)*sizes[size],sizes[size]));
        }
        assert(!get_sector(end,&block) && seeks==2 && reads==2);
        unsigned position=offset+(end-150)*sizes[size];
        assert(sought==(position&~511u) && block.data==cache+(position&511));
        assert(!memcmp(block.data,image+position,sizes[size]));
        setup(sizes[size],offset,size==0?1:2);read_result=FR_DISK_ERR;
        memset(&block,0x5a,sizeof block);BLOCK before;memcpy(&before,&block,sizeof before);
        assert(get_sector(150,&block)==-2);failed(&block,&before);
        read_result=FR_OK;cdb.play_type=PLAYTYPE_FILE;cdb.status=STAT_PLAY;
        assert(!get_sector(150,&block) && seeks==2 && reads==2 && block.data==cache+offset);
        assert(locks==2 && unlocks==2 && !locked && block.fad==150);
    }
}
static void track_and_metadata_controls(void)
{
    const unsigned sizes[]={2048,2324,2336,2352,2352},modes[]={1,2,2,2,3};
    for(unsigned format=0;format<5;format++) {
        setup(sizes[format],511,modes[format]);BLOCK block;
        assert(!get_sector(150,&block));int raw_mode2=format==2 || format==3;
        assert(block.fn==(raw_mode2?7:0) && block.cn==(raw_mode2?9:0));
        assert(block.sm==(raw_mode2?0x28:format==1?0x20:0) && block.ci==(raw_mode2?0x11:0));
        assert(block.fad==150 && !memcmp(block.data,image+511,sizes[format]));
    }
    setup(2048,0,1);assert(!get_sector(150,NULL) && !locks && !reads && !seeks);
    cdb.play_track=NULL;BLOCK block,before;memset(&block,0x5a,sizeof block);memcpy(&before,&block,sizeof before);
    assert(get_sector(150,&block)==-1 && lookups==1 && !locks && !reads && !seeks);
    assert(!memcmp(&block,&before,sizeof block) && !cdb.play_type && cdb.status==STAT_ERROR);
    setup(2048,0,1);tracks[1]=tracks[0];cdb.track_num=2;cdb.play_track=NULL;lookup_result=2;
    assert(!get_sector(150,NULL) && cdb.play_track==&tracks[1] && lookups==1 && !reads);
}
static void read_error_controls(void)
{
    const unsigned sizes[]={2048,2324,2336,2352},offsets[]={0,1,511};
    for(unsigned size=0;size<4;size++)for(unsigned offset=0;offset<3;offset++)
    for(unsigned error=1;error<=FR_INVALID_PARAMETER;error++) {
        setup(sizes[size],offsets[offset],size==0?1:2);read_result=(FRESULT)error;
        BLOCK block,before;memset(&block,0x5a,sizeof block);memcpy(&before,&block,sizeof before);
        assert(get_sector(150,&block)==-2 && reads==1 && seeks==1);failed(&block,&before);
    }
}
static void directory_error_ownership(void)
{
    for(unsigned kind=0;kind<4;kind++) {
        setup(2352,511,2);cdb.play_type=PLAYTYPE_DIR;
        if(kind==0) seek_result=FR_DISK_ERR;
        if(kind==1) read_result=FR_DISK_ERR;
        if(kind==2) supplied=511+2352-1;
        if(kind==3) cdb.play_track=NULL;
        BLOCK block,before;memset(&block,0x5a,sizeof block);memcpy(&before,&block,sizeof before);
        assert(get_sector(150,&block)<0 && cdb.play_type==PLAYTYPE_DIR && cdb.status==STAT_ERROR);
        assert(!memcmp(&block,&before,sizeof block) && !buf_fad_end && !locked);
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned test=(unsigned)atoi(argv[1]);assert(test<7);
    if(test==0)seek_errors();else if(test==1)short_reads();else if(test==2)exact_and_full_reads();
    else if(test==3)cache_boundaries_and_recovery();else if(test==4)track_and_metadata_controls();
    else if(test==5) read_error_controls();else directory_error_ownership();puts("pass: actual sector-input error, cache, payload and valid controls");return 0;
}
