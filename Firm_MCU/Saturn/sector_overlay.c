/* Adapted from paps-g's guarded SD overlay and standalone release CRC. */
#include "sector_overlay.h"
#include <string.h>

enum { RAW_BYTES = 2352, DATA_START = 16, TAIL_START = 2064, RECORD_TAIL = 60 };

static uint8_t pack[SECTOR_OVERLAY_MAX_BYTES];
/* armed=1 edits sectors; armed=2 retains an admitted finite launch only. */
static uint8_t record_count, armed;
static uint16_t selected, pending;

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
    selected = pending = 0;
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

const uint8_t *sector_overlay_loaded_guard(void)
{
    return armed && pack[4] == 4 ? record_at(0) + 24 : NULL;
}

int sector_overlay_rom_cart(void)
{
    return armed && pack[4] == 5;
}

int sector_overlay_validate(size_t size, const char product[10])
{
    sector_overlay_clear();
    if (size < SECTOR_OVERLAY_HEADER_BYTES || size > sizeof(pack) ||
        memcmp(pack, "SSO1", 4) || !pack[5] ||
        pack[5] > SECTOR_OVERLAY_MAX_RECORDS || pack[6] > 1 ||
        (pack[6] && pack[5] < 2) ||
        pack[4] < 1 || pack[4] > 6 ||
        ((pack[7] & 0x7f) != 0 && (pack[7] & 0x7f) != 1 && (pack[7] & 0x7f) != 4) ||
        (pack[7] & 0x80 && pack[4] < 3) ||
        (pack[4] == 4 && !(pack[7] & 0x80)) ||
        (pack[4] == 5 && (pack[6] || pack[7] != 0x80)) ||
        (pack[4] == 6 && (pack[5] < 2 || pack[6] || !(pack[7] & 0x80))) ||
        size != SECTOR_OVERLAY_HEADER_BYTES + pack[5] * SECTOR_OVERLAY_RECORD_BYTES ||
        be16(pack + 18) != size || memcmp(pack + 8, product, 10)) return -1;

    uint8_t saved[4];
    memcpy(saved, pack + 20, sizeof(saved));
    memset(pack + 20, 0, sizeof(saved));
    uint32_t checksum = sector_overlay_crc32(pack, size);
    memcpy(pack + 20, saved, sizeof(saved));
    if (checksum != be32(saved)) return -1;

    if (pack[4] == 4) {
        const uint8_t *r = record_at(0), *d = r + 24;
        uint32_t load = be32(d), length = be32(d + 4), offset = be32(d + 12);
        if (pack[5] != 2 || pack[6] || r[8] != 1 || r[14] != 2 ||
            r[SECTOR_OVERLAY_RECORD_BYTES + 8] ||
            r[SECTOR_OVERLAY_RECORD_BYTES + 9] != 1 ||
            (load & 3) || length - 2u > 0xfdffeu ||
            load - 0x06002000u > 0xfe000u - length ||
            (offset & 1) || offset > length - 2 ||
            be16(d + 16) != be16(r + 16) || be16(d + 18) != be16(r + 20) ||
            sector_overlay_crc32(d, 20) != be32(d + 20) ||
            be16(r + 12) != DATA_START + offset % 2048 ||
            (length - 1) / 2048 <= offset / 2048 ||
            be32(r + SECTOR_OVERLAY_RECORD_BYTES) <
                be32(r) + (length - 1) / 2048 - offset / 2048) return -1;
    }

    if (pack[4] == 6 && (record_at(pack[5] - 1)[8] ||
        be32(record_at(pack[5] - 1)) <= be32(record_at(pack[5] - 2)))) return -1;
    unsigned format = pack[4] >= 5 ? 3 : pack[4];
    unsigned has_edits = 0;
    uint32_t previous = 149;
    for (unsigned i = 0; i < pack[5]; ++i) {
        const uint8_t *record = record_at(i);
        uint32_t fad = be32(record);
        unsigned edits = record[8];
        if (fad < 150 || fad < previous ||
            fad >= 450000 || nonzero(record + 10, 2) ||
            record[9] != ((pack[4] == 4 && i == 1) ||
                          (pack[4] == 6 && i + 1 == pack[5]))) return -1;
        if (fad == previous) {
            const uint8_t *prior = record_at(i - 1);
            if (pack[6]) {
                if (be32(record + 4) <= be32(prior + 4)) return -1;
            } else {
                /* One callback-replacing source span may use adjacent ABI 3
                 * records. All fragments bind the same original and final
                 * EDC/ECC; refuse overlap, gaps and mixed record kinds. */
                if (format != 3 || !(pack[7] & 0x80) ||
                    !(edits & 0x80) || !(prior[8] & 0x80) ||
                    be32(record + 4) != be32(prior + 4) ||
                    be16(record + 12) != be16(prior + 12) + (prior[8] & 0x7f) ||
                    memcmp(record + RECORD_TAIL, prior + RECORD_TAIL, RAW_BYTES - TAIL_START))
                    return -1;
            }
        }
        previous = fad;
        if (!edits) {
            if (pack[4] == 1 || pack[6] || nonzero(record + 12, 336)) return -1;
            continue;
        }
        has_edits = 1;
        if (nonzero(record + RECORD_TAIL + 4, 8)) return -1;
        if (edits & 0x80) {
            unsigned width = edits & 0x7f, offset = be16(record + 12);
            if (format != 3 || width < 5 || width > 44 ||
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
                if ((pack[4] != 4 || j == 3) && nonzero(edit, 12)) return -1;
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
    if (pack[4] == 6 && !has_edits) return -1;
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
    pending = 0;
    if (pack[4] == 6)
        for (unsigned i = 0; i < record_count; ++i)
            if (record_at(i)[8]) pending |= (uint16_t)(1u << i);
    armed = selected != 0;
}

int sector_overlay_apply(uint32_t fad, uint8_t sector[RAW_BYTES])
{
    if (!(armed & 1)) return 0;
    unsigned applied = 0;
    for (unsigned i = 0; i < record_count; ++i) {
        if (!(selected & (1u << i))) continue;
        const uint8_t *record = record_at(i);
        uint32_t target = be32(record);
        if (target > fad) break;
        if (target != fad) continue;
        /* Authenticate the untouched sector before any same-FAD write. */
        if (!applied && sector_overlay_check(i, sector)) return -1;
        if (!record[8]) {
            if (pack[4] == 4) armed = 0;
            else if (record[9] && !pending) armed = 2;
            return 0;
        }
        if (record[8] & 0x80) {
            memcpy(sector + be16(record + 12), record + 16, record[8] & 0x7f);
        } else {
            for (unsigned j = 0; j < record[8]; ++j) {
                const uint8_t *edit = record + 12 + j * 12;
                memcpy(sector + be16(edit), edit + 8, edit[2]);
            }
        }
        if (pack[4] == 6) pending &= (uint16_t)~(1u << i);
        applied = 1;
        if (i + 1 < record_count && (selected & (1u << (i + 1))) &&
            sector_overlay_fad(i + 1) == fad) continue;
        /* Every fragment has the complete final tail; publish it once. */
        memcpy(sector + TAIL_START, record + RECORD_TAIL, RAW_BYTES - TAIL_START);
        return 1;
    }
    return 0;
}
