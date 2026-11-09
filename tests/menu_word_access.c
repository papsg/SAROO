/* Actual helpers; independent word/byte oracle adapted from paps-g 4df2e67a. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "words.inc"
static unsigned checks;
static void check(u32 value,int big)
{
    for(unsigned offset=0;offset<4;offset++) {
        u8 bytes[12];memset(bytes,0xa5,sizeof bytes);
        LE32W(bytes+4+offset,value);
        for(unsigned i=0;i<4;i++)assert(bytes[4+offset+i]==(u8)(value>>(i*8)));
        for(unsigned i=0;i<sizeof bytes;i++)if(i<4+offset || i>=8+offset)assert(bytes[i]==0xa5);
        assert(LE16(bytes+4+offset)==(value&0xffff));
        u32 result=big?BE32(bytes+4+offset):LE32(bytes+4+offset);
        u32 expected=big?((value&0xff)<<24)|((value&0xff00)<<8)|((value>>8)&0xff00)|(value>>24):value;
        assert(result==expected); ++checks;
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);int big=!strncmp(argv[1],"big",3),low=strstr(argv[1],"low")!=NULL;
    const u32 special[]={0,1,127,128,255,0x12345678,0x80000000,0xffffffff};
    if (low) { check(0,big); check(1,big); check(127,big); check(0x12345678,big); }
    else for(unsigned i=0;i<sizeof special/sizeof special[0];i++)check(special[i],big);
    for(unsigned byte=0;byte<(low?128u:256u);byte++)for(unsigned position=0;position<4;position++)check((u32)byte<<(position*8),big);
    printf("pass: %u word checks\n",checks);return 0;
}
