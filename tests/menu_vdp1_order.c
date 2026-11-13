/* Adapted from paps-g 74bee684. Host fixture for launch-time VDP1 writes. */
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static _Alignas(4) u8 vram[0x80000], high[0x100000], config[256];
static _Alignas(4) u8 expected[sizeof vram], untouched[sizeof vram];
enum rom_kind { ROM_NONE, ROM_KOF95, ROM_ULTRAMAN };
static enum rom_kind rom_ready;
static int CHEAT_ADDRES, need_bup, skip_patch;
static int game_break_pc, to_stm32, gets_from_stm32;
static int disc_type __attribute__((unused));
static int loads, cards, jumps, others, cfg_reads, breaks, installs;
static int clears;
static int jump_returns;
static u32 mailbox;
static u32 overlay_handoff;
static jmp_buf takeover;

#define SYSINFO_ADDR ((uintptr_t)config - 0x100)
#define MASK_EXMEM 0x3000
#define CS0_RAM1M 0x1000
#define CS0_RAM4M 0x2000
#define REG16(a) (*(u16*)memory_at(a))
#define REG32(a) (*(u32*)memory_at(a))
static void *memory_at(uintptr_t address)
{
    if (address == SYSINFO_ADDR + 0x38) return &overlay_handoff;
    address &= 0x1fffffff; /* Cached and uncached hardware aliases. */
    if(address >= 0x05c00000 && address < 0x05c80000) return vram + (address - 0x05c00000);
    if(address >= 0x06000000 && address < 0x06100000) return high + (address - 0x06000000);
    assert(address == 0x02820000);
    return &mailbox;
}
static u32 LE32(const u32 *p)
{
    ++cfg_reads;
    const u8 *b = (const u8*)p;
    assert(b >= config && b + 4 <= config + sizeof config);
    return b[0] | (u32)b[1]<<8 | (u32)b[2]<<16 | (u32)b[3]<<24;
}
static void printk(const char *fmt, ...) { (void)fmt; }
static void *observed_memset(void *dest, int value, size_t bytes)
{
    assert(dest == vram+512 && value == 0 && bytes == sizeof vram-512);
    ++clears;
    return memset(dest, value, bytes);
}
static void ssctrl_set(int mask, int value) { assert(mask == MASK_EXMEM); assert(value == CS0_RAM1M || value == CS0_RAM4M); }
static void bup_init(void) { assert(0); }
static void my_cdplayer(void) { assert(0); }
static void my_06b0(void) { assert(0); }
static void kof95_handle(void) { assert(0); }
static void kof95_yzb_hack_handle(void) { assert(0); }
static void card_init(void) { ++cards; }
static void other_patch(void) { ++others; }
static void set_break_pc(int pc, int mode) { assert(pc == game_break_pc && !mode); ++breaks; }
static void install_ubr_isr(void) { ++installs; }
static void loader(void)
{
    ++loads;
    /* Real BIOS code remains outside this host check. The callback has already
     * loaded the IP/game and returns with this video-memory starting pattern. */
    assert(!memcmp(vram, untouched, sizeof vram));
}
static void (*loader_target(u32 address))(void) { assert(address == 0x6001234); return loader; }
static void rom_target(int address)
{
    ++jumps;
    assert(address == 0x6002222 && cards == 1 && need_bup == 0);
    assert(!memcmp(vram, untouched, sizeof vram));
    assert(cfg_reads == 0 && breaks == 0 && installs == 0 && clears == 0);
    if(!jump_returns) longjmp(takeover, 1);
}

/* Adapted from paps-g 6bcc069b. Model the uncached checksum input and
 * the native-endian instruction word separately. */
enum { CNC_IMAGE_BYTES=950320, CNC_INSTRUCTION_OFFSET=0x76344-0x3800 };
static u8 cnc_image[CNC_IMAGE_BYTES];
static u16 cnc_instruction;
static unsigned cnc_reads, cnc_instruction_accesses;
#define crc32 menu_crc32
#include "crc32.inc"
#undef crc32
static __attribute__((unused)) u8 *cnc_image_at(u32 address)
{
    assert(address==0x26003800);
    return cnc_image;
}
static __attribute__((unused)) u32 crc32(u8 *data, int size, u32 seed)
{
    assert(data==cnc_image && size==CNC_IMAGE_BYTES && !seed);
    cnc_reads++;
    return menu_crc32(data,size,seed);
}
static __attribute__((unused)) u16 *cnc_instruction_at(u32 address)
{
    assert(address==0x06076344);
    cnc_instruction_accesses++;
    return &cnc_instruction;
}

#include "actual.inc"

static const u32 clock_callback = 0x060006b8 + sizeof code_06b8;

static void put32(unsigned offset, u32 value)
{
    for(unsigned i=0;i<4;++i) config[offset+i] = value >> (8*i);
}
static void reset(const char *id, unsigned seed)
{
    for(unsigned i=0;i<sizeof vram;++i) vram[i] = (i*17 + seed) & 255;
    memcpy(untouched, vram, sizeof vram);
    memcpy(expected, vram, 512);
    memset(expected+512, 0, sizeof expected-512);
    /* Every command slot must start with END; the other first-512-byte contents
     * are preserved. Everything beyond that prefix is initially zero. */
    for(unsigned i=0;i<sizeof expected;i+=32) *(u16*)(expected+i) = 0x8000;
    memset(high, 0xa5, sizeof high);
    memset(config, 0, sizeof config);
    memcpy(high+0x2020, id, strlen(id)+1);
    REG32(0x06000284) = 0x6001234;
    REG32(0x06002fb4) = REG32(0x06002e28) = 0;
    loads = cards = jumps = others = cfg_reads = breaks = installs = clears = 0;
    CHEAT_ADDRES = 13; need_bup = 17; skip_patch = jump_returns = 0;
    rom_ready = ROM_NONE;
    game_break_pc = 0; to_stm32 = gets_from_stm32 = 0; mailbox = 0x76543210;
    disc_type = 0;
    overlay_handoff = 0;
}
static void finished(void)
{
    assert(loads == 1 && CHEAT_ADDRES == 0 && clears == 1);
    assert(!memcmp(vram, expected, sizeof vram));
    assert(!memcmp(high+0x6b8, code_06b8, sizeof code_06b8));
    assert(REG32(clock_callback) == (u32)(uintptr_t)my_06b0);
    assert(clock_callback + 4 <= 0x060006dc && REG32(0x060006dc) == 0xa5a5a5a5);
    if(need_bup){
        assert(REG32(0x06000358) == (u32)(uintptr_t)bup_init);
        assert(REG32(0x0600026c) == (u32)(uintptr_t)my_cdplayer);
    }else{
        /* card_init is a boundary double: its separate complete-copy tests
         * qualify the actual ROM helpers. The caller must not replace hooks. */
        assert(REG32(0x06000358) == 0xa5a5a5a5 && REG32(0x0600026c) == 0xa5a5a5a5);
    }
    assert(breaks == !!game_break_pc && installs == !!game_break_pc);
    assert(to_stm32 == !!game_break_pc && gets_from_stm32 == !!game_break_pc);
    assert(mailbox == (game_break_pc ? 0 : 0x76543210));
}

static void cnc_reset(void)
{
    const char *id="T-7028H   V1.000";
    /* Authored zero-filled fixture with a matching CRC. No game bytes. */
    const u8 suffix[]={0xc7,0x14,0x50,0xf2};
    cnc_image[CNC_INSTRUCTION_OFFSET]=0x22;
    cnc_image[CNC_INSTRUCTION_OFFSET+1]=0x12;
    memcpy(cnc_image+sizeof cnc_image-sizeof suffix,suffix,sizeof suffix);
    assert(menu_crc32(cnc_image,sizeof cnc_image,0)==0xd606ee5b);

    reset(id,71);
    cnc_instruction=0x2212;
    cnc_reads=cnc_instruction_accesses=0;
    read_1st();finished();
    assert(cnc_instruction==0x0009 && cnc_reads==1 && cnc_instruction_accesses==2);

    for (unsigned skip=0;skip<2;skip++) {
        reset(skip ? id : "T-7028H   V1.001",71);
        skip_patch=skip;
        cnc_instruction=0x2212;
        cnc_reads=cnc_instruction_accesses=0;
        read_1st();finished();
        assert(cnc_instruction==0x2212 && !cnc_reads && !cnc_instruction_accesses);
    }
    const u16 alternatives[]={0,0x0009,0x2210,0xffff};
    for (unsigned i=0;i<sizeof alternatives/sizeof *alternatives;i++) {
        reset(id,71);
        cnc_instruction=alternatives[i];
        cnc_reads=cnc_instruction_accesses=0;
        read_1st();finished();
        assert(cnc_instruction==alternatives[i] && cnc_reads==1 && cnc_instruction_accesses==1);
    }
    const unsigned offsets[]={0,CNC_INSTRUCTION_OFFSET,sizeof cnc_image/2,sizeof cnc_image-1};
    for (unsigned i=0;i<sizeof offsets/sizeof *offsets;i++) {
        reset(id,71);
        cnc_image[offsets[i]]^=1;
        cnc_instruction=0x2212;
        cnc_reads=cnc_instruction_accesses=0;
        read_1st();finished();
        assert(cnc_instruction==0x2212 && cnc_reads==1 && !cnc_instruction_accesses);
        cnc_image[offsets[i]]^=1;
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *mode = argv[1];
    if(!strcmp(mode, "croc")){
        for(unsigned seed=0;seed<256;seed+=51){
            reset("T-5029H-50V1.000", seed);
            *(u16*)(expected+0x7fffe) = 0xffff;
            read_1st(); finished(); assert(cfg_reads == 1);
        }
    }else if(!strcmp(mode, "config")){
        for(int croc=0;croc<2;++croc){
            reset(croc ? "T-5029H-50V1.000" : "UNKNOWN", 97);
            /* Cover preserved prefix, initialized bulk, command boundary and
             * final word. Explicit user override wins over the Croc sentinel. */
            put32(0, 0x10000000 | 0x05c0001f); put32(4, 0x81); expected[31] = 0x81;
            put32(8, 0x20000000 | 0x05c7fffe); put32(12, 0x1234); *(u16*)(expected+0x7fffe) = 0x1234;
            put32(16, 0x40000000 | 0x05c00200); put32(20, 0xfeedabcd); *(u32*)(expected+512) = 0xfeedabcd;
            game_break_pc = 0x6004320;
            read_1st(); finished(); assert(cfg_reads == 7);
        }
    }else if(!strcmp(mode, "plain")){
        for(int skip=0;skip<2;++skip){
            reset(skip ? "T-5029H-50V1.000" : "UNKNOWN", 255);
            skip_patch = skip;
            rom_ready = ROM_ULTRAMAN; /* The game ID still controls callback selection. */
            if(skip) memset(config, 0xff, sizeof config); /* Must not interpret it. */
            read_1st(); finished(); assert(cfg_reads == !skip && !others);
        }
    }else if(!strcmp(mode, "takeover")){
        reset("T-13308G", 63); rom_ready = ROM_ULTRAMAN; REG32(0x06002fb4) = 0x06004000;
        game_break_pc = 0x6004320;
        if(!setjmp(takeover)){ read_1st(); assert(0); }
        assert(loads == 1 && jumps == 1 && cards == 1 && !need_bup && !clears);
        assert(!memcmp(vram, untouched, sizeof vram));
        assert(REG32(clock_callback) == 0xa5a5a5a5 && !cfg_reads && !breaks && !installs);
    }else if(!strcmp(mode, "rom-return")){
        for(int ready=ROM_NONE;ready<=ROM_ULTRAMAN;++ready) for(int stock=0;stock<2;++stock) for(int skip=0;skip<2;++skip){
            reset("T-13308G", 37); rom_ready = ready; skip_patch = skip; jump_returns = 1;
            REG32(0x06002fb4) = stock ? 0x06004000 : 0;
            read_1st(); finished();
            assert(jumps == (ready == ROM_ULTRAMAN && stock && !skip) && cards == jumps);
            assert(need_bup == skip && cfg_reads == !skip);
        }
    }else if(!strcmp(mode, "kof")){
        for(int edition=0;edition<2;++edition) for(int slot=0;slot<2;++slot){
            reset(edition ? "MK-81088" : "T-3101G", 19); rom_ready = ROM_KOF95;
            REG32(0x06002e28) = 0x0607CD80;
            u32 addr = 0x0607cdc4 + slot*4; REG32(addr) = 0x06002048;
            read_1st(); finished(); assert(!jumps && !cards && !need_bup);
            assert(REG32(addr) == (u32)(uintptr_t)kof95_handle);
        }
#ifdef TEST_OVERLAY_HANDOFF
    }else if(!strcmp(mode, "overlay")){
        for (int type = 0; type <= 1; ++type) {
            for (int valid = 0; valid <= 1; ++valid) {
                reset("T-7102G", 77);
                disc_type = type;
                overlay_handoff = __builtin_bswap32(SD_OVERLAY_REPLACEMENT ^ (valid ? 0 : 1u));
                read_1st(); finished();
                assert(others == (type || !valid) && cfg_reads == 1 && need_bup);
            }
        }
#endif
    }else if(!strcmp(mode, "cnc")){
        cnc_reset();
    }else{
        assert(!strcmp(mode, "table"));
        for(GAME_DB *g=game_dbs;g->id;++g){
            if(g->patch_func == Croc_LOG_patch || g->patch_func == ULTRAMAN_patch || g->patch_func == kof95_patch) continue;
            if(!strcmp(g->id,"T-7028H   V1.000")) continue; /* Covered by cnc_reset. */
            reset(g->id, 73); game_break_pc = 0x6004320;
            read_1st(); finished(); assert(others == 1 && cfg_reads == 1 && !cards && !jumps);
        }
    }
    puts("complete launch/VDP1 ordering passed");
    return 0;
}
