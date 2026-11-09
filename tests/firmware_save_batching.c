/* Adapted from paps-g a9ddfcc6: actual service, public synchronous MCU model. */
#define main resize_test_main
#include "firmware_save_resize.c"
#undef main

static int fixture_count;
static int fixture_header[448], fixture_slot[448];
static BUPDIR fixture_dir[448];

static void fixture(int count, const char *layout)
{
    initialize();
    fixture_count = count;
    int permutation[448];
    for (int i = 0; i < count; i++) permutation[i] = i;
    if (!strcmp(layout, "permuted")) {
        u32 seed = 0x12345678;
        for (int i = count - 1; i > 0; i--) {
            seed = seed * 1664525u + 1013904223u;
            int j = seed % (i + 1), value = permutation[i];
            permutation[i] = permutation[j]; permutation[j] = value;
        }
    }
    static const int mixed[] = {8063, 200, 9, 202, 8, 8062, 100, 10, 203, 201};
    for (int i = 0; i < count; i++) {
        int header = 8 + i, slot = i;
        if (!strcmp(layout, "reverse")) header = 8 + count - 1 - i;
        if (!strcmp(layout, "permuted")) header = 8 + permutation[i];
        if (!strcmp(layout, "holes")) slot = (i * 3 + 5) % 448;
        if (!strcmp(layout, "high")) header = 8064 - count + i;
        if (!strcmp(layout, "sparse")) header = 8 + i * 18;
        if (!strcmp(layout, "mixed")) { assert(count == 10); header = mixed[i]; }
        if (!strcmp(layout, "overlap")) header = 64 + i;
        assert(header >= 8 && header < 8064);
        fixture_header[i] = header; fixture_slot[i] = slot;
        char name[12]; snprintf(name, sizeof(name), "IDX%08d", i);
        int size = !strcmp(layout, "overlap") && i < 2 ? 2048 : 32;
        fixture_dir[i] = directory(name, size);
        u8 *entry = mems_disk + 1024 + slot * 16;
        memcpy(entry, name, 11); *(u16 *)(entry + 14) = header;
        u8 *bp = mems_disk + header * 1024;
        memcpy(bp, name, 11); *(u32 *)(bp + 12) = size;
        memcpy(bp + 16, fixture_dir[i].comment, 10); bp[27] = 1;
        *(u32 *)(bp + 28) = fixture_dir[i].date;
        set_bitmap(mems_disk + 16, header, 1);
        if (size > 960) {
            for (int k = 0; k < 2; k++) {
                int block = (i == 0 ? 8 : 130) + k;
                *(u16 *)(bp + 64 + k * 2) = block;
                set_bitmap(mems_disk + 16, block, 1);
                u8 *data = payload(1024, i * 31 + k * 7);
                memcpy(mems_disk + block * 1024, data, 1024); free(data);
            }
        } else {
            for (int k = 0; k < size; k++) bp[64 + k] = (i * 7 + k) % 251;
        }
    }
    int available = 0;
    for (int i = 8; i < 8064; i++)
        if (!(mems_disk[16 + i / 8] & (1u << (i % 8)))) available++;
    *(u16 *)(mems_disk + 12) = available;
    reload_model();
}
static void clear_counts(void)
{
    loads_header = loads_window = 0;
}
static void batch_cost(int count, const char *layout, int enforce)
{
    fixture(count, layout); clear_counts();
    BUPSTAT stat;
    assert(sro_bup_stat(1, 32, &stat) == 0);
    int ranges, singles;
    if (!strcmp(layout, "sparse")) { ranges = 0; singles = count; }
    else if (!strcmp(layout, "mixed")) { ranges = 0; singles = 10; }
    else { ranges = count / 64; singles = count % 64; }
    if (enforce) { assert(loads_window == ranges); assert(loads_header == singles); }
    assert(loads_window * 65536 + loads_header * 1024 == count * 1024);
    assert(flushes == 0);
    printf("layout=%s saves=%d requests=%d ranges=%d singles=%d header_bytes=%d window_commands=%d\n",
           layout, count, loads_window + loads_header, loads_window, loads_header,
           loads_window * 65536 + loads_header * 1024, loads_window);
    BUPDIR output[448]; memset(output, 0xcc, sizeof(output));
    assert(sro_bup_dir(1, "", 448, output) == count);
    int index = 0;
    for (int slot = 0; slot < 448; slot++) {
        u8 *entry = mems_disk + 1024 + slot * 16;
        if (!entry[0]) continue;
        assert(memcmp(output[index].file_name, entry, 11) == 0);
        assert(output[index].data_size == 32); index++;
    }
    if (count) {
        for (int chosen = 0; chosen < 2; chosen++) {
            int id = chosen ? count - 1 : 0;
            u8 output_data[32], expected[32];
            for (int k = 0; k < 32; k++) expected[k] = (id * 7 + k) % 251;
            assert(sro_bup_read(1, fixture_dir[id].file_name, output_data) == 0);
            assert(memcmp(output_data, expected, 32) == 0);
        }
    }
    assert(flushes == 0);
}
static void cache_and_flush(void)
{
    fixture(64, "overlap");
    u8 *expected = malloc(2048), *out = malloc(2048); assert(expected && out);
    memcpy(expected, mems_disk + 8 * 1024, 2048);
    assert(sro_bup_read(1, fixture_dir[0].file_name, out) == 0);
    assert(memcmp(out, expected, 2048) == 0);
    assert(mblk_start == 8 && mblk_end == 71);
    int reads = loads_window;
    clear_counts();
    assert(sro_bup_read(1, fixture_dir[0].file_name, out) == 0);
    assert(memcmp(out, expected, 2048) == 0);
    assert(loads_window == 0 && mblk_start == 8);
    assert(flushes == 0 && reads == 2);
    for (int i = 0; i < 2048; i++) expected[i] = (i * 13 + 17) % 251;
    fixture_dir[0].date++;
    assert(sro_bup_write(1, &fixture_dir[0], expected, 0) == 0);
    assert(mblk_bmp[0] == 0 && mblk_bmp[1] == 0);
    int writes = flushes;
    assert(sro_bup_read(1, fixture_dir[0].file_name, out) == 0);
    assert(memcmp(out, expected, 2048) == 0);
    assert(sro_bup_read(1, fixture_dir[1].file_name, out) == 0);
    assert(memcmp(out, mems_disk + 130 * 1024, 2048) == 0);
    assert(flushes == writes); /* A read cannot reflush an already saved window. */
    reload_model(); check_save(1, &fixture_dir[0], expected);
    free(expected); free(out);
}
static void full_live_cache(void)
{
    fixture(448, "dense");
    BUPDIR target = fixture_dir[0]; target.data_size = 2048;
    u8 *data = payload(2048, 41);
    assert(sro_bup_write(1, &target, data, 0) == 0);
    assert(mblk_start && mblk_end && !mblk_bmp[0] && !mblk_bmp[1]);
    int start = mblk_start, end = mblk_end, writes = flushes;
    u8 *window = malloc(65536); assert(window);
    memcpy(window, (void *)MEMS_BUFFER, 65536);
    BUPSTAT stat;
    assert(sro_bup_stat(1, 32, &stat) == 0);
    clear_counts();
    assert(sro_bup_stat(1, 32, &stat) == 0);
    assert(loads_header == 448 && loads_window == 0);
    assert(mblk_start == start && mblk_end == end && flushes == writes);
    assert(memcmp(window, (void *)MEMS_BUFFER, 65536) == 0);
    u8 out[2048];
    assert(sro_bup_read(1, target.file_name, out) == 0);
    assert(memcmp(out, data, sizeof(out)) == 0);
    assert(loads_window == 0 && flushes == writes);
    reload_model(); check_save(1, &target, data);
    free(window); free(data);
}
static void batch_failure(int fault)
{
    fixture(64, "dense");
    /* Preseed a complete valid batch so a missing status check cannot
     * happen to pass by rejecting unrelated stale bytes after a reported failed read. */
    memcpy((void *)MEMS_BUFFER, mems_disk + 8 * 1024, 65536);
    u8 *before = malloc(sizeof(mems_disk)); assert(before); memcpy(before, mems_disk, sizeof(mems_disk));
    BUPSTAT stat, old_stat; memset(&stat, 0xcc, sizeof(stat)); old_stat = stat;
    load_failure = fault;
    assert(sro_bup_stat(1, 32, &stat) == BUP_BROKEN);
    assert(loads_window == 1 && mblk_start == 0 && mblk_end == 0);
    assert(memcmp(&stat, &old_stat, sizeof(stat)) == 0);
    assert(memcmp(before, mems_disk, sizeof(mems_disk)) == 0 && flushes == 0);
    assert(memcmp((void *)MEMS_HEADER, mems_disk, 8192) == 0);
    load_failure = 0;
    assert(sro_bup_stat(1, 32, &stat) == 0);
    u8 out[32]; assert(sro_bup_read(1, fixture_dir[0].file_name, out) == 0);
    assert(memcmp(out, mems_disk + 8 * 1024 + 64, 32) == 0);
    free(before);
}
static void dirty_window(void)
{
    fixture(64, "dense");
    mblk_bmp[0] = 0x12; mblk_bmp[1] = 0x34;
    BUPSTAT stat;
    assert(sro_bup_stat(1, 32, &stat) == BUP_BROKEN);
    assert(mblk_bmp[0] == 0x12 && mblk_bmp[1] == 0x34);
    assert(loads_window == 0 && flushes == 0);
}
static void changed_metadata(int reason)
{
    fixture(64, "dense");
    BUPSTAT stat;
    assert(sro_bup_stat(1, 32, &stat) == 0);
    int requests = loads_window;
    u8 *header = mems_disk + (8 + reason) * 1024;
    *(u32 *)(header + 12) = 0xffffffff;
    assert(sro_bup_stat(1, 32, &stat) == BUP_BROKEN);
    assert(loads_window == requests + 1 && flushes == 0);
    *(u32 *)(header + 12) = 32;
    assert(sro_bup_stat(1, 32, &stat) == 0);
}
static void full_validation(int reason)
{
    fixture(64, "overlap");
    u8 *header = mems_disk + (64 + (reason == 4 ? 63 : 0)) * 1024;
    if (reason == 0) *(u16 *)(header + 64) = 65; /* Neighbor header. */
    else if (reason == 1) *(u16 *)(header + 64) = 130; /* Shared payload. */
    else if (reason == 2) *(u16 *)(header + 66) = 8; /* Duplicate payload. */
    else if (reason == 3) *(u16 *)(header + 68) = 9; /* Bad terminator. */
    else if (reason == 4) header[0] = 'Q';
    else if (reason == 5) *(u16 *)(header + 64) = 8064;
    else if (reason == 6 || reason == 7) {
        int chosen = reason == 6 ? 0 : 63;
        header = mems_disk + fixture_header[chosen] * 1024;
        memcpy(mems_disk + 1024 + fixture_slot[chosen] * 16, "ABC\0AAAAAAA", 11);
        memcpy(header, "ABC\0BBBBBBB", 11);
    }
    else abort();
    reload_model();
    BUPSTAT stat, previous;
    memset(&stat, 0xcc, sizeof(stat)); previous = stat;
    assert(sro_bup_stat(1, 32, &stat) == BUP_BROKEN);
    assert(loads_window == 1 && flushes == 0);
    assert(memcmp(&stat, &previous, sizeof(stat)) == 0);
    assert(memcmp((void *)MEMS_HEADER, mems_disk, 8192) == 0);
}
static void mutation_after_batch(const char *kind)
{
    int shrink = !strcmp(kind, "shrink");
    int fresh = !strcmp(kind, "new-inline") || !strcmp(kind, "new-blocks");
    int remove = !strcmp(kind, "delete");
    fixture(64, shrink ? "overlap" : "dense");
    u8 *before = malloc(sizeof(mems_disk)); assert(before);
    memcpy(before, mems_disk, sizeof(mems_disk));
    int size = (!strcmp(kind, "grow") || !strcmp(kind, "new-blocks")) ? 2048 : 32;
    BUPDIR target = fresh ? directory("NEW00000001", size) : fixture_dir[0];
    target.data_size = size; target.date++;
    u8 *data = payload(size, 41);
    if (remove) assert(sro_bup_delete(1, target.file_name) == 0);
    else assert(sro_bup_write(1, &target, data, 0) == 0);
    assert(loads_window >= 1); /* The mutation follows a full batch validation. */
    for (int i = fresh ? 0 : 1; i < 64; i++) {
        int offset = fixture_header[i] * 1024;
        assert(memcmp(before + offset, mems_disk + offset, 1024) == 0);
    }
    if (shrink) assert(memcmp(before + 130 * 1024, mems_disk + 130 * 1024, 2048) == 0);
    reload_model();
    if (remove) assert(sro_bup_read(1, target.file_name, data) == BUP_NOT_FOUND);
    else check_save(1, &target, data);
    BUPSTAT stat; assert(sro_bup_stat(1, 32, &stat) == 0);
    BUPDIR entries[65];
    assert(sro_bup_dir(1, "", 65, entries) == 64 + fresh - remove);
    free(before); free(data);
}
int main(int argc, char **argv)
{
    assert(argc >= 2);
    if (!strcmp(argv[1], "cost") || !strcmp(argv[1], "profile")) {
        assert(argc == 4); batch_cost(atoi(argv[2]), argv[3], !strcmp(argv[1], "cost"));
    } else if (!strcmp(argv[1], "mutate")) mutation_after_batch(argv[2]);
    else if (!strcmp(argv[1], "cache")) cache_and_flush();
    else if (!strcmp(argv[1], "live-full")) full_live_cache();
    else if (!strcmp(argv[1], "failure")) batch_failure(atoi(argv[2]));
    else if (!strcmp(argv[1], "dirty")) dirty_window();
    else if (!strcmp(argv[1], "changed")) changed_metadata(atoi(argv[2]));
    else if (!strcmp(argv[1], "corrupt")) full_validation(atoi(argv[2]));
    else abort();
    return 0;
}
