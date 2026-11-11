/* Adapted from paps-g 11836b8c. Compare actual MCU hashes against zlib. */
/* The firmware function reads exactly one title, at every byte alignment. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "title-hash.inc"

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *input = fopen(argv[1], "rb");
    assert(input);
    uint8_t record[68];
    unsigned count = 0;
    size_t bytes;
    while ((bytes = fread(record, 1, sizeof(record), input))) {
        assert(bytes == sizeof(record));
        uint32_t expected = (uint32_t)record[64] | (uint32_t)record[65] << 8 |
                            (uint32_t)record[66] << 16 | (uint32_t)record[67] << 24;
        for (unsigned offset = 0; offset < 8; offset++) {
            /* ASan's right redzone begins immediately after the 64th byte. */
            uint8_t *allocation = malloc(64 + offset);
            assert(allocation);
            memset(allocation, 0xa5, offset);
            uint8_t *title = allocation + offset;
            memcpy(title, record, 64);
            assert(test_title_hash(title) == expected);
            assert(!memcmp(title, record, 64));
            for (unsigned i = 0; i < offset; i++) assert(allocation[i] == 0xa5);
            free(allocation);
        }
        count++;
    }
    assert(!ferror(input) && !fclose(input));
    printf("%u titles, %u aligned/unaligned calls\n", count, count * 8);
}
