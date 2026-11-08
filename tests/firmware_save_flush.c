/* Actual-module dirty bookkeeping; no fault injection is linked into firmware. */
#define main resize_test_main
#include "firmware_save_resize.c"
#undef main

static void flush_status(int flag, int status)
{
    initialize();
    BUPDIR dir = directory("TEST0000001", 2048);
    u8 *data = payload(2048, 19);
    assert(sro_bup_write(1, &dir, data, 0) == 0);
    int start = mblk_start, end = mblk_end, header = msav_blk;
    mblk_bmp[0] = (int)0x80000001u;
    mblk_bmp[1] = (int)0x80000010u;
    flush_failure = status;
    int writes = flushes;
    bup_update(flag);
    assert(flushes == writes + 1);
    assert(SS_ARG == (u16)status);
    int clean = (flag & MEMS_BUF) && status == 0;
    assert((u32)mblk_bmp[0] == (clean ? 0 : 0x80000001u));
    assert((u32)mblk_bmp[1] == (clean ? 0 : 0x80000010u));
    assert(mblk_start == start && mblk_end == end && msav_blk == header);
    if ((flag & MEMS_BUF) && status) {
        flush_failure = 0;
        bup_update(flag);
        assert(mblk_bmp[0] == 0 && mblk_bmp[1] == 0);
        assert(mblk_start == start && mblk_end == end && msav_blk == header);
    }
    free(data);
}

static void read_after_write(void)
{
    initialize();
    /* More than one 64 KiB window makes the following read evict the final
     * written window. No reload/reset hides dirty state between requests. */
    BUPDIR dir = directory("TEST0000001", 65 * 1024);
    u8 *data = payload(dir.data_size, 23), *out = malloc(dir.data_size);
    assert(out);
    assert(sro_bup_write(1, &dir, data, 0) == 0);
    int writes = flushes;
    assert(sro_bup_read(1, dir.file_name, out) == 0);
    assert(memcmp(out, data, dir.data_size) == 0);
    assert(flushes == writes);
    assert(sro_bup_verify(1, dir.file_name, out) == 0 && flushes == writes);
    reload_model();
    check_save(1, &dir, data);
    free(data); free(out);
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    if (!strcmp(argv[1], "status")) {
        assert(argc == 4);
        flush_status(atoi(argv[2]), atoi(argv[3]));
    } else if (!strcmp(argv[1], "read-after-write")) {
        read_after_write();
    } else abort();
    return 0;
}
