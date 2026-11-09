/* Adapted from paps-g 3e072573: actual public C, independent synthetic fixtures. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config-match.inc"

static unsigned u16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    uint8_t header[6];
    unsigned count = 0;
    size_t got;
    while ((got = fread(header, 1, sizeof(header), file)) == sizeof(header)) {
        unsigned sizes[2] = {u16(header), u16(header + 2)};
        unsigned expected = u16(header + 4);
        char *input[2], *original[2];
        assert(expected <= 1);
        for (unsigned i = 0; i < 2; i++) {
            assert(sizes[i] && sizes[i] <= 8192);
            input[i] = malloc(sizes[i]);
            original[i] = malloc(sizes[i]);
            assert(input[i] && original[i]);
            assert(fread(input[i], 1, sizes[i], file) == sizes[i]);
            memcpy(original[i], input[i], sizes[i]);
        }
        for (unsigned reverse = 0; reverse < 2; reverse++) {
            int result = gameid_match(input[reverse], input[1 - reverse]);
            if (result != (int)expected) {
                fprintf(stderr, "Pair %u direction %u: expected %u, got %d\n",
                        count, reverse, expected, result);
                return 1;
            }
            for (unsigned i = 0; i < 2; i++)
                assert(!memcmp(input[i], original[i], sizes[i]));
        }
        for (unsigned i = 0; i < 2; i++) {
            free(input[i]);
            free(original[i]);
        }
        count++;
    }
    assert(!got && !ferror(file) && feof(file));
    assert(!fclose(file));
    printf("{\"pairs\":%u,\"calls\":%u}\n", count, count * 2);
    return 0;
}
