/* Adapted from paps-g 3e62998c: actual decoder and workspace handoff. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define GIF_TEST_ASAN 1
#endif
#endif
#ifdef __SANITIZE_ADDRESS__
#define GIF_TEST_ASAN 1
#endif
#ifdef GIF_TEST_ASAN
#include <sanitizer/asan_interface.h>
#else
#define __asan_poison_memory_region(address, size) ((void)0)
#define __asan_unpoison_memory_region(address, size) ((void)0)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t FSIZE_t;
typedef struct { unsigned opened; } FIL;
#define FA_READ 1
#define FR_OK 0
#define MAX_BLOCKS 200
static u8 axi[0x80000], gfx[0x100000];
#define GIF_TEST_AXI axi
#define GIF_TEST_GFX gfx
static u8 *file_data;
static unsigned file_size, fault, reads, closes;
static FSIZE_t reported_size;
static u32 ticks;
static int f_open(FIL *f, const char *name, int mode)
{
    assert(!strcmp(name, "/SAROO/mainmenu_bg.gif") && mode == FA_READ);
    f->opened = fault != 1;
    return fault == 1 ? 4 : 0;
}
#define f_size(f) (reported_size)
static int f_read(FIL *f, void *dst, unsigned size, u32 *count)
{
    assert(f->opened && dst == axi + 0x20000);
    ++reads;
    /* Independent extent: 200 * 2352 bytes after 0xa000, before CD info. */
    assert(size <= 0x5cd80u && size <= file_size);
    *count = fault == 3 ? size - 1 : size;
    __asan_unpoison_memory_region(dst, *count);
    memcpy(dst, file_data, *count);
    return fault == 2 ? 1 : 0;
}
static int f_close(FIL *f)
{
    assert(f->opened); ++closes; f->opened = 0;
    return fault == 4 ? 1 : 0;
}
static void __attribute__((unused)) printk(const char *format, ...) { (void)format; }
static u32 osKernelGetTickCount(void) { return ticks; }
static void osDelay(unsigned delay) { ticks += delay; }
/* A deterministic cancellation request at the real publication CAS boundary.
 * Only the intrinsic is intercepted; the state machine expressions are intact. */
static int cancel_transition;
static void cancel_at_publication(volatile int *state)
{
    if (cancel_transition) {
        __atomic_store_n(state, -1, __ATOMIC_SEQ_CST);
        cancel_transition = 0;
    }
}
static int __attribute__((unused)) test_exchange(volatile int *state, int *expected,
        int desired, int weak, int success, int failure)
{
    cancel_at_publication(state);
    return __atomic_compare_exchange_n(state, expected, desired, weak, success, failure);
}
static int __attribute__((unused)) test_legacy_exchange(volatile int *state,
        int expected, int desired)
{
    cancel_at_publication(state);
    return __sync_val_compare_and_swap(state, expected, desired);
}
#define __atomic_compare_exchange_n test_exchange
#define __sync_val_compare_and_swap test_legacy_exchange
void gif_decode_exit(void);
#include "gif.inc"
#undef __atomic_compare_exchange_n
#undef __sync_val_compare_and_swap

#define SSCMD_LOADDISC 3
#define SSCMD_SAVE_ACK 0x20
static unsigned SS_CMD, SS_ARG, handoff_calls;
static int pause_disc(void)
{
    assert(gd_state == -1 && !in_decoding); ++handoff_calls;
    return fault == 2 ? -7 : 0;
}
static int unload_disc(void)
{
    assert(gd_state == -1 && !in_decoding); ++handoff_calls; return -8;
}
static int load_disc(int index, int saves)
{
    assert(gd_state == -1 && !in_decoding && index == 5 && saves == 1);
    ++handoff_calls; return -9;
}
#include "handoff.inc"

static u8 *read_bytes(const char *name, unsigned *length)
{
    FILE *fp = fopen(name, "rb"); assert(fp);
    assert(!fseek(fp, 0, SEEK_END)); long size = ftell(fp); assert(size >= 0);
    assert(!fseek(fp, 0, SEEK_SET));
    u8 *data = malloc(size ? (size_t)size : 1); assert(data);
    assert(fread(data, 1, size, fp) == (size_t)size); assert(!fclose(fp));
    *length = size; return data;
}

static void reset(void)
{
    __asan_unpoison_memory_region(axi, sizeof(axi));
    __asan_unpoison_memory_region(gfx, sizeof(gfx));
    memset(axi, 0xa5, sizeof(axi)); memset(gfx, 0xa5, sizeof(gfx));
    __asan_poison_memory_region(axi + 0x20000, 0x5cd80);
    __asan_poison_memory_region(gfx + 0x41000, sizeof(gfx) - 0x41000);
    reads = closes = 0; ticks = 0;
    reported_size = file_size;
}

static unsigned word(const u8 *p) { return p[0] | (unsigned)p[1] << 8; }

static void valid(const char *expected_path, int first_only)
{
    unsigned size;
    u8 *expected = read_bytes(expected_path, &size);
    assert(size >= 6);
    unsigned w = word(expected), h = word(expected + 2), frames = word(expected + 4);
    const u8 *p = expected + 6, *end = expected + size;
    gif_decode_init(); assert(gd_state == 0 && reads == 1 && closes == 1);
    for (unsigned frame = 0; frame < frames; ++frame) {
        assert(gd_state == 0);
        gif_decode_timer(); assert(gd_state == 1 && gfx[0] == 1 && gfx[2] == 1);
        assert((unsigned)(end - p) >= 10 + w * h * 3);
        assert(word(gfx + 4) == word(p)); assert(word(gfx + 6) == word(p + 2));
        assert(word(gfx + 8) == word(p + 4)); assert(word(gfx + 10) == word(p + 6));
        unsigned delay = word(p + 8); p += 10;
        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x, p += 3) {
                unsigned value = gfx[0x1000 + y * 512 + x];
                u32 color; memcpy(&color, gfx + 0x100 + value * 4, 4);
                assert(color == ((u32)p[0] << 16 | (u32)p[1] << 8 | p[2]));
            }
        if (first_only) {
            assert(frames == 1 && p == end);
            gif_decode_exit();
            free(expected);
            return;
        }
        ticks = UINT32_MAX - 2;
        gfx[0] = 0;
        gif_decode_timer(); assert(gd_state == 1); /* Palette still belongs to SS. */
        gfx[2] = 0;
        gif_decode_timer();
        if (delay) {
            assert(gd_state == 2);
            gif_decode_timer(); assert(gd_state == 2); /* before unsigned wrap */
            ticks += delay - 1; gif_decode_timer(); assert(gd_state == 2);
            ++ticks; gif_decode_timer();
        }
        assert(gd_state == 0);
    }
    assert(p == end);
    gif_decode_timer();
    assert(frames == 1 ? gd_state < 0 : gd_state == 0);
    gif_decode_exit(); assert(gd_state == -1);
    free(expected);
}

int main(int argc, char **argv)
{
    assert(argc >= 3);
    file_data = read_bytes(argv[2], &file_size);
    reset();
    if (!strcmp(argv[1], "valid") || !strcmp(argv[1], "first")) {
        assert(argc == 4); valid(argv[3], !strcmp(argv[1], "first"));
    } else if (!strcmp(argv[1], "cancel")) {
        gif_decode_init();
        assert(gd_state == 0);
        cancel_transition = 1;
        gif_decode_timer();
        assert(!cancel_transition && gd_state == -1 && !in_decoding);
        assert(gfx[0] == 0 && gfx[2] == 0);
        u32 before = ticks;
        gif_decode_exit();
        assert(ticks == before);
        gif_decode_init();
        assert(gd_state == 0 && reads == 2 && closes == 2);
        gif_decode_timer();
        assert(gd_state == 1 && gfx[0] == 1 && gfx[2] == 1);
        gif_decode_exit();
    } else if (!strcmp(argv[1], "reject")) {
        gif_decode_init();
        if (gd_state == 0) gif_decode_timer();
        assert(gd_state < 0 && gfx[0] != 1 && gfx[2] == 0);
    } else if (!strcmp(argv[1], "handoff")) {
        assert(argc == 4); fault = atoi(argv[3]);
        gd_state = 0; SS_ARG = fault ? 0xffff : 5; SS_CMD = SSCMD_LOADDISC;
        dispatch_load();
        assert(gd_state == -1 && SS_CMD == 0);
        assert(handoff_calls == (fault == 1 ? 2u : 1u));
        assert((int)SS_ARG == (fault == 0 ? -9 : fault == 1 ? -8 : -7));
    } else if (!strcmp(argv[1], "io")) {
        assert(argc == 4); fault = atoi(argv[3]);
        if (fault == 5) reported_size = 0x5cd81;
        if (fault == 6) reported_size = UINT32_MAX;
        if (fault == 7) reported_size = (uint64_t)UINT32_MAX + 1;
        gif_decode_init();
        assert(gd_state == -1 && gfx[0] == 4 && gfx[2] == 0);
        assert(reads == (fault >= 2 && fault <= 4));
        assert(closes == (fault != 1));
    } else return 2;
    /* A rejected frame must not write below its logical canvas before noticing
     * its bad pixel count. The fallback deliberately clears its own 128x192 area. */
    if (gfx[0] != 4 && g_gd.height > 0 && g_gd.height <= 512)
        for (unsigned offset = 0x1000 + (unsigned)g_gd.height * 512; offset < 0x41000; ++offset)
            assert(gfx[offset] == 0xa5);
    free(file_data);
    puts("pass");
    return 0;
}
