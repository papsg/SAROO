/* Production lookup/decoder/raster fixture; no Saturn register access. */
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

static unsigned be16(const u8 *p) { return (unsigned)p[0]*256+p[1]; }
static unsigned be24(const u8 *p) { return (unsigned)p[0]*65536+(unsigned)p[1]*256+p[2]; }

int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *file = fopen(argv[1], "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long length = ftell(file);
    assert(length > 2 && length < 0x80000 && !fseek(file, 0, SEEK_SET));
    u8 *data = malloc((size_t)length);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    font_cjk = data;
    assert(find_font('A') == find_ucs(font_latin, 'A')); /* Latin-first control. */
    file = fopen(argv[2], "r");
    assert(file);
    unsigned code, checked = 0;
    while (fscanf(file, "%x", &code) == 1) {
        u8 *expected_record = NULL;
        for (unsigned i = 0; i < be16(data); ++i) {
            const u8 *index = data+2+5*i;
            if (be16(index) == code) expected_record = data+be24(index+2);
        }
        assert(expected_record && "required codepoint missing");
        assert(!find_ucs(font_latin, (int)code));
        u8 *glyph = find_font((int)code);
        assert(glyph == expected_record);
        assert(code >= 0x80 && code <= 0xffff);
        char encoded[4] = {0};
        unsigned bytes = 0;
        if (code < 0x800) {
            encoded[bytes++] = (char)(0xc0 | code>>6);
        } else {
            encoded[bytes++] = (char)(0xe0 | code>>12);
            encoded[bytes++] = (char)(0x80 | (code>>6 & 63));
        }
        encoded[bytes++] = (char)(0x80 | (code & 63));
        char *cursor = encoded;
        assert(utf8_to_ucs(&cursor) == (int)code && cursor == encoded+bytes);
        assert(utf8_to_ucs(&cursor) == 0 && cursor == encoded+bytes);

        unsigned width = glyph[1]>>4, height = glyph[1]&15;
        unsigned left = glyph[2]>>4, top = glyph[2]&15;
        unsigned row_bytes = width>8 ? 2 : 1;
        assert(width && height && top+height <= 16 && 4+left+width <= 32);
        u8 actual[2+18*32], expected[sizeof(actual)];
        memset(actual, 0xa5, sizeof(actual));
        memcpy(expected, actual, sizeof(actual));
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                unsigned bit = (glyph[3+y*row_bytes+x/8] >> (7-x%8)) & 1;
                expected[1+(top+y)*32+4+left+x] = (u8)bit;
            }
        }
        assert(fb_draw_font_buf(actual+1, 4, 0x10, glyph, 0, 31) == glyph[0]);
        assert(!memcmp(actual, expected, sizeof(actual)));
        ++checked;
    }
    assert(feof(file) && checked == (unsigned)strtoul(argv[3], NULL, 10));
    fclose(file);
    free(data);
    printf("%u required glyphs pass production lookup, UTF-8 decoding and guarded raster checks\n", checked);
    return 0;
}
