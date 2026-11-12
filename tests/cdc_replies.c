/* Adapted from paps-g 48f6a105. Independent CD reply oracles. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ff.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"

static CDBLOCK cdb;
static volatile u16 wire[4];
static unsigned accesses[8], access_count, irq_depth, starts, cases;
static volatile u16 *reg(unsigned index)
{
    assert(access_count < 8);
    accesses[access_count++] = index;
    return &wire[index];
}
#undef SSCR1
#undef SSCR2
#undef SSCR3
#undef SSCR4
#define SSCR1 (*reg(0))
#define SSCR2 (*reg(1))
#define SSCR3 (*reg(2))
#define SSCR4 (*reg(3))
#define SSLOG(...) ((void)0)
static int disable_irq(void) { return irq_depth++; }
static void restore_irq(int old) { assert(irq_depth == (unsigned)old+1); irq_depth=old; }
static void trans_start(void) { starts++; }
#include "handlers.inc"

static void check(int (*handler)(void), u16 a, u16 b, u16 c, u16 d,
                  int interrupt, unsigned transfer)
{
    CDBLOCK before;
    memcpy(&before, &cdb, sizeof before);
    unsigned old_starts = starts;
    access_count = 0;
    for (unsigned i=0; i<4; i++) wire[i]=0xa55a;
    assert(handler() == interrupt);
    assert(access_count == 4);
    for (unsigned i=0; i<4; i++) assert(accesses[i] == i);
    assert(wire[0] == a && wire[1] == b && wire[2] == c && wire[3] == d);
    assert(!irq_depth && starts == old_starts + !!transfer);
    if (transfer) before.trans_type=transfer;
    if (handler == auth_device) {
        if (cdb.cr2 & 255) before.mpeg_auth=2;
        else before.saturn_auth=4;
    }
    assert(!memcmp(&before, &cdb, sizeof cdb));
    cdb.trans_type=0;
    cases++;
}

static void session_replies(int absent)
{
    const unsigned states[] = {STAT_BUSY, STAT_PAUSE, STAT_PLAY, STAT_STANDBY, STAT_SEEK,
                               STAT_SCAN, STAT_RETRY, STAT_ERROR, STAT_FATAL};
    const u32 endpoints[] = {0, 149, 150, 0xffff, 0x10000, 0x654321, 0x7fffff, 0xffffff};
    u32 toc[102] = {0};
    memset(&cdb, 0, sizeof cdb);
    unsigned state_count = absent ? 2 : sizeof states / sizeof *states;
    unsigned positions = absent ? 2 : sizeof endpoints / sizeof *endpoints;
    for (unsigned s = 0; s < state_count; s++) {
        cdb.status = absent ? STAT_OPEN + s : states[s];
        for (unsigned p = 0; p < positions; p++) {
            cdb.TOC = absent && p ? NULL : toc;
            for (unsigned sid = 0; sid < 256; sid++) {
                cdb.cr1 = 0x0300 | sid;
                cdb.trans_type = TRANS_TOC;
                toc[101] = bswap32((((sid * 17 + s + 0x41) & 255) << 24) | endpoints[p]);
                u32 saved = toc[101];
                u16 high = 0xffff, low = 0xffff;
                if (!absent && sid < 2) {
                    high = 0x0100 | (sid ? 0 : endpoints[p] >> 16);
                    low = sid ? 0 : endpoints[p];
                }
                check(get_session_info, cdb.status << 8, 0, high, low, 0, 0);
                assert(toc[101] == saved);
            }
        }
    }
    printf("%u session replies pass\n", cases);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "byteorder")) {
        for (u32 i = 0; i <= 0xffff; ++i) {
            const u32 word = (i << 16) | ((i * 257u + 53u) & 0xffffu);
            const u8 expected[] = {word >> 24, word >> 16, word >> 8, word};
            const u32 swapped = bswap32(word);
            assert(!memcmp(&swapped, expected, sizeof(expected)));
        }
        puts("65536 CD words have the expected byte order");
        return 0;
    }
    if (argc == 2) {
        assert(!strcmp(argv[1], "unavailable") || !strcmp(argv[1], "valid"));
        session_replies(!strcmp(argv[1], "unavailable"));
        return 0;
    }
    assert(argc == 1);
    TRACK_INFO track = {0};
    u8 subheader[10];
    BLOCK block = {0};
    /* Sweep both byte lanes, high-bit halfwords and changing 24-bit metadata.
     * This is 18 layouts × 65536 inputs, not every combination of all fields. */
    for (u32 v=0; v<65536; v++) {
        memset(&cdb, 0, sizeof cdb);
        cdb.status=v;
        u16 status=(v&255)<<8;
        unsigned selector=v%MAX_SELECTORS;
        cdb.cr3=selector<<8;
        cdb.cddev_filter=v;
        cdb.last_buffer=v>>8;
        cdb.block_free=v%(MAX_BLOCKS+1);
        cdb.part[selector].numblocks=v%(MAX_BLOCKS+1);
        FILTER *f=&cdb.filter[selector];
        f->fad=(v*257)&0xffffff;
        f->range=(v*251)&0xffffff;
        f->chan=v; f->fid=v>>8; f->mode=v^0x5a;
        f->smmask=v>>8; f->cimask=v;
        f->smval=(v>>8)^0xa5; f->cival=v^0x5a;
        f->c_true=v%MAX_SELECTORS; f->c_false=(v&1)?255:selector;
        check(get_hw_info,status,1,0,0x400,0,0);
        check(get_toc_info,status|0x4000,204,0,0,HIRQ_DRDY,TRANS_TOC);
        check(open_tray,status,0,0,0,0,0);
        cdb.cr1=0; cdb.track=1; cdb.tracks=&track; cdb.SUBH=subheader;
        cdb.fad=150; cdb.track_num=1; track.fad_start=150;
        check(get_subcode,status|0x4000,5,0,0,HIRQ_DRDY,TRANS_SUBQ);
        cdb.cr1=1; cdb.subrw_rp=0; cdb.subrw_wp=1;
        check(get_subcode,status|0x4000,12,0,0,HIRQ_DRDY,TRANS_SUBRW);
        check(get_cdev_connect,status,0,cdb.cddev_filter*256,0,0,0);
        check(get_last_buffer,status,0,cdb.last_buffer*256,0,0,0);
        check(get_filter_range,status|(f->fad/65536),f->fad,
              f->range/65536,f->range,0,0);
        check(get_filter_subheader,status|f->chan,f->smmask*256+f->cimask,
              f->fid,f->smval*256+f->cival,0,0);
        check(get_filter_mode,status|f->mode,0,0,0,0,0);
        check(get_filter_connection,status,f->c_true*256+f->c_false,0,0,0,0);
        check(get_buffer_size,status,cdb.block_free,MAX_SELECTORS*256,MAX_BLOCKS,0,0);
        check(get_sector_num,status,0,0,cdb.part[selector].numblocks,0,0);
        cdb.actsize=v*7;
        check(get_actual_size,status|((cdb.actsize/2)/65536),cdb.actsize/2,0,0,0,0);
        cdb.part[selector].numblocks=1; cdb.part[selector].head=&block;
        cdb.cr2=0; block.fad=f->fad; block.fn=v; block.cn=v>>8;
        block.sm=f->smval; block.ci=f->cival;
        check(get_sector_info,status|(block.fad/65536),block.fad,
              block.fn*256+block.cn,block.sm*256+block.ci,0,0);
        copy_move_error=v;
        check(get_copy_error,status|copy_move_error,0,0,0,0,0);
        cdb.rdir_state=3; cdb.cdir_pfnum=v%255; cdb.cdir_drend=v&1;
        cdb.cdir_pfid=(v*257)&0xffffff;
        check(get_file_scope,status,cdb.cdir_pfnum,
              cdb.cdir_drend*256+cdb.cdir_pfid/65536,cdb.cdir_pfid,0,0);
        cdb.cr2=v&1;
        check(auth_device,255,65535,65535,65535,
              (v&1)?HIRQ_MPED:HIRQ_EFLS|HIRQ_CSCT,0);
    }
    printf("%u complete replies pass; ordered halfwords and command state preserved\n",cases);
    return 0;
}
