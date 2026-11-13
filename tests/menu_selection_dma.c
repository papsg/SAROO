/* Adapted from paps-g 591f7c2b. Model a deferred last SCU write. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
enum { STRIDE = 512, ROWS = 16, MENU_LB = 8, MENU_RB = 311, START = 40 };
static u8 source[ROWS * STRIDE], screen[(START + ROWS + 1) * STRIDE];
static u8 *sel_pbuf = source, *fbptr = screen;
static int llen = STRIDE;
static u8 *pending_source, *pending_dest;
static size_t pending_bytes;
static unsigned busy, foreign, starts, polls, pending_polls;

static void complete_dma(void)
{
    if (!pending_source) return;
    memcpy(pending_dest, pending_source, pending_bytes);
    pending_source = NULL;
}

static unsigned read_status(void)
{
    ++polls;
    if (!pending_source) return foreign;
    if (pending_polls) {
        --pending_polls;
        return foreign | busy;
    }
    complete_dma();
    return foreign;
}

static void scu_dmemcpy(void *dst, void *src, int size, int channel)
{
    assert(channel == 0 && size == MENU_RB - MENU_LB + 1);
    complete_dma();
    ++starts;
    pending_dest = dst;
    pending_source = src;
    pending_bytes = size;
    pending_polls = busy ? 2 : 0;
    if (!busy) complete_dma();
}

static void *copy_final_row(void *dst, const void *src, size_t size)
{
    assert(!pending_source && "CPU copied while the previous DMA was pending");
    return memcpy(dst, src, size);
}

#define SDSTAT read_status()
#undef memcpy
#define memcpy copy_final_row
#include "selection-dma.inc"
#undef memcpy

static u8 pixel(unsigned y, unsigned x)
{
    return (u8)(17 * y + x);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    busy = (unsigned)strtoul(argv[1], NULL, 0);
    foreign = (unsigned)strtoul(argv[2], NULL, 0);
    memset(screen, 0xa5, sizeof screen);
    for (unsigned row = 0; row < ROWS; ++row)
        for (unsigned x = 0; x < STRIDE; ++x)
            source[row * STRIDE + x] = pixel(row, x);

    put_select_item(1);
    assert(!pending_source && "scroll returned while the final DMA owned its source");
    assert(starts == 15 && polls == (busy ? 3u : 1u));
    memset(source, 0x55, sizeof source);
    complete_dma();
    for (unsigned y = 0; y < START + ROWS + 1; ++y)
        for (unsigned x = 0; x < STRIDE; ++x) {
            u8 expected = y >= START && y < START + ROWS &&
                          x >= MENU_LB && x <= MENU_RB ? pixel(y - START, x) : 0xa5;
            assert(screen[y * STRIDE + x] == expected);
        }
    puts("selection DMA ownership: pass");
}
