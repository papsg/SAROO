/* Actual menu status rendering with a guarded, 512-byte-stride framebuffer. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef struct MENU_DESC MENU_DESC;
#include "../Firm_Saturn/font_latin.h"

enum { fbw = 320, llen = 512, status_left = 16, status_right = 303,
       status_top = 219, status_height = 16, frame_height = 240 };
static u8 frame[2 + llen*frame_height], expected[sizeof(frame)];
static u8 *fbptr = frame+1;
static const u8 *font_cjk;
static int text_color = 0x10;
#define VDP2_VRAM (frame+1)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "font_renderer_source.inc"
#pragma GCC diagnostic pop

static void expect_status(char *text)
{
    memset(expected, 0xa5, sizeof(expected));
    for (int y = status_top; y < status_top+status_height; ++y)
        memset(expected+1+y*llen+status_left, 0, status_right-status_left+1);
    if (text) {
        fbptr = expected+1;
        menu_draw_string(status_left, status_top, text_color, text, status_left, status_right);
        fbptr = frame+1;
    }
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    FILE *font = fopen(argv[1], "rb");
    assert(font && !fseek(font, 0, SEEK_END));
    long length = ftell(font);
    assert(length > 2 && length < 0x60000 && !fseek(font, 0, SEEK_SET));
    u8 *data = malloc((size_t)length);
    assert(data && fread(data, 1, (size_t)length, font) == (size_t)length);
    assert(!fclose(font));
    font_cjk = data;

    memset(frame, 0xa5, sizeof(frame));
    char long_status[81], short_status[] = "Ready", empty[] = "";
    memset(long_status, 'W', sizeof(long_status)-1);
    long_status[sizeof(long_status)-1] = '\0';

    if (!strcmp(argv[2], "bounds")) {
        menu_status(NULL, long_status);
        expect_status(long_status);
        assert(!memcmp(frame, expected, sizeof(frame)) && "status writes outside its cleared area");
    } else if (!strcmp(argv[2], "replace")) {
        menu_status(NULL, long_status);
        menu_status(NULL, short_status);
        expect_status(short_status);
        assert(!memcmp(frame, expected, sizeof(frame)) && "replacement leaves an old suffix");
        menu_status(NULL, NULL);
        expect_status(NULL);
        assert(!memcmp(frame, expected, sizeof(frame)));
    } else {
        assert(!strcmp(argv[2], "controls"));
        menu_status(NULL, short_status);
        expect_status(short_status);
        assert(!memcmp(frame, expected, sizeof(frame)));
        menu_status(NULL, empty);
        expect_status(NULL);
        assert(!memcmp(frame, expected, sizeof(frame)));
    }
    free(data);
    return 0;
}
