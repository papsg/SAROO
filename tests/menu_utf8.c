/* Adapted from paps-g SSR-158 / 20c97a9d. The extracted production decoder
 * runs against Python's independent byte oracle on a host with sanitizers. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
_Static_assert(CHAR_BIT == 8 && INT_MAX == 2147483647, "menu scalar contract");
#include "decoder.inc"

static void scalar(const char *bytes, int expected, int extent)
{
    char *p = (char *)bytes;
    int actual = utf8_to_ucs(&p);
    if (actual != expected || p - bytes != extent) {
        fprintf(stderr, "scalar expected=%X/%d actual=%X/%td\n",
                expected, extent, actual, p - bytes);
        exit(42);
    }
    assert(utf8_to_ucs(&p) == 'Z');
    char *end = p;
    assert(utf8_to_ucs(&p) == 0 && p == end);
}

static uint32_t word(FILE *file)
{
    unsigned char bytes[4];
    assert(fread(bytes, 1, 4, file) == 4);
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 |
           (uint32_t)bytes[2] << 8 | bytes[3];
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    if (!strcmp(argv[1], "controls")) {
        scalar("AZ", 0x41, 1);
        scalar("\xc3\xa9Z", 0xe9, 2);
        scalar("\xe4\xb8\xadZ", 0x4e2d, 3);
        scalar("\xef\xba\x8fZ", 0xfe8f, 3);
        scalar("\xf0\x9f\x98\x80Z", 0x1f600, 4);
        scalar("\xf0\xa0\x80\x80Z", 0x20000, 4);
        scalar("\xf4\x8f\xbf\xbfZ", 0x10ffff, 4);
        puts("pass: seven named scalar/extent/sentinel controls");
        return 0;
    }
    assert(argc == 3);
    FILE *file = fopen(argv[2], "rb");
    assert(file);
    if (!strcmp(argv[1], "scalars")) {
        assert(!fseek(file, 0, SEEK_END));
        long size = ftell(file);assert(size > 0);rewind(file);
        char *data = malloc((size_t)size + 1);assert(data);
        assert(fread(data, 1, (size_t)size, file) == (size_t)size);
        data[size] = 0;
        char *p = data;unsigned count = 0;
        for (int expected = 1; expected <= 0x10ffff; expected++) {
            if (expected >= 0xd800 && expected <= 0xdfff) continue;
            int extent = expected < 0x80 ? 1 : expected < 0x800 ? 2 : expected < 0x10000 ? 3 : 4;
            char *before = p;
            int actual = utf8_to_ucs(&p);
            if (actual != expected || p - before != extent) {
                fprintf(stderr, "scalar expected=%X/%d actual=%X/%td\n",
                        expected, extent, actual, p - before);
                return 42;
            }
            count++;
        }
        assert(p == data + size && utf8_to_ucs(&p) == 0 && p == data + size);
        free(data);
        printf("pass: %u nonzero Unicode scalars, %ld reference bytes\n", count, size);
    } else {
        assert(!strcmp(argv[1], "malformed"));
        unsigned cases = 0, calls = 0;
        while (fgetc(file) != EOF) {
            assert(!fseek(file, -1, SEEK_CUR));
            unsigned length = word(file), count = word(file);
            assert(length <= 33 && count <= 33);
            /* Exact allocation puts the byte after NUL in ASan's redzone. */
            char *data = malloc(length + 1);assert(data);
            assert(fread(data, 1, length, file) == length);data[length] = 0;
            char *p = data;
            for (unsigned i = 0; i < count; i++) {
                unsigned expected = word(file), extent = word(file);
                char *before = p;
                int actual = utf8_to_ucs(&p);
                if (actual != (int)expected || p - before != (int)extent) {
                    fprintf(stderr, "case=%u index=%u expected=%X/%u actual=%X/%td\n",
                            cases, i, expected, extent, actual, p - before);
                    return 42;
                }
                calls++;
            }
            assert(p == data + length && utf8_to_ucs(&p) == 0 && p == data + length);
            free(data);cases++;
        }
        assert(!ferror(file));
        printf("pass: %u guarded strings, %u exact subpart/scalar calls\n", cases, calls);
    }
    assert(!fclose(file));
    return 0;
}
