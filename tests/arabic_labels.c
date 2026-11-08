/* Host execution of the production translation, formatter, decoder and raster. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _MAIN_H_
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
void stm32_puts(char *text) { (void)text; }
#define snprintf saroo_snprintf
#define vsnprintf saroo_vsnprintf
#include "../Firm_Saturn/printk.c"
#include "../Firm_Saturn/language.c"
#undef snprintf
#undef vsnprintf
#include "../Firm_Saturn/font_latin.h"
static const u8 *font_cjk;
static int llen = 320;
#include "font_renderer_source.inc"

static const u8 *reference_glyph(unsigned code)
{
    const u8 *fonts[] = {font_latin, font_cjk};
    for (unsigned f = 0; f < 2; ++f) {
        const u8 *data = fonts[f];
        unsigned count = (unsigned)data[0]*256+data[1];
        for (unsigned i = 0; i < count; ++i) {
            const u8 *entry = data+2+5*i;
            if ((unsigned)entry[0]*256+entry[1] == code)
                return data+(unsigned)entry[2]*65536+(unsigned)entry[3]*256+entry[4];
        }
    }
    return NULL;
}

static unsigned reference_decode(const unsigned char **cursor)
{
    const unsigned char *s = *cursor;
    if (*s < 128) { *cursor = s+1; return *s; }
    if (*s < 224) { *cursor = s+2; return (unsigned)(s[0]&31)*64+(s[1]&63); }
    *cursor = s+3;
    return (unsigned)(s[0]&15)*4096+(unsigned)(s[1]&63)*64+(s[2]&63);
}

static const u8 *reference_resolved(unsigned code)
{
    const u8 *glyph = reference_glyph(code);
    /* Preserve the old renderer's replacement-square behavior for pre-existing
     * missing glyphs. New Arabic coverage is checked separately without fallback. */
    return glyph ? glyph : reference_glyph(0x25a1);
}

static void raster(const char *text, FILE *image, int capture)
{
    u8 actual[2+18*320], expected[sizeof(actual)];
    memset(actual, 0xa5, sizeof(actual));
    memcpy(expected, actual, sizeof(actual));
    char *cursor = (char *)text;
    int code, x = 16;
    while ((code = utf8_to_ucs(&cursor)) != 0) {
        u8 *glyph = find_font(code);
        assert(glyph && glyph == reference_resolved((unsigned)code));
        x += fb_draw_font_buf(actual+1, x, 0x10, glyph, 16, 303);
    }
    if (capture) assert(x <= 304);
    const unsigned char *reference = (const unsigned char *)text;
    unsigned position = 16;
    while (*reference) {
        const u8 *glyph = reference_resolved(reference_decode(&reference));
        assert(glyph);
        unsigned width = glyph[1]>>4, height = glyph[1]&15;
        unsigned left = glyph[2]>>4, top = glyph[2]&15;
        assert(top+height <= 18);
        if (capture) assert(top+height <= 16 && position+left+width <= 304);
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned px = 0; px < width; ++px) {
                unsigned row_bytes = width <= 8 ? 1 : 2;
                unsigned screen_x = position+left+px;
                if (screen_x >= 16 && screen_x <= 303)
                    expected[1+(top+y)*320+screen_x] =
                        (glyph[3+y*row_bytes+px/8] >> (7-px%8)) & 1;
            }
        }
        position += glyph[0];
    }
    assert(position == (unsigned)x && !memcmp(actual, expected, sizeof(actual)));
    if (capture) {
        for (unsigned i = 1; i <= 18*320; ++i) {
            int pixel = actual[i] == 1 ? 255 : 0;
            assert(fputc(pixel, image) != EOF);
        }
    }
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *font = fopen(argv[1], "rb");
    assert(font && !fseek(font, 0, SEEK_END));
    long length = ftell(font);
    assert(length > 2 && length < 0x60000 && !fseek(font, 0, SEEK_SET));
    u8 *data = malloc((size_t)length);
    assert(data && fread(data, 1, (size_t)length, font) == (size_t)length);
    fclose(font);
    font_cjk = data;
    FILE *cases = fopen(argv[2], "r"), *image = fopen(argv[3], "wb");
    assert(cases && image);
    assert(fprintf(image, "P5\n320 648\n255\n") > 0); /* 36 independent Arabic cases. */
    char line[512];
    unsigned checked = 0, arabic = 0;
    while (fgets(line, sizeof(line), cases)) {
        int id, slot, arg1, arg2, offset = 0;
        assert(sscanf(line, "%d %d %d %d\t%n", &id, &slot, &arg1, &arg2, &offset) == 4);
        assert(offset && id >= 0 && id < lang_nr && slot >= 0 && slot < LANG_STR_NR);
        char *expected = line+offset;
        size_t n = strlen(expected);
        assert(n && expected[n-1] == '\n');
        expected[n-1] = 0;
        lang_id = id;
        lang_init();
        char actual[256];
        saroo_snprintf(actual, sizeof(actual), TT(lang_zhcn[slot]), arg1, arg2);
        assert(!strcmp(actual, expected));
        raster(actual, image, id == 18);
        arabic += id == 18;
        ++checked;
    }
    assert(feof(cases) && checked == 684 && arabic == 36);
    assert(!fclose(image));
    fclose(cases);
    free(data);
    printf("%u unchanged prior-language and %u independent Arabic cases pass actual formatting and guarded raster\n",
           checked-arabic, arabic);
    return 0;
}
