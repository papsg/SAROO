/* Actual-source host checks. No Saturn registers or firmware installation. */
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _MAIN_H_
typedef uint8_t u8;
typedef uint32_t u32;
typedef struct menu_desc_t { int marker; } MENU_DESC;

static char displayed[256];
static MENU_DESC menu = {123};
static unsigned displays;
static unsigned console_writes;
static void console_puts(char *text)
{
    (void)text;
    ++console_writes;
}
static void menu_status(MENU_DESC *owner, char *text)
{
    assert(owner == &menu);
    assert(strlen(text) < sizeof(displayed));
    strcpy(displayed, text);
    ++displays;
}
void stm32_puts(char *text) { (void)text; }

/* Keep the firmware's custom API separate from the host libc oracle. */
#define vsnprintf saroo_vsnprintf
#define snprintf saroo_snprintf
#include "../Firm_Saturn/printk.c"
#include "../Firm_Saturn/language.c"
#define sprintf(dst, fmt, ...) saroo_snprintf(dst, 1024, fmt, __VA_ARGS__)
#include "menu_error_source.inc"
#undef sprintf
#undef snprintf
#undef vsnprintf

static void formats(void)
{
    for (unsigned length = 0; length <= 160; ++length) {
        char input[161];
        memset(input, 'x', length);
        input[length] = 0;
        for (int capacity = 1; capacity <= 80; ++capacity) {
            unsigned char guarded[82];
            memset(guarded, 0xa5, sizeof(guarded));
            char *out = (char *)guarded + 1;
            int n = saroo_snprintf(out, capacity, "%s", input);
            unsigned stored = length < (unsigned)capacity-1 ? length : (unsigned)capacity-1;
            assert(out[stored] == 0);
            assert(n == (int)stored+1);
            assert(!memcmp(out, input, stored));
            assert(guarded[0] == 0xa5);
            for (unsigned i = (unsigned)capacity+1; i < sizeof(guarded); ++i)
                assert(guarded[i] == 0xa5);
        }
    }
    char out[80], untouched = 'q';
    assert(saroo_snprintf(&untouched, 0, "x") == 0 && untouched == 'q');
    assert(saroo_snprintf(&untouched, -1, "x") == 0 && untouched == 'q');
    assert(saroo_snprintf(NULL, 10, "x") == 0);
    assert(saroo_snprintf(out, sizeof(out), NULL) == 0);
    char *formats[] = {"%d", "%+d", "%010d", "%-14d", "%+014d", "%14d"};
    int values[] = {INT_MIN, INT_MAX, -123, -1, 0, 1, 123};
    for (unsigned i = 0; i < sizeof(formats)/sizeof(formats[0]); ++i) {
        for (unsigned j = 0; j < sizeof(values)/sizeof(values[0]); ++j) {
            char expected[80];
            snprintf(expected, sizeof(expected), formats[i], values[j]);
            int n = saroo_snprintf(out, sizeof(out), formats[i], values[j]);
            assert(!strcmp(out, expected) && n == (int)strlen(expected)+1);
        }
    }
    assert(saroo_snprintf(out, sizeof(out), "%u %x %X %o", UINT_MAX, UINT_MAX,
                          UINT_MAX, UINT_MAX) == sizeof("4294967295 ffffffff FFFFFFFF 37777777777"));
    assert(!strcmp(out, "4294967295 ffffffff FFFFFFFF 37777777777"));
    assert(saroo_snprintf(out, sizeof(out), "a\nb %% %c %s", 'z', NULL) == sizeof("a\r\nb % z <NUL>"));
    assert(!strcmp(out, "a\r\nb % z <NUL>"));
    assert(saroo_snprintf(out, 8, "%999999999999999999999999999d", 1) == 8);
    assert(!strcmp(out, "       "));
    /* A rejected format must not publish uninitialized console storage. */
    printk_puts = console_puts;
    assert(printk(NULL) == 0 && console_writes == 0);
    assert(printk("valid") == sizeof("valid") && console_writes == 1);
    puts("formatter capacities, signs, widths, word extrema and valid controls passed");
}

static void actual_error(void)
{
    lang_id = 12; /* Existing Greek: ordinary error 1 exceeds the old 40 bytes. */
    lang_init();
    char expected[256];
    snprintf(expected, sizeof(expected), TT("游戏启动失败! %d"), 1);
    assert(strlen(expected)+1 > 40);
    test_game_error(&menu, 1);
    assert(!strcmp(displayed, expected) && displays == 1);
    puts("actual game-error call preserves the complete existing Greek message");
}

static void translated_errors(void)
{
#ifdef HAS_MENU_ERROR
    const int values[] = {INT_MIN, INT_MAX, -2, -1, 0, 1};
    unsigned cases = 0;
    for (int id = 0; id < lang_nr; ++id) {
        lang_id = id;
        lang_init();
        for (unsigned v = 0; v < sizeof(values)/sizeof(values[0]); ++v) {
            for (unsigned kind = 0; kind < 3; ++kind) {
                char expected[256];
                unsigned before = displays;
                if (kind == 2) {
                    snprintf(expected, sizeof(expected), TT("不是游戏光盘!%d"), values[v]);
                    test_disc_error(&menu, values[v]);
                } else {
                    char *format = TT(kind ? "文件加载失败! %d" : "游戏启动失败! %d");
                    snprintf(expected, sizeof(expected), format, values[v]);
                    menu_error(&menu, format, values[v]);
                }
                assert(displays == before+1 && !strcmp(displayed, expected));
                ++cases;
            }
        }
        /* All real selector-title storage is 64 bytes. Check full normal
         * labels and independent byte-prefix truncation at extreme values. */
        for (unsigned kind = 0; kind < 2; ++kind) {
            char *format = TT(kind ? "选择文件(%d/%d)" : "选择游戏(%d/%d)");
            const int pages[] = {0, 1, 214, INT_MAX};
            for (unsigned p = 0; p < sizeof(pages)/sizeof(pages[0]); ++p) {
                char expected[64], title[64];
                int length = snprintf(expected, sizeof(expected), format, pages[p], pages[p]);
                if (pages[p] == 214) assert(length < (int)sizeof(title));
                saroo_snprintf(title, sizeof(title), format, pages[p], pages[p]);
                assert(!strcmp(title, expected));
            }
        }
    }
    printf("%u complete translated errors and all selector titles passed\n", cases);
#else
    assert(!"bounded menu_error helper missing");
#endif
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "formats")) formats();
    else if (!strcmp(argv[1], "actual-error")) actual_error();
    else if (!strcmp(argv[1], "translations")) translated_errors();
    else assert(!"unknown test mode");
    return 0;
}
