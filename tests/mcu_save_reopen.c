/* Adapted from paps-g d5cb72ea. Actual FatFs with faults at the disk boundary. */
#define MCU_RUNTIME_TEST_ENTRY existing_runtime_main
#define disk_write ordinary_disk_write
#include "mcu_save_runtime.c"
#undef disk_write
#undef MCU_RUNTIME_TEST_ENTRY

static int fail_writes;
static int fail_append;
static unsigned payload_writes;
DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
    if (fail_writes) return RES_ERROR;
    uintptr_t address = (uintptr_t)data;
    if (fail_append && address >= SSAVE_ADDR && address - SSAVE_ADDR < 65536 && ++payload_writes == 2)
        return RES_ERROR;
    return ordinary_disk_write(drive, data, sector, count);
}

static int read_save(int kind)
{
    return kind ? load_smems(kind == 1 ? 100 : 0x8064) : load_savefile((char *)key_b);
}
static int write_save(int kind) { return kind ? flush_smems(7) : flush_savefile(); }
static FIL *target_file;
static FATFS *mounted;
static const char *alteration;
static const char *old_name = "/SAROO/OLD.BIN";
static u8 *altered[2];
static FSIZE_t altered_size[2];
static void compare_altered(int capture)
{
    for (int i = 0; i < 2; i++) {
        FIL file; assert(!f_open(&file, filename(i), FA_READ));
        u8 *bytes = read_archive(&file);
        if (capture) { altered[i] = bytes; altered_size[i] = f_size(&file); }
        else {
            assert(f_size(&file) == altered_size[i] && !memcmp(bytes, altered[i], altered_size[i]));
            free(bytes); free(altered[i]); altered[i] = NULL;
        }
        assert(!f_close(&file));
    }
}
static void alter_before_open(FIL *fp, const TCHAR *name, BYTE mode)
{
    assert(fp == target_file && mode == (FA_READ | FA_WRITE) && !fp->obj.fs);
    assert(lock_depth == 1);
    before_open = NULL;
    if (!strcmp(alteration, "generation")) {
        mounted->id++;
    } else if (!strcmp(alteration, "missing") || !strcmp(alteration, "replaced")) {
        assert(!f_rename(name, old_name));
        if (!strcmp(alteration, "replaced"))
            make_archive(fp == &sm_fp, fp == &sm_fp ? 0x800000 : slot_size, 0);
    } else {
        FIL other; UINT count; u8 byte = 'X';
        assert(!f_open(&other, name, FA_READ | FA_WRITE));
        if (!strcmp(alteration, "size")) assert(!f_lseek(&other, f_size(&other)));
        else assert(!strcmp(alteration, "signature"));
        assert(!f_write(&other, &byte, 1, &count) && count == 1 && !f_close(&other));
    }
    if (strcmp(alteration, "missing")) compare_altered(1);
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    const char *mode = argv[3]; int kind = atoi(argv[4]);
    disk = fopen(argv[1], "r+b"); assert(disk); fault = "none";
    BYTE work[32768];
    MKFS_PARM options = {.fmt = (!strcmp(argv[2], "fat32") ? FM_FAT32 : FM_EXFAT) | FM_SFD,
                         .n_fat = 1, .au_size = !strcmp(argv[2], "fat32") ? 512 : 4096};
    assert(!f_mkfs("0:", &options, work, sizeof work));
    FATFS fs; assert(!f_mount(&fs, "0:", 1)); mounted = &fs;
    initialize(); require_lock = 1; target_file = kind ? &sm_fp : &ss_fp;
    unsigned healthy_opens = api_opens;
    if (!strcmp(mode, "media-unlatched")) {
        media_unready = media_failure = 1;
        for (int i = 0; i < 3; i++) assert(read_save(kind) == FR_DISK_ERR);
        assert(!f_error(target_file) && api_opens == healthy_opens && media_restarts == 3);
        media_failure = 0;
        assert(!read_save(kind) && media_restarts == 4 && api_opens == healthy_opens);
    } else if (!strcmp(mode, "write-replaced")) {
        if (kind) mems_prepare(1);
        else { assert(!load_savefile((char *)key_a)); memset((void *)SSAVE_ADDR, 0xab, 65536); }
        u8 *pending = malloc(sizeof test_shared); assert(pending);
        fail_writes = 1; assert(write_save(kind) == FR_DISK_ERR);
        memcpy(pending, test_shared, sizeof test_shared);
        fail_writes = 0; alteration = "replaced"; before_open = alter_before_open;
        assert(write_save(kind) == FR_INT_ERR && !before_open);
        unsigned writes = api_writes, opens = api_opens;
        for (int i = 0; i < 3; i++) assert(write_save(kind) == FR_INT_ERR);
        assert(api_writes == writes && api_opens == opens);
        assert(!memcmp(pending, test_shared, sizeof test_shared)); free(pending);
        goto finish;
    } else if (!strcmp(mode, "append")) {
        assert(!kind);
        raw_write(&ss_fp, 32, key_c, 16); memcpy(slot_before + 32, key_c, 16);
        fail_append = 1;
        assert(load_savefile((char *)key_d) == FR_DISK_ERR && !ss_index && payload_writes == 2);
        fail_append = 0;
        slot_size += fs.csize * 512;
        slot_before = realloc(slot_before, slot_size); assert(slot_before);
        memset(slot_before + 4 * 65536, 0, slot_size - 4 * 65536);
        assert(!load_savefile((char *)key_a) && ss_index == 1);
        assert_archives(slot_before, slot_size, mems_before);
        assert(!load_savefile((char *)key_d) && ss_index == 4);
        slot_size = 5 * 65536; slot_before = realloc(slot_before, slot_size); assert(slot_before);
        memset(slot_before + 4 * 65536, 0, 65536); memcpy(slot_before + 64, key_d, 16);
    } else if (!strcmp(mode, "write") || !strcmp(mode, "close-failure")) {
        if (kind) mems_prepare(1);
        else { assert(!load_savefile((char *)key_a)); memset((void *)SSAVE_ADDR, 0xab, 65536); }
        assert(api_opens == healthy_opens && !media_restarts);
        fail_writes = 1;
        assert(write_save(kind) == FR_DISK_ERR && f_error(target_file) == FR_DISK_ERR);
        if (!strcmp(mode, "close-failure")) {
            unsigned opens = api_opens;
            for (int i = 0; i < 3; i++) {
                assert(write_save(kind) == FR_DISK_ERR && target_file->obj.fs);
                assert(api_opens == opens && f_error(target_file) == FR_DISK_ERR);
            }
        }
        fail_writes = 0;
        assert(!write_save(kind) && api_opens == healthy_opens + 1);
        if (kind) mems_expected(7, 1); else memset(slot_before + 65536, 0xab, 65536);
    } else {
        disk_read_fault = 1;
        assert(read_save(kind) == FR_DISK_ERR && f_error(target_file) == FR_DISK_ERR);
        unsigned opens = api_opens;
        if (!strcmp(mode, "header-io")) {
            fault_target = !!kind; fault = "read-disk-error"; armed = 1;
            assert(read_save(kind) == FR_DISK_ERR && hits == 1);
            assert(target_file->obj.fs && f_error(target_file) == FR_DISK_ERR);
            armed = 0;
        } else if (!strcmp(mode, "open-failure")) {
            fault_target = !!kind; fault = "open-error"; fault_value = FR_DENIED;
            for (int i = 0; i < 3; i++) {
                armed = 1; fault_seen = 0;
                assert(read_save(kind) == FR_DENIED && !target_file->obj.fs);
                assert(api_opens == opens + i + 1);
            }
            armed = 0;
        } else if (!strcmp(mode, "structural")) {
            /* Model FatFs's structural-error latch, not a recoverable I/O abort. */
            target_file->err = FR_INT_ERR;
            unsigned checks = media_checks;
            for (int i = 0; i < 3; i++) assert(read_save(kind) == FR_INT_ERR);
            assert(api_opens == opens && media_checks == checks);
            goto finish;
        } else if (!strcmp(mode, "media")) {
            media_unready = media_failure = 1;
            for (int i = 0; i < 3; i++) assert(read_save(kind) == FR_DISK_ERR);
            assert(media_restarts == 3 && api_opens == opens && target_file->obj.fs);
            media_failure = 0;
        } else if (strcmp(mode, "read")) {
            alteration = mode; before_open = alter_before_open;
            int error = !strcmp(mode, "missing") ? FR_NO_FILE : FR_INT_ERR;
            assert(read_save(kind) == error && !before_open);
            if (!strcmp(mode, "missing")) {
                assert(!target_file->obj.fs);
                for (int i = 0; i < 2; i++) assert(read_save(kind) == FR_NO_FILE);
                assert(!f_rename(old_name, kind ? SMEMS_FILE : SSAVE_FILE));
            } else {
                unsigned reads = api_reads, writes = api_writes, checked = media_checks;
                opens = api_opens;
                for (int i = 0; i < 3; i++) assert(read_save(kind) == FR_INT_ERR);
                assert(api_reads == reads && api_writes == writes && api_opens == opens && media_checks == checked);
                goto finish;
            }
        }
        assert(!read_save(kind) && !f_error(target_file));
        if (kind) {
            u8 *out = kind == 1 ? (u8 *)SMEMS_BUF : (u8 *)SMEMS_HDR + 8192;
            assert(!memcmp(out, mems_before + 100 * 1024, kind == 1 ? 65536 : 1024));
        } else assert(!memcmp((void *)SSAVE_ADDR, slot_before + 3 * 65536, 65536));
    }
    assert_archives(slot_before, slot_size, mems_before);
finish:
    if (altered[0]) compare_altered(0);
    /* Closing is cleanup only; no test code reopens or clears an aborted FIL. */
    if (strcmp(mode, "generation")) {
        if (ss_fp.obj.fs) assert(!f_close(&ss_fp));
        if (sm_fp.obj.fs) assert(!f_close(&sm_fp));
    }
    assert(!f_mount(NULL, "0:", 0) && !fclose(disk));
    free(slot_before); free(mems_before);
    printf("save reopen fs=%s mode=%s kind=%d opens=%u media-restarts=%u\n",
           argv[2], mode, kind, api_opens, media_restarts);
    return 0;
}
