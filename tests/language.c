/* Host regression for the actual menu language module. No Saturn I/O. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Supply only the platform types and standard string functions used here.
 * The production module itself is included without rewriting its source. */
#define _MAIN_H_
typedef uint8_t u8;
typedef uint32_t u32;
#include "../Firm_Saturn/language.c"

int main(void)
{
    char **expected[] = {lang_zhcn, lang_en, lang_ptbr, lang_ja, lang_fr,
        lang_ru, lang_zhtw, lang_de, lang_es, lang_it, lang_pl, lang_swe,
        lang_el, lang_ro, lang_pt, lang_cym, lang_nl, lang_ca, lang_ar};
    const unsigned count = sizeof(expected)/sizeof(expected[0]);
    unsigned lookups = 0;

    /* The old initialization performs out-of-bounds writes before lookup. */
    lang_id = 1;
    lang_init();
    assert(lang_nr == (int)count);
    for (unsigned i = 0; i < count; ++i)
        assert(lang_list[i] == expected[i]);
    assert(!strcmp(TT("选择游戏"), "Select Game"));

    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        for (unsigned language = 0; language < count; ++language) {
            lang_id = (int)language;
            lang_init();
            for (unsigned key = 0; key < LANG_STR_NR; ++key) {
                char copy[256];
                assert(strlen(lang_zhcn[key]) < sizeof(copy));
                strcpy(copy, lang_zhcn[key]);
                assert(!strcmp(TT(copy), expected[language][key]));
                ++lookups;
            }
            char unknown[] = "This label has no translation";
            assert(TT(unknown) == unknown);
            lang_next();
            assert(lang_id == (int)((language + 1) % count));
            for (unsigned key = 0; key < LANG_STR_NR; ++key) {
                assert(!strcmp(TT(lang_zhcn[key]), expected[lang_id][key]));
                ++lookups;
            }
        }
    }
    lang_id = (int)count;
    lang_init();
    assert(lang_id == 0 && lang_cur == lang_zhcn);
    printf("%u language IDs, %u translated lookups, repeated initialization, "
           "selection/wrap, copied keys and unknown-label controls passed.\n",
           count, lookups);
    return 0;
}
