/* Bounded disk validation/replay against the independent SJR1 oracle. */
#define main original_journal_main
#include "test_save_journal.c"
#undef main

static struct {
    uint8_t before[32], bytes[SJ_WRITE_BYTES], after[32];
} scratch;
static uint8_t primary[8 * 1024 * 1024], expected[sizeof primary];
static unsigned replay_checks;

typedef struct {
    const uint8_t *source;
    unsigned length, reads, writes, bytes, syncs, read_fail, write_fail, kind;
    int admitted;
} RecoveryIO;

static void replay_guards(void)
{
    for (unsigned i = 0; i < 32; i++)
        assert(scratch.before[i] == 0xa5 && scratch.after[i] == 0xa5);
}

static int read_record(void *context, uint32_t offset, uint8_t *data, uint32_t size)
{
    RecoveryIO *io = context;
    assert(size && size <= SJ_REPLAY_BYTES);
    assert(data >= scratch.bytes && data <= scratch.bytes + SJ_WRITE_BYTES - size);
    unsigned call = ++io->reads;
    if (call == io->read_fail && !io->kind) return -1;
    unsigned count = offset >= io->length ? 0 : io->length - offset;
    if (count > size) count = size;
    if (call == io->read_fail && io->kind == 1) count /= 2;
    if (count) memcpy(data, io->source + offset, count);
    replay_guards();
    return call == io->read_fail && io->kind == 2 ? -1 : (int)count;
}

static int apply_record(void *context, uint32_t offset, const uint8_t *data, uint32_t size)
{
    RecoveryIO *io = context;
    assert(io->admitted && size <= SJ_REPLAY_BYTES);
    unsigned call = ++io->writes;
    if (!size) {
        assert(!offset && !data); io->syncs++;
        return call == io->write_fail ? -1 : 0;
    }
    assert(data >= scratch.bytes && data <= scratch.bytes + SJ_REPLAY_BYTES - size);
    assert(offset <= sizeof primary && size <= sizeof primary - offset);
    assert(!memcmp(data, expected + offset, size));
    if (call == io->write_fail && !io->kind) return -1;
    unsigned copied = call == io->write_fail && io->kind == 1 ? size / 2 : size;
    memcpy(primary + offset, data, copied); io->bytes += copied;
    replay_guards();
    if (call == io->write_fail) return io->kind == 1 ? (int)copied : -1;
    return size;
}

static void make_replay(unsigned mode)
{
    if (mode < 4) make(mode);
    else {
        SaveJournalBuilder b;
        uint8_t key[] = "TESTGAME00000001";
        unsigned count = mode == 4 ? 4 : SJ_MAX_RECORDS;
        assert(save_journal_begin(&b, blob, sizeof blob, claimed, 1, key, count) == SJ_READY);
        for (unsigned i = 0; i < count; i++) {
            unsigned size = !i ? 8192 : mode == 4 && i == 2 ? 17 : 1024;
            for (unsigned k = 0; k < size; k++) payload[k] = pattern(i, k);
            assert(save_journal_append(&b, i ? (7 + i) * 1024 : 0, payload, size) == SJ_READY);
        }
        assert(save_journal_seal(&b) == SJ_READY); full_size = b.length;
        assert(save_journal_validate(blob, full_size, claimed, &view) == SJ_READY);
    }
    memset(primary, 0xa5, sizeof primary); memcpy(expected, primary, sizeof primary);
    SaveJournalCursor cursor = {0}; uint32_t offset, size; const uint8_t *data;
    while ((data = save_journal_next(&view, &cursor, &offset, &size)))
        memcpy(expected + offset, data, size);
    memcpy(copy, blob, full_size);
}

static int load_record(SaveJournalRecovery *v, RecoveryIO *io)
{
    memset(&scratch, 0xa5, sizeof scratch);
    int result = save_journal_load(v, scratch.bytes, claimed, read_record, io);
    if (result != SJ_READY)
        assert(!v->workspace && !v->length && !v->count && !v->device);
    else {
        assert(v->workspace == scratch.bytes && v->length == full_size);
        assert(v->count == view.count && v->device == view.device);
        io->admitted = 1;
    }
    assert(!io->writes && !io->bytes && !io->syncs); replay_guards();
    return result;
}

static void failures_and_batching(void)
{
    for (unsigned mode = 0; mode < 6; mode++) {
        make_replay(mode); SaveJournalRecovery v;
        RecoveryIO good = {.source = blob, .length = full_size};
        assert(load_record(&v, &good) == SJ_READY);
        unsigned reads = good.reads, payload_size = full_size - SJ_HEADER_BYTES - view.count * 8;
        unsigned chunks = (payload_size + SJ_REPLAY_BYTES - 1) / SJ_REPLAY_BYTES;
        assert(reads == chunks + 3);
        assert(save_journal_replay(&v, read_record, apply_record, &good) == SJ_READY);
        assert(good.reads == reads + chunks && good.bytes == payload_size && good.syncs == 1);
        assert(!memcmp(primary, expected, sizeof primary)); replay_guards();
        if (mode == 4) assert(good.writes == 3); /* Adjacent records, then a gap. */
        if (mode == 5 || mode == 0) assert(good.writes == chunks + 1);
        replay_checks++;

        for (unsigned at = 1; at <= reads; at++) for (unsigned kind = 0; kind < 3; kind++) {
            if (at == reads && kind == 1) continue; /* A short EOF is still EOF. */
            RecoveryIO io = {.source = blob, .length = full_size, .read_fail = at, .kind = kind};
            assert(load_record(&v, &io) == (kind == 1 ? SJ_INVALID : SJ_IO_ERROR));
            assert(io.reads == at); replay_checks++;
        }
        for (unsigned at = 1; at <= chunks; at++) for (unsigned kind = 0; kind < 3; kind++) {
            RecoveryIO io = {.source = blob, .length = full_size};
            assert(load_record(&v, &io) == SJ_READY);
            io.read_fail = io.reads + at; io.kind = kind;
            assert(save_journal_replay(&v, read_record, apply_record, &io) ==
                   (kind == 1 ? SJ_INVALID : SJ_IO_ERROR));
            assert(!io.syncs && io.bytes == (at - 1) * SJ_REPLAY_BYTES);
            io.read_fail = 0;
            assert(save_journal_replay(&v, read_record, apply_record, &io) == SJ_READY);
            assert(!memcmp(primary, expected, sizeof primary)); replay_checks++;
        }
        unsigned phases[] = {1, 2, good.writes / 2, good.writes - 1, good.writes};
        for (unsigned n = 0; n < sizeof phases / sizeof phases[0]; n++) {
            unsigned at = phases[n];
            if (!at) continue;
            for (unsigned kind = 0; kind < 3; kind++) {
                RecoveryIO io = {.source = blob, .length = full_size, .write_fail = at, .kind = kind};
                assert(load_record(&v, &io) == SJ_READY);
                assert(save_journal_replay(&v, read_record, apply_record, &io) == SJ_IO_ERROR);
                assert(io.writes == at && io.syncs == (at == good.writes));
                io.write_fail = 0;
                assert(save_journal_replay(&v, read_record, apply_record, &io) == SJ_READY);
                assert(!memcmp(primary, expected, sizeof primary)); replay_checks++;
            }
        }
    }
}

static void changed_chunks(void)
{
    make_replay(5); unsigned start = SJ_HEADER_BYTES + view.count * 8;
    unsigned chunks = (full_size - start + SJ_REPLAY_BYTES - 1) / SJ_REPLAY_BYTES;
    for (unsigned chunk = 0; chunk < chunks; chunk++) {
        SaveJournalRecovery v;
        RecoveryIO io = {.source = copy, .length = full_size};
        assert(load_record(&v, &io) == SJ_READY);
        unsigned at = start + chunk * SJ_REPLAY_BYTES;
        copy[at] ^= 1;
        assert(save_journal_replay(&v, read_record, apply_record, &io) == SJ_INVALID);
        assert(io.bytes == chunk * SJ_REPLAY_BYTES && !io.syncs);
        copy[at] ^= 1;
        assert(save_journal_replay(&v, read_record, apply_record, &io) == SJ_READY);
        assert(!memcmp(primary, expected, sizeof primary)); replay_checks++;
    }
}

static void rejected_records(void)
{
    make_replay(3); SaveJournalRecovery v;
    const unsigned places[] = {0, 4, 8, 12, 16, 32, 36, 40, 511, 512, 4359, 65535, 504071};
    for (unsigned i = 0; i < sizeof places / sizeof places[0]; i++) {
        memcpy(copy, blob, full_size); copy[places[i]] ^= 1;
        RecoveryIO io = {.source = copy, .length = full_size};
        assert(load_record(&v, &io) == SJ_INVALID); replay_checks++;
    }
    memcpy(copy, blob, full_size); copy[full_size] = 1;
    for (unsigned length = full_size - 1; length <= full_size + 1; length += 2) {
        RecoveryIO io = {.source = copy, .length = length};
        assert(load_record(&v, &io) == SJ_INVALID); replay_checks++;
    }
    memset(copy, 0, SJ_HEADER_BYTES);
    const unsigned lengths[] = {0, 1, 511, 512, full_size};
    for (unsigned i = 0; i < sizeof lengths / sizeof lengths[0]; i++) {
        RecoveryIO io = {.source = copy, .length = lengths[i]};
        assert(load_record(&v, &io) == (lengths[i] && lengths[i] < 512 ? SJ_INVALID : SJ_EMPTY));
        replay_checks++;
    }
    RecoveryIO io = {.source = blob, .length = full_size};
    assert(save_journal_load(NULL, scratch.bytes, claimed, read_record, &io) == SJ_INVALID);
    assert(save_journal_load(&v, NULL, claimed, read_record, &io) == SJ_INVALID);
    assert(save_journal_load(&v, scratch.bytes, NULL, read_record, &io) == SJ_INVALID);
    assert(save_journal_load(&v, scratch.bytes, claimed, NULL, &io) == SJ_INVALID);
    assert(save_journal_replay(NULL, read_record, apply_record, &io) == SJ_INVALID);
    assert(save_journal_replay(&v, read_record, apply_record, &io) == SJ_INVALID);
    assert(!io.reads && !io.writes); replay_checks += 6;
}

int main(void)
{
    rejected_records(); failures_and_batching(); changed_chunks();
    printf("PASS %u bounded replay cases\n", replay_checks);
    return 0;
}
