/* Adapted from paps-g 49b2bce5. The source seam schedules a higher priority
 * consumer at producer cursor loads; all slot and worker updates are real. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
#include "types.h"

static CDBLOCK cdb;
static TRACK_INFO track;
static u8 payload[2352], sidecar[96], ring[64][24];
static u8 *sector_buffer = payload;
static u32 buf_fad_start, buf_fad_end;
static u16 buf_fad_offset, buf_fad_size;
static u8 buf_subcode_mode;
static unsigned mask, armed, pending, consumed, first_pack;
static unsigned hirq, early_polls, directory_count, flush_count;
static int filter_result, audio_depth, sector_delay;

#define SSLOG(...) ((void)0)
#define LEDEV_CSCT 1
static int fad_to_track(u32 fad) { assert(fad == 150); return 1; }
static TRACK_INFO *get_track_info(int n) { assert(n == 0x100); return &track; }
static int track_subindex(const TRACK_INFO *t, int fad) { assert(t == &track && fad == 150); return 1; }
static void subcode_convert(void) { abort(); }
static int read_disc_bytes(FIL *file, u32 position, u32 length)
{ (void)file; (void)position; (void)length; abort(); }
static unsigned count(void) { return (cdb.subrw_wp + 64u - cdb.subrw_rp) % 64; }

static void consume(unsigned limit)
{
    assert(!mask);
    while (limit-- && count()) {
        const u8 *pack = ring[cdb.subrw_rp];
        if (!consumed) first_pack = pack[0];
        for (unsigned i = 1; i < 24; i++) assert(pack[i] == (u8)(pack[0] + i));
        ++consumed;
        cdb.subrw_rp = (cdb.subrw_rp + 1) % 64;
    }
}

static __attribute__((unused)) int disable_irq(void) { unsigned prior = mask; mask = 1; return prior; }
static void restore_irq(int prior)
{
    assert(mask);
    mask = prior;
    if (!mask && pending) { pending = 0; consume(2); }
}

static u8 read_cursor(u8 value)
{
    if (armed) {
        armed = 0;
        if (mask) pending = 1;
        else consume(2);
    }
    return value;
}

static void signal_irq(unsigned bits) { assert(!mask); hirq |= bits; }
static void led_event(int event) { assert(event == LEDEV_CSCT); }
static void set_peri_report(void)
{
    assert(hirq & HIRQ_SCDQ);
    if (early_polls) {
        consume(4);
        hirq &= ~HIRQ_SCDQ;
    }
}

static int fill_audio_buffer(const u8 *data) { assert(data == payload); return audio_depth; }
static void spi2_flush(void) { ++flush_count; }
static int hw_delay(int ticks) { assert(ticks > 0); return 0; }
static int filter_sector(TRACK_INFO *t, BLOCK *b)
{ assert(t == &track && b->fad == 150 && b->data == payload); return filter_result; }
static int handle_diread(void) { directory_count = count(); return 2; }
static void finish_read(int valid) { assert(!valid); directory_count = count(); }
static void advance_file_unit(void) { abort(); }
#include "producer.inc"

static void ready(unsigned rp, unsigned packs, unsigned decode, int audio)
{
    memset(&cdb, 0, sizeof cdb);
    track = (TRACK_INFO){.fad_0=150, .fad_start=150, .fad_end=150,
                         .mode=audio ? 3 : 1, .sector_size=audio ? 2352 : 2048};
    cdb.fad = 150; cdb.play_fad_end = 151; cdb.play_track = &track;
    cdb.play_type = PLAYTYPE_SECTOR;
    cdb.iflag = decode ? 2 : 0; cdb.has_subrw = decode;
    cdb.subrw_rp = rp; cdb.subrw_wp = (rp + packs) % 64;
    cdb.subcode_buf = sidecar; cdb.subrw_buf = (u8 *)&ring;
    memset(ring, 0x55, sizeof ring);
    for (unsigned i = 0; i < packs; i++)
        for (unsigned j = 0; j < 24; j++) ring[(rp + i) % 64][j] = i + j;
    for (unsigned i = 0; i < 4; i++)
        for (unsigned j = 0; j < 24; j++) sidecar[i*24 + j] = 0xa0 + i + j;
    buf_fad_start = 150; buf_fad_end = 151;
    buf_fad_size = track.sector_size; buf_fad_offset = 0; buf_subcode_mode = decode;
    mask = armed = pending = consumed = first_pack = hirq = early_polls = flush_count = 0;
    directory_count = 99;
    filter_result = 0; audio_depth = 1;
}

static void check_new_packs(void)
{
    unsigned slot = (cdb.subrw_wp + 60) % 64;
    for (unsigned i = 0; i < 4; i++)
        assert(!memcmp(ring[(slot + i) % 64], sidecar + i*24, 24));
}

static void retry(void)
{
    const unsigned sizes[] = {0, 1, 59, 62, 63};
    for (unsigned rp = 0; rp < 64; rp++)
    for (unsigned n = 0; n < sizeof sizes / sizeof *sizes; n++)
    for (int audio = 0; audio <= 1; audio++) {
        unsigned packs = sizes[n];
        ready(rp, packs, 2, audio);
        u8 before[sizeof ring]; memcpy(before, ring, sizeof ring);
        filter_result = 2; audio_depth = -1;
        for (unsigned attempt = 0; attempt < 2; attempt++) {
            assert(worker_step() == 0 && cdb.fad == 150 && cdb.play_wait);
            assert(cdb.subrw_rp == rp && count() == packs && !memcmp(before, ring, sizeof ring));
            assert(hirq & HIRQ_SCDQ);
        }
        cdb.play_wait = 0; filter_result = 0; audio_depth = 1;
        assert(worker_step() == 1 && cdb.fad == 151 && !mask);
        assert(count() == (packs + 4 > 63 ? 63 : packs + 4));
        check_new_packs();
    }
}

static void reset(void)
{
    for (unsigned rp = 0; rp < 64; rp++)
    for (unsigned packs = 1; packs <= 63; packs += 31)
    for (unsigned outer = 0; outer <= 1; outer++) {
        ready(rp, packs, 2, 0);
        cdb.play_track = NULL; armed = 1; mask = outer;
        assert(!get_sector(150, NULL) && !armed && mask == outer);
        if (outer) restore_irq(0);
        assert(!count() && cdb.subrw_rp == rp && !pending && !consumed);
    }
}

static void full(void)
{
    for (unsigned rp = 0; rp < 64; rp++) {
        ready(rp, 63, 2, 0); armed = 1;
        assert(worker_step() == 1 && !armed && !mask && !pending);
        assert(consumed == 2 && first_pack == 1 && count() == 63);
        for (unsigned i = 0; i < 59; i++) assert(ring[(cdb.subrw_rp + i) % 64][0] == i + 4);
        check_new_packs();
    }
}

static void terminal(void)
{
    for (int audio = 0; audio <= 1; audio++) {
        ready(62, 0, 2, audio); early_polls = 1;
        assert(worker_step() == 1 && cdb.fad == cdb.play_fad_end && !consumed);
        assert(hirq & HIRQ_SCDQ);
        assert(flush_count == (unsigned)audio);
        consume(64); assert(consumed == 4 && first_pack == 0xa0 && !count());
    }
}

static void directory(void)
{
    ready(62, 0, 2, 0); cdb.play_type = PLAYTYPE_DIR;
    assert(worker_step() == 2 && directory_count == 4);
    check_new_packs();
    ready(62, 0, 2, 0); cdb.play_type = PLAYTYPE_DIR; filter_result = 1;
    assert(worker_step() == 0 && directory_count == 0 && !count());
    ready(62, 0, 2, 0); filter_result = 1;
    assert(worker_step() == 1 && cdb.fad == 151 && count() == 4);
    check_new_packs();
}

static void controls(void)
{
    for (int audio = 0; audio <= 1; audio++)
    for (unsigned decode = 0; decode <= 2; decode++) {
        ready(63, 0, decode, audio);
        assert(worker_step() == 1 && cdb.fad == 151 && count() == (decode ? 4u : 0u));
        if (decode) check_new_packs();
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "retry")) retry();
    else if (!strcmp(argv[1], "reset")) reset();
    else if (!strcmp(argv[1], "full")) full();
    else if (!strcmp(argv[1], "terminal")) terminal();
    else if (!strcmp(argv[1], "directory")) directory();
    else { assert(!strcmp(argv[1], "controls")); controls(); }
    return 0;
}
