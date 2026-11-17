/* Physical redo records for both existing save representations. The menu
 * already links crc32 and its table; reuse them rather than adding a checksum. */
#include "save_journal.h"
#include <string.h>

/* Match main.h's unsigned-int u32, including targets where uint32_t is long. */
extern unsigned int crc32(unsigned char *, int, unsigned int);
_Static_assert(sizeof(unsigned int) == 4, "menu CRC words must be 32 bits");
_Static_assert((uint32_t)SJ_REPLAY_BYTES >= (uint32_t)SJ_HEADER_BYTES &&
               SJ_REPLAY_BYTES + SJ_TABLE_BYTES + SJ_REPLAY_CHUNKS * 4 <= SJ_WRITE_BYTES,
               "replay metadata and CRC checkpoints must fit the same workspace");

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}

static void put_word(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static uint32_t checksum(const uint8_t *p, uint32_t size)
{
    return crc32((uint8_t *)p, (int)size, 0);
}

static int geometry(uint32_t device, uint32_t count)
{
    return device <= 1 && count && count <= SJ_MAX_RECORDS &&
           (device || count == 1);
}

/* Keep this bounded check in its callers: the pinned SH-2 build otherwise
 * nests another frame on publication's validation path. */
static inline __attribute__((always_inline)) int claim(uint32_t device, uint32_t index, uint32_t offset,
                 uint32_t size, uint8_t *claimed)
{
    if (!index) return !offset && size == (device ? 8192u : 65536u);
    if (!device || !size || size > 1024 || (offset & 1023) ||
        offset < 8192 || offset >= 8064u * 1024)
        return 0;
    uint32_t block = offset / 1024, byte = block / 8, mask = 1u << (block & 7);
    if (claimed[byte] & mask) return 0;
    claimed[byte] |= mask;
    return 1;
}

int save_journal_begin(SaveJournalBuilder *b, uint8_t *bytes, uint32_t capacity,
                       uint8_t *claimed, uint32_t device, const uint8_t game_id[16],
                       uint32_t records)
{
    if (!b || !bytes || !claimed || !game_id || !geometry(device, records) ||
        (!device && !game_id[0]) || capacity > SJ_MAX_BYTES ||
        capacity < SJ_HEADER_BYTES + records * 8)
        return SJ_INVALID;
    *b = (SaveJournalBuilder){bytes, claimed, capacity, records, 0,
                             SJ_HEADER_BYTES + records * 8, device};
    memset(bytes, 0, b->length);
    memset(claimed, 0, SJ_BITMAP_BYTES);
    put_word(bytes + 8, records); put_word(bytes + 12, device);
    memcpy(bytes + 16, game_id, 16);
    return SJ_READY;
}

int save_journal_append(SaveJournalBuilder *b, uint32_t offset,
                        const void *data, uint32_t size)
{
    if (!b || !data || b->added >= b->count || b->length > b->capacity ||
        size > b->capacity - b->length ||
        !claim(b->device, b->added, offset, size, b->claimed))
        return SJ_INVALID;
    memcpy(b->bytes + b->length, data, size);
    uint8_t *entry = b->bytes + SJ_HEADER_BYTES + b->added * 8;
    put_word(entry, offset); put_word(entry + 4, size);
    b->length += size; b->added++;
    return SJ_READY;
}

int save_journal_seal(SaveJournalBuilder *b)
{
    if (!b || b->length > b->capacity || b->added != b->count) return SJ_INVALID;
    memcpy(b->bytes, "SJR1", 4);
    put_word(b->bytes + 4, b->length);
    put_word(b->bytes + 32, checksum(b->bytes + SJ_HEADER_BYTES,
                                   b->length - SJ_HEADER_BYTES));
    put_word(b->bytes + 36, checksum(b->bytes, 36));
    return SJ_READY;
}

/* Metadata is common to contiguous, split and disk-backed records. Prefix is
 * the logical split boundary; only the complete header/table is read here. */
static int validate_metadata(const uint8_t *bytes, uint32_t length,
                             uint32_t prefix, uint8_t *claimed)
{
    unsigned nonzero = 0;
    for (unsigned i = 0; i < SJ_HEADER_BYTES; i++) nonzero |= bytes[i];
    if (!nonzero) return SJ_EMPTY; /* Inactive body cannot authorize a replay. */
    uint32_t count = word(bytes + 8), device = word(bytes + 12);
    if (length < SJ_HEADER_BYTES || length > SJ_MAX_BYTES ||
        word(bytes) != 0x534a5231u || word(bytes + 4) != length ||
        !geometry(device, count) || (!device && !bytes[16]) ||
        word(bytes + 36) != checksum(bytes, 36))
        return SJ_INVALID;
    for (unsigned i = 40; i < SJ_HEADER_BYTES; i++)
        if (bytes[i]) return SJ_INVALID;
    uint32_t cursor = SJ_HEADER_BYTES + count * 8;
    if (cursor > prefix) return SJ_INVALID;
    /* Every valid prefix is nonzero. Retire it when its record boundary is
     * reached, so a second flag need not survive the whole descriptor walk. */
    if (cursor == prefix) prefix = 0;
    memset(claimed, 0, SJ_BITMAP_BYTES);
    for (unsigned i = 0; i < count; i++) {
        const uint8_t *p = bytes + SJ_HEADER_BYTES + i * 8;
        uint32_t size = word(p + 4);
        if (size > length - cursor || !claim(device, i, word(p), size, claimed))
            return SJ_INVALID;
        cursor += size;
        if (cursor == prefix) prefix = 0;
    }
    return cursor == length && !prefix ? SJ_READY : SJ_INVALID;
}

int save_journal_validate(const uint8_t *bytes, uint32_t length,
                          uint8_t *claimed, SaveJournalView *view)
{
    if (!view) return SJ_INVALID;
    memset(view, 0, sizeof(*view));
    if (!length) return SJ_EMPTY;
    if (!bytes || !claimed || length < SJ_HEADER_BYTES || length > SJ_MAX_BYTES)
        return SJ_INVALID;
    int result = validate_metadata(bytes, length, length, claimed);
    if (result != SJ_READY) return result;
    if (word(bytes + 32) != checksum(bytes + SJ_HEADER_BYTES, length - SJ_HEADER_BYTES))
        return SJ_INVALID;
    *view = (SaveJournalView){bytes, length, word(bytes + 8), word(bytes + 12)};
    return SJ_READY;
}

const uint8_t *save_journal_next(const SaveJournalView *v, SaveJournalCursor *cursor,
                                uint32_t *offset, uint32_t *length)
{
    if (!v || !v->bytes || !cursor || cursor->index >= v->count || !offset || !length)
        return NULL;
    if (!cursor->index) cursor->position = SJ_HEADER_BYTES + v->count * 8;
    const uint8_t *p = v->bytes + SJ_HEADER_BYTES + cursor->index * 8;
    *offset = word(p); *length = word(p + 4);
    const uint8_t *data = v->bytes + cursor->position;
    cursor->position += *length; cursor->index++;
    return data;
}

int save_journal_publish(uint8_t *bytes, uint32_t length,
                         SaveJournalWrite write, void *context)
{
    if (!bytes || !write || length < SJ_HEADER_BYTES || length > SJ_MAX_BYTES ||
        word(bytes) != 0x534a5231u || word(bytes + 4) != length)
        return SJ_INVALID;
    uint8_t header[40];
    memcpy(header, bytes, sizeof(header));
    memset(bytes, 0, SJ_HEADER_BYTES);
    uint32_t offset = 0;
    while (offset < length) {
        uint32_t count = length - offset;
        if (count > SJ_WRITE_BYTES) count = SJ_WRITE_BYTES;
        if (write(context, offset ? (int)offset : -1, bytes + offset, count) != (int)count)
            break;
        offset += count;
    }
    memcpy(bytes, header, sizeof(header));
    if (offset != length) return SJ_IO_ERROR;
    return write(context, 0, bytes, SJ_HEADER_BYTES) == SJ_HEADER_BYTES ?
           SJ_READY : SJ_IO_ERROR;
}

int save_journal_clear(uint8_t *bytes, SaveJournalWrite write, void *context)
{
    if (!bytes || !write) return SJ_INVALID;
    memset(bytes, 0, SJ_HEADER_BYTES);
    return write(context, 0, bytes, SJ_HEADER_BYTES) == SJ_HEADER_BYTES ?
           SJ_EMPTY : SJ_IO_ERROR;
}

int save_journal_validate_parts(const SaveJournalParts *parts, uint8_t *claimed)
{
    if (!parts || !parts->prefix || !claimed ||
        parts->prefix_bytes < SJ_HEADER_BYTES || parts->prefix_bytes > SJ_WRITE_BYTES ||
        parts->payload_bytes > SJ_MAX_BYTES - parts->prefix_bytes ||
        (parts->payload_bytes && !parts->payload))
        return SJ_INVALID;
    const uint8_t *bytes = parts->prefix;
    uint32_t length = parts->prefix_bytes + parts->payload_bytes;
    int result = validate_metadata(bytes, length, parts->prefix_bytes, claimed);
    if (result != SJ_READY) return result;
    uint32_t crc = checksum(bytes + SJ_HEADER_BYTES, parts->prefix_bytes - SJ_HEADER_BYTES);
    if (parts->payload_bytes)
        crc = crc32((uint8_t *)parts->payload, (int)parts->payload_bytes, crc);
    return word(bytes + 32) == crc ? SJ_READY : SJ_INVALID;
}

int save_journal_reference(SaveJournalBuilder *b, uint32_t offset, uint32_t size)
{
    if (!b || b->added >= b->count || b->length > SJ_MAX_BYTES ||
        (b->length <= b->capacity && b->length > SJ_WRITE_BYTES) ||
        size > SJ_MAX_BYTES - b->length ||
        !claim(b->device, b->added, offset, size, b->claimed))
        return SJ_INVALID;
    /* Every claimed record is nonempty, so closing the writable extent makes
     * length > capacity and the normal append bounds reject further copies. */
    if (b->length <= b->capacity) b->capacity = b->length;
    uint8_t *entry = b->bytes + SJ_HEADER_BYTES + b->added * 8;
    put_word(entry, offset); put_word(entry + 4, size);
    b->length += size; b->added++;
    return SJ_READY;
}

int save_journal_seal_parts(SaveJournalBuilder *b, const uint8_t *payload,
                            SaveJournalParts *parts)
{
    if (!parts) return SJ_INVALID;
    if (!b || b->added != b->count) goto invalid;
    uint32_t prefix = b->length > b->capacity ? b->capacity : b->length;
    if (prefix > SJ_WRITE_BYTES || b->length < prefix ||
        b->length > SJ_MAX_BYTES || (b->length != prefix && !payload))
        goto invalid;
    uint32_t tail = b->length - prefix;
    uint32_t crc = checksum(b->bytes + SJ_HEADER_BYTES, prefix - SJ_HEADER_BYTES);
    if (tail) crc = crc32((uint8_t *)payload, (int)tail, crc);
    memcpy(b->bytes, "SJR1", 4);
    put_word(b->bytes + 4, b->length);
    put_word(b->bytes + 32, crc);
    put_word(b->bytes + 36, checksum(b->bytes, 36));
    *parts = (SaveJournalParts){b->bytes, payload, prefix, tail};
    return SJ_READY;
invalid:
    memset(parts, 0, sizeof(*parts));
    return SJ_INVALID;
}

/* Keep the 40-byte commit-header lifetime after validation. The pinned SH-2
 * compiler tail-calls this helper, so its frame does not nest with validation. */
static __attribute__((noinline)) int publish_parts_body(SaveJournalParts *parts,
                                                        SaveJournalWrite write, void *context)
{
    uintptr_t workspace = (uintptr_t)parts->prefix, source = (uintptr_t)parts->payload;
    if (parts->payload_bytes && (workspace <= source ? source - workspace < SJ_WRITE_BYTES :
                                 workspace - source < parts->payload_bytes))
        return SJ_INVALID;
    uint8_t header[40];
    memcpy(header, parts->prefix, sizeof(header));
    memset(parts->prefix, 0, SJ_HEADER_BYTES);
    uint32_t copied = SJ_WRITE_BYTES - parts->prefix_bytes;
    if (copied > parts->payload_bytes) copied = parts->payload_bytes;
    if (copied) memcpy(parts->prefix + parts->prefix_bytes, parts->payload, copied);
    uint32_t offset = parts->prefix_bytes + copied;
    int complete = write(context, -1, parts->prefix, offset) == (int)offset;
    while (complete && copied < parts->payload_bytes) {
        uint32_t count = parts->payload_bytes - copied;
        if (count > SJ_WRITE_BYTES) count = SJ_WRITE_BYTES;
        complete = write(context, (int)offset, parts->payload + copied, count) == (int)count;
        copied += count; offset += count;
    }
    /* Later file transfers may have used the entire prefix allocation as their
     * bounce buffer. Restore the reserved zeros too before committing. */
    memset(parts->prefix, 0, SJ_HEADER_BYTES);
    memcpy(parts->prefix, header, sizeof(header));
    if (!complete) return SJ_IO_ERROR;
    return write(context, 0, parts->prefix, SJ_HEADER_BYTES) == SJ_HEADER_BYTES ?
           SJ_READY : SJ_IO_ERROR;
}

int save_journal_publish_parts(SaveJournalParts *parts, uint8_t *claimed,
                               SaveJournalWrite write, void *context)
{
    if (!write || save_journal_validate_parts(parts, claimed) != SJ_READY)
        return SJ_INVALID;
    return publish_parts_body(parts, write, context);
}

int save_journal_load(SaveJournalRecovery *view, uint8_t *workspace,
                      uint8_t *claimed, SaveJournalRead read, void *context)
{
    if (!view) return SJ_INVALID;
    memset(view, 0, sizeof(*view));
    if (!workspace || !claimed || !read) return SJ_INVALID;
    uint8_t *table = workspace + SJ_REPLAY_BYTES;
    int result = read(context, 0, table, SJ_HEADER_BYTES);
    if (result < 0) return SJ_IO_ERROR;
    if (!result) return SJ_EMPTY;
    if (result != SJ_HEADER_BYTES) return SJ_INVALID;
    uint32_t count = word(table + 8), length = word(table + 4);
    if (count > SJ_MAX_RECORDS) return SJ_INVALID;
    uint32_t entries = count * 8;
    if (entries) {
        result = read(context, SJ_HEADER_BYTES, table + SJ_HEADER_BYTES, entries);
        if (result < 0) return SJ_IO_ERROR;
        if (result != (int)entries) return SJ_INVALID;
    }
    result = validate_metadata(table, length, length, claimed);
    if (result != SJ_READY) return result;
    uint32_t crc = checksum(table + SJ_HEADER_BYTES, entries);
    uint32_t position = SJ_HEADER_BYTES + entries, chunk = 0;
    while (position < length) {
        uint32_t size = length - position;
        if (size > SJ_REPLAY_BYTES) size = SJ_REPLAY_BYTES;
        result = read(context, position, workspace, size);
        if (result < 0) return SJ_IO_ERROR;
        if (result != (int)size) return SJ_INVALID;
        crc = crc32(workspace, (int)size, crc);
        put_word(table + SJ_TABLE_BYTES + chunk++ * 4, crc);
        position += size;
    }
    if (crc != word(table + 32)) return SJ_INVALID;
    result = read(context, length, workspace, 1);
    if (result < 0) return SJ_IO_ERROR;
    if (result) return SJ_INVALID; /* The declared record must end at EOF. */
    *view = (SaveJournalRecovery){workspace, length, count, word(table + 12)};
    return SJ_READY;
}

int save_journal_replay(const SaveJournalRecovery *view, SaveJournalRead read,
                        SaveJournalApply apply, void *context)
{
    if (!view || !view->workspace || !read || !apply ||
        !geometry(view->device, view->count) || view->length > SJ_MAX_BYTES ||
        view->length <= SJ_HEADER_BYTES + view->count * 8)
        return SJ_INVALID;
    uint8_t *workspace = view->workspace;
    const uint8_t *table = workspace + SJ_REPLAY_BYTES;
    uint32_t entries = view->count * 8;
    uint32_t position = SJ_HEADER_BYTES + entries;
    uint32_t crc = checksum(table + SJ_HEADER_BYTES, entries);
    uint32_t record = 0, within = 0, chunk = 0;
    while (position < view->length) {
        uint32_t size = view->length - position;
        if (size > SJ_REPLAY_BYTES) size = SJ_REPLAY_BYTES;
        int result = read(context, position, workspace, size);
        if (result < 0) return SJ_IO_ERROR;
        if (result != (int)size) return SJ_INVALID;
        crc = crc32(workspace, (int)size, crc);
        if (crc != word(table + SJ_TABLE_BYTES + chunk++ * 4)) return SJ_INVALID;

        uint32_t used = 0;
        while (used < size) {
            const uint8_t *entry = table + SJ_HEADER_BYTES + record * 8;
            uint32_t offset = word(entry) + within, run = 0;
            do {
                uint32_t take = word(entry + 4) - within;
                if (take > size - used - run) take = size - used - run;
                run += take; within += take;
                if (within == word(entry + 4)) {
                    record++; within = 0; entry += 8;
                }
            } while (used + run < size && record < view->count &&
                     word(entry) + within == offset + run);
            if (apply(context, offset, workspace + used, run) != (int)run)
                return SJ_IO_ERROR;
            used += run;
        }
        position += size;
    }
    return apply(context, 0, NULL, 0) == 0 ? SJ_READY : SJ_IO_ERROR;
}
