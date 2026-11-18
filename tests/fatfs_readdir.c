/* Actual FatFs enumeration; the file-backed block device alone injects faults. */
#define main unused_sync_main
#define disk_read ordinary_disk_read
#include "fatfs_seek.c"
#undef disk_read
#undef main

static unsigned read_number, readdir_fail_read, refused_reads;
static int armed;
DRESULT disk_read(BYTE drive, BYTE *out, LBA_t sector, UINT count)
{
    if (armed && ++read_number == readdir_fail_read) {
        refused_reads++;
        return RES_ERROR;
    }
    return ordinary_disk_read(drive, out, sector, count);
}
static unsigned bad_names;
static void observation(const char *kind, unsigned fault, FRESULT result, DIR *dir, FILINFO *info)
{
    printf("{\"event\":\"%s\",\"fault\":%u,\"result\":%u,\"name\":\"%s\","
           "\"index\":%lu,\"cluster\":%lu,\"sector\":%llu,\"size\":%llu,"
           "\"date\":%u,\"time\":%u,\"attributes\":%u,\"reads\":%u}\n",
           kind, fault, (unsigned)result, info->fname, (unsigned long)dir->dptr,
           (unsigned long)dir->clust, (unsigned long long)dir->sect,
           (unsigned long long)info->fsize, info->fdate, info->ftime, info->fattrib, read_number);
}
static unsigned scan(FATFS *fs, unsigned fault)
{
    DIR dir; FILINFO info;
    assert(!f_opendir(&dir, "/A"));
    fs->winsect = (LBA_t)-1;
    read_number = refused_reads = 0; readdir_fail_read = fault; armed = 1;
    unsigned entries = 0;
    for (;;) {
        memset(&info, 0, sizeof info);
        strcpy(info.fname, "STALE.BIN");
        FRESULT result = f_readdir(&dir, &info);
        observation("read", fault, result, &dir, &info);
        if (result) {
            assert(fault && refused_reads == 1 && result == FR_DISK_ERR);
            if (info.fname[0]) bad_names++;
            break;
        }
        if (!info.fname[0]) { assert(!fault); break; }
        char expected[32]; snprintf(expected, sizeof expected, "FILE%03u.BIN", entries);
        assert(entries < 40 && !strcmp(info.fname, expected));
        assert(info.fsize == entries + 1 && (info.fattrib & AM_ARC));
        entries++;
    }
    armed = 0;
    if (!fault) assert(entries == 40 && !refused_reads);
    assert(!f_closedir(&dir));
    return read_number;
}
int main(int argc, char **argv)
{
    assert(argc == 4); setvbuf(stdout, NULL, _IONBF, 0);
    int fat32 = !strcmp(argv[2], "fat32");
    unsigned cluster_bytes = (unsigned)atoi(argv[3]) * 512;
    assert(sizeof(FSIZE_t) * 8 == EXPECT_FSIZE_BITS);
    disk = fopen(argv[1], "r+b"); assert(disk && !fseeko(disk, 0, SEEK_END));
    sectors = ftello(disk) / 512;
    BYTE work[32768];
    MKFS_PARM options = {.fmt=(fat32 ? FM_FAT32 : FM_EXFAT)|FM_SFD,
                         .n_fat=1, .au_size=cluster_bytes};
    assert(!f_mkfs("0:", &options, work, sizeof work));
    FATFS fs; assert(!f_mount(&fs, "0:", 1));
    assert(fs.fs_type == (fat32 ? FS_FAT32 : FS_EXFAT));
    assert(fs.csize * 512 == cluster_bytes && !f_mkdir("/A"));
    for (unsigned i = 0; i < 40; i++) {
        char path[32]; FIL file; UINT count; BYTE bytes[40];
        snprintf(path, sizeof path, "/A/FILE%03u.BIN", i);
        memset(bytes, (int)(0x80 + i), i + 1);
        assert(!f_open(&file, path, FA_CREATE_NEW | FA_WRITE));
        assert(!f_write(&file, bytes, i + 1, &count) && count == i + 1);
        assert(!f_close(&file));
    }
    assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
    unsigned reads = scan(&fs, 0);
    assert(reads > 0);
    for (unsigned ordinal = 1; ordinal <= reads; ordinal++) scan(&fs, ordinal);
    assert(scan(&fs, 0) == reads);
    DIR dir; FILINFO info;
    memset(&dir, 0, sizeof dir); memset(&info, 0, sizeof info);
    strcpy(info.fname, "STALE.BIN");
    FRESULT result = f_readdir(&dir, &info);
    assert(result == FR_INVALID_OBJECT);
    observation("invalid", 0, result, &dir, &info);
    if (info.fname[0]) bad_names++;
    assert(f_readdir(&dir, NULL) == FR_INVALID_OBJECT);
    assert(!f_opendir(&dir, "/A") && !f_readdir(&dir, &info));
    assert(!strcmp(info.fname, "FILE000.BIN"));
    assert(!f_readdir(&dir, NULL) && !f_readdir(&dir, &info));
    assert(!strcmp(info.fname, "FILE000.BIN") && !f_closedir(&dir));
    for (unsigned i = 0; i < 40; i++) {
        char path[32]; FIL file; UINT count; BYTE bytes[40];
        snprintf(path, sizeof path, "/A/FILE%03u.BIN", i);
        assert(!f_open(&file, path, FA_READ));
        assert(f_size(&file) == i + 1);
        assert(!f_read(&file, bytes, sizeof bytes, &count) && count == i + 1);
        for (unsigned j = 0; j < count; j++) assert(bytes[j] == 0x80 + i);
        assert(!f_close(&file));
    }
    assert(!f_mount(NULL, "0:", 0) && !fclose(disk));
    printf("{\"event\":\"summary\",\"faults\":%u,\"bad_names\":%u,"
           "\"entries\":40,\"content_checked\":40,\"fsize_bits\":%u}\n",
           reads, bad_names, (unsigned)(sizeof(FSIZE_t) * 8));
    assert(bad_names == 0);
    return 0;
}
