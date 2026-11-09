/* Actual loader with synthetic payloads; adapted from paps-g 4df2e67a. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
_Alignas(4) static u8 test_catalog[0x23000], payload[0x1000];
#define IMGINFO_ADDR ((uintptr_t)test_catalog)
static void put32(void *ptr,u32 v)
{ u8 *p=ptr;for(unsigned i=0;i<4;i++)p[i]=(u8)(v>>(i*8)); }
#define printk(...) ((void)0)
static unsigned long number(const char *s,char **end,int base,void *unused)
{ assert(!unused);return strtoul(s,end,base); }
#define strtoul number
static int reads, jumps, returned=-7, pad_suspends, pad_resumes;
static u32 expected_addr=0x06004000, expected_capacity=0xfc000, jump_offset;
static char expected[128];
static void *binary_buffer(u32 addr) { assert(addr==expected_addr);return payload; }
static void pad_suspend(void) { ++pad_suspends; }
static void pad_resume(void) { assert(pad_suspends==pad_resumes+1); ++pad_resumes; }
static void jumped(void) { assert(pad_suspends==1 && pad_resumes==0); ++jumps; }
static void (*binary_jump(u32 address))(void)
{ assert(address==expected_addr+jump_offset);return jumped; }
static int read_file(const char *name,int off,int count,void *out)
{ reads++;assert(!off && !count && out==payload && !strcmp(name,expected));return returned; }
static int read_file_bounded(const char *name,int off,int count,void *out,u32 capacity)
{ assert(capacity==expected_capacity);return read_file(name,off,count,out); }
#include "binary.inc"
int main(int argc,char **argv)
{
    assert(argc==2);int value=atoi(argv[1]),index=0,run=1;
    test_catalog[0]=1;test_catalog[5]=0x28;
    char *name=(char*)test_catalog+0x2800;strcpy(name,"Demo.bin");
    int valid=1, expected_result=-7, expected_jumps=0;
    if(value<=3){valid=0;if(value==0)test_catalog[0]=0;else index=value==1?-1:value==2?1:0x7fffffff;}
    if(value>=4 && value<=6){int len=value==4?116:value==5?117:255;memset(name,'x',len);name[len]=0;valid=value==4;}
    if(value==8){strcpy(name,"Demo@06005000.bin");expected_addr=0x06005000;expected_capacity=0xfb000;}
    if(value==9){put32(test_catalog,2560);valid=0;}
    if(value==10){put32(test_catalog+4,4);valid=0;}
    if(value==11){put32(test_catalog+4,0x23000);valid=0;}
    if(value==12){put32(test_catalog+4,0x22fff);test_catalog[0x22fff]='x';valid=0;}
    const char *bad[] = {"Demo@.bin","Demo@106004000.bin","Demo@06004001.bin",
        "Demo@06000000.bin","Demo@02004000.bin","Demo@02401000.bin",
        "Demo@06100000.bin","Demo@e6004000.bin","Demo@06004000oops.bin"};
    if(value>=13 && value<=21){strcpy(name,bad[value-13]);valid=0;}
    if(value==22){strcpy(name,"Demo@0x06004000.bin");}
    if(value==23){strcpy(name,"Demo@00200000.bin");expected_addr=0x00200000;expected_capacity=0x100000;}
    if(value==24){strcpy(name,"Demo@020c7100.bin");expected_addr=0x020c7100;expected_capacity=0x338f00;}
    if(value==25){strcpy(name,"Demo@26004000.bin");expected_addr=0x26004000;}
    if(value==26){strcpy(name,"Demo@02800000.bin");expected_addr=0x02800000;expected_capacity=0x800000;}
    if(value==36){put32(test_catalog,0x80000000);valid=0;}
    if(value==37){put32(test_catalog+4,0xffffffff);valid=0;}
    if(value==38){put32(test_catalog,0xffffffff);valid=0;}
    if(value==39){put32(test_catalog+4,0xfffffff8);valid=0;}
    if(value>=27 && value<=35){
        const int sizes[]={0,1,2,8,0xf01,0xf02,4,0,1};returned=sizes[value-27];
        expected_result=-1;
        if(value>=30 && value<=32){memcpy(payload,"SEGA SEG",8);jump_offset=0xf00;}
        if(value==29 || value==32 || value==33){expected_result=0;expected_jumps=1;}
        if(value==34 || value==35){run=0;expected_result=0;}
    }
    if(valid)assert(snprintf(expected,sizeof(expected),"/SAROO/BIN/%s",name)<(int)sizeof(expected));
    int result=run_binary(index,run);
    if (!(result==(valid?expected_result:-1) && reads==valid && jumps==expected_jumps)) fprintf(stderr,"case %d: result %d/%d reads %d/%d jumps %d/%d\n",value,result,valid?expected_result:-1,reads,valid,jumps,expected_jumps);
    assert(result==(valid?expected_result:-1) && reads==valid && jumps==expected_jumps);
    assert(pad_suspends==expected_jumps && pad_resumes==expected_jumps);
    return 0;
}
