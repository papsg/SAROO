/* Adapted from paps-g be30cf11; actual public run_binary with I/O stopped before payload access. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
_Alignas(4) static u8 catalog[0x23000];
#define IMGINFO_ADDR ((uintptr_t)catalog)
static int le32(const void *ptr)
{ const u8 *p=ptr;return (int)((u32)p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24); }
#define LE32(ptr) le32(ptr)
#define printk(...) ((void)0)
static unsigned long number(const char *s,char **end,int base,void *unused)
{ assert(!unused);return strtoul(s,end,base); }
#define strtoul number
static int reads,expected_addr;
static char expected[128];
static int read_file(const char *name,int off,int count,void *out)
{
    reads++;assert(!off && !count && !strcmp(name,expected));
    assert((uintptr_t)out==(uintptr_t)expected_addr);return -7;
}
static void pad_suspend(void) { assert(0); }
static void pad_resume(void) { assert(0); }
#include "binary.inc"
int main(int argc,char **argv)
{
    assert(argc==2);int value=atoi(argv[1]),index=0;
    catalog[0]=1;catalog[4]=0;catalog[5]=0x28;
    char *name=(char*)catalog+0x2800;strcpy(name,"Demo.bin");expected_addr=0x06004000;
    if(value==0)catalog[0]=0; /* Stale name remains after an empty scan. */
    if(value==1)index=-1;
    if(value==2)index=1;
    if(value==3)index=0x7fffffff;
    if(value>=4 && value<=6){int len=value==4?116:value==5?117:255;memset(name,'x',len);name[len]=0;}
    if(value==8){strcpy(name,"Demo@06005000.bin");expected_addr=0x06005000;}
    int valid=value==4 || value==7 || value==8;
    if(valid)assert(snprintf(expected,sizeof(expected),"/SAROO/BIN/%s",name)<(int)sizeof(expected));
    int result=run_binary(index,1);
    assert(result==(valid?-7:-1) && reads==valid);
    return 0;
}
