#include "main.h"
#include "ff.h"
#include "cdc.h"
/* Bounded decoder adapted from paps-g 3e62998c; original repository notices apply. */

/* GIF data shares the unused CD block pool while the menu owns it. */
#define GIF_FILE_BYTES (0xa000u + MAX_BLOCKS * 2352u - 0x20000u)
#define GIF_STRIDE 512u
#define GIF_HEIGHT 512u
#define GIF_PIXEL_OFFSET 0x1000u
_Static_assert(GIF_PIXEL_OFFSET + GIF_STRIDE * GIF_HEIGHT <= COVER_INDEX_OFFSET,
               "GIF framebuffer must end before the cover index");
#define SCR_W 320
#define SCR_H 240

typedef struct {
    const u8 *next, *end, *start, *global_pal;
    u32 bits, gtimer, nbits, left;
    u16 width, height, x, y, w, h, delay;
    u16 global_colors, colors;
    u8 flags, transparent, interlaced, frames;
} GIF_DECODER;

static GIF_DECODER g_gd;
/* Twelve prefix bits and an eight-bit suffix fit in one aligned entry. */
static u32 *const gif_dict = (u32 *)0x24011000;
static u8 *const gif_stack = (u8 *)0x24010000;
static u8 *const gifbuf = (u8 *)(0x61400000 + GIF_PIXEL_OFFSET);
static u32 *const gif_pal = (u32 *)0x61400100;
volatile int gd_state = -1;
static volatile int in_decoding;

static const u8 *gif_take(GIF_DECODER *gd, unsigned size)
{
    if (size > (unsigned)(gd->end - gd->next)) return NULL;
    const u8 *p = gd->next;
    gd->next += size;
    return p;
}

static unsigned gif_u16(const u8 *p) { return p[0] | (unsigned)p[1] << 8; }

/* Read directly from bounded sub-blocks; no block copies or speculative bytes. */
static int get_code(GIF_DECODER *gd, unsigned width)
{
    while (gd->nbits < width) {
        if (!gd->left) {
            const u8 *p = gif_take(gd, 1);
            if (!p || !*p || *p > (unsigned)(gd->end - gd->next)) return -1;
            gd->left = *p;
        }
        gd->bits |= (u32)*gd->next++ << gd->nbits;
        gd->nbits += 8;
        --gd->left;
    }
    int code = gd->bits & ((1u << width) - 1);
    gd->bits >>= width;
    gd->nbits -= width;
    return code;
}

static int gif_skip_blocks(GIF_DECODER *gd)
{
    if (!gif_take(gd, gd->left)) return -1;
    gd->left = 0;
    for (;;) {
        const u8 *p = gif_take(gd, 1);
        if (!p) return -1;
        if (!*p) return 0;
        if (!gif_take(gd, *p)) return -1;
    }
}

static void gif_palette(GIF_DECODER *gd, const u8 *p, unsigned count)
{
    gd->colors = count;
    for (unsigned i = 0; i < count; ++i, p += 3)
        gif_pal[i] = (u32)p[0] << 16 | (u32)p[1] << 8 | p[2];
}

static int gif_decode_clip(GIF_DECODER *gd)
{
    const u8 *p = gif_take(gd, 9);
    if (!p) return -1;
    gd->x = gif_u16(p); gd->y = gif_u16(p + 2);
    gd->w = gif_u16(p + 4); gd->h = gif_u16(p + 6);
    unsigned total = (unsigned)gd->w * gd->h;
    if (!total || gd->x + gd->w > gd->width ||
        gd->y + gd->h > gd->height) return -1;
    gd->interlaced = p[8] & 0x40;
    unsigned count = gd->global_colors;
    const u8 *pal = gd->global_pal;
    if (p[8] & 0x80) {
        count = 2u << (p[8] & 7);
        pal = gif_take(gd, count * 3);
    }
    if (!pal || !count) return -1;
    gif_palette(gd, pal, count);
    p = gif_take(gd, 1);
    if (!p || *p < 2 || *p > 8) return -1;
    unsigned clear = 1u << *p;
    unsigned initial_width = *p + 1, width = initial_width, top = clear + 2;
    /* One sentinel removes a transparency-flag test from every output pixel. */
    int transparent = gd->flags & 1 ? gd->transparent : -1;
    unsigned x = 0, pass = 0, y = 0, previous_first = 0;
    int previous = -1;
    gd->left = gd->nbits = 0;
    gd->bits = 0;
    for (;;) {
        unsigned code = get_code(gd, width);
        if ((code & ~1u) == clear) {
            if (code & 1u) return total == 0 ? gif_skip_blocks(gd) : -1;
            top = clear + 2; width = initial_width; previous = -1;
            continue;
        }
        /* width never exceeds 12; a negative reader result also fails here. */
        if (code > top || (previous < 0 && code >= clear)) return -1;
        unsigned input = code, length = 0;
        if (code == top) {
            gif_stack[length++] = previous_first;
            code = previous;
        }
        while (code >= clear) {
            /* Constructed prefixes always point to an earlier dictionary entry. */
            u32 entry = gif_dict[code];
            gif_stack[length++] = entry;
            code = entry >> 8;
        }
        /* Every stored suffix came from an already validated literal. */
        if (code >= gd->colors) return -1;
        gif_stack[length++] = code;
        if (previous >= 0 && top < 4096) {
            gif_dict[top++] = (unsigned)previous << 8 | code;
            if (top == (1u << width) && width < 12) ++width;
        }
        previous = input; previous_first = code;
        if (length > total) return -1;
        total -= length;
        while (length) {
            unsigned value = gif_stack[--length];
            if (value != (unsigned)transparent)
                gifbuf[(gd->y + y) * GIF_STRIDE + gd->x + x] = value;
            if (++x == gd->w) {
                x = 0;
                if (gd->interlaced) {
                    y += 8u >> (pass ? pass - 1 : 0);
                    /* Four passes: starts 0/4/2/1, strides 8/8/4/2. */
                    while (y >= gd->h && pass < 3) y = 4u >> pass++;
                } else ++y;
            }
        }
    }
}

/* The only caller has checked the complete 13-byte header and file extent. */
static int gif_decode_info(GIF_DECODER *gd, u8 *data, int size)
{
    if (memcmp(data, "GIF8", 4) || (data[4] != '7' && data[4] != '9') || data[5] != 'a') return -1;
    memset(gd, 0, sizeof(*gd));
    gd->width = gif_u16(data + 6); gd->height = gif_u16(data + 8);
    if ((unsigned)(gd->width - 1) >= GIF_STRIDE ||
        (unsigned)(gd->height - 1) >= GIF_HEIGHT)
        return -1;
    gd->next = data + 13; gd->end = data + size;
    if (data[10] & 0x80) {
        gd->global_colors = 2u << (data[10] & 7);
        gd->global_pal = gif_take(gd, gd->global_colors * 3);
        if (!gd->global_pal) return -1;
    }
    gd->start = gd->next;
    return 0;
}

/* 0: complete frame, 1: trailer, -1: invalid or truncated input. */
int gif_decode_frame(GIF_DECODER *gd)
{
    gd->flags = gd->delay = 0;
    for (;;) {
        const u8 *p = gif_take(gd, 1);
        if (!p) return -1;
        if (*p == 0x3b) return 1;
        if (*p == 0x2c) return gif_decode_clip(gd);
        if (*p != 0x21) return -1;
        p = gif_take(gd, 1);
        if (!p) return -1;
        if (*p == 0xf9) {
            p = gif_take(gd, 6);
            if (!p || p[0] != 4 || p[5] != 0) return -1;
            gd->flags = p[1]; gd->delay = gif_u16(p + 2); gd->transparent = p[4];
        } else if (gif_skip_blocks(gd)) return -1;
    }
}

static void gif_blank(void)
{
    *(u32 *)0x61400004 = 0x00800080;
    *(u32 *)0x61400008 = 0;
    *(u32 *)0x6140000c = 0x005800b0;
    memset(gifbuf, 0, 128 * 192);
    *(volatile u8 *)0x61400000 = 4;
}

void gif_decode_init(void)
{
    gif_decode_exit();
    FIL fp;
    u8 *data = (u8 *)0x24020000;
    GIF_DECODER *gd = &g_gd;
    *(u32 *)0x61400000 = 0;
    if (f_open(&fp, "/SAROO/mainmenu_bg.gif", FA_READ)) { gif_blank(); return; }
    FSIZE_t size = f_size(&fp);
    u32 read = 0;
    int result = -1;
    if (size >= 13 && size <= GIF_FILE_BYTES)
        result = f_read(&fp, data, (u32)size, &read);
    int closed = f_close(&fp);
    if (result || closed || read != size || gif_decode_info(gd, data, (int)size)) {
        gif_blank();
        return;
    }
    memset(gifbuf, 0, gd->height * GIF_STRIDE);
    *(u16 *)0x6140000c = (SCR_W - gd->width) / 2;
    *(u16 *)0x6140000e = (SCR_H - gd->height) / 2;
    __atomic_store_n(&gd_state, 0, __ATOMIC_RELEASE);
}

void gif_decode_timer(void)
{
    if (__atomic_load_n(&gd_state, __ATOMIC_RELAXED) < 0) return;
    /* Only main_task calls this timer while its shell waits for input. */
    __atomic_store_n(&in_decoding, 1, __ATOMIC_SEQ_CST);
    GIF_DECODER *gd = &g_gd;
    int state = __atomic_load_n(&gd_state, __ATOMIC_ACQUIRE), next_state = state;
    if (state < 0) goto done;
    if (state == 0) {
        int result = gif_decode_frame(gd);
        if (result < 0) {
            next_state = -1;
        } else if (result) {
            if (gd->frames > 1) { gd->next = gd->start; gd->frames = 0; }
            else next_state = -1;
        } else {
            /* Saturation distinguishes a static image without an overflow. */
            gd->frames = gd->frames ? 2 : 1;
            next_state = 1;
        }
    } else if (state == 1) {
        if (!(*(volatile u32 *)0x61400000 & 0x00ff00ffu)) {
            next_state = 0;
            if (gd->delay) { gd->gtimer = osKernelGetTickCount(); next_state = 2; }
        }
    } else if ((u32)(osKernelGetTickCount() - gd->gtimer) >= gd->delay) {
        next_state = 0;
    }
    /* Waiting needs no atomic write. Every transition preserves cancellation. */
    if (next_state == state) goto done;
    if (__atomic_compare_exchange_n(&gd_state, &state, next_state, 0,
                                    __ATOMIC_RELAXED, __ATOMIC_RELAXED) &&
        next_state == 1) {
        *(u16 *)0x61400004 = gd->w; *(u16 *)0x61400006 = gd->h;
        *(u16 *)0x61400008 = gd->x; *(u16 *)0x6140000a = gd->y;
        __sync_synchronize();
        *(volatile u8 *)0x61400002 = 1;
        *(volatile u8 *)0x61400000 = 1;
    }
done:
    __atomic_store_n(&in_decoding, 0, __ATOMIC_RELEASE);
}

void gif_decode_exit(void)
{
    __atomic_store_n(&gd_state, -1, __ATOMIC_SEQ_CST);
    while (__atomic_load_n(&in_decoding, __ATOMIC_ACQUIRE)) osDelay(1);
}
