/* Exercise the staged planner at the actual public allocator's write boundary.
 * The storage model and injected observation calls exist only in this test. */
#define main original_resize_main
#include "firmware_save_resize.c"
#undef main
#include "save_journal_plan.c"

static const u8 journal_game_id[16] = "TESTGAME00000001";
static u8 claims[SJ_BITMAP_BYTES];
static SaveJournalBuilder builder;
static SaveJournalParts parts;
static int observing, planned;
static struct {
    u8 before[32], bytes[SJ_WRITE_BYTES], after[32];
} stream_workspace __attribute__((aligned(4)));

static void check_split(void)
{
    assert(save_journal_validate_parts(&parts, claims) == SJ_READY);
    assert(parts.prefix == stream_workspace.bytes);
    assert(parts.prefix_bytes <= 13576);
    assert(parts.prefix_bytes + parts.payload_bytes == builder.length);
    for (unsigned i = parts.prefix_bytes; i < SJ_WRITE_BYTES; i++)
        assert(stream_workspace.bytes[i] == 0xa5);
    for (unsigned i = 0; i < 32; i++)
        assert(stream_workspace.before[i] == 0xa5 && stream_workspace.after[i] == 0xa5);
}

static void probe_plan_write(int dev, BUPDIR *dir, u8 *data, u8 *header)
{
    if (!observing || !dev) return;
    assert(!planned);
    int writes = flushes, header_reads = loads_header, window_reads = loads_window;
    unsigned root_size = dev ? 8192 : 65536;
    u8 *root = (u8 *)(dev ? MEMS_HEADER : BUP_MEM_ADDR);
    u8 *before = malloc(root_size);
    assert(before);
    memcpy(before, root, root_size);
    u8 old_header[1024];
    memcpy(old_header, header, dev ? 1024 : 128);
    memset(&stream_workspace, 0xa5, sizeof stream_workspace);
    assert(journal_plan_mems_parts(&builder, &parts, stream_workspace.bytes + 1,
                                   claims, journal_game_id, dir, data, header) == SJ_INVALID);
    assert(journal_plan_mems_parts(&builder, &parts, stream_workspace.bytes,
                                   claims, journal_game_id, dir, data, header + 1) == SJ_INVALID);
    u32 saved_size = *(u32 *)(header + 12);
    *(u32 *)(header + 12) = saved_size + 1;
    assert(journal_plan_mems_parts(&builder, &parts, stream_workspace.bytes,
                                   claims, journal_game_id, dir, data, header) == SJ_INVALID);
    *(u32 *)(header + 12) = saved_size;
    if (dir->data_size > 960) {
        u16 first = *(u16 *)(header + 64);
        *(u16 *)(header + 64) = 0;
        assert(journal_plan_mems_parts(&builder, &parts, stream_workspace.bytes,
                                       claims, journal_game_id, dir, data, header) == SJ_INVALID);
        *(u16 *)(header + 64) = first;
    }
    assert(journal_plan_mems_parts(&builder, &parts, stream_workspace.bytes,
                                   claims, journal_game_id, dir, data, header) == SJ_READY);
    assert(parts.payload == (dir->data_size > 960 ? data : NULL));
    assert(parts.payload_bytes == (dir->data_size > 960 ? dir->data_size : 0));
    check_split();
    assert(writes == flushes && header_reads == loads_header && window_reads == loads_window);
    assert(!memcmp(before, root, root_size));
    assert(!memcmp(old_header, header, dev ? 1024 : 128));
    free(before);
    planned++;
}

static void probe_plan_root(int dev)
{
    if (!observing || planned) return;
    int writes = flushes, header_reads = loads_header, window_reads = loads_window;
    memset(&stream_workspace, 0xa5, sizeof stream_workspace);
    assert(journal_plan_parts_root(&builder, &parts, stream_workspace.bytes,
                                   claims, journal_game_id, dev) == SJ_READY);
    assert(parts.payload == (const u8 *)(dev ? MEMS_HEADER : BUP_MEM_ADDR));
    assert(parts.prefix_bytes == 520);
    check_split();
    assert(writes == flushes && header_reads == loads_header && window_reads == loads_window);
    planned++;
}

static void compare_persisted(int dev, const u8 *before, size_t extent)
{
    u8 *expected = malloc(extent);
    assert(expected);
    memcpy(expected, before, extent);
    assert(save_journal_validate_parts(&parts, claims) == SJ_READY);
    const u8 *table = parts.prefix;
    unsigned count = ((unsigned)table[8] << 24) | ((unsigned)table[9] << 16) |
                     ((unsigned)table[10] << 8) | table[11];
    unsigned cursor = SJ_HEADER_BYTES + count * 8;
    assert(count == builder.count);
    for (unsigned i = 0; i < count; i++) {
        const u8 *entry = table + SJ_HEADER_BYTES + i * 8;
        unsigned offset = ((unsigned)entry[0] << 24) | ((unsigned)entry[1] << 16) |
                          ((unsigned)entry[2] << 8) | entry[3];
        unsigned size = ((unsigned)entry[4] << 24) | ((unsigned)entry[5] << 16) |
                        ((unsigned)entry[6] << 8) | entry[7];
        const u8 *bytes = cursor < parts.prefix_bytes ?
                          parts.prefix + cursor : parts.payload + cursor - parts.prefix_bytes;
        assert(offset <= extent && size <= extent - offset);
        memcpy(expected + offset, bytes, size);
        cursor += size;
    }
    assert(cursor == builder.length);
    assert(!memcmp(expected, dev ? mems_disk : slot_disk, extent));
    free(expected);
}

static void exercise(int dev, int size, const char *change)
{
    initialize();
    BUPDIR neighbor = directory("KEEP0000001", 32);
    u8 *keep = payload(32, 7);
    assert(sro_bup_write(dev, &neighbor, keep, 0) == 0);
    BUPDIR target = directory("TEST0000001", 8192);
    u8 *old = payload(8192, 3);
    if (strcmp(change, "new")) assert(sro_bup_write(dev, &target, old, 0) == 0);
    reload_model();
    target.data_size = size;
    target.date++;
    memcpy(target.comment, "Chapter 2 ", 10);
    target.language = 2;
    if (!strcmp(change, "new")) memcpy(target.file_name, "NEW00000001", 11);
    u8 *next = payload(size, 71);
    size_t extent = dev ? sizeof(mems_disk) : sizeof(slot_disk);
    u8 *before = malloc(extent);
    assert(before);
    memcpy(before, dev ? mems_disk : slot_disk, extent);
    planned = 0;
    observing = 1;
    int result = !strcmp(change, "delete") ? sro_bup_delete(dev, target.file_name) :
                 sro_bup_write(dev, &target, next, 0);
    observing = 0;
    assert(result == 0 && planned == 1);
    compare_persisted(dev, before, extent);
    reload_model();
    check_save(dev, &neighbor, keep);
    if (!strcmp(change, "delete")) {
        BUPDIR found;
        assert(sro_bup_dir(dev, target.file_name, 1, &found) == 0);
    } else {
        check_save(dev, &target, next);
    }
    printf("PASS device=%d size=%d change=%s records=%u plan=%u\n",
           dev, size, change, builder.count, builder.length);
    free(before); free(next); free(old); free(keep);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    exercise(atoi(argv[1]), atoi(argv[2]), argv[3]);
    return 0;
}
