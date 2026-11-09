/* Extend paps-g's 169cb468 extent investigation on unmodified public FatFs.
 * A full volume causes a real short write, without the private write-abort fix. */
#define MCU_RUNTIME_TEST_ENTRY runtime_test_main
#include "mcu_save_runtime.c"
#undef MCU_RUNTIME_TEST_ENTRY

int main(int argc, char **argv)
{
    assert(argc == 6);
    disk = fopen(argv[1], "r+b"); assert(disk);
    fault = "none";
    int fat32 = !strcmp(argv[2], "fat32");
    MKFS_PARM options = {.fmt = (fat32 ? FM_FAT32 : FM_EXFAT) | FM_SFD,
                         .n_fat = 1, .au_size = fat32 ? 512 : 4096};
    u8 work[32768];
    assert(f_mkfs("0:", &options, work, sizeof work) == FR_OK);
    FATFS fs; assert(f_mount(&fs, "0:", 1) == FR_OK);
    initialize();
    raw_write(&ss_fp, 32, key_c, 16); memcpy(slot_before + 32, key_c, 16);

    /* Reserve all but one cluster in a separate file. The next slot payload
     * must encounter actual FAT/exFAT exhaustion after one complete cluster. */
    FIL filler; DWORD free_clusters; FATFS *mounted;
    assert(f_open(&filler, "/SAROO/FILLER.BIN", FA_READ | FA_WRITE | FA_CREATE_NEW) == FR_OK);
    assert(f_getfree("0:", &free_clusters, &mounted) == FR_OK && mounted == &fs && free_clusters > 2);
    FSIZE_t reserve = (FSIZE_t)(free_clusters - 1) * fs.csize * 512;
    assert(f_lseek(&filler, reserve) == FR_OK && f_tell(&filler) == reserve);
    assert(f_sync(&filler) == FR_OK);
    assert(f_getfree("0:", &free_clusters, &mounted) == FR_OK && free_clusters == 1);
    clear_io();
    assert(load_savefile((char *)key_d) == FR_INT_ERR && ss_index == 0);
    unsigned partial_size = 4 * 65536 + fs.csize * 512;
    assert(f_size(&ss_fp) == partial_size && ss_fp.err == 0 && api_writes == 1);
    assert(f_close(&filler) == FR_OK);
    /* Explicit close/reopen preserves the real short-write prefix. It does
     * not model automatic recovery of a latched physical I/O error. */
    assert(f_close(&ss_fp) == FR_OK);
    assert(f_open(&ss_fp, SSAVE_FILE, FA_READ | FA_WRITE) == FR_OK);
    assert(f_size(&ss_fp) == partial_size);
    slot_before = realloc(slot_before, partial_size); assert(slot_before);
    memset(slot_before + slot_size, 0, partial_size - slot_size); slot_size = partial_size;
    assert_archives(slot_before, slot_size, mems_before);

    const char *mode = argv[3];
    if (strcmp(mode, "runtime")) {
        assert(f_close(&ss_fp) == FR_OK && f_close(&sm_fp) == FR_OK);
        assert(f_mount(NULL, "0:", 0) == FR_OK && f_mount(&fs, "0:", 1) == FR_OK);
        reset_service(); unsigned before = disk_writes;
        assert(open_savefile() == FR_OK && disk_writes == before);
    }
    clear_io();
    if (!strcmp(mode, "dangling")) {
        raw_write(&ss_fp, 64, key_d, 16); memcpy(slot_before + 64, key_d, 16);
        clear_io(); unsigned before = disk_writes;
        assert(load_savefile((char *)key_a) == FR_INT_ERR && !ss_index && !api_writes);
        assert(disk_writes == before);
    } else {
        unsigned before = disk_writes;
        assert(load_savefile((char *)key_a) == FR_OK && ss_index == 1);
        assert(!memcmp((void *)SSAVE_ADDR, slot_before + 65536, 65536));
        assert(!api_writes && disk_writes == before);
        if (!strcmp(mode, "complete")) {
            assert(f_unlink("/SAROO/FILLER.BIN") == FR_OK);
            assert(load_savefile((char *)key_d) == FR_OK && ss_index == 4);
            slot_before = realloc(slot_before, 5 * 65536); assert(slot_before);
            memset(slot_before + 4 * 65536, 0, 65536);
            memcpy(slot_before + 64, key_d, 16); slot_size = 5 * 65536;
            assert(f_size(&ss_fp) == slot_size);
        }
    }
    assert_archives(slot_before, slot_size, mems_before);
    assert(f_close(&ss_fp) == FR_OK && f_close(&sm_fp) == FR_OK);
    assert(f_mount(NULL, "0:", 0) == FR_OK && fclose(disk) == 0);
    free(slot_before); free(mems_before);
    printf("extent fs=%s mode=%s short-prefix=%u archive-size=%lu\n", argv[2], mode,
           partial_size - 4 * 65536, (unsigned long)slot_size);
    return 0;
}
