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
static _Alignas(4) u8 vram[0x80000], high[0x100000], low[0x100000], config[256];
static _Alignas(4) u8 expected[sizeof vram], untouched[sizeof vram];
enum rom_kind { ROM_NONE, ROM_KOF95, ROM_ULTRAMAN };
static enum rom_kind rom_ready;
static u8 kaitei_music_ready;
static int CHEAT_ADDRES, need_bup, skip_patch;
static int game_break_pc, to_stm32, gets_from_stm32;
static int disc_type __attribute__((unused));
static int loads, cards, jumps, others, cfg_reads, breaks, installs;
static int clears;
static int jump_returns;
static unsigned rom_profile_phase;
static u32 mailbox;
static u32 overlay_handoff;
static u8 cs0_ready;
static u8 guard_descriptor[24];
static u16 cnc_instruction;
static jmp_buf guard_failure;
static unsigned cart_writes[8], cart_count;
static jmp_buf takeover;
static int host_rom_after_read(void)
{
    if (!rom_profile_phase || skip_patch) return 0;
    need_bup = 0;
    assert(!clears && !cfg_reads && !CHEAT_ADDRES);
    if (rom_profile_phase == 2) { cards++; jumps++; longjmp(takeover, 1); }
    return 1;
}

#define SYSINFO_ADDR ((uintptr_t)config - 0x100)
#define MASK_EXMEM 0x3000
#define CS0_RAM1M 0x2000
#define CS0_RAM4M 0x3000
#define REG8(a) (*(u8*)memory_at(a))
#define REG16(a) (*(u16*)memory_at(a))
#define REG32(a) (*(u32*)memory_at(a))
static void *memory_at(uintptr_t address)
{
    if (address == SYSINFO_ADDR + 0x38) return &overlay_handoff;
    address &= 0x1fffffff; /* Cached and uncached hardware aliases. */
    if(address == 0x025e7fff) return &cs0_ready;
    if(address == 0x06076344) return &cnc_instruction;
    if(address >= 0x05c00000 && address < 0x05c80000) return vram + (address - 0x05c00000);
    if(address >= 0x06000000 && address < 0x06100000) return high + (address - 0x06000000);
    if(address >= 0x00200000 && address < 0x00300000) return low + (address - 0x00200000);
    assert(address == 0x02820000);
    return &mailbox;
}
static void *native_high_half(uintptr_t address)
{
    const uint16_t endian = 1;
    return (u8*)memory_at(address) + (*(const u8*)&endian ? 2 : 0);
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
static void ssctrl_set(int mask, int value)
{
    assert(mask == MASK_EXMEM && (value == CS0_RAM1M || value == CS0_RAM4M));
    assert(cart_count < sizeof(cart_writes) / sizeof(cart_writes[0]));
    cart_writes[cart_count++] = value;
}
static void bup_init(void) { assert(0); }
static void my_cdplayer(void) { assert(0); }
static void my_06b0(void) { assert(0); }
static void kof95_handle(void) { assert(0); }
static void kof95_yzb_hack_handle(void) { assert(0); }
static void card_init(void) { ++cards; }
static int bios_cd_cmd(int type) { (void)type; assert(0); return 0; }
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
    assert(!seed);
    if(data == cnc_image) {
        assert(size == CNC_IMAGE_BYTES);
        assert(!cfg_reads);
        cnc_reads++;
    } else if(data == guard_descriptor) assert(size == 20);
    else assert(data == high + 0x10000 && (size == 179656 || size == 244484));
    return menu_crc32(data,size,seed);
}
static __attribute__((unused)) u16 *cnc_instruction_at(u32 address)
{
    assert(address==0x06076344);
    cnc_instruction_accesses++;
    return &cnc_instruction;
}
static __attribute__((unused)) u8 *guard_pointer(uintptr_t address)
{
    if(address == SYSINFO_ADDR + 0x40) return guard_descriptor;
    assert(address == 0x26003800);
    return cnc_image;
}
static __attribute__((unused)) void guard_write(u32 address, u16 value)
{
    assert(address == 0x06076344);
    cnc_instruction = value;
    cnc_image[CNC_INSTRUCTION_OFFSET] = value >> 8;
    cnc_image[CNC_INSTRUCTION_OFFSET+1] = value;
}
static __attribute__((unused)) void guard_refused(void) { longjmp(guard_failure,1); }
#define SD_GUARD_POINTER(a) guard_pointer(a)
#define SD_GUARD_READ16(a) (*cnc_instruction_at(a))
#define SD_GUARD_WRITE16(a,v) guard_write(a,v)
#define SD_GUARD_FIRST_LOAD() REG32(0x060020f0)

#include "actual.inc"
#ifndef SD_OVERLAY_TAG
#define SD_OVERLAY_TAG 0x534f5600u
#endif

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
    memset(low, 0xa5, sizeof low);
    memset(config, 0, sizeof config);
    memcpy(high+0x2020, id, strlen(id)+1);
    REG32(0x06000284) = 0x6001234;
    REG32(0x06002fb4) = REG32(0x06002e28) = 0;
    loads = cards = jumps = others = cfg_reads = breaks = installs = clears = 0;
    CHEAT_ADDRES = 13; need_bup = 17; skip_patch = jump_returns = 0;
    rom_ready = ROM_NONE;
    rom_profile_phase = 0;
    game_break_pc = 0; to_stm32 = gets_from_stm32 = 0; mailbox = 0x76543210;
    disc_type = 0;
    overlay_handoff = 0;
    cart_count = 0;
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

static void music_launches(void)
{
    const char *ids[] = {"T-10001G  V1.000", "T-15006G  V1.004"};
    for (unsigned region = 0; region < 2; ++region)
        for (unsigned route = 0; route < 5; ++route) {
            reset(ids[region], 93);
            kaitei_music_ready = 7;
            u32 body = region ? 0x0600a2d4 : 0x0600a3b8;
            u32 copy = region ? 0x0600a310 : 0x0600a3f4;
            REG32(0x06007324) = body;
            REG32(0x060044e0) = copy;
            overlay_handoff = __builtin_bswap32(route ? SD_OVERLAY_REPLACEMENT : 0);
            if (route == 2) disc_type = 4;
            if (route == 3) skip_patch = 1;
            if (route == 4) {
                put32(0, 0x40000000 | 0x060044e0); put32(4, 0x0600aa00);
            }
            read_1st(); finished();
            int fallback = route == 0 || route == 2;
            assert(REG32(0x06007324) == (fallback ?
                   (u32)(uintptr_t)KAITEI_DAISENSOU_handle1 : body));
            assert(REG32(0x060044e0) == (route == 4 ? 0x0600aa00 : fallback ?
                   (u32)(uintptr_t)KAITEI_DAISENSOU_handle2 : copy));
            assert(kaitei_music_ready == (fallback ? 0 : 7));
            assert(!others && !cards && !jumps);
            for (unsigned i = 0; i < 108; ++i) assert(high[body-0x06000000+i] == 0xa5);
        }
}

static void fenrir_launches(void)
{
    for (unsigned route = 0; route < 5; ++route) {
        reset("CD0000001 V1.000", 79);
        REG16(0x0600dcca) = 0x6012;
        REG16(0x0600dccc) = 0x400b;
        REG32(0x0600dd18) = 0x02000f30;
        overlay_handoff = __builtin_bswap32(route ? SD_OVERLAY_REPLACEMENT : 0);
        if (route == 2) disc_type = 4;
        if (route == 3) skip_patch = 1;
        if (route == 4) { put32(0, 0x4600dd18); put32(4, 0x0600aa00); }
        read_1st(); finished();
        int fallback = route == 0 || route == 2;
        assert(REG16(0x0600dcca) == (fallback ? 9 : 0x6012));
        u16 expected_call = 0x400b;
        if (fallback) *(u8 *)&expected_call = 0x41;
        assert(REG16(0x0600dccc) == expected_call);
        assert(REG32(0x0600dd18) == (route == 4 ? 0x0600aa00 : fallback ?
               (u32)(uintptr_t)bios_cd_cmd : 0x02000f30));
        assert(!others && !cards && !jumps);
    }
}

static void rom_profile_launches(void)
{
    for (unsigned phase=1;phase<=2;phase++) {
        reset(phase==1 ? "T-3101G   V1.000" : "T-13308G  V1.001",81);
        rom_profile_phase=phase;
        overlay_handoff=__builtin_bswap32(SD_OVERLAY_ROM);
        if (!setjmp(takeover)) { read_1st();assert(phase==1);finished(); }
        if (phase==2) {
            assert(loads==1 && !CHEAT_ADDRES && !need_bup && cards==1 && jumps==1);
            assert(!clears && !cfg_reads && !memcmp(vram,untouched,sizeof vram));
        } else assert(!cards && !jumps && !others && !need_bup);
    }
    for (unsigned bad=0;bad<3;bad++) {
        reset("T-3101G   V1.000",81);
        rom_profile_phase=bad==1 ? 0 : 1;
        skip_patch=bad==2;
        overlay_handoff=__builtin_bswap32(SD_OVERLAY_ROM | (bad==0));
        if (!setjmp(guard_failure)) { read_1st();assert(0); }
        assert(loads==1 && !cards && !jumps && !clears && !cfg_reads);
    }
    reset("TEST",81);rom_profile_phase=2;
    overlay_handoff=__builtin_bswap32(SD_OVERLAY_REPLACEMENT);
    read_1st();finished();assert(!cards && !jumps && need_bup==1);
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

static void descriptor_word(unsigned at, u32 value)
{
    for(unsigned i=0;i<4;++i) guard_descriptor[at+i]=value>>(24-8*i);
}

static void cnc_overlay(void)
{
    const u8 suffix[]={0xc7,0x14,0x50,0xf2};
    for(unsigned scenario=0;scenario<10;++scenario) {
        memset(cnc_image,0,sizeof cnc_image);
        cnc_image[CNC_INSTRUCTION_OFFSET]=0x22;cnc_image[CNC_INSTRUCTION_OFFSET+1]=0x12;
        memcpy(cnc_image+sizeof cnc_image-sizeof suffix,suffix,sizeof suffix);
        assert(menu_crc32(cnc_image,sizeof cnc_image,0)==0xd606ee5b);
        reset("T-7028H   V1.000",71);
        REG32(0x060020f0)=0x06003800;
        cnc_instruction=0x0009;
        cnc_image[CNC_INSTRUCTION_OFFSET]=0;cnc_image[CNC_INSTRUCTION_OFFSET+1]=9;
        assert(menu_crc32(cnc_image,sizeof cnc_image,0)==0xa851cea7);
        descriptor_word(0,0x06003800);descriptor_word(4,CNC_IMAGE_BYTES);
        descriptor_word(8,0xa851cea7);descriptor_word(12,CNC_INSTRUCTION_OFFSET);
        guard_descriptor[16]=0x22;guard_descriptor[17]=0x12;
        guard_descriptor[18]=0;guard_descriptor[19]=9;
        descriptor_word(20,menu_crc32(guard_descriptor,20,0));
        overlay_handoff=__builtin_bswap32(0x534f5780);
        if(scenario==1) cnc_image[0]^=1;
        if(scenario==2) cnc_instruction=0x1234;
        if(scenario==3 || scenario==4) {
            cnc_instruction=0x2212;cnc_image[CNC_INSTRUCTION_OFFSET]=0x22;cnc_image[CNC_INSTRUCTION_OFFSET+1]=0x12;
            if(scenario==3) overlay_handoff=0;
            else disc_type=1;
        }
        if(scenario==5) {put32(0,0x26076344);put32(4,0x1234);}
        if(scenario==6) {put32(0,0x26003810);put32(4,0x1234);}
        if(scenario==7) skip_patch=1;
        if(scenario==8) guard_descriptor[4]^=1;
        if(scenario==9) REG32(0x060020f0)+=4;
        cnc_reads=cnc_instruction_accesses=0;
        if(scenario>=8) {
            if(!setjmp(guard_failure)) {read_1st();assert(0);}
            assert(loads==1 && !clears && !cfg_reads && !cnc_reads && !cnc_instruction_accesses);
            assert(!memcmp(vram,untouched,sizeof vram));
            continue;
        }
        read_1st();finished();
        u16 expected=scenario==1 || scenario==7 ? 0x2212 : scenario==2 || scenario==5 ? 0x1234 : 9;
        assert(cnc_instruction==expected && !others);
        if(scenario==6) assert(REG16(0x06003810)==0x1234);
        if(scenario==7) assert(!cfg_reads && !cnc_reads);
    }
}

#include "shared-fixtures.inc"

static void shared_id_callbacks(void)
{
    static const struct {
        const char *id;
        unsigned size, first, second;
        u32 checksum;
        u16 before[2];
    } cases[] = {
        {"T-1245G   V1.000", 179656, 0x51a6, 0x51ae, 0x15e2d749, {0x2131, 0x2301}},
        {"T-20109G  V1.003", 244484, 0x328e8, 0x3298a, 0xf4bfcb42, {0x2122, 0x2122}}
    };
    u8 reference[244484];
    for(unsigned game = 0; game < 2; ++game) {
        const unsigned size = cases[game].size;
        const unsigned first = cases[game].first, second = cases[game].second;
        for(unsigned words = 0; words < 4; ++words) {
            for(unsigned damage = 0; damage < 4; ++damage) {
                for(unsigned admitted = 0; admitted < 2; ++admitted) {
                    reset(cases[game].id, 37);
                    u8 *image = high + 0x10000;
                    memset(image, 0, size);
                    *(u16*)(image + first) = cases[game].before[0] ^ !!(words & 1);
                    *(u16*)(image + second) = cases[game].before[1] ^ !!(words & 2);
                    memcpy(image + size - 4, sibling_suffix[game][words], 4);
                    assert(menu_crc32(image, size, 0) == cases[game].checksum);
                    const unsigned offsets[] = {0, 1, size / 2, size - 1};
                    if(damage) image[offsets[damage]] ^= 1;
                    memcpy(reference, image, size);
                    if(!admitted && !words && !damage) {
                        *(u16*)(reference + first) = 0x0009;
                        *(u16*)(reference + second) = 0x0009;
                    }
                    overlay_handoff = admitted ? __builtin_bswap32(SD_OVERLAY_REPLACEMENT) : 0;
                    read_1st(); finished();
                    assert(!memcmp(image, reference, size) && !others);
                }
            }
        }
        /* Sibling discs carry different code, or no first-file extent here. */
        reset(cases[game].id, 53);
        REG16(0x06010000 + first) = game ? 0xa5a5 : 0x6014;
        REG16(0x06010000 + second) = game ? 0xa5a5 : 0x6014;
        memcpy(reference, high + 0x10000, size);
        read_1st(); finished();
        assert(!memcmp(high + 0x10000, reference, size));
    }
}

static void cartridge_actions(void)
{
    static const u32 tags[] = {
        SD_OVERLAY_REPLACEMENT, SD_OVERLAY_TAG | 1, SD_OVERLAY_TAG | 4,
        SD_OVERLAY_REPLACEMENT | 1, SD_OVERLAY_REPLACEMENT | 4,
        0, SD_OVERLAY_TAG, SD_OVERLAY_TAG | 2, SD_OVERLAY_TAG | 5,
        SD_OVERLAY_REPLACEMENT | 2, SD_OVERLAY_REPLACEMENT | 5,
        SD_OVERLAY_REPLACEMENT ^ 0x200, 0x81564f53, 0xffffffff
    };
    for(unsigned i = 0; i < sizeof(tags) / sizeof(tags[0]); ++i) {
        for(unsigned physical = 0; physical < 2; ++physical) {
            for(unsigned override = 0; override < 3; ++override) {
                reset("T-7102G", 31);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[i]);
                if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                read_1st(); finished();
                int valid = i < 5 && !physical;
                unsigned mode = valid ? tags[i] & 0x7f : 0;
                unsigned replacements = valid && (tags[i] & 0x80);
                assert(others == !replacements);
                assert(cart_count == (mode != 0) + (override != 0));
                if(mode) assert(cart_writes[0] == (mode == 1 ? CS0_RAM1M : CS0_RAM4M));
                if(override)
                    assert(cart_writes[cart_count - 1] == (override == 1 ? CS0_RAM1M : CS0_RAM4M));
            }
        }
    }
    reset("T-7102G", 31);
    skip_patch = 1;
    overlay_handoff = __builtin_bswap32(SD_OVERLAY_REPLACEMENT | 1);
    put32(0, 0x30000004);
    read_1st(); finished();
    assert(!cart_count && !others);
}

static void cyber_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT | 1,
                       SD_OVERLAY_REPLACEMENT | 2, 0x81564f53};
    for(unsigned tag = 0; tag < sizeof tags / sizeof tags[0]; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned override = 0; override < 3; ++override) {
                reset("T-1217G   V1.002", 47);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[tag]);
                REG32(0x060dd058) = 0x0600d000;
                REG16(0x0601695a) = 0x2122;
                if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                read_1st(); finished();
                int admitted = tag == 1 && !physical;
                assert(REG32(0x060dd058) == (admitted ? 0x0600d000 :
                       (u32)(uintptr_t)CYBER_BOTS_handle1));
                /* The built-in late edit must not execute at menu launch. */
                assert(REG16(0x0601695a) == 0x2122 && !others);
                assert(cart_count == 1 + (override != 0));
                assert(cart_writes[0] == CS0_RAM1M);
                if(override) assert(cart_writes[1] ==
                                    (override == 1 ? CS0_RAM1M : CS0_RAM4M));
            }
}

static void groove_launches(void)
{
    for(unsigned admitted = 0; admitted < 2; ++admitted)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned override = 0; override < 3; ++override) {
                reset("T-14411G  V1.001", 53);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(admitted ? SD_OVERLAY_REPLACEMENT : 0);
                int selected = admitted && !physical;
                REG32(0x00200460) = 0x06004000;
                REG16(0x00200a44) = selected ? 9 : 0x2012;
                REG16(0x00202eb8) = selected ? 9 : 0x2322;
                REG16(0x06008338) = 0x2012;
                REG16(0x06099a84) = 0x2322;
                if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                read_1st(); finished();
                assert(REG16(0x00200a44) == 9 && REG16(0x00202eb8) == 9);
                assert(REG32(0x00200460) == (selected ? 0x06004000 :
                       (u32)(uintptr_t)GROOVE_ON_FIGHT_handle));
                assert(REG16(0x06008338) == 0x2012 && REG16(0x06099a84) == 0x2322);
                assert(!others && cart_count == (override != 0));
                if(override) assert(cart_writes[0] ==
                                    (override == 1 ? CS0_RAM1M : CS0_RAM4M));
            }
}

static void habitat_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
    for(unsigned tag = 0; tag < sizeof tags / sizeof tags[0]; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned override = 0; override < 3; ++override) {
                reset("GS-7105   V1.004", 59);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[tag]);
                REG32(0x060105b4) = 0x06020000;
                REG16(0x0602b52e) = 0x8b02;
                if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                read_1st(); finished();
                int selected = tag == 1 && !physical;
                assert(REG32(0x060105b4) == (selected ? 0x06020000 :
                       (u32)(uintptr_t)HABITAT2_handle));
                assert(REG16(0x0602b52e) == 0x8b02 && !others);
                assert(cart_count == (override != 0));
            }
    /* Valid control for the other existing table entry sharing this callback.
     * No Habitat pack is admitted for that title. */
    reset("GS-7101   V1.007", 59);
    REG32(0x060105b4) = 0x06020000;
    read_1st(); finished();
    assert(REG32(0x060105b4) == (u32)(uintptr_t)HABITAT2_handle && !others);
}

static void marvel_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
    for(unsigned tag = 0; tag < sizeof tags / sizeof tags[0]; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned override = 0; override < 3; ++override) {
                reset("T-1238G   V1.000", 61);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[tag]);
                /* These two instructions are edited by BYTE, so preserve
                 * their SH-2 byte order independently of host word order. */
                REG8(0x060f000a) = 0xd1;
                REG8(0x060f000b) = 0x20;
                REG16(0x060f000c) = 0x410b;
                REG16(0x060f000e) = 9;
                REG8(0x060f001c) = 0xd1;
                REG8(0x060f001d) = 0x1b;
                REG16(0x060f001e) = 0x412b;
                REG16(0x060f0020) = 9;
                REG32(0x060f008c) = 0x06002000;
                REG32(0x060f0124) = 0;
                REG16(0x0600286e) = 0x2012;
                REG16(0x0600205a) = 0x2102;
                if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                read_1st(); finished();
                int selected = tag == 1 && !physical;
                assert(REG8(0x060f000a) == 0xd1 && REG8(0x060f000b) == (selected ? 0x20 : 0x46));
                assert(REG8(0x060f001c) == 0xd1 && REG8(0x060f001d) == (selected ? 0x1b : 0x41));
                assert(REG32(0x060f0124) == (selected ? 0 : (u32)(uintptr_t)mshvssf_handle1));
                assert(REG16(0x060f000c) == 0x410b && REG16(0x060f000e) == 9);
                assert(REG16(0x060f001e) == 0x412b && REG16(0x060f0020) == 9);
                assert(REG32(0x060f008c) == 0x06002000);
                assert(REG16(0x0600286e) == 0x2012 && REG16(0x0600205a) == 0x2102);
                assert(!others && cart_count == (override != 0));
            }
}

static void phantasy_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
    const u32 targets[] = {0x06020000, 0x06020004, 0x0600083c, 0xffffffff};
    for(unsigned tag = 0; tag < sizeof tags / sizeof tags[0]; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned which = 0; which < sizeof targets / sizeof targets[0]; ++which) {
                reset("GS-9186   V1.003", 61);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[tag]);
                REG32(0x060053dc) = targets[which];
                REG32(0x0602345c) = 0x06021b58;
                REG16(0x06023424) = 0x410b;
                REG32(0x0600083c) = 0x55aa1234; /* No stub is needed by this launch. */
                read_1st(); finished();
                int replaced = tag == 1 && !physical;
                u32 expected = !replaced && which == 0
                    ? (u32)(uintptr_t)Phantasy_Star_Collection_handle : targets[which];
                assert(REG32(0x060053dc) == expected);
                assert(REG32(0x0602345c) == 0x06021b58);
                assert(REG16(0x06023424) == 0x410b && REG32(0x0600083c) == 0x55aa1234);
                assert(!others && !cart_count);
            }
}

static void vampire_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
    const u32 branches[] = {0x060a02d4, 0x060a442c};
    const u32 literals[] = {0x060a03b8, 0x060a447c};
    const u32 tables[] = {0x060ddb30, 0x060ddb68};
    for(unsigned tag = 0; tag < sizeof tags / sizeof tags[0]; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical) {
            reset("T-1229G   V1.000", 61);
            disc_type = physical;
            overlay_handoff = __builtin_bswap32(tags[tag]);
            int replaced = tag == 1 && !physical;
            for(unsigned i = 0; i < 2; ++i) {
                REG16(branches[i] - 2) = replaced ? 0x6233 : 0x6232;
                REG8(branches[i]) = 0x42; REG8(branches[i] + 1) = 0x2b;
                REG16(branches[i] + 2) = 0x6ef6;
                REG32(literals[i]) = replaced ? 0x0600d000 : tables[i];
                REG32(tables[i]) = 0x0600d000;
            }
            REG16(0x06014a52) = 0x2012;
            read_1st(); finished();
            for(unsigned i = 0; i < 2; ++i) {
                assert(REG16(branches[i] - 2) == (replaced ? 0x6233 : 0x6232));
                assert(REG8(branches[i]) == (replaced ? 0x42 : 0x43));
                assert(REG8(branches[i] + 1) == 0x2b && REG16(branches[i] + 2) == 0x6ef6);
                assert(REG32(literals[i]) == (replaced ? 0x0600d000 : (u32)(uintptr_t)VAMPIRE_SAVIOR_handle));
                assert(REG32(tables[i]) == 0x0600d000);
            }
            assert(REG16(0x06014a52) == 0x2012 && !others && !cart_count);
        }
}

static void batman_launches(void)
{
    const char *ids[] = {"T-8140H   V1.000", "T-8118G   V1.000", "T-8140H-50V1.000"};
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
    const u32 pointers[] = {0x0600af9c, 0x0600d000, 0xffffffff};
    for(unsigned id = 0; id < 3; ++id)
        for(unsigned tag = 0; tag < 3; ++tag)
            for(unsigned physical = 0; physical < 2; ++physical)
                for(unsigned pointer = 0; pointer < 3; ++pointer) {
                    reset(ids[id], 61);
                    disc_type = physical;
                    overlay_handoff = __builtin_bswap32(tags[tag]);
                    int replaced = tag == 1 && !physical;
                    REG32(0x060043c4) = replaced ? 0x0600d000 : pointers[pointer];
                    REG32(0x06023e44) = 0xd439490b;
                    read_1st(); finished();
                    u32 expected = replaced ? 0x0600d000 : pointer == 0
                        ? (u32)(uintptr_t)Batman_Forever_The_Arcade_Game_handle1 : pointers[pointer];
                    assert(REG32(0x060043c4) == expected);
                    assert(REG32(0x06023e44) == 0xd439490b && !others && !cart_count);
                }
}

static void msh_launches(void)
{
    const char *ids[] = {"T-1214H   V1.001", "T-1215G   V1.003", "6106664   V1.000", "T-7032H-50V1.000"};
    const u32 pointers[] = {0x060d13f8, 0x060d1424, 0x060d15ac};
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT | 1, SD_OVERLAY_REPLACEMENT | 2};
    for(unsigned id = 0; id < 4; ++id)
        for(unsigned tag = 0; tag < 3; ++tag)
            for(unsigned physical = 0; physical < 2; ++physical)
                for(unsigned guards = 0; guards < 8; ++guards)
                    for(unsigned override = 0; override < 3; ++override) {
                        reset(ids[id], 61);
                        disc_type = physical;
                        overlay_handoff = __builtin_bswap32(tags[tag]);
                        unsigned replaced = tag == 1 && !physical;
                        int chosen = -1;
                        for(unsigned i = 0; i < 3; ++i) {
                            REG32(pointers[i]) = guards & (1u << i) ? 0x060d0ff4 : 0x55550000+i;
                            if(chosen < 0 && (guards & (1u << i))) chosen = (int)i;
                        }
                        REG32(0x060d0e1c) = 0x060d0ff4;
                        if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                        read_1st(); finished();
                        for(unsigned i = 0; i < 3; ++i) {
                            u32 original = guards & (1u << i) ? 0x060d0ff4 : 0x55550000+i;
                            assert(REG32(pointers[i]) == (!replaced && (int)i == chosen
                                ? (u32)(uintptr_t)MARVEL_SUPER_handle : original));
                        }
                        assert(REG32(0x060d0e1c) == 0x060d0ff4 && !others);
                        assert(cart_count == replaced + (override != 0));
                        if(replaced) assert(cart_writes[0] == CS0_RAM1M);
                        if(override) assert(cart_writes[cart_count-1] == (override == 1 ? CS0_RAM1M : CS0_RAM4M));
                    }
}

static void pia_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
    for(unsigned tag = 0; tag < 3; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned guards = 0; guards < 4; ++guards)
                for(unsigned override = 0; override < 3; ++override) {
                    reset("T-20114G  V1.002", 61);
                    disc_type = physical;
                    overlay_handoff = __builtin_bswap32(tags[tag]);
                    REG32(0x0601068c) = guards & 1 ? 0x00210000 : 0x77778888;
                    REG32(0x060106b0) = guards & 2 ? 0x06021fe4 : 0x55556666;
                    REG32(0x002100c0) = 0x06010000;
                    if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                    read_1st(); finished();
                    unsigned replaced = tag == 1 && !physical;
                    u32 expected = !replaced && guards == 3
                        ? (u32)(uintptr_t)Pia_Carrot_e_Youkoso_2_handle1
                        : guards & 2 ? 0x06021fe4 : 0x55556666;
                    assert(REG32(0x060106b0) == expected);
                    assert(REG32(0x002100c0) == 0x06010000 && !others);
                    assert(cart_count == (override != 0));
                    if(override) assert(cart_writes[0] == (override == 1 ? CS0_RAM1M : CS0_RAM4M));
                }
}

static void xmen_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT | 4, SD_OVERLAY_REPLACEMENT | 2};
    for(unsigned tag = 0; tag < 3; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned override = 0; override < 3; ++override) {
                reset("T-1226G   V1.002", 61);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[tag]);
                REG8(0x060084dd) = 0x0f;
                REG16(0x060084ec) = 0x400b;
                REG32(0x06008520) = 0x55556666;
                REG32(0x06008530) = 0x77778888;
                if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                read_1st(); finished();
                unsigned replaced = tag == 1 && !physical;
                assert(REG8(0x060084dd) == (replaced ? 0x0f : 0x10));
                assert(REG16(0x060084ec) == (replaced ? 0x400b : 0xb11c));
                assert(REG32(0x06008520) == (replaced ? 0x55556666 : (u32)(uintptr_t)xmvsf_handle1));
                assert(REG32(0x06008530) == (replaced ? 0x77778888 : (u32)(uintptr_t)xmvsf_handle2));
                assert(!others && cart_count == replaced + (override != 0));
                if(replaced) assert(cart_writes[0] == CS0_RAM4M);
                if(override) assert(cart_writes[cart_count-1] == (override == 1 ? CS0_RAM1M : CS0_RAM4M));
            }
}

static void pad_nifty_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
    const u32 pointers[] = {0x06020000, 0x11112222, 0xffffffff};
    for(unsigned tag = 0; tag < 3; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned pointer = 0; pointer < 3; ++pointer)
                for(unsigned override = 0; override < 3; ++override) {
                    reset("GS-7101   V1.007", 61);
                    disc_type = physical;
                    overlay_handoff = __builtin_bswap32(tags[tag]);
                    REG32(0x060105b4) = pointers[pointer];
                    REG16(0x0602a15e) = 0x8b02;
                    if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                    read_1st(); finished();
                    unsigned replaced = tag == 1 && !physical;
                    assert(REG32(0x060105b4) == (replaced ? pointers[pointer] :
                           (u32)(uintptr_t)HABITAT2_handle));
                    assert(REG16(0x0602a15e) == 0x8b02 && !others);
                    assert(cart_count == (override != 0));
                    if(override) assert(cart_writes[0] == (override == 1 ? CS0_RAM1M : CS0_RAM4M));
                }
}

static void kof97_native_launches(void)
{
    const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT | 4, SD_OVERLAY_REPLACEMENT | 2};
    const u32 words[] = {0xd302d201, 0x4b2b2322, 0xa718e209, 0x0600622c, 0xd301410b};
    for(unsigned tag = 0; tag < 3; ++tag)
        for(unsigned physical = 0; physical < 2; ++physical)
            for(unsigned guards = 0; guards < 4; ++guards)
                for(unsigned override = 0; override < 3; ++override) {
                    reset("T-4401G   V1.000", 61);
                    disc_type = physical;
                    overlay_handoff = __builtin_bswap32(tags[tag]);
                    REG32(0x06002f40) = guards & 1 ? 0x480b4601 : 0x12345678;
                    REG32(0x06002f44) = guards & 2 ? 0x930b0303 : 0x87654321;
                    REG16(0x06002f5e) = 0x30b6;
                    REG16(0x06007066) = 0x3450;
                    for(unsigned i = 0; i < 5; ++i) REG32(0x06007050+4*i) = 0x11223344;
                    REG16(0x06007064) = 0x5566;
                    REG32(0x06007068) = 0x778899aa;
                    REG32(0x06066f80) = 0x11112222; /* Stock branch does not match. */
                    if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                    read_1st(); finished();
                    unsigned replaced = tag == 1 && !physical;
                    unsigned fallback = !replaced && guards == 3;
                    assert(REG16(0x06002f5e) == (fallback ? 0x4106 : 0x30b6));
                    for(unsigned i = 0; i < 5; ++i)
                        assert(REG32(0x06007050+4*i) == (fallback ? words[i] : 0x11223344));
                    assert(REG16(0x06007064) == (fallback ? 0x2321 : 0x5566));
                    assert(REG16(0x06007066) == 0x3450);
                    assert(REG32(0x06007068) == (fallback ? 0x06066f80 : 0x778899aa));
                    assert(REG32(0x06066f80) == 0x11112222 && !others);
                    assert(cart_count == replaced + fallback + (override != 0));
                    if(replaced || fallback) assert(cart_writes[0] == CS0_RAM4M);
                    if(override) assert(cart_writes[cart_count-1] == (override == 1 ? CS0_RAM1M : CS0_RAM4M));
                }
}

static void kof97_launches(void)
{
    for(unsigned four_mb = 0; four_mb < 2; ++four_mb) {
        for(unsigned state = 0; state < (four_mb ? 2u : 3u); ++state) {
            for(unsigned override = 0; override < 3; ++override) {
                reset(four_mb ? "T-4401G" : "T-3121G", 29);
                if(four_mb) {
                    REG32(0x06002f40) = 0x480b4601;
                    REG32(0x06002f44) = 0x930b0303;
                } else {
                    REG32(0x06066f80) = 0x2122d110;
                    REG32(0x06066fc0) = 0x23301ff0;
                }
                if(state == 1)
                    overlay_handoff = __builtin_bswap32(SD_OVERLAY_REPLACEMENT | 2);
                if(state == 2) {
                    REG16(0x06066f80) = 9; /* Delivered replacement sector. */
                    overlay_handoff = __builtin_bswap32(SD_OVERLAY_REPLACEMENT | 1);
                }
                if(override) put32(0, 0x30000000 | (override == 1 ? 1 : 4));
                read_1st(); finished();
                assert(!others && cart_count == 1u + (override != 0));
                assert(cart_writes[0] == (four_mb ? CS0_RAM4M : CS0_RAM1M));
                if(override)
                    assert(cart_writes[1] == (override == 1 ? CS0_RAM1M : CS0_RAM4M));
                if(four_mb) {
                    assert(REG16(0x06002f5e) == 0x4106);
                    const u32 words[] = {0xd302d201, 0x4b2b2322, 0xa718e209,
                                         0x0600622c, 0xd301410b};
                    for(unsigned i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
                        assert(REG32(0x06007050 + 4*i) == words[i]);
                    assert(REG16(0x06007064) == 0x2321);
                    assert(REG32(0x06007068) == 0x06066f80);
                } else {
                    assert(REG16(0x06066f80) == 9);
                    assert(REG32(0x06066fc0) == 0x23301ff0);
                }
            }
        }
    }
    for(unsigned first = 0; first < 2; ++first) {
        for(unsigned second = 0; second < 2; ++second) {
            if(first && second) continue;
            reset("T-3121G", 29);
            u32 word = first ? 0x2122d110 : 0x2122d112;
            REG32(0x06066f80) = word;
            REG32(0x06066fc0) = second ? 0x23301ff0 : 0;
            read_1st(); finished();
            assert(!cart_count && REG32(0x06066f80) == word);
        }
    }
}

struct revision_write { uintptr_t address; unsigned width, before, after; };
struct revision_case {
    const char *id;
    unsigned mode;
    uintptr_t guard;
    unsigned guard_value;
    struct revision_write writes[8];
};

static void revision_launches(const struct revision_case *cases, unsigned count)
{
    static _Alignas(4) u8 reference[sizeof high];
    for(unsigned game=0;game<count;++game) for(unsigned physical=0;physical<2;++physical)
        for(unsigned tag=0;tag<3;++tag) for(unsigned override=0;override<2;++override) {
            const struct revision_case *c = cases + game;
            reset(c->id, 53);
            disc_type = physical;
            int admitted = !physical && tag == 1;
            overlay_handoff = __builtin_bswap32(tag == 1 ? SD_OVERLAY_REPLACEMENT | c->mode :
                                                tag == 2 ? SD_OVERLAY_REPLACEMENT | 2 : 0);
            if(c->guard) REG8(c->guard) = c->guard_value;
            for(const struct revision_write *w=c->writes;w->address;++w) {
                if(w->width == 1) REG8(w->address) = w->after;
                else if(w->width == 2) REG16(w->address) = w->after;
                else { assert(w->width == 4); REG32(w->address) = w->after; }
            }
            memcpy(reference, high, sizeof high);
            if(!admitted) for(const struct revision_write *w=c->writes;w->address;++w) {
                if(w->width == 1) REG8(w->address) = w->before;
                else if(w->width == 2) REG16(w->address) = w->before;
                else REG32(w->address) = w->before;
            }
            if(override) put32(0, 0x30000004);
            read_1st(); finished();
            /* read_1st always installs the shared BIOS helper after dispatch. */
            memcpy(reference+0x6b8, code_06b8, sizeof code_06b8);
            *(u32*)(reference+0x6d4) = (u32)(uintptr_t)my_06b0;
            *(u32*)(reference+0x358) = (u32)(uintptr_t)bup_init;
            *(u32*)(reference+0x26c) = (u32)(uintptr_t)my_cdplayer;
            if(memcmp(high, reference, sizeof high))
                for(unsigned i=0;i<sizeof high;++i) if(high[i] != reference[i]) {
                    fprintf(stderr, "revision %u physical %u tag %u override %u at %08x: %02x != %02x\n",
                            game, physical, tag, override, 0x06000000+i, high[i], reference[i]);
                    break;
                }
            assert(!memcmp(high, reference, sizeof high) && !others);
            assert(cart_count == (c->mode != 0) + override);
            if(c->mode) assert(cart_writes[0] == (c->mode == 1 ? CS0_RAM1M : CS0_RAM4M));
            if(override) assert(cart_writes[cart_count-1] == CS0_RAM4M);
        }
}

static void japanese_revisions(void)
{
    static const struct revision_case cases[] = {
        {"GS-9126   V1.001", 0, 0, 0, {{0x060203f4, 2, 0x400b, 9}, {0x060203f6, 2, 9, 9}}},
        {"GS-9101   V1.013", 0, 0, 0, {{0x06022a60, 2, 0x400b, 9}, {0x06022a62, 2, 9, 9}}},
        {"GS-9101   V1.010", 0, 0, 0, {{0x06022a60, 2, 0xd13b, 0xd13b}}},
        {"T-3111G   V1.002", 1, 0, 0, {{0x06079dec, 2, 0x2122, 9}}},
        {"GS-9102   V1.001", 0, 0x060219b1, 1, {
            {0x060219b3, 1, 0x0f, 3}, {0x060219e7, 1, 0x0f, 3},
            {0x0602037d, 1, 0x4f, 0x43}, {0x060203fd, 1, 0x4f, 0x43},
            {0x060204d9, 1, 0x4f, 0x43}, {0x06020619, 1, 0x1f, 0x13}}},
        {"T-36102G  V1.000", 0, 0, 0, {{0x06003192, 1, 0xe1, 0xe1}, {0x06003193, 1, 9, 8}}},
    };
    revision_launches(cases, sizeof cases / sizeof cases[0]);
}

static void direct_revisions(void)
{
    static const struct revision_case cases[] = {
        {"T-25403H  V1.007", 0, 0, 0, {
            {0x0601875a, 2, 0x480b, 9}, {0x0601875c, 2, 0xe408, 0xe408},
            {0x060187ae, 2, 0x480b, 9}, {0x060187b0, 2, 0xe408, 0xe408}}},
        {"T-25403H  V2.000", 0, 0, 0, {
            {0x0601876a, 2, 0x480b, 9}, {0x0601876c, 2, 0xe408, 0xe408},
            {0x060187be, 2, 0x480b, 9}, {0x060187c0, 2, 0xe408, 0xe408}}},
        {"T-1504G   V1.002", 0, 0, 0, {
            {0x0601d379, 1, 1, 0x56}, {0x0601d2f5, 1, 1, 0x56}}},
        {"T-16510G  V1.003", 0, 0, 0, {{0x06011208, 2, 0x2232, 9}}},
        {"T-1503G   V1.003", 0, 0x0601085d, 3, {{0x0601088d, 1, 1, 0x78}}},
        {"999999999 V1.000", 0, 0, 0, {
            {0x060108c0, 4, 0x04000000, 0x02400000}, {0x060106c4, 2, 0x2102, 9},
            {0x060106c6, 2, 0xe000, 0xe000}, {0x060106c8, 2, 0x1102, 0x1102}}},
        /* Already-public policies are controls, not additional profiles. */
        {"T-1503G   V1.007", 0, 0, 0, {{0x0601085d, 1, 1, 0x56}}},
        {"T-3111G   V1.005", 1, 0, 0, {{0x06079f90, 2, 0x2122, 9}}},
    };
    revision_launches(cases, sizeof cases / sizeof cases[0]);
    reset("999999999 V1.000", 53);
    REG32(0x060108c0) = 0x04000001;
    REG16(0x060106c4) = 0x2102;
    read_1st(); finished();
    assert(REG32(0x060108c0) == 0x04000001 && REG16(0x060106c4) == 0x2102);
}

static void manx_international(void)
{
    static const struct revision_case cases[] = {
        {"MK-81210  V1.000", 0, 0x060219b1, 1, {
            {0x060219b3, 1, 0x0f, 3}, {0x060219e7, 1, 0x0f, 3},
            {0x0602037d, 1, 0x4f, 0x43}, {0x060203fd, 1, 0x4f, 0x43},
            {0x060204d9, 1, 0x4f, 0x43}, {0x06020619, 1, 0x1f, 0x13}}},
        {"MK-81210  V1.001", 0, 0x06021a11, 1, {
            {0x06021a13, 1, 0x0f, 3}, {0x06021a47, 1, 0x0f, 3},
            {0x060203dd, 1, 0x4f, 0x43}, {0x0602045d, 1, 0x4f, 0x43},
            {0x06020539, 1, 0x4f, 0x43}, {0x06020679, 1, 0x1f, 0x13},
            {0x060219b1, 1, 2, 2}}},
    };
    revision_launches(cases, sizeof cases / sizeof cases[0]);
    for(unsigned game=0;game<2;++game) {
        reset(cases[game].id, 53);
        REG8(0x060219b1) = REG8(0x06021a11) = 2;
        for(const struct revision_write *w=cases[game].writes;w->address;++w)
            REG8(w->address) = w->before;
        read_1st(); finished();
        for(const struct revision_write *w=cases[game].writes;w->address;++w)
            assert(REG8(w->address) == w->before);
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
    }else if(!strcmp(mode, "croc-overlay")){
        static const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT, SD_OVERLAY_REPLACEMENT | 2};
        for(unsigned physical=0;physical<2;++physical) for(unsigned tag=0;tag<3;++tag)
            for(unsigned override=0;override<2;++override) {
                reset("T-5029H-50V1.000", 53);
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[tag]);
                if(physical || tag != 1) *(u16*)(expected+0x7fffe) = 0xffff;
                if(override) {
                    put32(0, 0x20000000 | 0x05c7fffe); put32(4, 0x1234);
                    *(u16*)(expected+0x7fffe) = 0x1234;
                }
                read_1st(); finished(); assert(!others && !cart_count);
            }
    }else if(!strcmp(mode, "diehard")){
        static const char *ids[] = {"T-16103H  V1.000", "GS-9123   V1.000"};
        static const u32 tags[] = {0, SD_OVERLAY_REPLACEMENT,
                                  SD_OVERLAY_REPLACEMENT | 2, SD_OVERLAY_TAG};
        for(unsigned game=0;game<2;++game) for(unsigned physical=0;physical<2;++physical)
            for(unsigned tag=0;tag<4;++tag) {
                reset(ids[game], 53);
                cs0_ready = 0x32;
                disc_type = physical;
                overlay_handoff = __builtin_bswap32(tags[tag]);
                read_1st(); finished();
                assert(cs0_ready == ((!physical && tag == 1) ? 0x32 : 0xff));
                assert(!others && !cart_count);
            }
    }else if(!strcmp(mode, "japanese-revisions")){
        japanese_revisions();
    }else if(!strcmp(mode, "direct-revisions")){
        direct_revisions();
    }else if(!strcmp(mode, "manx-international")){
        manx_international();
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
                overlay_handoff = __builtin_bswap32(SD_OVERLAY_REPLACEMENT ^ (valid ? 0 : 2u));
                read_1st(); finished();
                assert(others == (type || !valid) && cfg_reads == 1 && need_bup);
            }
        }
#endif
    }else if(!strcmp(mode, "kof97")){
        kof97_launches();
    }else if(!strcmp(mode, "cyber")){
        cyber_launches();
    }else if(!strcmp(mode, "groove")){
        groove_launches();
    }else if(!strcmp(mode, "habitat")){
        habitat_launches();
    }else if(!strcmp(mode, "marvelvs")){
        marvel_launches();
    }else if(!strcmp(mode, "phantasy")){
        phantasy_launches();
    }else if(!strcmp(mode, "vampire")){
        vampire_launches();
    }else if(!strcmp(mode, "batman")){
        batman_launches();
    }else if(!strcmp(mode, "msh")){
        msh_launches();
    }else if(!strcmp(mode, "pia")){
        pia_launches();
    }else if(!strcmp(mode, "xmen")){
        xmen_launches();
    }else if(!strcmp(mode, "pad-nifty")){
        pad_nifty_launches();
    }else if(!strcmp(mode, "kof97-native")){
        kof97_native_launches();
    }else if(!strcmp(mode, "cart-action")){
        cartridge_actions();
    }else if(!strcmp(mode, "siblings")){
        shared_id_callbacks();
    }else if(!strcmp(mode, "cnc")){
        cnc_reset();
    }else if(!strcmp(mode, "cnc-overlay")){
        cnc_overlay();
    }else if(!strcmp(mode, "music")){
        music_launches();
    }else if(!strcmp(mode, "fenrir")){
        fenrir_launches();
    }else if(!strcmp(mode, "rom-profile")){
        rom_profile_launches();
    }else{
        assert(!strcmp(mode, "table"));
        for(GAME_DB *g=game_dbs;g->id;++g){
            if(g->patch_func == Croc_LOG_patch || g->patch_func == ULTRAMAN_patch ||
               g->patch_func == kof95_patch || g->patch_func == DND2_patch ||
               g->patch_func == FRIENDS_patch || g->patch_func == kof97_patch ||
               g->patch_func == CYBER_BOTS_patch || g->patch_func == GROOVE_ON_FIGHT_patch ||
               g->patch_func == HABITAT2_patch || g->patch_func == mshvssf_patch ||
               g->patch_func == Phantasy_Star_Collection_patch ||
               g->patch_func == VAMPIRE_SAVIOR_patch ||
               g->patch_func == Batman_Forever_The_Arcade_Game_patch ||
               g->patch_func == MARVEL_SUPER_patch ||
               g->patch_func == Pia_Carrot_e_Youkoso_2_patch ||
               g->patch_func == xmvsf_patch ||
               g->patch_func == Die_Hard_Trilogy_patch ||
               g->patch_func == FIGHTERS_MEGAMIX_JAP_patch ||
               g->patch_func == Fighting_Vipers_JAP_patch ||
               g->patch_func == Metal_Slug_patch ||
               g->patch_func == MANX_TT_SUPER_BIKE_patch ||
               g->patch_func == WHIZZ_patch ||
               g->patch_func == Ultimate_Mortal_Kombat_3_patch ||
               g->patch_func == Hebereke_Popoitto_patch ||
               g->patch_func == srmp7_patch ||
               g->patch_func == Hissatsu_Pachinko_Collection_patch ||
               g->patch_func == Heart_of_Darkness_patch ||
               g->patch_func == Metal_Slug_A_patch ||
               g->patch_func == KAITEI_DAISENSOU_patch ||
               g->patch_func == Fenrir_patch) continue;
            if(!strcmp(g->id,"T-7028H   V1.000")) continue; /* Covered by cnc_reset. */
            reset(g->id, 73); game_break_pc = 0x6004320;
            read_1st(); finished(); assert(others == 1 && cfg_reads == 1 && !cards && !jumps);
        }
    }
    puts("complete launch/VDP1 ordering passed");
    return 0;
}
