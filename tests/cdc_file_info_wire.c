/* SPDX-License-Identifier: GPL-3.0-only
 * paps-g, SSR-037 (acf616d6); public source adaptation by papsg. */
/* Actual producer, native types, full transfer and public query bodies.
 * FIFO/SH-2 endianness and command acknowledgement are explicit model seams. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "metadata.inc"
#include "native-types.inc"
_Static_assert(sizeof(int)==4 && sizeof(CdcFile)==12, "Native 32-bit file ABI");
_Static_assert(offsetof(CdcFile,unit)==8 && offsetof(CdcFile,gap)==9 &&
    offsetof(CdcFile,fn)==10 && offsetof(CdcFile,attr)==11, "Native file byte offsets");
static FILE_INFO files[256];
static struct { FILE_INFO *fi; } cdb={files};
static u16 fifo[256*6];
static unsigned written, consumed, reads32, commands, ends;
static unsigned query_start, query_count, query_fid;
static volatile u16 *next_word(void) { assert(written<256*6); return &fifo[written++]; }
#define FIFO_DATA (*next_word())
static u32 bswap32(u32 n) { return __builtin_bswap32(n); }
#include "producer.inc"
static void produce(unsigned start,unsigned count)
{
    assert(start+count<=256);
    written=consumed=reads32=0;fill_file_info_fifo(start,count);assert(written==count*6);
}
static u16 bus16(void)
{
    assert(consumed<written);u16 q=fifo[consumed++];
    /* Exact inspected SSMaster ss_fifo_data_out concatenation. */
    return (u16)((q<<8)|(q>>8));
}
static u32 host_register(u32 address)
{
    assert(address==0x25818000);reads32++;
    u32 high=bus16(),low=bus16(),value=high<<16|low;
    /* Preserve SH-2 memory representation in both actual C store branches.
     * This adapts only the host register boundary, not metadata fields. */
    const u16 endian=1;
    return *(const u8*)&endian ? bswap32(value) : value;
}
#define REG(address) host_register(address)
enum { HIRQ_DRDY=2 };
static int cdc_cmd(int wait,CDCMD *cmd,CDCMD *resp,char *name)
{
    assert(wait==HIRQ_DRDY && !strcmp(name,"cdc_get_file_info"));commands++;
    assert(cmd->cr1==0x7300 && !cmd->cr2 &&
        cmd->cr3==(query_fid>>16) && cmd->cr4==(query_fid&0xffff));
    produce(query_start,query_count);*resp=(CDCMD){0x4100,(u16)written,0,0};return 0;
}
static int cdc_end_trans(int *count)
{
    assert(!count && consumed==written && reads32*2==written);ends++;return 0;
}
#include "consumer.inc"
static u32 big32(const u8 *p) { return (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|p[3]; }
static int matches(const u8 *p,const FILE_INFO *fi)
{
    assert(big32(p)==fi->lba+150 && big32(p+4)==fi->size);
    assert(p[offsetof(CdcFile,fn)]==fi->fid && p[offsetof(CdcFile,attr)]==(fi->attr&0xfa));
    return p[offsetof(CdcFile,unit)]==fi->unit && p[offsetof(CdcFile,gap)]==fi->gap;
}
static int one(unsigned unit,unsigned gap,int verbose)
{
    files[0]=(FILE_INFO){.lba=100,.size=8192,.unit=unit,.gap=gap,.fid=37,.attr=0x29};
    _Alignas(4) u8 output[24];unsigned bad=0;
    produce(0,1);
    /* Sega's cached _CDREG_TgetData disassembly copies native words as-is.
     * This is a reference store model, not execution of the vendor binary. */
    for(unsigned i=0;i<6;i++){u16 word=bus16();output[i*2]=word>>8;output[i*2+1]=word;}
    bad+=!matches(output,files);
    for(unsigned offset=0;offset<4;offset++) {
        memset(output,0xa5,sizeof output);
        if(!offset) {
            query_start=0;query_count=1;query_fid=2;commands=ends=0;
            assert(!cdc_get_file_info(2,(CdcFile*)(void*)(output+4)));
            assert(commands==1 && ends==1);
        } else {
            produce(0,1);cdc_trans_data(output+4+offset,12);
        }
        assert(consumed==written && reads32==3);
        for(unsigned i=0;i<4+offset;i++)assert(output[i]==0xa5);
        for(unsigned i=16+offset;i<sizeof output;i++)assert(output[i]==0xa5);
        bad+=!matches(output+4+offset,files);
    }
    if(verbose)printf("unit=%u gap=%u fifo=%04x native_unit=%u native_gap=%u mismatched_paths=%u\n",
        unit,gap,fifo[4],output[7+8],output[7+9],bad);
    return bad!=0;
}
static unsigned batches(void)
{
    unsigned cases=0,bad=0,records=0;
    for(unsigned count=1;count<=254;count++) {
        for(unsigned i=0;i<256;i++)files[i]=(FILE_INFO){
            .lba=0x123400+i*37,.size=0xf1234500u+i*8191,
            .unit=(u8)(1+i%255),.gap=(u8)(255-i),.fid=i,.attr=i};
        query_count=count;query_start=2;query_fid=0xffffff;
        _Alignas(4) u8 output[256*12+8];memset(output,0xa5,sizeof output);
        commands=ends=0;
        assert(!cdc_get_file_info(0xffffff,(CdcFile*)(void*)(output+4)));
        assert(commands==1 && ends==1 && consumed==count*6 && reads32==count*3);
        for(unsigned i=0;i<4;i++)assert(output[i]==0xa5);
        for(unsigned i=4+count*12;i<sizeof output;i++)assert(output[i]==0xa5);
        for(unsigned i=0;i<count;i++){records++;bad+=!matches(output+4+i*12,&files[i+2]);}
        cases++;
    }
    printf("batch counts=%u records=%u mismatches=%u %s\n",cases,records,bad,bad?"FAIL":"pass");
    return bad!=0;
}
static unsigned literal(void)
{
    files[0]=(FILE_INFO){.lba=100,.size=8192,.unit=2,.gap=3,.fid=37,.attr=0x29};
    const u8 expected[12]={0,0,0,250,0,0,32,0,2,3,37,40};
    unsigned bad=0;
    for(unsigned offset=0;offset<4;offset++) {
        _Alignas(4) u8 output[24];memset(output,0xa5,sizeof output);
        produce(0,1);cdc_trans_data(output+4+offset,12);
        bad+=memcmp(output+4+offset,expected,sizeof expected)!=0;
        for(unsigned i=0;i<4+offset;i++)assert(output[i]==0xa5);
        for(unsigned i=16+offset;i<sizeof output;i++)assert(output[i]==0xa5);
    }
    printf("literal 12-byte native record: %u mismatches %s\n",bad,bad?"FAIL":"pass");
    return bad!=0;
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned total=0,bad=0;
    if(!strcmp(argv[1],"distinct")){total=1;bad=one(2,3,1);}
    else if(!strcmp(argv[1],"equal"))for(unsigned i=0;i<256;i++){total++;bad+=one(i,i,0);}
    else if(!strcmp(argv[1],"domain")){
        for(unsigned unit=1;unit<256;unit++)for(unsigned gap=0;gap<256;gap++){
            total++;bad+=one(unit,gap,0);
        }
        total++;bad+=one(0,0,0);
    } else if(!strcmp(argv[1],"batch"))return batches()?42:0;
    else {assert(!strcmp(argv[1],"literal"));return literal()?42:0;}
    printf("metadata pairs=%u paths=%u mismatched_pairs=%u %s\n",total,total*5,bad,bad?"FAIL":"pass");
    return bad?42:0;
}
