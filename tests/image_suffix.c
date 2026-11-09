/* Independent suffix oracle adapted from paps-g 96f9329a. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned cases;
#include "suffix.inc"
static int independent_suffix(const char *name, unsigned which)
{
    static const char *const forms[2][8] = {
        {".cue", ".Cue", ".cUe", ".CUe", ".cuE", ".CuE", ".cUE", ".CUE"},
        {".iso", ".Iso", ".iSo", ".ISo", ".isO", ".IsO", ".iSO", ".ISO"}
    };
    for (unsigned i = 0; i < 8; ++i) if (!strcmp(name, forms[which][i])) return 1;
    return 0;
}
static void check_suffix(const char *text)
{
    for (unsigned which = 0; which < 2; ++which) {
        assert(image_suffix(text, which ? ".iso" : ".cue") == independent_suffix(text, which));
        ++cases;
    }
}
static void suffixes(void)
{
    for (unsigned which = 0; which < 2; ++which)
    for (unsigned upper = 0; upper < 8; ++upper) {
        char base[5]; memcpy(base, which ? ".iso" : ".cue", 5);
        for (unsigned j = 0; j < 3; ++j) if (upper & (1u << j)) base[j+1] -= 32;
        for (unsigned cut = 0; cut <= 4; ++cut) {
            /* Exact small allocations make reads after the terminator visible to ASan. */
            char *short_name = malloc(cut+1); assert(short_name);
            memcpy(short_name, base, cut); short_name[cut] = 0;
            check_suffix(short_name); free(short_name);
        }
        for (unsigned place = 0; place < 5; ++place)
        for (unsigned byte = 0; byte < 256; ++byte) {
            char text[7] = {0}; memcpy(text, base, 5);
            text[place] = byte; check_suffix(text);
        }
    }
    check_suffix(".c\xc3\xbc" "e"); check_suffix(".\xc4\xb0so");
    check_suffix(".cuex"); check_suffix(".cue.iso"); check_suffix(".iso.cue");
}
int main(void) { suffixes(); printf("pass: %u suffix checks\n", cases); return 0; }
