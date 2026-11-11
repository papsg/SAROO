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
    assert(argc == 4 || argc == 5);
    FILE *file = fopen(argv[1], "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long length = ftell(file);
    assert(length > 2 && length < 0x80000 && !fseek(file, 0, SEEK_SET));
    u8 *data = malloc((size_t)length);
    assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    font_cjk = data;
    assert(find_font('A') == find_ucs(font_latin, 'A')); /* Latin-first control. */
    u8 *table = data;
    size_t table_size = (size_t)length;
    if (argc == 5) {
        assert(!strcmp(argv[4], "latin"));
        table = font_latin;
        table_size = sizeof(font_latin);
    }
    file = fopen(argv[2], "r");
    assert(file);
    unsigned code, checked = 0;
    while (fscanf(file, "%x", &code) == 1) {
        unsigned count = be16(table);
        assert(2 + 5*count <= table_size);
        u8 *expected_record = NULL;
        size_t record_size = 0;
        for (unsigned i = 0; i < count; ++i) {
            const u8 *index = table+2+5*i;
            if (be16(index) == code) {
                size_t start = be24(index+2);
                size_t end = i+1 < count ? be24(index+7) : table_size;
                assert(start+3 <= end && end <= table_size);
                expected_record = table+start;
                record_size = end-start;
                break;
            }
        }
        assert(expected_record && "required codepoint missing");
        if (table == data)
            assert(!find_ucs(font_latin, (int)code));
        u8 *glyph = find_font((int)code);
        assert(glyph == expected_record);

        assert(code && code <= 0xffff);
        char encoded[4] = {0};
        unsigned bytes = 0;
        if (code < 0x80) {
            encoded[bytes++] = (char)code;
        } else {
            if (code < 0x800) {
                encoded[bytes++] = (char)(0xc0 | code>>6);
            } else {
                encoded[bytes++] = (char)(0xe0 | code>>12);
                encoded[bytes++] = (char)(0x80 | (code>>6 & 63));
            }
            encoded[bytes++] = (char)(0x80 | (code & 63));
        }
        char *cursor = encoded;
        assert(utf8_to_ucs(&cursor) == (int)code && cursor == encoded+bytes);
        assert(utf8_to_ucs(&cursor) == 0 && cursor == encoded+bytes);

        unsigned width = glyph[1]>>4;
        unsigned left = glyph[2]>>4, top = glyph[2]&15;
        unsigned row_bytes = width>8 ? 2 : 1;
        assert((record_size-3) % row_bytes == 0);
        unsigned height = (unsigned)((record_size-3)/row_bytes);
        assert((width != 0) == (height != 0));
        assert((height & 15) == (glyph[1] & 15));
        assert(4+left+width <= 32);
        u8 actual[2+32*32], expected[sizeof(actual)];
        memset(actual, 0xa5, sizeof(actual));
        memcpy(expected, actual, sizeof(actual));
        for (unsigned y = 0; y < height && top+y < 16; ++y) {
            for (unsigned x = 0; x < width; ++x) {
                unsigned bit = (glyph[3+y*row_bytes+x/8] >> (7-x%8)) & 1;
                expected[1+(top+y)*32+4+left+x] = (u8)bit;
            }
        }
        /* Put the empty record in an exact-size allocation so ASan can see
         * any attempt to read bitmap bytes after its header. */
        u8 *empty = NULL;
        if (!width) {
            empty = malloc(record_size);
            assert(empty);
            memcpy(empty, glyph, record_size);
        }
        assert(fb_draw_font_buf(actual+1, 4, 0x10, empty ? empty : glyph, 0, 31) == glyph[0]);
        free(empty);
        assert(!memcmp(actual, expected, sizeof(actual)) && "raster differs from indexed bitmap");
        assert(top+height <= 16 && "glyph exceeds the row cell");
        ++checked;
    }
    assert(feof(file) && checked == (unsigned)strtoul(argv[3], NULL, 10));
    fclose(file);
    free(data);
    printf("%u required glyphs pass production lookup, UTF-8 decoding and guarded raster checks\n", checked);
    return 0;
}
