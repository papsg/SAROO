/* Adapted from paps-g's guarded SD overlay and standalone release CRC. */
#include "sector_overlay.h"
#include <string.h>

enum { RAW_BYTES = 2352, DATA_START = 16, TAIL_START = 2064, RECORD_TAIL = 60 };

static uint8_t pack[SECTOR_OVERLAY_MAX_BYTES];
static uint8_t record_count, armed;
static uint16_t selected;

#if SECTOR_OVERLAY_MAX_RECORDS > 16
#error Overlay selection exceeds its mask
#endif

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)((unsigned)p[0] << 8 | p[1]);
}

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}

uint32_t sector_overlay_crc32(const void *data, size_t size)
{
    const uint8_t *p = data;
    uint32_t crc = UINT32_MAX;
    while (size--) {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

void sector_overlay_clear(void)
{
    armed = record_count = 0;
    selected = 0;
}

uint8_t *sector_overlay_staging(void)
{
    sector_overlay_clear();
    return pack;
}

unsigned sector_overlay_count(void)
{
    return record_count;
}

static const uint8_t *record_at(unsigned index)
{
    return pack + SECTOR_OVERLAY_HEADER_BYTES + index * SECTOR_OVERLAY_RECORD_BYTES;
}

uint32_t sector_overlay_fad(unsigned index)
{
    return be32(record_at(index));
}

int sector_overlay_alternatives(void)
{
    return record_count && pack[6] == 1;
}

int sector_overlay_select(unsigned index)
{
    if (armed || !sector_overlay_alternatives() || index >= record_count) return -1;
    selected = (uint16_t)(1u << index);
    return 0;
}

static int nonzero(const uint8_t *bytes, unsigned size)
{
    while (size--)
        if (*bytes++) return 1;
    return 0;
}

int sector_overlay_replaces_callback(void)
{
    return (sector_overlay_action() & 0x80) != 0;
}

unsigned sector_overlay_action(void)
{
    return armed ? pack[7] : 0;
}

int sector_overlay_validate(size_t size, const char product[10])
{
    sector_overlay_clear();
    if (size < SECTOR_OVERLAY_HEADER_BYTES || size > sizeof(pack) ||
        memcmp(pack, "SSO1", 4) || !pack[5] ||
        pack[5] > SECTOR_OVERLAY_MAX_RECORDS || pack[6] > 1 ||
        (pack[6] && pack[5] < 2) ||
        pack[4] < 1 || pack[4] > 3 ||
        ((pack[7] & 0x7f) != 0 && (pack[7] & 0x7f) != 1 && (pack[7] & 0x7f) != 4) ||
        (pack[7] & 0x80 && pack[4] != 3) ||
        size != SECTOR_OVERLAY_HEADER_BYTES + pack[5] * SECTOR_OVERLAY_RECORD_BYTES ||
        be16(pack + 18) != size || memcmp(pack + 8, product, 10)) return -1;

    uint8_t saved[4];
    memcpy(saved, pack + 20, sizeof(saved));
    memset(pack + 20, 0, sizeof(saved));
    uint32_t checksum = sector_overlay_crc32(pack, size);
    memcpy(pack + 20, saved, sizeof(saved));
    if (checksum != be32(saved)) return -1;

    uint32_t previous = 149;
    for (unsigned i = 0; i < pack[5]; ++i) {
        const uint8_t *record = record_at(i);
        uint32_t fad = be32(record);
        unsigned edits = record[8];
        if (fad < 150 || fad < previous || (fad == previous && !pack[6]) ||
            fad >= 450000 || nonzero(record + 9, 3)) return -1;
        if (i && fad == previous && be32(record + 4) <= be32(record_at(i - 1) + 4))
            return -1;
        previous = fad;
        if (!edits) {
            if (pack[4] == 1 || pack[6] || nonzero(record + 12, 336)) return -1;
            continue;
        }
        if (nonzero(record + RECORD_TAIL + 4, 8)) return -1;
        if (edits & 0x80) {
            unsigned width = edits & 0x7f, offset = be16(record + 12);
            if (pack[4] != 3 || width < 5 || width > 44 ||
                offset < DATA_START || offset + width > TAIL_START ||
                nonzero(record + 14, 2) || nonzero(record + 16 + width, 44 - width))
                return -1;
            continue;
        }
        if (edits > 4) return -1;
        unsigned end = DATA_START;
        for (unsigned j = 0; j < 4; ++j) {
            const uint8_t *edit = record + 12 + j * 12;
            if (j >= edits) {
                if (nonzero(edit, 12)) return -1;
                continue;
            }
            unsigned offset = be16(edit), width = edit[2];
            if ((width != 1 && width != 2 && width != 4) || offset < end ||
                offset + width > TAIL_START || edit[3] ||
                !memcmp(edit + 4, edit + 8, width) ||
                nonzero(edit + 4 + width, 4 - width) ||
                nonzero(edit + 8 + width, 4 - width)) return -1;
            end = offset + width;
        }
    }
    record_count = pack[5];
    selected = pack[6] ? 0 : (uint16_t)((1u << record_count) - 1);
    return 0;
}

static unsigned bcd(unsigned n)
{
    return (n / 10 << 4) | (n % 10);
}

int sector_overlay_check(unsigned index, const uint8_t sector[RAW_BYTES])
{
    static const uint8_t sync[] = {0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0};
    const uint8_t *record = record_at(index);
    uint32_t fad = be32(record);
    if (memcmp(sector, sync, sizeof(sync)) || sector[15] != 1 ||
        sector[12] != bcd(fad / 4500) || sector[13] != bcd(fad / 75 % 60) ||
        sector[14] != bcd(fad % 75) ||
        sector_overlay_crc32(sector, RAW_BYTES) != be32(record + 4)) return -1;
    if (record[8] & 0x80) return 0;
    for (unsigned i = 0; i < record[8]; ++i) {
        const uint8_t *edit = record + 12 + i * 12;
        if (memcmp(sector + be16(edit), edit + 4, edit[2])) return -1;
    }
    return 0;
}

void sector_overlay_arm(void)
{
    armed = selected != 0;
}

int sector_overlay_apply(uint32_t fad, uint8_t sector[RAW_BYTES])
{
    if (!armed) return 0;
    for (unsigned i = 0; i < record_count; ++i) {
        if (!(selected & (1u << i))) continue;
        const uint8_t *record = record_at(i);
        uint32_t target = be32(record);
        if (target > fad) break;
        if (target != fad) continue;
        if (sector_overlay_check(i, sector)) return -1;
        if (!record[8]) return 0;
        if (record[8] & 0x80) {
            memcpy(sector + be16(record + 12), record + 16, record[8] & 0x7f);
        } else {
            for (unsigned j = 0; j < record[8]; ++j) {
                const uint8_t *edit = record + 12 + j * 12;
                memcpy(sector + be16(edit), edit + 8, edit[2]);
            }
        }
        memcpy(sector + TAIL_START, record + RECORD_TAIL, RAW_BYTES - TAIL_START);
        return 1;
    }
    return 0;
}
