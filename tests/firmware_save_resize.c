#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "game_save.c"

_Alignas(8) u8 test_shared[0x23000];
u16 test_arg;
static u16 command;
static u8 slot_disk[0x10000], mems_disk[0x800000];
static int disk_header, disk_window, flushes, loads_header, loads_window;
static int load_failure, flush_failure;

/* Model the MCU's synchronous load_smems/flush_smems commands, including its
 * separate header buffer and overlapping 64 KiB data window. Persistent bytes
 * use host byte order here; this tests API/allocation/flush logic, not bus timing
 * or the Saturn's disk-byte encoding. The firmware module itself is unchanged. */
static u16 *test_command(void)
{
    if (!command) return &command;
    if (command == SSCMD_SSAVE) {
        memcpy(slot_disk, test_shared, sizeof(slot_disk));
        flushes++;
    } else if (command == SSCMD_LMEMS) {
        int id = test_arg & 0x7fff;
        if (id) {
            if (test_arg & 0x8000) {
                assert(id < 8192);
                loads_header++;
                memcpy((void *)MEMS_TMP, mems_disk + id * 1024, 1024);
                disk_header = id;
            } else {
                assert(id + 64 <= 8192);
                loads_window++;
                memcpy((void *)MEMS_BUFFER, mems_disk + id * 1024, load_failure ? 32768 : 65536);
                disk_window = id;
            }
        }
    } else {
        assert(command == SSCMD_SMEMS);
        int d = disk_header - disk_window;
        if (d >= 0 && d < 64)
            memcpy((u8 *)MEMS_BUFFER + d * 1024, (void *)MEMS_TMP, 1024);
        if (test_arg & MEMS_HDR)
            memcpy(mems_disk, (void *)MEMS_HEADER, 8192);
        if ((test_arg & MEMS_SAV) && (d < 0 || d >= 64 || !(test_arg & MEMS_BUF)))
            memcpy(mems_disk + disk_header * 1024, (void *)MEMS_TMP, 1024);
        if (test_arg & MEMS_BUF)
            memcpy(mems_disk + disk_window * 1024, (void *)MEMS_BUFFER, 65536);
        flushes++;
    }
    /* The real dispatcher publishes the helper result before clearing SS_CMD.
     * A failed flush here is deliberately late/uncertain, not a rollback. */
    test_arg = (command == SSCMD_SMEMS || command == SSCMD_SSAVE) ? flush_failure :
               (command == SSCMD_LMEMS && !(test_arg & 0x8000)) ? load_failure : 0;
    command = 0;
    return &command;
}

static void reload_model(void)
{
    memset(test_shared, 0, sizeof(test_shared));
    memcpy(test_shared, slot_disk, sizeof(slot_disk));
    memcpy((void *)MEMS_HEADER, mems_disk, 8192);
    mblk_start = mblk_end = msav_blk = disk_header = disk_window = 0;
    mblk_bmp[0] = mblk_bmp[1] = 0;
    command = test_arg = 0;
}

static void initialize(void)
{
    memset(slot_disk, 0, sizeof(slot_disk));
    memset(mems_disk, 0, sizeof(mems_disk));
    reload_model();
    BUPMEM->magic0 = MEMS->magic0 = 0x5361726f;
    BUPMEM->magic1 = 0x53617665;
    BUPMEM->total_size = 65536;
    BUPMEM->block_size = 128;
    BUPMEM->free_block = 511;
    BUPMEM->bitmap[0] = 1;
    MEMS->magic1 = 0x4d656d73;
    MEMS->total_size = sizeof(mems_disk);
    MEMS->free_block = 8056;
    MEMS->bitmap[0] = 255;
    memcpy(slot_disk, test_shared, sizeof(slot_disk));
    memcpy(mems_disk, (void *)MEMS_HEADER, 8192);
    flushes = loads_header = loads_window = 0;
    load_failure = flush_failure = 0;
}

static u8 *payload(int size, int seed)
{
    u8 *data = malloc(size ? (size_t)size : 1);
    assert(data);
    for (int i = 0; i < size; i++) data[i] = (i * 7 + seed) % 251;
    return data;
}

static BUPDIR directory(const char *name, int size)
{
    BUPDIR dir = {0};
    memcpy(dir.file_name, name, 11);
    memcpy(dir.comment, "Keep meta!", 10);
    dir.language = 1;
    dir.date = 0x123456;
    dir.data_size = size;
    return dir;
}

static void check_save(int dev, BUPDIR *expected, const u8 *data)
{
    BUPDIR actual = {0};
    assert(sro_bup_dir(dev, expected->file_name, 1, &actual) == 1);
    assert(actual.data_size == expected->data_size);
    assert(actual.date == expected->date);
    assert(memcmp(actual.comment, expected->comment, 10) == 0);
    assert(actual.language == expected->language);
    u8 *readback = malloc(expected->data_size + 16);
    assert(readback);
    memset(readback, 0xcc, expected->data_size + 16);
    assert(sro_bup_read(dev, expected->file_name, readback) == 0);
    assert(memcmp(readback, data, expected->data_size) == 0);
    for (u32 i = 0; i < 16; i++) assert(readback[expected->data_size + i] == 0xcc);
    assert(sro_bup_verify(dev, expected->file_name, readback) == 0);
    free(readback);
}

static void round_trip(int dev, int old_size, int new_size)
{
    initialize();
    BUPDIR target = directory("TEST0000001", old_size);
    BUPDIR neighbor = directory("KEEP0000001", 32);
    u8 *old = payload(old_size, 3), *replacement = payload(new_size, 19);
    u8 *keep = payload(32, 7);
    assert(sro_bup_write(dev, &target, old, 0) == 0);
    assert(sro_bup_write(dev, &neighbor, keep, 0) == 0);
    reload_model();
    target.data_size = new_size;
    target.date++;
    assert(sro_bup_write(dev, &target, replacement, 0) == 0);
    check_save(dev, &target, replacement);
    check_save(dev, &neighbor, keep);
    reload_model();
    check_save(dev, &target, replacement);
    check_save(dev, &neighbor, keep);
    int size = dev ? 1024 : 128;
    int data_blocks = dev && new_size <= 960 ? 0 : (new_size + size - 1) / size;
    int expected_free = (dev ? 8056 : 511) - 2 - data_blocks - (dev ? 0 : 1);
    assert((dev ? MEMS->free_block : BUPMEM->free_block) == expected_free);
    free(old); free(replacement); free(keep);
    puts("Replacement and neighboring save survive a modeled RAM discard/reload.");
}

static void rejection(int dev, const char *reason)
{
    initialize();
    int expected = BUP_NOT_ENOUGH_MEMORY, mode = 0, size = 4097;
    BUPDIR target = directory("TEST0000001", 2048);
    if (!strcmp(reason, "directory-full")) {
        assert(dev == 1);
        for (int i = 0; i < 448; i++) {
            char name[12];
            snprintf(name, sizeof(name), "OLD%08d", i);
            u8 *entry = (u8 *)MEMS_HEADER + 1024 + i * 16;
            memcpy(entry, name, 11);
            *(u16 *)(entry + 14) = 8 + i;
            memcpy(mems_disk + (8 + i) * 1024, name, 11);
            set_bitmap((u8 *)MEMS->bitmap, 8 + i, 1);
        }
        MEMS->free_block = 8056 - 448;
        memcpy(mems_disk, (void *)MEMS_HEADER, 8192);
        size = 32;
    } else {
        u8 *old = payload(2048, 3);
        assert(sro_bup_write(dev, &target, old, 0) == 0);
        free(old);
        if (!strcmp(reason, "full")) {
            memset(dev ? (void *)MEMS->bitmap : (void *)BUPMEM->bitmap,
                   255, dev ? 1008 : 64);
            if (dev) MEMS->free_block = 0; else BUPMEM->free_block = 0;
            bup_update(dev ? MEMS_HDR : BUPSAVE);
        } else if (!strcmp(reason, "bad-chain")) {
            bup_dev = dev;
            int block = find_save(target.file_name, 0, NULL);
            assert(block > 0);
            u8 *bp = get_block_addr(START_BLOCK | block);
            if (dev) *(u16 *)(bp + 66) = *(u16 *)(bp + 64);
            else set_bitmap(bp + 64, block, 1);
            bup_update(dev ? MEMS_SAV : BUPSAVE);
            expected = BUP_BROKEN;
        } else if (!strcmp(reason, "protected")) {
            expected = BUP_FOUND;
            mode = 1;
        } else {
            assert(!strcmp(reason, "oversized") || !strcmp(reason, "new-oversized"));
            size = dev ? 479 * 1024 + 1 : 510 * 128 + 1;
            if (!strcmp(reason, "new-oversized")) memcpy(target.file_name, "NEW0000001", 11);
        }
    }
    reload_model();
    u8 *before_slot = malloc(sizeof(slot_disk)), *before_mems = malloc(sizeof(mems_disk));
    assert(before_slot && before_mems);
    memcpy(before_slot, slot_disk, sizeof(slot_disk));
    memcpy(before_mems, mems_disk, sizeof(mems_disk));
    u8 bitmap_before[1008];
    memcpy(bitmap_before, dev ? (void *)MEMS->bitmap : (void *)BUPMEM->bitmap, dev ? 1008 : 64);
    int writes_before = flushes;
    u8 *data = payload(size, 13);
    target.data_size = size;
    target.date++;
    assert(sro_bup_write(dev, &target, data, mode) == expected);
    assert(flushes == writes_before);
    assert(memcmp(slot_disk, before_slot, sizeof(slot_disk)) == 0);
    assert(memcmp(mems_disk, before_mems, sizeof(mems_disk)) == 0);
    assert(memcmp(bitmap_before, dev ? (void *)MEMS->bitmap : (void *)BUPMEM->bitmap,
                  dev ? 1008 : 64) == 0);
    free(before_slot); free(before_mems); free(data);
    puts("Rejected write leaves persistent saves and the allocation bitmap unchanged.");
}

int main(int argc, char **argv)
{
    _Static_assert(offsetof(BUPDIR, comment) == 12, "BUP comment offset");
    _Static_assert(offsetof(BUPDIR, language) == 23, "BUP language offset");
    _Static_assert(offsetof(BUPDIR, data_size) == 28, "BUP size offset");
    assert(argc == 4);
    if (!strcmp(argv[1], "reject")) rejection(atoi(argv[2]), argv[3]);
    else round_trip(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    return 0;
}
