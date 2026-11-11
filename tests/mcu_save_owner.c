/* Adapted from paps-g b7720d00. Actual primary archives on synthetic media. */
/* Actual primary owners, exact whole-archive oracles and faulting FatFs I/O. */
#define MCU_RUNTIME_TEST_ENTRY existing_runtime_main
#include "mcu_save_runtime.c"
#undef MCU_RUNTIME_TEST_ENTRY
#include <limits.h>

static void identity(const void *expected)
{
    const u8 zero[16] = {0};
    assert(!memcmp((void *)SAVE_ID_ADDR, expected ? expected : zero, 16));
}

static void arm(const char *name, unsigned device)
{
    clear_io(); fault = name; fault_target = device;
    fault_value = FR_DISK_ERR; fault_nth = 1; armed = 1;
}

static void ownership(void)
{
    assert(!load_savefile((char *)key_a));
    assert(!load_smems(100)); assert(!load_smems(0x8000 | 104));
    assert(ss_index == 1 && sm_index0 == 104 && sm_index1 == 100);
    identity(key_a);
    clear_io();
}

static void check_identity(void)
{
    identity(NULL);
    assert(!load_savefile((char *)key_a)); identity(key_a);
    assert(!load_savefile((char *)key_b)); identity(key_b);
    /* Fixed-width, non-NUL-terminated identity may alias the published field. */
    assert(!load_savefile((char *)SAVE_ID_ADDR)); identity(key_b);
    assert(!load_savefile("SHORT"));
    char short_key[16] = "SHORT"; identity(short_key);
    memcpy(slot_before + 32, short_key, 16);
    memset(slot_before + 2 * 65536, 0, 65536);
    arm("read-partial-error", 0);
    assert(load_savefile((char *)key_a) == FR_DISK_ERR); identity(NULL);
    armed = 0; assert(!load_savefile((char *)key_a)); identity(key_a);
    assert(load_savefile(NULL) == FR_INVALID_PARAMETER); identity(NULL);
    assert(!load_savefile((char *)key_b)); identity(key_b);
    assert(load_savefile("") == FR_INVALID_PARAMETER); identity(NULL);
    /* Startup must revoke a prior published identity, even on failure. */
    require_lock = 0;
    assert(!f_close(&ss_fp) && !f_close(&sm_fp));
    memset((void *)SAVE_ID_ADDR, 0xa5, 16);
    arm("open-error", 0);
    assert(open_savefile() != 0); identity(NULL);
    armed = 0; assert(!open_savefile()); require_lock = 1;
}

static void invalid_requests(unsigned device, u8 *buffer)
{
    ownership();
    u32 end = device ? 0x800000 : slot_size;
    struct { unsigned device; u32 offset; void *buffer; int length; } cases[] = {
        {2, 65536, buffer, 1}, {UINT_MAX, 65536, buffer, -1},
        {device, 0, buffer, 0}, {device, 1, NULL, 0},
        {device, 65536, NULL, 1}, {device, 65536, NULL, -1},
        {device, 65536, buffer, 65537}, {device, 65536, buffer, -65537},
        {device, 65536, buffer, INT_MIN}, {device, 65536, buffer, INT_MAX},
        {device, end, buffer, 1}, {device, end, buffer, -1},
        {device, end - 1, buffer, 2}, {device, end - 1, buffer, -2},
        {device, UINT_MAX, buffer, 1}, {device, UINT_MAX, buffer, -1},
        {0, 0, buffer, -1}, {0, 65535, buffer, -2}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
        assert(save_archive_io(cases[i].device, cases[i].offset,
                               cases[i].buffer, cases[i].length) == FR_INVALID_PARAMETER);
        assert(!api_reads && !api_writes && !api_seeks && !api_syncs);
        assert(ss_index == 1 && sm_index0 == 104 && sm_index1 == 100);
        identity(key_a);
    }
    FIL *fp = device ? &sm_fp : &ss_fp;
    FSIZE_t original_size = f_size(fp);
    const FSIZE_t bad_sizes[] = {0, 65535, device ? 0x7fffff : UINT64_C(0x100000000)};
    for (unsigned i = 0; i < sizeof bad_sizes / sizeof *bad_sizes; i++) {
        fp->obj.objsize = bad_sizes[i];
        assert(save_archive_io(device, 0, buffer, 65536) == FR_INT_ERR);
        assert(!api_reads && !api_writes && !api_seeks && !api_syncs);
    }
    fp->obj.objsize = original_size;
}

static void hide_before_open(FIL *fp, const TCHAR *name, BYTE mode)
{
    assert(fp == (fault_target ? &sm_fp : &ss_fp) && mode == (FA_READ | FA_WRITE));
    assert(lock_depth == 1 && !fp->obj.fs);
    before_open = NULL;
    assert(!f_rename(name, "/SAROO/HIDDEN.BIN"));
}

static void reopen(unsigned device, u8 *buffer, const char *mode)
{
    ownership();
    FIL *fp = (device ? &sm_fp : &ss_fp);
    unsigned opens = api_opens;
    memset(buffer, 0xa5, 65536);
    if (!strcmp(mode, "media")) {
        media_unready = media_failure = 1;
        assert(save_archive_io(device, 65536, buffer, 65536) == FR_DISK_ERR);
        assert(!api_reads && !f_error(fp) && api_opens == opens);
        for (unsigned i = 0; i < 65536; i++) assert(buffer[i] == 0xa5);
        media_failure = 0;
    } else {
        fault_target = device;
        disk_read_fault = 1;
        assert(save_archive_io(device, 65536, buffer, 65536) == FR_DISK_ERR);
        assert(f_error(fp) == FR_DISK_ERR && !disk_read_fault);
        armed = 0;
        if (!strcmp(mode, "missing")) {
            before_open = hide_before_open;
            assert(save_archive_io(device, 65536, buffer, 65536) == FR_NO_FILE);
            assert(!before_open && !fp->obj.fs && (device ? mems_reopen.pending : slot_reopen.pending));
            FIL probe;
            assert(f_open(&probe, filename(device), FA_READ) == FR_NO_FILE);
            assert(!f_rename("/SAROO/HIDDEN.BIN", filename(device)));
            opens++;
        }
        opens++;
    }
    assert(!save_archive_io(device, 65536, buffer, 65536));
    assert(api_opens == opens);
    assert(!memcmp(buffer, (device ? mems_before : slot_before) + 65536, 65536));
    assert(ss_index == 1 && sm_index0 == 104 && sm_index1 == 100); identity(key_a);
}

static void root_reload_failure(void)
{
    ownership();
    arm("read-partial-error", 1);
    assert(save_archive_io(1, 0, (void *)SMEMS_HDR, 8192) == FR_DISK_ERR);
    armed = 0;
    unsigned writes = api_writes;
    assert(flush_smems(1) != 0 && load_smems(100) != 0);
    assert(api_writes == writes);
    assert(!save_archive_io(1, 0, (void *)SMEMS_HDR, 8192));
    assert(!flush_smems(1));
}

static void transfer(unsigned device, u8 *buffer, const char *mode)
{
    ownership();
    u8 *expected = device ? mems_before : slot_before;
    u32 end = device ? 0x800000 : slot_size;
    unsigned opens = api_opens;
    if (!strcmp(mode, "read")) {
        for (unsigned i = 0; i < 3; i++) {
            u32 offset = i == 0 ? 0 : i == 1 ? 65536 : end - 65536;
            assert(!save_archive_io(device, offset, buffer, 65536));
            assert(!memcmp(buffer, expected + offset, 65536));
            assert(ss_index == 1 && sm_index0 == 104 && sm_index1 == 100);
            identity(key_a);
        }
        assert(api_reads == 3 && api_seeks == 3 && !api_writes && !api_syncs);
    } else {
        u32 offset = end - 65536;
        memset(buffer, 0xad, 65536);
        int faulting = strcmp(mode, "write") != 0;
        if (faulting) arm(mode, device);
        int result = save_archive_io(device, offset, buffer, -65536);
        if (faulting) {
            assert(result == expected_fault() && hits == 1);
            armed = 0;
            /* A changed error prefix must revoke even root-only writeback. */
        } else assert(!result);
        if (device) {
            assert(ss_index == 1); identity(key_a);
            unsigned writes = api_writes;
            for (int flags = 1; flags <= 7; flags++) assert(flush_smems(flags) != 0);
            assert(load_smems(100) != 0 && load_smems(0x8064) != 0);
            assert(api_writes == writes);
        } else { assert(!ss_index); identity(NULL); check_no_flush(); }
        assert(!save_archive_io(device, offset, buffer, -65536));
        memset(expected + offset, 0xad, 65536);
        arm("sync-error", device);
        assert(save_archive_io(device, 0, NULL, 0) == FR_DISK_ERR && hits == 1);
        armed = 0; assert(!save_archive_io(device, 0, NULL, 0));
        if (device) {
            /* An ordinary read cannot grant legacy root/window ownership. */
            assert(!save_archive_io(1, 0, buffer, 8192));
            assert(flush_smems(1) != 0);
            arm("read-partial-error", 1);
            assert(save_archive_io(1, 0, (void *)SMEMS_HDR, 8192) == FR_DISK_ERR);
            armed = 0; assert(flush_smems(1) != 0);
            assert(!save_archive_io(1, 0, (void *)SMEMS_HDR, 8192));
            assert(!sm_index0 && !sm_index1 && !flush_smems(1));
            assert(flush_smems(2) != 0 && flush_smems(4) != 0);
            assert(!load_smems(100) && !load_smems(0x8064));
        } else { assert(!load_savefile((char *)key_b)); identity(key_b); }
    }
    assert(api_opens == opens);
}

#ifndef SAVE_OWNER_TEST_ENTRY
#define SAVE_OWNER_TEST_ENTRY main
#endif
int SAVE_OWNER_TEST_ENTRY(int argc, char **argv)
{
    assert(argc == 5); unsigned device = atoi(argv[4]); assert(device < 2);
    disk = fopen(argv[1], "r+b"); assert(disk); fault = "none";
    BYTE work[32768];
    MKFS_PARM options = {.fmt = (!strcmp(argv[2], "fat32") ? FM_FAT32 : FM_EXFAT) | FM_SFD,
                         .n_fat = 1, .au_size = !strcmp(argv[2], "fat32") ? 512 : 4096};
    assert(!f_mkfs("0:", &options, work, sizeof work));
    FATFS fs; assert(!f_mount(&fs, "0:", 1));
    memset(test_save_sysinfo, 0xa5, sizeof test_save_sysinfo);
    fixture_trailer = 1;
    initialize(); require_lock = 1; identity(NULL);
    u8 *buffer = malloc(65536); assert(buffer);
    if (!strcmp(argv[3], "identity")) check_identity();
    else if (!strcmp(argv[3], "invalid")) invalid_requests(device, buffer);
    else if (!strcmp(argv[3], "root-read-error")) root_reload_failure();
    else if (!strcmp(argv[3], "reopen") || !strcmp(argv[3], "missing") || !strcmp(argv[3], "media"))
        reopen(device, buffer, argv[3]);
    else transfer(device, buffer, argv[3]);
    assert_archives(slot_before, slot_size, mems_before);
    assert(!lock_depth);
    require_lock = 0;
    assert(!f_close(&ss_fp) && !f_close(&sm_fp) && !f_mount(NULL, "0:", 0));
    assert(!fclose(disk)); free(buffer); free(slot_before); free(mems_before);
    puts("PASS complete primary archives, bounded I/O and ownership");
    return 0;
}
