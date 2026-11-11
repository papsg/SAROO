/* Adapted from paps-g 08226bbe. File/save dispatch and capability checks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "protocol.h"
#include "invariants.inc"
static u16 command_value, argument, file_ack, save_ack;
static u32 sysinfo[1024];
static char scratch[256];
static int result, calls, operation, supplied;
static unsigned checks;
#define SS_CMD command_value
#define SS_ARG argument
#define SYSINFO_ADDR ((uintptr_t)sysinfo)
#define TMPBUFF_ADDR ((uintptr_t)scratch)
#define SSLOG(...) ((void)0)
static int ss_file_transfer(int writing)
{ calls++; operation=writing ? SSCMD_FILEWR : SSCMD_FILERD; return result; }
static int list_bins(int first)
{ assert(first==1); calls++; operation=SSCMD_LISTBIN; return result; }
static int flush_savefile(void) { calls++; operation=SSCMD_SSAVE; return result; }
static int load_savefile(char *name)
{ assert(name==scratch+16); calls++; operation=SSCMD_LSAVE; return result; }
static int load_smems(int id)
{ calls++; operation=SSCMD_LMEMS; supplied=id; return result; }
static int flush_smems(int flags)
{ calls++; operation=SSCMD_SMEMS; supplied=flags; return result; }
#include "command.inc"
static void publish(void)
{
#include "save-publish.inc"
#include "file-publish.inc"
}
#include "le32.inc"
static void latch(void)
{
#include "save-latch.inc"
#include "file-latch.inc"
}
static void capability(void)
{
    memset(sysinfo,0,sizeof sysinfo);file_ack=save_ack=123;latch();
    assert(!file_ack && !save_ack);checks++;
    publish();latch();assert(file_ack==SSCMD_SAVE_ACK && save_ack==SSCMD_SAVE_ACK);
    static const u8 bytes[]={0x31,0x56,0x41,0x53,0x31,0x4c,0x49,0x46};
    assert(!memcmp((u8*)sysinfo+16,bytes,sizeof bytes));
    for(unsigned i=0;i<sizeof sysinfo;i++)if(i<16 || i>=24)assert(!((u8*)sysinfo)[i]);
    checks++;
    for(unsigned bit=0;bit<32;bit++) {
        publish();sysinfo[5]^=1u<<bit;latch();assert(!file_ack && save_ack==SSCMD_SAVE_ACK);
        publish();sysinfo[4]^=1u<<bit;latch();assert(file_ack==SSCMD_SAVE_ACK && !save_ack);
        checks+=2;
    }
    publish();sysinfo[5]=0;latch(); /* Existing save-aware MCU, legacy files. */
    assert(!file_ack && save_ack==SSCMD_SAVE_ACK);checks++;
    publish();latch();memset(sysinfo,0,sizeof sysinfo);
    /* Reset cannot silently renegotiate either in-flight caller. */
    assert(file_ack==SSCMD_SAVE_ACK && save_ack==SSCMD_SAVE_ACK);checks++;
}
static u32 normalize(u32 cmd)
{
#include "normalizer.inc"
    return save_cmd;
}

static unsigned expected_opcode(unsigned opcode)
{
    /* Independent oracle: six exact aliases, not a duplicate range expression. */
    switch(opcode) {
    case 0x26: return 6; case 0x27: return 7;
    case 0x29: return 9; case 0x2a: return 10;
    case 0x2b: return 11; case 0x2c: return 12;
    default: return opcode;
    }
}
static void check_command(unsigned opcode, int error)
{
    unsigned base=expected_opcode(opcode);
    assert(normalize(opcode)==base);
    command_value=opcode;argument=0xa5c3;calls=0;result=error;supplied=-1;
    command();
    if(base>=6 && base<=12) {
        assert(!command_value && calls==1 && (unsigned)operation==base);
        unsigned tag=opcode & SSCMD_SAVE_ACK;
        int expected=base<8 ? (error ? -error : (int)tag) : base==8 ? error : error|tag;
        assert(argument==(u16)expected);
        if(base==11 || base==12)assert(supplied==0xa5c3);
    } else assert(!calls && command_value==opcode && argument==0xa5c3);
    checks++;
}
int main(int argc,char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"capability"))capability();
    else if(!strcmp(argv[1],"dispatch"))
        for(unsigned opcode=0;opcode<=UINT16_MAX;opcode++)check_command(opcode,0);
    else {
        assert(!strcmp(argv[1],"errors"));
        for(unsigned tag=0;tag<=32;tag+=32)
            for(unsigned opcode=6;opcode<=12;opcode++)
                for(int error=0;error<=FR_INVALID_PARAMETER;error++)check_command(opcode|tag,error);
    }
    printf("PASS %s %u checks\n",argv[1],checks);
}
