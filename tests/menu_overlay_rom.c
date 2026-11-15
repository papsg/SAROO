/* paps-g 3fafb587 adapted to the public transport; modeled memory/I/O only. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "constants.inc"
static _Alignas(4) u8 cart[0x400000], high[0x100000], low[0x100000];
static u8 original_cart[sizeof cart];
static int no_game_patches;
static int skip_patch, need_bup, reads, cards, starts, count, metadata_crcs, payload_crcs;
static int expected_virtual = 1;
static u32 resume;
static void *memory(uintptr_t address)
{
    address &= 0x1fffffff;
    if (address >= 0x02000000 && address < 0x02400000) return cart+(address-0x02000000);
    if (address >= 0x06000000 && address < 0x06100000) return high+(address-0x06000000);
    assert(address >= 0x00200000 && address < 0x00300000);
    return low+(address-0x00200000);
}
#define REG32(a) (*(u32 *)memory(a))
#define wire_action REG32(SYSINFO_ADDR+0x38)
#define crc32 actual_crc32
#include "crc32.inc"
#undef crc32
static u8 payload(unsigned i) { return (u8)((i*29u) ^ (i>>8) ^ 0x5a); }
static u32 crc32(u8 *p, int length, u32 seed)
{
    assert(!seed);
    if (p == high+0x9fc0) { assert(length==60); metadata_crcs++; }
    else { assert(p==memory(ROM_IMAGE_ADDR) && length==ROM_IMAGE_SIZE); payload_crcs++; }
    return actual_crc32(p,length,seed);
}
static int read_file(char *path, int offset, int length, void *out)
{
    assert(!strcmp(path,"/SAROO/ISO/TEST.BIN") && !offset && length==ROM_IMAGE_SIZE);
    assert(out==memory(ROM_IMAGE_ADDR)); reads++;
    unsigned n = count < 0 ? 0 : count > length ? (unsigned)length : (unsigned)count;
    for (unsigned i=0;i<n;++i) ((u8 *)out)[i]=payload(i);
    return count;
}
void card_init(void) { assert(payload_crcs==1 && reads==1); cards++; }
static void host_go(void) { starts++; }
static void (*host_target(int entry))(void) { assert((u32)entry==resume); return host_go; }
static void host_copy(int entry);
#include "service.inc"
static void host_copy(int entry)
{
    resume=entry;
    jump_copy_code(entry);
}
static void put32(u8 *p, u32 value)
{ for (unsigned i=0;i<4;i++) p[i]=(u8)(value>>(24-i*8)); }
static void seal(void)
{ u8 *d=high+0x9fc0;put32(d+60,actual_crc32(d,60,0)); }
static void descriptor(unsigned phase)
{
    u8 *d=high+0x9fc0;memset(d,0,64);memcpy(d,"SOR1",4);d[4]=phase;
    put32(d+8,phase==2 ? 0x06002222 : 0x06002048);
    put32(d+12,ROM_IMAGE_SIZE);put32(d+16,EXPECTED_PAYLOAD_CRC);
    memcpy(d+24,"/SAROO/ISO/TEST.BIN",20);seal();
}
static void reset(unsigned phase)
{
    memset(cart,0xa5,sizeof cart);memset(high,0x73,sizeof high);memset(low,0xb9,sizeof low);
    REG32(0x060020e0)=0x1800;descriptor(phase);rom_ready=ROM_KOF95;
    wire_action=__builtin_bswap32(SD_OVERLAY_ROM);count=ROM_IMAGE_SIZE;
    memcpy(original_cart,cart,sizeof cart);
    reads=cards=starts=metadata_crcs=payload_crcs=skip_patch=0;
    need_bup=1;resume=0;expected_virtual=1;
}
static int prepare(void)
{
    int result=overlay_prepare_rom(expected_virtual);
    assert(rom_ready==ROM_NONE);
    return result;
}
static void assert_no_owner(void)
{
    assert(!overlay_rom_phase && !overlay_rom_entry);
    assert(!overlay_rom_after_read());overlay_rom_launch();
    assert(!cards && !starts && need_bup==1);
}
static void verify_copy(void)
{
    assert(cards==1 && starts==1 && !need_bup);
    for (unsigned i=0;i<ROM_IMAGE_SIZE;i++) assert(cart[i]==payload(i));
    assert(!memcmp(cart+ROM_IMAGE_SIZE,original_cart+ROM_IMAGE_SIZE,sizeof cart-ROM_IMAGE_SIZE));
    for (unsigned i=0;i<sizeof low;i++) assert(low[i]==0xb9);
}
int main(int argc,char **argv)
{
    assert(argc==2);int mode=atoi(argv[1]);
    if (mode==0) {
        for(unsigned phase=1;phase<=2;phase++) {
            reset(phase);assert(prepare()==1 && reads==1 && metadata_crcs==1 && payload_crcs==1);
            assert(overlay_rom_phase==phase && overlay_rom_entry==(phase==1?0x06002048:0x06002222));
            unsigned offset=(ROM_IMAGE_ADDR&0x1fffffff)-0x02000000;
            for(unsigned i=0;i<sizeof cart;i++) assert(cart[i]==
                (i>=offset && i<offset+ROM_IMAGE_SIZE ? payload(i-offset) : original_cart[i]));
            memcpy(original_cart,cart,sizeof cart);
            assert(overlay_rom_after_read()==1);
            if(phase==1){assert(!cards&&!starts);overlay_rom_launch();}
            verify_copy();assert(resume==(phase==1?0x06002048:0x06002222));
        }
    } else if(mode==1) {
        const int counts[]={-19,-1,0,1,0x100000,0x1fffff,0x200001};
        for(unsigned i=0;i<sizeof counts/sizeof *counts;i++) {
            reset(1);count=counts[i];assert(prepare()<0 && reads==1 && !payload_crcs);assert_no_owner();
        }
        reset(1);put32(high+0x9fc0+16,EXPECTED_PAYLOAD_CRC^1);seal();
        assert(prepare()<0 && payload_crcs==1);assert_no_owner();
    } else if(mode==2) {
        for(unsigned i=0;i<64;i++) {
            reset(1);high[0x9fc0+i]^=0x80;
            assert(prepare()<0 && !reads);assert_no_owner();
        }
        const unsigned offsets[]={4,5,6,7,8,11,12,20,56,59};
        for(unsigned i=0;i<sizeof offsets/sizeof *offsets;i++) {
            reset(1);high[0x9fc0+offsets[i]]^=offsets[i]==11 ? 1 : 0x80;seal();
            assert(prepare()<0 && !reads);assert_no_owner();
        }
        const u32 sizes[]={0,0xffc,0x1001,0x7fc4,0x8000,0xffffffff};
        for(unsigned i=0;i<sizeof sizes/sizeof *sizes;i++) {
            reset(1);REG32(0x060020e0)=sizes[i];assert(prepare()<0&&!reads);assert_no_owner();
        }
    } else if(mode==3) {
        const char *paths[]={"/SAROO/ISO/../TEST.BIN","/SAROO/ISO/X/TEST.BIN","/SAROO/ISO/X\\TEST.BIN",
                             "/SAROO/ISO/.BIN","/SAROO/ISO/test.BIN","/SAROO/ISO/TEST.bin","/SAROO/ISO/TEST.BINX"};
        for(unsigned i=0;i<sizeof paths/sizeof *paths;i++) {
            reset(1);memset(high+0x9fc0+24,0,32);strcpy((char *)high+0x9fc0+24,paths[i]);seal();
            assert(prepare()<0&&!reads);assert_no_owner();
        }
        reset(1);memset(high+0x9fc0+24,'X',32);seal();assert(prepare()<0&&!reads);assert_no_owner();
        reset(1);high[0x9fc0+55]=1;seal();assert(prepare()<0&&!reads);assert_no_owner();
    } else if(mode==4) {
        const u32 actions[]={0,0x534f5680,0x534f5780,0xffffffff,SD_OVERLAY_ROM|1,SD_OVERLAY_ROM|4};
        for(unsigned i=0;i<sizeof actions/sizeof *actions;i++) {
            reset(1);wire_action=__builtin_bswap32(actions[i]);
            assert(prepare()==(i>=4?-1:0)&&!reads);assert_no_owner();
        }
        reset(1);skip_patch=1;assert(!prepare()&&!reads);assert_no_owner();
        reset(1);expected_virtual=0;assert(!prepare()&&!reads);assert_no_owner();
    } else {
        assert(mode==5);
        for(unsigned next=0;next<5;next++) {
            reset(1);assert(prepare()==1);reads=metadata_crcs=payload_crcs=0;
            if(next==0)wire_action=0;
            if(next==1)expected_virtual=0;
            if(next==2)count=-4;
            if(next==3)skip_patch=1;
            no_game_patches = next==4;
            assert(prepare()==(next==2?-4:0));assert_no_owner();
            no_game_patches = 0;
        }
    }
    puts("checked ROM ownership, complete payload and forward copy passed");
}
