/* Adapted from paps-g 8078e501. Modeled SH7604 registers and cache. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
static u8 memory[0x1000000];
static struct { u32 address; int valid; u8 bytes[16]; } lines[4][64];
static u32 status, initial_status, cpu_address, physical, requested, changed;
static unsigned commands, line_writes, whole_purges, ccr_writes, status_writes;
static int outcome, pending_ccr, finished, expected_commands;
static u8 control, ccr_value, initial_control;
static volatile u32 purge_value;
static volatile uint16_t command_value, argument;
#define TMPBUFF_ADDR ((uintptr_t)(memory+0xc7000))
#include "protocol.inc"
static uint16_t file_ack;
static uint16_t archive_ack __attribute__((unused));
static uint16_t ram_control = 0xb000u, initial_ram_control;
#define SS_CTRL ram_control

/* Independent model of the Saturn-side FPGA address decoder. */
static u32 memory_offset(u32 address)
{
    u32 offset = address & 0xffffffu;
    if ((ram_control & 0x3000u) == 0x2000u && (offset >> 22) == 1)
        offset &= ~0x180000u;
    return offset;
}

static void flush_ccr(void)
{
    if (!pending_ccr) return;
    pending_ccr=0;
    assert((status & 0xf0)==0xf0); /* Only the short CCR sequence masks IRQs. */
    assert(!(ccr_value & 0x20));
    assert(!(control & 1) || !(ccr_value & 1)); /* Disable before changing mode. */
    if (ccr_value & 0x10) {
        assert(!(control & 1) && !(ccr_value & 1));
        ++whole_purges;
        for(unsigned w=0;w<4;w++)for(unsigned e=0;e<64;e++)lines[w][e].valid=0;
    }
    control=ccr_value & ~0x10;
    ++ccr_writes;
}
static volatile u8 *control_register(u32 address)
{
    assert(address==0xfffffe92u && (!expected_commands || finished));
    flush_ccr();ccr_value=control;pending_ccr=1;
    return &ccr_value;
}
static u8 cache_control_read(u32 address)
{ assert(address==0xfffffe92u);flush_ccr();return control; }
static u32 cache_sr_read(void) { return status; }
static void cache_sr_write(u32 value)
{
    if (pending_ccr) flush_ccr();
    status=value;++status_writes;
}
static volatile u32 *purge_register(u32 address)
{
    assert((address & 0xf000000f)==0x40000000u);
    assert(!expected_commands || finished);
    assert(status==initial_status);
    u32 tag=address & 0x1ffffff0u;
    for(unsigned w=0;w<4;w++)
        if(lines[w][(tag>>4)&63].address==tag)lines[w][(tag>>4)&63].valid=0;
    ++line_writes;return &purge_value;
}
#define REG32(address) (*purge_register(address))
#define REG8(address) (*control_register(address))
static u32 host_bus_address(void *buffer)
{
    uintptr_t address=(uintptr_t)buffer, base=(uintptr_t)memory;
    if (address==base+(physical-0x02000000u)) return cpu_address;
    if (address>=base && address-base<sizeof(memory))
        return 0x02000000u+(u32)(address-base);
    return buffer ? 0x06004000 : 0;
}
static void store_le(void *p,u32 value)
{ for(unsigned i=0;i<4;i++)((u8*)p)[i]=(u8)(value>>(8*i)); }
static u8 pattern(u32 address) { return (u8)(address*29+(address>>8)+3); }
static volatile uint16_t *command_register(void)
{
    if(command_value) {
        ++commands;assert(!finished);
        /* Outcome 10 leaves the request count, while 11 writes a short prefix. */
        changed=outcome==10 || outcome==2 ? 0 : outcome==1 ? 17 :
                outcome==11 || (outcome>=3 && outcome<=6) ? 13 : requested;
        if(changed>requested)changed=requested;
        if((command_value & ~SSCMD_SAVE_ACK)==SSCMD_FILERD)
            for(u32 i=0;i<changed;i++)memory[memory_offset(physical+i)]=pattern(physical+i);
        else { assert(outcome==7);changed=0; }
        store_le((void*)(TMPBUFF_ADDR+4),outcome==3 ? 0 : outcome==4 ? requested+1 :
                 outcome==5 ? 0x80000000u : outcome==6 ? 0xffffffffu :
                 outcome==7 || outcome==10 ? requested : changed);
        argument=outcome==3 ? (uint16_t)-1 :
                 outcome>=10 ? 0 : command_value & SSCMD_SAVE_ACK;
        if(outcome==12)ram_control=0x0100; /* Reset changes the FPGA mode before reply. */
        command_value=0;finished=1;
    }
    return &command_value;
}
#define SS_CMD (*command_register())
#define SS_ARG argument
#define menu_command_wait() 0
#include "cache.inc"

static u8 cache_read(u32 address, int instruction)
{
    unsigned e=(address>>4)&63;
    if(control & 1) {
        for(unsigned w=0;w<4;w++)if(lines[w][e].valid && lines[w][e].address==(address&~15u))
            return lines[w][e].bytes[address&15];
        if(!(control & (instruction?2:4))) {
            unsigned w=3;
            lines[w][e].address=address&~15u;lines[w][e].valid=1;
            memcpy(lines[w][e].bytes,memory+memory_offset(address&~15u),16);
        }
    }
    return memory[memory_offset(address)];
}

int main(int argc,char **argv)
{
    assert(argc==8);
    initial_ram_control=ram_control=(uint16_t)strtoul(argv[7],NULL,0);
    int mode=atoi(argv[1]);cpu_address=strtoul(argv[2],NULL,0);
    file_ack=atoi(argv[6]);assert(!file_ack || file_ack==SSCMD_SAVE_ACK);
    physical=cpu_address & 0x1fffffff;requested=strtoul(argv[3],NULL,0);
    initial_control=control=(u8)strtoul(argv[4],NULL,0);outcome=atoi(argv[5]);
    assert(!(control&0x30) && physical>=0x02000000 && requested<=0x03000000-physical);
    initial_status=status=0x301u|((control&15u)<<4);
    memset(memory,0x55,sizeof(memory));
    for(unsigned w=0;w<4;w++)for(unsigned e=0;e<64;e++) {
        u32 tag=(w==0 ? physical : w==1 ? physical+requested : w==2 ? 0x02200000u : 0x02c00000u);
        if((ram_control&0x3000)==0x2000 && physical>=0x02400000 && physical<0x02800000)
            tag=(physical&~0x180000u)+w*0x80000u;
        tag=(tag&~1023u)+16*e;if(tag>=0x03000000)tag-=1024;
        lines[w][e].address=tag;lines[w][e].valid=1;
        memcpy(lines[w][e].bytes,memory+memory_offset(tag),16);
    }
    if(mode==1) {
        expected_commands=outcome<8 || outcome>=10;
        int result=file_command(outcome==8?NULL:"/cache.bin",0,
                                outcome==9 ? 0x1000000u : requested,
                                memory+(physical-0x02000000),outcome==7);
        int expected=outcome==1 ? (int)changed : outcome==2 ? 0 : outcome==3 || outcome>=10 ? -1 :
                     outcome>=4 && outcome<=6 ? -19 : outcome>=8 ? -19 : (int)requested;
        if(result!=expected)
            fprintf(stderr,"cache outcome %d returned %d, expected %d\n",outcome,result,expected);
        assert(result==expected && commands==(unsigned)expected_commands);
    } else {
        changed=requested;
        for(u32 i=0;i<changed;i++)memory[memory_offset(physical+i)]=pattern(physical+i);
        test_cache_purge(physical,requested);
    }
    flush_ccr();
    assert(control==initial_control && status==initial_status);
    assert(!whole_purges || (whole_purges==1 && ccr_writes==3 && status_writes==2));
    assert(whole_purges || (!ccr_writes && !status_writes));
    u32 invalidated=mode==1 ? (outcome>=7 && outcome<=9 ? 0 : outcome>=3 ? requested : changed) : requested;
    for(unsigned w=0;w<4;w++)for(unsigned e=0;e<64;e++) {
        u32 tag=lines[w][e].address;
        int overlaps=invalidated && tag<physical+invalidated && tag+16>physical;
        if(overlaps)assert(!lines[w][e].valid);
        else if(!whole_purges)assert(lines[w][e].valid);
        if(invalidated && !whole_purges)assert(line_writes==((physical&15)+invalidated+15)/16);
    }
    if(mode==2)assert(line_writes<=256 && ccr_writes<=3);
    if(!invalidated)assert(!line_writes && !whole_purges);
    for(u32 i=0;i<changed && i<8192;i++) {
        assert(cache_read(physical+i,0)==memory[memory_offset(physical+i)]);
        assert(cache_read(physical+i,1)==memory[memory_offset(physical+i)]);
    }
    if((ram_control&0x3000)==0x2000 && physical>=0x02400000 && physical<0x02800000 &&
       changed<=0x80000u-(physical&0x7ffffu))
        for(unsigned alias=0;alias<4;alias++)for(u32 i=0;i<changed && i<8192;i++) {
            u32 address=(physical&~0x180000u)+alias*0x80000u+i;
            assert(cache_read(address,0)==memory[memory_offset(address)]);
            assert(cache_read(address,1)==memory[memory_offset(address)]);
        }
    if(mode==1 && invalidated && invalidated+(physical&15u)<=4096) {
        int mirrored=(initial_ram_control&0x3000)==0x2000 && physical<0x02800000 &&
                     physical+invalidated>0x02400000;
        assert(whole_purges==(unsigned)mirrored);
    }
    puts("Actual file completion and cache invalidation checks pass.");
    return 0;
}
