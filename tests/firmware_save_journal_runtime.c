/* Test-only synchronous journal file and primary-archive owners. The allocator,
 * planner, record core and runtime are the public production C sources. */
#define main original_resize_main
#include "firmware_save_resize.c"
#undef main

#ifdef SAROO_SAVE_GENERATION
/* Only the command gate and external transports are modeled here. The menu
 * allocator, journal binding, public backup entries and record core are the
 * actual production sources compiled with the staged integration enabled. */
static int model_lease, model_busy, model_fault, model_leaves;

int command_enter(void)
{
    if (model_busy || model_lease || model_fault) return 0;
    model_lease = 1;
    return 1;
}
int backup_enter(void) { return command_enter(); }
int command_leave(void)
{
    assert(model_lease);
    model_leaves++;
    if (model_fault) return 0;
    model_lease = 0;
    return 1;
}
int command_faulted(void) { return model_fault; }
void command_fault(void) { model_fault = 1; }
int command_reply(int tag, int minimum, int maximum)
{
    (void)tag; (void)minimum; (void)maximum;
    assert(model_lease || journal_state == JOURNAL_LEGACY);
    return test_arg;
}
int command_printk(char *fmt, ...) { (void)fmt; return 0; }
int printk(char *fmt, ...) { (void)fmt; return 0; }
int command_memcmp(void *left, void *right, int count)
{ return memcmp(left, right, (size_t)count); }
#endif

const u8 test_game_id[16] = "TESTGAME00000001";
_Alignas(8) u8 test_work[0x100 + SJ_WRITE_BYTES];
static u8 journal_disk[SJ_MAX_BYTES];
static unsigned journal_size, journal_exists, journal_writes, primary_writes;
static int fail_body, fail_header_reply, fail_apply, fail_partial;

int read_file_bounded(char *name, int offset, int size, void *data, u32 capacity)
{
    assert(!strcmp(name, JOURNAL_FILE));
    assert(offset >= 0 && size >= 0 && (u32)size <= capacity);
    if (!journal_exists) return -4; /* FR_NO_FILE */
    unsigned pos = offset, count = size;
    if (pos >= journal_size) return 0;
    if (count > journal_size - pos) count = journal_size - pos;
    memcpy(data, journal_disk + pos, count);
    return count;
}

int write_file(char *name, int offset, int size, void *data)
{
    assert(!strcmp(name, JOURNAL_FILE));
    assert(size > 0 && size <= SJ_WRITE_BYTES && data);
    if (fail_body && offset == -1) { fail_body = 0; return -1; }
    if (offset == -1) { journal_size = 0; journal_exists = 1; offset = 0; }
    assert(offset >= 0 && (unsigned)offset <= SJ_MAX_BYTES - (unsigned)size);
    memcpy(journal_disk + offset, data, size);
    if (journal_size < (unsigned)offset + (unsigned)size)
        journal_size = offset + size;
    journal_writes++;
    if (fail_header_reply && !offset && size == SJ_HEADER_BYTES &&
        !memcmp(data, "SJR1", 4)) {
        fail_header_reply = 0;
        return -1; /* Durable publication, reply lost. */
    }
    return size;
}

int archive_find_slot(const u8 *key, void *workspace, u32 capacity)
{
    (void)workspace;
    assert(capacity >= 512 && capacity <= SJ_WRITE_BYTES);
    return key && !memcmp(key, test_game_id, 16) ? 65536 : -4;
}

int archive_command(unsigned device, u32 offset, void *data, int length)
{
    assert(device <= 1);
    if (!length) { assert(!offset && !data); return 0; }
    unsigned size = length < 0 ? -length : length;
    assert(size <= SJ_WRITE_BYTES && data);
    u8 *disk;
    unsigned extent;
    if (device) { disk = mems_disk; extent = sizeof(mems_disk); }
    else {
        assert(offset >= 65536);
        offset -= 65536;
        disk = slot_disk;
        extent = sizeof(slot_disk);
    }
    assert(offset <= extent && size <= extent - offset);
    if (length < 0) {
        if (fail_apply) { fail_apply = 0; return -1; }
        if (fail_partial) {
            fail_partial = 0;
            memcpy(disk + offset, data, size / 2);
            primary_writes++;
            return -1;
        }
        memcpy(disk + offset, data, size);
        primary_writes++;
    } else memcpy(data, disk + offset, size);
    return size;
}

#ifdef SAROO_SAVE_GENERATION
int read_file_bounded_owned(char *name, int offset, int size, void *data, u32 capacity)
{ assert(model_lease); return read_file_bounded(name, offset, size, data, capacity); }
int write_file_owned(char *name, int offset, int size, void *data)
{ assert(model_lease); return write_file(name, offset, size, data); }
int archive_find_slot_owned(const u8 *key, void *workspace, u32 capacity)
{ assert(model_lease); return archive_find_slot(key, workspace, capacity); }
int archive_command_owned(unsigned device, u32 offset, void *data, int length)
{ assert(model_lease); return archive_command(device, offset, data, length); }
#endif

static int operation(int dev, const char *change, BUPDIR *dir, u8 *bytes)
{
#ifdef SAROO_SAVE_GENERATION
    if (!strcmp(change, "delete")) return bup_delete(dev, dir->file_name);
    if (!strcmp(change, "format")) return bup_format(dev);
    return bup_write(dev, dir, bytes, 0);
#else
    if (!strcmp(change, "delete")) return sro_bup_delete(dev, dir->file_name);
    if (!strcmp(change, "format")) return sro_bup_format(dev);
    return sro_bup_write(dev, dir, bytes, 0);
#endif
}

static void baseline(int dev, const char *change, BUPDIR *dir, u8 *bytes,
                     u8 *before_slot, u8 *before_mems,
                     u8 *expected_slot, u8 *expected_mems)
{
    journal_state = JOURNAL_LEGACY;
    initialize();
    BUPDIR neighbor = directory("KEEP0000001", 32);
    u8 *keep = payload(32, 7);
    assert(sro_bup_write(dev, &neighbor, keep, 0) == 0);
    if (strcmp(change, "new")) {
        BUPDIR old = directory("TEST0000001", 8192);
        u8 *old_data = payload(8192, 3);
        assert(sro_bup_write(dev, &old, old_data, 0) == 0);
        free(old_data);
    }
    reload_model();
    memcpy(before_slot, slot_disk, sizeof(slot_disk));
    memcpy(before_mems, mems_disk, sizeof(mems_disk));
    assert(operation(dev, change, dir, bytes) == 0);
    memcpy(expected_slot, slot_disk, sizeof(slot_disk));
    memcpy(expected_mems, mems_disk, sizeof(mems_disk));
    memcpy(slot_disk, before_slot, sizeof(slot_disk));
    memcpy(mems_disk, before_mems, sizeof(mems_disk));
    reload_model();
    free(keep);
}

static void exercise(int dev, int size, const char *change, const char *failure)
{
    u8 *before_slot = malloc(sizeof(slot_disk)), *before_mems = malloc(sizeof(mems_disk));
    u8 *expected_slot = malloc(sizeof(slot_disk)), *expected_mems = malloc(sizeof(mems_disk));
    assert(before_slot && before_mems && expected_slot && expected_mems);
    BUPDIR target = directory(!strcmp(change, "new") ? "NEW00000001" : "TEST0000001", size);
    target.date++;
    memcpy(target.comment, "Chapter 2 ", 10);
    target.language = 2;
    u8 *bytes = payload(size, 71);
    baseline(dev, change, &target, bytes, before_slot, before_mems,
             expected_slot, expected_mems);
    journal_exists = journal_size = journal_writes = primary_writes = 0;
    memset(journal_disk, 0, sizeof(journal_disk));
    int legacy_flushes = flushes;
    assert(journal_start() == 0 && journal_select(test_game_id) == 0);
#ifdef SAROO_SAVE_GENERATION
    /* A foreign command owner must refuse a public API call before changing
     * either the active record or the selected archive. */
    unsigned before_busy_writes = journal_writes;
    model_busy = 1;
    assert(bup_write(dev, &target, bytes, 0) == BUP_BROKEN);
    model_busy = 0;
    assert(journal_writes == before_busy_writes && !model_lease);
    BUPDIR *overlapping_dir = (BUPDIR *)(test_work + 0x200);
    memcpy(overlapping_dir, &target, sizeof(*overlapping_dir));
    assert(bup_write(dev, overlapping_dir, bytes, 0) == BUP_BROKEN);
    if (target.data_size)
        assert(bup_write(dev, &target, test_work + 0x200, 0) == BUP_BROKEN);
    assert(bup_delete(dev, (char *)(test_work + 0x200)) == BUP_BROKEN);
    assert(journal_writes == before_busy_writes && !model_lease);
#endif
    if (!strcmp(failure, "body")) fail_body = 1;
    if (!strcmp(failure, "header-reply")) fail_header_reply = 1;
    if (!strcmp(failure, "apply")) fail_apply = 1;
    if (!strcmp(failure, "partial")) fail_partial = 1;
    if (!strcmp(failure, "corrupt")) fail_apply = 1;
    int result = operation(dev, change, &target, bytes);
    if (strcmp(failure, "none")) {
        assert(result == BUP_BROKEN && journal_state == JOURNAL_RECOVER);
        assert(!memcmp(slot_disk, before_slot, sizeof(slot_disk)) ||
               !strcmp(failure, "partial"));
        assert(!memcmp(mems_disk, before_mems, sizeof(mems_disk)) ||
               !strcmp(failure, "partial"));
        if (!strcmp(failure, "corrupt")) {
            assert(journal_size > SJ_HEADER_BYTES + 8);
            journal_disk[journal_size - 1] ^= 1;
            unsigned old_writes = primary_writes;
#ifdef SAROO_SAVE_GENERATION
            assert(backup_enter());
#endif
            assert(journal_admit(1) == BUP_BROKEN);
#ifdef SAROO_SAVE_GENERATION
            assert(command_leave());
#endif
            assert(primary_writes == old_writes);
            goto done;
        }
        if (!strcmp(failure, "partial")) {
            reload_model();
            journal_state = JOURNAL_OFF;
            assert(journal_start() == 0 && journal_select(test_game_id) == 0);
        } else {
#ifdef SAROO_SAVE_GENERATION
            assert(backup_enter());
#endif
            assert(journal_admit(1) == 0);
#ifdef SAROO_SAVE_GENERATION
            assert(command_leave());
#endif
        }
        if (!strcmp(failure, "body")) {
            assert(!memcmp(slot_disk, before_slot, sizeof(slot_disk)));
            assert(!memcmp(mems_disk, before_mems, sizeof(mems_disk)));
            assert(operation(dev, change, &target, bytes) == 0);
        }
    } else assert(result == 0);
    assert(journal_writes > 0 && primary_writes > 0);
    assert(!memcmp(slot_disk, expected_slot, sizeof(slot_disk)));
    assert(!memcmp(mems_disk, expected_mems, sizeof(mems_disk)));
    SaveJournalRecovery recovered;
#ifdef SAROO_SAVE_GENERATION
    assert(backup_enter());
#endif
    assert(save_journal_load(&recovered, JOURNAL_WORK, resize_bitmap,
                             journal_read, NULL) == SJ_EMPTY);
#ifdef SAROO_SAVE_GENERATION
    assert(command_leave());
#endif
    reload_model();
#ifdef SAROO_SAVE_GENERATION
    assert(backup_enter());
#endif
    if (!strcmp(change, "delete") || !strcmp(change, "format")) {
        BUPDIR found;
        assert(sro_bup_dir(dev, target.file_name, 1, &found) == 0);
        if (!strcmp(change, "delete")) {
            BUPDIR neighbor = directory("KEEP0000001", 32);
            u8 *keep = payload(32, 7);
            check_save(dev, &neighbor, keep);
            free(keep);
        } else {
            assert(sro_bup_dir(dev, "KEEP0000001", 1, &found) == 0);
        }
    } else check_save(dev, &target, bytes);
#ifdef SAROO_SAVE_GENERATION
    assert(command_leave());
#endif
done:
    assert(flushes == legacy_flushes);
#ifdef SAROO_SAVE_GENERATION
    assert(!model_lease && model_leaves > 0);
#endif
    printf("PASS device=%d size=%d change=%s failure=%s journal=%u primary=%u\n",
           dev, size, change, failure, journal_writes, primary_writes);
    free(before_slot); free(before_mems); free(expected_slot); free(expected_mems); free(bytes);
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    exercise(atoi(argv[1]), atoi(argv[2]), argv[3], argv[4]);
    return 0;
}
