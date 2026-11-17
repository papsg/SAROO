/* Production decoder, font lookup and raster control for SSR-158.
 * Adapted by papsg from paps-g 20c97a9d; no Saturn register access. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
#include "../Firm_Saturn/font_latin.h"
static const u8 *font_cjk;
static int llen = 32;
#include "font_renderer_source.inc"

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size > 2 && size < 0x80000 && !fseek(file, 0, SEEK_SET));
    u8 *data = malloc((size_t)size);
    assert(data && fread(data, 1, (size_t)size, file) == (size_t)size);
    assert(!fclose(file));
    font_cjk = data;

    char label[] = "\xf0\x9f\x98\x80";
    char *cursor = label;
    int scalar = utf8_to_ucs(&cursor);
    assert(scalar == 0x1f600 && cursor == label + 4);
    assert(utf8_to_ucs(&cursor) == 0 && cursor == label + 4);
    u8 *square = find_font(0x25a1);
    assert(square && find_font(scalar) == square);
    assert(find_font(0x4e2d) != square); /* Existing BMP glyph stays distinct. */

    u8 actual[32 * 16 + 2], expected[sizeof(actual)];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(expected, actual, sizeof(actual));
    int advance = fb_draw_font_buf(actual + 1, 0, 0x10, find_font(scalar), 0, 31);
    assert(advance == fb_draw_font_buf(expected + 1, 0, 0x10, square, 0, 31));
    assert(!memcmp(actual, expected, sizeof(actual)));
    free(data);
    puts("one supplementary scalar renders one guarded fallback square");
    return 0;
}
