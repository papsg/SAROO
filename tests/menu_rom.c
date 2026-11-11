/* Adapted from paps-g 198a079f. Synthetic ROM payloads and fixed-address stubs. */
/* Actual ROM admission/handoff code; only bus memory, file I/O and jumps modeled. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define TMPBUFF_ADDR 0x220c7000u
_Alignas(4) static u8 cart[0x800000], ip[0x8000];
static int skip_patch, need_bup, file_result, reads, installs, boots, target_pc;
static const char *expected_file;
static void *bus(u32 address, unsigned size)
{
    if (address >= 0x22000000 && address - 0x22000000 <= sizeof cart - size)
        return cart + (address - 0x22000000);
    assert(address >= 0x06002000 && address - 0x06002000 <= sizeof ip - size);
    return ip + (address - 0x06002000);
}
#define REG32(address) (*(u32*)bus((u32)(address),4))
#define REG16(address) (*(u16*)bus((u32)(address),2))
static int read_file(char *name, int offset, int count, void *destination)
{
    assert(!offset && count == 0x200000 && !strcmp(name, expected_file));
#ifdef LEGACY_ROM_READ
    assert((uintptr_t)destination == 0x22400000);
#else
    assert((uintptr_t)destination >= TMPBUFF_ADDR + 0x100 &&
           (uintptr_t)destination + count <= 0x22400000);
#endif
    reads++;
    unsigned bytes = file_result > 0 ? (unsigned)file_result : 0;
    assert(bytes <= (unsigned)count);
    u8 *out = bus((u32)(uintptr_t)destination, bytes);
    for (unsigned i=0;i<bytes;i++) out[i]=(u8)(i*17u+(i>>11));
    return file_result;
}
static void card_init(void) { installs++; }
static void booted(void) { boots++; }
static void (*rom_jump(int address))(void) { target_pc=address;return booted; }
static void copy_and_go(int address);
#include "rom.inc"
static void copy_and_go(int address) { jump_copy_code(address); }

static void initialize(int kind)
{
    memset(cart,0xa5,sizeof cart);memset(ip,0,sizeof ip);
    REG32(0x6002e28)=0x0607CD80;REG32(0x6002fb4)=0x06004000;
    expected_file=kind==2?"/SAROO/ISO/ULTRAMAN.BIN":"/SAROO/ISO/KOF95.BIN";
    file_result=0x200000;reads=installs=boots=target_pc=skip_patch=0;
    need_bup=1;
}
static void complete_copy(int kind)
{
    u8 *tail = malloc(sizeof cart - 0x200000); assert(tail);
    memcpy(tail, cart + 0x200000, sizeof cart - 0x200000);
    if(kind==2) ULTRAMAN_patch();else kof95_handle();
    assert(installs==1 && boots==1 && target_pc==(kind==2?0x6002222:0x6002048));
    for(unsigned i=0;i<0x200000;i++) assert(cart[i]==(u8)(i*17u+(i>>11)));
    /* Every byte beyond the destination survives, whether source overlaps or
     * is disjoint. Compare the actual pre-copy bytes instead of an address fit. */
#ifndef LEGACY_ROM_READ
    assert(!memcmp(tail, cart + 0x200000, sizeof cart - 0x200000));
#else
    assert(reads == 1);
#endif
    free(tail);
}
static void no_handoff(int kind)
{
    if(kind==2)ULTRAMAN_patch();else kof95_handle();
    assert(!installs && !boots);
    assert(!REG16(0x60020c2) && !REG16(0x600217e) && !REG16(0x6002180));
}
int main(int argc,char **argv)
{
    assert(argc==4);int mode=atoi(argv[1]),kind=atoi(argv[2]),value=atoi(argv[3]);
    initialize(kind);
#ifdef LEGACY_ROM_READ
    assert(mode == 0);
    file_result = value;
    if (value == 0x200000)
        complete_copy(kind);
    else
        no_handoff(kind);
#else
    char id[16];memset(id,' ',sizeof id);
    memcpy(id,kind==2?"T-13308G":kind==1?"T-3101G":"MK-81088",kind==1?7:8);
    if(mode==0){
        file_result=value;
        int result=prepare_game_rom(id);
        assert(result==(value==0x200000?0:value<0?value:-1));
        assert(reads==1 && !installs && !boots);
        /* A rejected preflight must preserve the complete menu/cover region. */
        for(unsigned i=0x400000;i<sizeof cart;i++) assert(cart[i]==0xa5);
        if(value==0x200000) complete_copy(kind);
        else no_handoff(kind);
    } else if(mode==1){
        assert(!prepare_game_rom(id));
        reads=0;
        if(value==0)skip_patch=1;
        else if(value==1)memcpy(id,"UNRELATED",9);
        else if(kind==2)REG32(0x6002fb4)=0;else REG32(0x6002e28)=0;
        assert(!prepare_game_rom(id) && !reads && !installs && !boots && !rom_ready);
    } else if(mode==2){
        assert(!prepare_game_rom(id));
        file_result=-4;
        assert(prepare_game_rom(id)==-4);
        no_handoff(kind);
        assert(reads==2 && !rom_ready);
    } else if(mode==3){
        assert(!prepare_game_rom(id));
        no_handoff(kind==2 ? 1 : 2);
    } else return 2;
#endif
    puts("ROM admission and handoff pass");return 0;
}
