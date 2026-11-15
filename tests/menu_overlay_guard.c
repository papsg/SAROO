/* Host checks for the actual loaded-file guard. No game bytes are included. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static u8 ram[0x100000], reference[sizeof ram], descriptor[24];
static u32 header_load;
static unsigned reads, writes;
static int skip_patch;
#define SYSINFO_ADDR 0x02400000u

static u8 *pointer(uintptr_t address)
{
    if(address == SYSINFO_ADDR + 0x40) return descriptor;
    assert(address >= 0x26002000 && address < 0x26100000);
    ++reads;
    return ram + (address - 0x26000000);
}
static u16 read16(u32 address)
{
    assert(address >= 0x06002000 && address <= 0x060ffffe);
    return (u16)ram[address-0x06000000] << 8 | ram[address-0x06000000+1];
}
static void write16(u32 address, u16 value)
{
    assert(address >= 0x06002000 && address <= 0x060ffffe);
    ++writes;
    ram[address-0x06000000] = value >> 8;
    ram[address-0x06000000+1] = value;
}
#define SD_GUARD_POINTER(a) pointer(a)
#define SD_GUARD_READ16(a) read16(a)
#define SD_GUARD_WRITE16(a,v) write16(a,v)
#define SD_GUARD_FIRST_LOAD() header_load
#include "crc32.inc"
#include "guard.inc"

static void put32(u8 *p, u32 value)
{
    for(unsigned i=0;i<4;++i) p[i] = value >> (24-8*i);
}
static void seal(void) { put32(descriptor+20, crc32(descriptor,20,0)); }
static void reset(void)
{
    for(unsigned i=0;i<sizeof ram;++i) ram[i] = (i*17+31) & 255;
    header_load = 0x06010000;
    write16(header_load+102,9);
    put32(descriptor, header_load); put32(descriptor+4,6144);
    put32(descriptor+8,crc32(ram+0x10000,6144,0)); put32(descriptor+12,102);
    descriptor[16]=0x22; descriptor[17]=0x12; descriptor[18]=0; descriptor[19]=9;
    seal(); reads=writes=skip_patch=0;
    memcpy(reference,ram,sizeof ram);
}
int main(int argc, char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"valid")) {
        for(unsigned mode=0;mode<=4;++mode) if(mode==0 || mode==1 || mode==4) {
            reset(); assert(sd_overlay_checked_action(0x534f5780|mode)==(0x534f5680|mode));
            assert(reads==1 && !writes && !memcmp(ram,reference,sizeof ram));
        }
    } else if(!strcmp(argv[1],"mismatch")) {
        const unsigned positions[]={0,101,104,6143};
        for(unsigned i=0;i<4;++i) {
            reset();ram[0x10000+positions[i]]^=1;reference[0x10000+positions[i]]^=1;
            reference[0x10000+102]=0x22;reference[0x10000+103]=0x12;
            assert(sd_overlay_checked_action(0x534f5780)==0);
            assert(reads==1 && writes==1 && !memcmp(ram,reference,sizeof ram));
        }
        reset();write16(header_load+102,0x1234);memcpy(reference,ram,sizeof ram);writes=0;
        assert(sd_overlay_checked_action(0x534f5780)==0);
        assert(!reads && !writes && !memcmp(ram,reference,sizeof ram));
    } else if(!strcmp(argv[1],"metadata")) {
        for(unsigned i=0;i<24;++i) {
            reset();descriptor[i]^=1;
            assert(sd_overlay_checked_action(0x534f5780)==UINT32_MAX);
            assert(!reads && !writes && !memcmp(ram,reference,sizeof ram));
        }
        reset();header_load+=4;
        assert(sd_overlay_checked_action(0x534f5780)==UINT32_MAX && !reads && !writes);
        reset();descriptor[16]=0;descriptor[17]=9;seal();
        assert(sd_overlay_checked_action(0x534f5780)==UINT32_MAX && !reads && !writes);
    } else if(!strcmp(argv[1],"bounds")) {
        const u32 loads[]={0,0x06001ffc,0x06002000,0x06002001,0x060ffffc,0x06100000,UINT32_MAX};
        const u32 lengths[]={0,1,2,3,4096,0xfe000,0x100001,UINT32_MAX};
        const u32 offsets[]={0,1,2,4094,4096,0xfdffe,UINT32_MAX};
        unsigned count=0;
        for(unsigned l=0;l<7;++l) for(unsigned n=0;n<8;++n) for(unsigned o=0;o<7;++o) {
            reset();u32 load=loads[l],length=lengths[n],offset=offsets[o];
            int valid=!(load&3) && length>=2 && load>=0x06002000 &&
                (uint64_t)load+length<=0x06100000 && !(offset&1) && (uint64_t)offset+2<=length;
            put32(descriptor,load);put32(descriptor+4,length);put32(descriptor+12,offset);seal();header_load=load;
            u32 result=sd_overlay_checked_action(0x534f5780);
            assert((result!=UINT32_MAX)==valid);
            if(!valid) assert(!reads && !writes);
            ++count;
        }
        assert(count==392);
    } else if(!strcmp(argv[1],"skip")) {
        reset();skip_patch=1;reference[0x10000+102]=0x22;reference[0x10000+103]=0x12;
        assert(sd_overlay_checked_action(0x534f5780)==0);
        assert(!reads && writes==1 && !memcmp(ram,reference,sizeof ram));
    } else {
        assert(!strcmp(argv[1],"actions"));
        const u32 legacy[]={0,0x534f5680,0x534f5604,0xdeadbeef};
        for(unsigned i=0;i<4;++i) {reset();assert(sd_overlay_checked_action(legacy[i])==legacy[i] && !reads && !writes);}
        for(unsigned mode=0;mode<128;++mode) if(mode!=0 && mode!=1 && mode!=4) {
            reset();assert(sd_overlay_checked_action(0x534f5780|mode)==UINT32_MAX && !reads && !writes);
        }
    }
    puts("actual loaded-file guard controls passed");
    return 0;
}
