/* Use the complete firmware decoder with independent host-generated sectors. */
#include "sector_overlay.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RAW_BYTES = 2352 };
static const char product[] = "T-TEST    ";

static size_t load(const char *path, uint8_t *data, size_t capacity)
{
    FILE *fp = fopen(path, "rb");
    assert(fp);
    size_t size = fread(data, 1, capacity, fp);
    assert(!ferror(fp) && fgetc(fp) == EOF && !fclose(fp));
    return size;
}

int main(int argc, char **argv)
{
    assert(argc >= 3);
    uint8_t *pack = sector_overlay_staging();
    size_t size = load(argv[2], pack, SECTOR_OVERLAY_MAX_BYTES);
    assert(sector_overlay_crc32("123456789", 9) == 0xcbf43926u);
    int valid = sector_overlay_validate(size, product);
    if (!strcmp(argv[1], "reject")) {
        assert(valid < 0 && sector_overlay_count() == 0);
        return 0;
    }
    assert(!valid);
    assert(argc >= 5 && argc <= 7);
    uint8_t original[RAW_BYTES], expected[RAW_BYTES], sector[RAW_BYTES];
    assert(load(argv[3], original, sizeof(original)) == sizeof(original));
    assert(load(argv[4], expected, sizeof(expected)) == sizeof(expected));
    unsigned index = argc >= 6 ? (unsigned)atoi(argv[5]) : sector_overlay_count() - 1;
    assert(index < sector_overlay_count());
    uint32_t fad = sector_overlay_fad(index);
    assert(sector_overlay_check(index, original) == 0);
    memcpy(sector, original, sizeof(sector));
    assert(sector_overlay_apply(fad, sector) == 0);
    assert(!memcmp(sector, original, sizeof(sector)));
    if (sector_overlay_alternatives()) {
        sector_overlay_arm();
        assert(!sector_overlay_apply(fad, sector));
        assert(sector_overlay_select(sector_overlay_count()) < 0);
        assert(!sector_overlay_select(index));
    }
    sector_overlay_arm();
    assert(sector_overlay_select(index) < 0);
    assert(sector_overlay_apply(149, sector) == 0);
    assert(!memcmp(sector, original, sizeof(sector)));
    unsigned damaged = argc == 7 ? (unsigned)atoi(argv[6]) : 2063;
    assert(damaged < sizeof(sector));
    sector[damaged] ^= 1;
    for (unsigned i = 0; i < 2; ++i) {
        assert(sector_overlay_apply(fad, sector) < 0);
        assert(sector[damaged] == (original[damaged] ^ 1));
        sector[damaged] ^= 1;
        assert(!memcmp(sector, original, sizeof(sector)));
        sector[damaged] ^= 1;
    }
    for (unsigned i = 0; i < 2; ++i) {
        memcpy(sector, original, sizeof(sector));
        assert(sector_overlay_apply(fad, sector) == (strcmp(argv[1], "guard") != 0));
        assert(!memcmp(sector, expected, sizeof(sector)));
    }
    sector_overlay_clear();
    memcpy(sector, original, sizeof(sector));
    assert(sector_overlay_apply(fad, sector) == 0);
    assert(!memcmp(sector, original, sizeof(sector)));
    return 0;
}
