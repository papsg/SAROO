/* Adapted from paps-g SSR-164 / a817842e. Actual FatFs; only sectors and
 * volume grants are supplied by the host. */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(DWORD) == 4, "exFAT checksum width");
static FILE *disk;
static LBA_t capacity, base, trace[2048];
static UINT physical = 512;
static unsigned reads, writes, metadata_reads, fail_read;
static pthread_mutex_t grant;
int ff_mutex_create(int volume) { assert(!volume); return !pthread_mutex_init(&grant, 0); }
void ff_mutex_delete(int volume) { assert(!volume); assert(!pthread_mutex_destroy(&grant)); }
int ff_mutex_take(int volume) { assert(!volume); return !pthread_mutex_lock(&grant); }
void ff_mutex_give(int volume) { assert(!volume); assert(!pthread_mutex_unlock(&grant)); }
DSTATUS disk_initialize(BYTE drive) { return drive ? STA_NOINIT : 0; }
DSTATUS disk_status(BYTE drive) { return disk_initialize(drive); }
DRESULT disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
    assert(!drive && count == 1 && reads < sizeof trace / sizeof *trace);
    trace[reads++] = sector;
    if (!disk || reads == fail_read) return RES_ERROR;
    assert(sector < capacity);
    if (sector >= base && sector - base >= 24) metadata_reads++;
    assert(!fseeko(disk, (off_t)sector * physical, SEEK_SET));
    return fread(data, physical, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
    assert(!drive && count && sector < capacity && count <= capacity - sector);
    writes++;
    assert(!fseeko(disk, (off_t)sector * physical, SEEK_SET));
    return fwrite(data, physical, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *out)
{
    assert(!drive);
    if (command == CTRL_SYNC) return fflush(disk) ? RES_ERROR : RES_OK;
    if (command == GET_SECTOR_COUNT) { *(LBA_t *)out = capacity; return RES_OK; }
    if (command == GET_SECTOR_SIZE) { *(WORD *)out = physical; return RES_OK; }
    if (command == GET_BLOCK_SIZE) { *(DWORD *)out = 1; return RES_OK; }
    return RES_PARERR;
}
#include "ff.c"
static void keep_payload(void)
{
    FIL file = {0}; BYTE data[512]; UINT count;
    assert(!f_open(&file, "/KEEP.BIN", FA_READ));
    assert(f_size(&file) == sizeof data);
    assert(!f_read(&file, data, sizeof data, &count) && count == sizeof data);
    for (unsigned i = 0; i < sizeof data; i++) assert(data[i] == 0xc9);
    assert(!f_close(&file));
}
int main(int argc, char **argv)
{
    assert(argc >= 3); setvbuf(stdout, 0, _IONBF, 0);
    const char *mode = argv[2]; FATFS fs = {0};
    if (!strcmp(mode, "bounds")) {
#ifndef SSR164_BASELINE
#if FF_MAX_SS != FF_MIN_SS
        fs.ssize = 512;
#endif
        fs.winsect = (LBA_t)-1;
        assert(check_exfat_boot(&fs, (LBA_t)-1, 0) == 3 && !reads);
        assert(check_exfat_boot(&fs, (LBA_t)-1 - 10, 12) == 3 && !reads);
        assert(check_exfat_boot(&fs, 0, 11) == 3 && !reads);
        assert(check_exfat_boot(&fs, (LBA_t)-1 - 11, 12) == 4 && reads == 1);
        assert(trace[0] == (LBA_t)-1 - 11);
        printf("{\"bounds\":true,\"reads\":%u,\"lba_bits\":%zu}\n", reads, sizeof(LBA_t) * 8);
        return 0;
#else
        assert(0 && "bounds helper unavailable in baseline");
#endif
    }
    physical = (UINT)strtoul(argv[argc - 1], 0, 10);
    assert(physical == 512 || physical == 1024 || physical == 2048 || physical == 4096);
    disk = fopen(argv[1], "r+b"); assert(disk && !fseeko(disk, 0, SEEK_END));
    off_t bytes = ftello(disk); assert(bytes > 0 && bytes % physical == 0); capacity = bytes / physical;
    if (!strcmp(mode, "format")) {
#if FF_USE_MKFS && !FF_FS_READONLY
        BYTE work[32768], data[512]; UINT count; FIL file = {0}; memset(data, 0xc9, sizeof data);
        MKFS_PARM options = {.fmt = FM_EXFAT | FM_SFD, .n_fat = 1, .au_size = physical};
        assert(!f_mkfs("0:", &options, work, sizeof work) && !f_mount(&fs, "0:", 1));
        assert(!f_open(&file, "/KEEP.BIN", FA_CREATE_NEW | FA_WRITE));
        assert(!f_write(&file, data, sizeof data, &count) && count == sizeof data && !f_close(&file));
        assert(!f_mount(0, "0:", 0) && !fclose(disk));
        printf("{\"formatted\":true,\"sector_bytes\":%u}\n", physical); return 0;
#else
        assert(0);
#endif
    }
    assert(argc == 8);
    base = (LBA_t)strtoull(argv[3], 0, 10); LBA_t span = (LBA_t)strtoull(argv[4], 0, 10);
    unsigned expected = (unsigned)strtoul(argv[5], 0, 10); fail_read = (unsigned)strtoul(argv[6], 0, 10);
    if (!strcmp(mode, "words-main") || !strcmp(mode, "words-backup")) {
        int backup = !strcmp(mode, "words-backup");
        for (unsigned i = 0; i < physical / 4; i++) {
            off_t offset = ((off_t)base + (backup ? 23 : 11)) * physical + i * 4;
            BYTE original[4], corrupt[4];
            assert(!fseeko(disk, offset, SEEK_SET) && fread(original, 1, 4, disk) == 4);
            memcpy(corrupt, original, 4); corrupt[0] ^= 1;
            assert(!fseeko(disk, offset, SEEK_SET) && fwrite(corrupt, 1, 4, disk) == 4 && !fflush(disk));
            reads = metadata_reads = 0;
            FRESULT result = f_mount(&fs, "0:", 1);
            assert(result == (backup ? FR_OK : FR_NO_FILESYSTEM));
            if (backup) keep_payload();
            else assert(!fs.fs_type && !metadata_reads);
            assert(!writes && !f_mount(0, "0:", 0));
            assert(!fseeko(disk, offset, SEEK_SET) && fwrite(original, 1, 4, disk) == 4 && !fflush(disk));
        }
        assert(!fclose(disk));
        printf("{\"words_checked\":%u,\"backup\":%s,\"filesystem_writes\":%u}\n", physical / 4, backup ? "true" : "false", writes);
        return 0;
    }
    assert(!strcmp(mode, "mount") || !strcmp(mode, "fault") || !strcmp(mode, "gate"));
#if FF_MAX_SS != FF_MIN_SS
    fs.ssize = physical;
#endif
    if (!strcmp(mode, "gate")) {
#ifndef SSR164_BASELINE
        fs.winsect = (LBA_t)-1;
        unsigned result = check_exfat_boot(&fs, base, span);
        assert(!writes && !fclose(disk));
        printf("{\"gate\":%u,\"expected\":%u,\"reads\":%u,\"metadata_reads\":%u,\"published\":%u}\n", result, expected, reads, metadata_reads, fs.fs_type);
        return result == expected && !fs.fs_type && !metadata_reads ? 0 : 1;
#else
        assert(0 && "boot gate unavailable in baseline");
#endif
    }
    FRESULT mounted = f_mount(&fs, "0:", 1); unsigned published = fs.fs_type;
    unsigned initial_reads = reads, initial_metadata_reads = metadata_reads;
    LBA_t volume_base = fs.volbase;
    if (mounted == FR_OK) keep_payload();
    if (mounted == expected && !strcmp(mode, "fault")) {
        assert(mounted != FR_OK && !published && !writes); fail_read = 0;
        assert(!f_mount(0, "0:", 0) && !f_mount(&fs, "0:", 1)); keep_payload();
    }
    assert(!writes && !f_mount(0, "0:", 0) && !fclose(disk));
    printf("{\"mount\":%u,\"expected\":%u,\"published\":%u,\"base\":%llu,\"span\":%llu,\"volume_base\":%llu,\"reads\":%u,\"metadata_reads\":%u,\"writes\":%u,\"trace\":[",
           (unsigned)mounted, expected, published, (unsigned long long)base, (unsigned long long)span,
           (unsigned long long)volume_base, initial_reads, initial_metadata_reads, writes);
    for (unsigned i = 0; i < initial_reads; i++) printf("%s%llu", i ? "," : "", (unsigned long long)trace[i]);
    printf("]}\n");
    return mounted == expected && published == (mounted == FR_OK ? FS_EXFAT : 0) ? 0 : 1;
}
