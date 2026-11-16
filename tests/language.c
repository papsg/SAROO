/* Host regression for the actual menu language module. No Saturn I/O. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Supply only the platform types and standard string functions used here.
 * The production module itself is included without rewriting its source. */
#define _MAIN_H_
typedef uint8_t u8;
typedef uint32_t u32;
#include "../Firm_Saturn/language.c"

/* Startup metadata cases adapted from paps-g 1ad9fb40. */
static void startup_language(int supplied, char ***languages, int count)
{
    int selected = supplied >= 0 && supplied < count ? supplied : 0;
    lang_id = supplied;
    lang_init();
    assert(lang_id == selected && lang_cur == languages[selected]);
    lang_next();
    assert(lang_id == (selected + 1) % count);
}

static int language_controls(void)
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
    const int wider[] = {INT_MIN, -65536, 65536, INT_MAX};
    for (unsigned i = 0; i < sizeof(wider)/sizeof(wider[0]); ++i)
        startup_language(wider[i], expected, (int)count);
    for (int id = INT16_MIN; id <= INT16_MAX; ++id)
        startup_language(id, expected, (int)count);
    puts("65,540 signed startup IDs preserve valid selections or fall back safely.");
    return 0;
}

/* papsg adaptation of paps-g SSR-021; collision found for this public table. */
static int collision_controls(void)
{
    char collision[] = "probe_uCuB4ef", neighbor[] = "probe_uCuB4eg";
    char source[] = "选择游戏";
    assert(str_hash(collision) == 0x95eb3646u);
    assert(str_hash(source) == 0x95eb3646u);
    assert(str_hash(neighbor) != 0x95eb3646u);
    /* Equal content at different addresses must still translate. Check every
     * actual key/language before the adversarial unknown lookup. */
    for (int language = 0; language < lang_nr; language++) {
        lang_id = language; lang_init();
        for (int key = 0; key < LANG_STR_NR; key++) {
            char copy[256];
            assert(strlen(lang_zhcn[key]) < sizeof copy);
            strcpy(copy, lang_zhcn[key]);
            assert(copy != lang_zhcn[key]);
            assert(!strcmp(TT(copy), lang_list[language][key]));
        }
    }
    for (int language = 0; language < lang_nr; language++) {
        lang_id = language; lang_init();
        assert(TT(neighbor) == neighbor);
        char *actual = TT(collision);
        if (actual != collision) {
            fprintf(stderr, "language=%d hash=%08x unknown=%s returned=%s\n",
                    language, (unsigned)str_hash(collision), collision, actual);
            return 42;
        }
    }
    puts("380 copied keys and 19 full-hash collision refusals passed.");
    return 0;
}

int main(int argc,char **argv)
{
    if(argc==1) {
        int result=language_controls();
        return result?result:collision_controls();
    }
    assert(argc==2);
    if(!strcmp(argv[1],"languages")) return language_controls();
    if(!strcmp(argv[1],"collision")) return collision_controls();
    return 2;
}
