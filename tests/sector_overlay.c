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
    if (!strcmp(argv[1], "finite")) {
        assert(argc == 8 && !sector_overlay_alternatives());
        uint8_t source[2][RAW_BYTES], post[2][RAW_BYTES], marker[RAW_BYTES], buffer[RAW_BYTES];
        assert(load(argv[3], source[0], RAW_BYTES) == RAW_BYTES);
        assert(load(argv[4], post[0], RAW_BYTES) == RAW_BYTES);
        assert(load(argv[5], source[1], RAW_BYTES) == RAW_BYTES);
        assert(load(argv[6], post[1], RAW_BYTES) == RAW_BYTES);
        assert(load(argv[7], marker, RAW_BYTES) == RAW_BYTES);
        uint32_t fads[2] = {0};
        unsigned sources = 0;
        assert(pack[4] == 6);
        for (unsigned i = 0; i < sector_overlay_count(); ++i) {
            uint32_t fad = sector_overlay_fad(i);
            if (!pack[24 + i * SECTOR_OVERLAY_RECORD_BYTES + 8]) continue;
            if (sources && fads[sources - 1] == fad) continue;
            assert(sources < 2);
            fads[sources++] = fad;
        }
        assert(sources == 2);
        uint32_t completion = sector_overlay_fad(sector_overlay_count() - 1);
        const unsigned action = pack[7];
        assert(action == 0x80 || action == 0x81 || action == 0x84);
        assert(!sector_overlay_action());
        for (unsigned order = 0; order < 2; ++order) {
            sector_overlay_arm();
            assert(sector_overlay_action() == action);
            /* Preflight and an early marker cannot stand in for delivery. */
            for (unsigned i = 0; i < sector_overlay_count(); ++i) {
                uint32_t fad = sector_overlay_fad(i);
                if (fad == fads[0] || fad == fads[1])
                    assert(!sector_overlay_check(i, source[fad == fads[1]]));
            }
            memcpy(buffer, marker, RAW_BYTES);
            assert(!sector_overlay_apply(completion, buffer) && !memcmp(buffer, marker, RAW_BYTES));
            unsigned first = order, second = order ^ 1;
            memcpy(buffer, source[first], RAW_BYTES); buffer[2064] ^= 1;
            assert(sector_overlay_apply(fads[first], buffer) < 0);
            assert(buffer[2064] == (source[first][2064] ^ 1));
            memcpy(buffer, source[second], RAW_BYTES);
            assert(sector_overlay_apply(fads[second], buffer) == 1 && !memcmp(buffer, post[second], RAW_BYTES));
            memcpy(buffer, marker, RAW_BYTES);
            assert(!sector_overlay_apply(completion, buffer));
            memcpy(buffer, source[first], RAW_BYTES);
            assert(sector_overlay_apply(fads[first], buffer) == 1 && !memcmp(buffer, post[first], RAW_BYTES));
            memcpy(buffer, marker, RAW_BYTES); buffer[2351] ^= 1;
            assert(sector_overlay_apply(completion, buffer) < 0);
            memcpy(buffer, source[second], RAW_BYTES);
            assert(sector_overlay_apply(fads[second], buffer) == 1 && !memcmp(buffer, post[second], RAW_BYTES));
            memcpy(buffer, marker, RAW_BYTES);
            assert(!sector_overlay_apply(completion, buffer) && !memcmp(buffer, marker, RAW_BYTES));
            assert(sector_overlay_action() == action && sector_overlay_replaces_callback());
            assert(sector_overlay_select(0) < 0);
            for (unsigned i = 0; i < 2; ++i) {
                memcpy(buffer, source[i], RAW_BYTES);
                assert(!sector_overlay_apply(fads[i], buffer) && !memcmp(buffer, source[i], RAW_BYTES));
            }
        }
        sector_overlay_clear();
        assert(!sector_overlay_action() && !sector_overlay_count());
        return 0;
    }
    if (!strcmp(argv[1], "guarded")) {
        assert(argc == 6 && sector_overlay_count() == 2);
        uint8_t original[RAW_BYTES], post[RAW_BYTES], marker[RAW_BYTES], sector[RAW_BYTES];
        assert(load(argv[3], original, sizeof original) == sizeof original);
        assert(load(argv[4], post, sizeof post) == sizeof post);
        assert(load(argv[5], marker, sizeof marker) == sizeof marker);
        sector_overlay_arm();
        assert(sector_overlay_loaded_guard() && sector_overlay_action() == 0x80);
        memcpy(sector,original,sizeof sector); sector[2064] ^= 1;
        assert(sector_overlay_apply(sector_overlay_fad(0),sector) < 0);
        memcpy(sector,original,sizeof sector);
        assert(sector_overlay_apply(sector_overlay_fad(0),sector) == 1 && !memcmp(sector,post,sizeof sector));
        memcpy(sector,marker,sizeof sector);sector[2351] ^= 1;
        assert(sector_overlay_apply(sector_overlay_fad(1),sector) < 0 && sector_overlay_loaded_guard());
        memcpy(sector,original,sizeof sector);
        assert(sector_overlay_apply(sector_overlay_fad(0),sector) == 1);
        memcpy(sector,marker,sizeof sector);
        assert(!sector_overlay_apply(sector_overlay_fad(1),sector) && !memcmp(sector,marker,sizeof sector));
        assert(!sector_overlay_loaded_guard() && !sector_overlay_action());
        memcpy(sector,original,sizeof sector);
        assert(!sector_overlay_apply(sector_overlay_fad(0),sector) && !memcmp(sector,original,sizeof sector));
        sector_overlay_clear();
        assert(!sector_overlay_loaded_guard() && !sector_overlay_count());
        return 0;
    }
    if (!strcmp(argv[1], "stream")) {
        assert(argc == 7 && !sector_overlay_alternatives());
        FILE *source = fopen(argv[3], "rb"), *post = fopen(argv[4], "rb");
        assert(source && post);
        unsigned fad = (unsigned)strtoul(argv[5], NULL, 10);
        const unsigned first_fad = fad;
        unsigned expected_hits = (unsigned)strtoul(argv[6], NULL, 10);
        assert(fad >= 150 && fad < 450000);
        uint8_t original[RAW_BYTES], expected[RAW_BYTES];
        unsigned hits = 0, count = 0;
        sector_overlay_arm();
        for (;;) {
            size_t bytes = fread(original, 1, sizeof original, source);
            if (!bytes) break;
            assert(bytes == sizeof original && count++ < 512 && fad < 450000);
            assert(fread(expected, 1, sizeof expected, post) == sizeof expected);
            int edited = sector_overlay_apply(fad++, original);
            assert(edited >= 0 && !memcmp(original, expected, sizeof original));
            hits += edited == 1;
        }
        assert(count && hits == expected_hits && !ferror(source));
        assert(fgetc(post) == EOF && !ferror(post));
        if (pack[4] == 6 && sector_overlay_fad(0) >= first_fad &&
            sector_overlay_fad(sector_overlay_count() - 1) < fad) {
            assert(sector_overlay_action() == pack[7]);
            rewind(source);
            for (unsigned at = first_fad; at < fad; ++at) {
                assert(fread(original, 1, RAW_BYTES, source) == RAW_BYTES);
                memcpy(expected, original, RAW_BYTES);
                assert(!sector_overlay_apply(at, original) && !memcmp(original, expected, RAW_BYTES));
            }
        }
        assert(!fclose(source) && !fclose(post));
        printf("%u streamed sectors, %u edits: complete raw output passed\n", count, hits);
        return 0;
    }
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
