/* Adapted from paps-g 2d8ba306. Inject I/O failures below the public FatFs provider. */
#define main sync_fixture_main
#define disk_read ordinary_disk_read
#define disk_write ordinary_disk_write
#define disk_ioctl ordinary_disk_ioctl
#include "fatfs_seek.c"
#undef disk_ioctl
#undef disk_write
#undef disk_read
#undef main

static int stage, partial_write;
static unsigned io[3][3], fail_at[3], fault_type;

static int should_fail(unsigned type)
{
    if (!stage) return 0;
    unsigned n = ++io[stage][type];
    int fail = type == fault_type && fail_at[stage] &&
               (stage == 2 ? n >= fail_at[stage] : n == fail_at[stage]);
    if (fail) {
        failures++;
        printf("fault stage=%d type=%u ordinal=%u\n", stage, type, n);
    }
    return fail;
}

DRESULT disk_read(BYTE drive, BYTE *out, LBA_t sector, UINT count)
{
    if (should_fail(0)) return RES_ERROR;
    return ordinary_disk_read(drive, out, sector, count);
}

DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
    if (should_fail(1)) {
        if (partial_write) {
            assert(!fseeko(disk, (off_t)sector * 512, SEEK_SET));
            assert(fwrite(data, 1, 256, disk) == 256 && !fflush(disk));
        }
        return RES_ERROR;
    }
    return ordinary_disk_write(drive, data, sector, count);
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void *out)
{
    if (command == CTRL_SYNC && should_fail(2)) return RES_ERROR;
    return ordinary_disk_ioctl(drive, command, out);
}

static DWORD le32(const BYTE *p)
{
    return (DWORD)p[0] | (DWORD)p[1] << 8 | (DWORD)p[2] << 16 | (DWORD)p[3] << 24;
}

typedef struct {
    BYTE *fat;
    BYTE *used;
} ALLOCATION;

static DWORD fat_value(const FATFS *fs, const BYTE *fat, DWORD cluster)
{
    DWORD at = fs->fs_type == FS_FAT12 ? cluster + cluster / 2 :
               fs->fs_type == FS_FAT16 ? cluster * 2 : cluster * 4;
    if (fs->fs_type == FS_FAT12) {
        DWORD value = (DWORD)fat[at] | (DWORD)fat[at + 1] << 8;
        return (cluster & 1 ? value >> 4 : value) & 0xfff;
    }
    if (fs->fs_type == FS_FAT16)
        return (DWORD)fat[at] | (DWORD)fat[at + 1] << 8;
    return le32(fat + at) & (fs->fs_type == FS_FAT32 ? 0x0fffffff : 0xffffffff);
}

static ALLOCATION allocation(const FATFS *fs)
{
    ALLOCATION result = {
        malloc((size_t)fs->fsize * 512),
        calloc(fs->n_fatent, 1)
    };
    assert(result.fat && result.used && !fflush(disk));
    assert(ordinary_disk_read(0, result.fat, fs->fatbase, fs->fsize) == RES_OK);
    if (fs->n_fats == 2) {
        BYTE *mirror = malloc((size_t)fs->fsize * 512);
        assert(mirror);
        assert(ordinary_disk_read(0, mirror, fs->fatbase + fs->fsize, fs->fsize) == RES_OK);
        assert(!memcmp(mirror, result.fat, (size_t)fs->fsize * 512));
        free(mirror);
    }
    BYTE *bitmap = NULL;
    if (fs->fs_type == FS_EXFAT) {
        UINT sectors = (fs->n_fatent - 2 + 4095) / 4096;
        bitmap = malloc((size_t)sectors * 512);
        assert(bitmap);
        assert(ordinary_disk_read(0, bitmap, fs->bitbase, sectors) == RES_OK);
    }
    for (DWORD cluster = 2; cluster < fs->n_fatent; ++cluster)
        result.used[cluster] = bitmap
            ? !!(bitmap[(cluster - 2) / 8] & (1u << ((cluster - 2) % 8)))
            : !!fat_value(fs, result.fat, cluster);
    free(bitmap);
    return result;
}

static void release_allocation(ALLOCATION *allocation)
{
    free(allocation->fat);
    free(allocation->used);
}

static void uniform_file(const char *name, unsigned bytes)
{
    FIL file;
    BYTE block[512];
    UINT count;
    memset(block, 0xc9, sizeof block);
    assert(!f_open(&file, name, FA_CREATE_NEW | FA_WRITE));
    while (bytes) {
        unsigned amount = bytes < sizeof block ? bytes : sizeof block;
        assert(!f_write(&file, block, amount, &count) && count == amount);
        bytes -= amount;
    }
    assert(!f_close(&file));
}

static void verify_uniform_file(const char *name, unsigned bytes)
{
    BYTE *expected = malloc(bytes);
    assert(expected);
    memset(expected, 0xc9, bytes);
    verify_file(name, expected, bytes);
    free(expected);
}

static unsigned fill_until(FATFS *fs, const char *name, DWORD target)
{
    ALLOCATION before = allocation(fs);
    DWORD cluster;
    for (cluster = 2; cluster < fs->n_fatent && before.used[cluster]; ++cluster) {}
    release_allocation(&before);
    assert(cluster <= target);
    unsigned bytes = (target - cluster) * fs->csize * 512;
    uniform_file(name, bytes);
    return bytes;
}

int main(int argc, char **argv)
{
    /* image format layout operation fault operation-ordinal close-ordinal partial fats */
    assert(argc == 10);
    setvbuf(stdout, NULL, _IONBF, 0);
    const char *format = argv[2], *layout = argv[3];
    int seek = !strcmp(argv[4], "seek"), exfat = !strcmp(format, "exfat");
    int fat32 = !strcmp(format, "fat32"), fat12 = !strcmp(format, "fat12");
    int initial = !strcmp(layout, "initial") || !strcmp(layout, "initial-split");
    int prefix = !strcmp(layout, "prefix");
    fault_type = !strcmp(argv[5], "read") ? 0 : !strcmp(argv[5], "write") ? 1 : 2;
    fail_at[1] = atoi(argv[6]);
    fail_at[2] = atoi(argv[7]);
    partial_write = atoi(argv[8]);
    unsigned fats = atoi(argv[9]);

    disk = fopen(argv[1], "r+b");
    assert(disk && !fseeko(disk, 0, SEEK_END));
    sectors = ftello(disk) / 512;
    BYTE work[32768];
    MKFS_PARM options = {
        .fmt = (exfat ? FM_EXFAT : fat32 ? FM_FAT32 : FM_FAT) | FM_SFD,
        .n_fat = fats,
        .au_size = exfat ? 65536 : 512
    };
    assert(!f_mkfs("0:", &options, work, sizeof work));
    FATFS fs;
    assert(!f_mount(&fs, "0:", 1));
    assert(fs.fs_type == (exfat ? FS_EXFAT : fat32 ? FS_FAT32 : fat12 ? FS_FAT12 : FS_FAT16));

    unsigned cluster_bytes = fs.csize * 512;
    unsigned old_size = initial ? 0 : cluster_bytes * 2;
    unsigned pad_size = 0, block_size = 0;
    if (!strcmp(layout, "initial-split") || !strcmp(layout, "split-tail")) {
        assert(fat12);
        pad_size = fill_until(&fs, "/PAD.BIN", !strcmp(layout, "split-tail") ? 340 : 341);
    }
    uniform_file("/SAVE.BIN", old_size);
    if (prefix) uniform_file("/HOLE.BIN", cluster_bytes);
    if (!strcmp(layout, "fragmented") || prefix || !strcmp(layout, "split-new")) {
        DWORD target = fat12 ? 341 : fat32 ? 256 : exfat ? 10 : 512;
        block_size = fill_until(&fs, "/BLOCK.BIN", target);
    } else if (!strcmp(layout, "split-tail")) {
        block_size = cluster_bytes;
        uniform_file("/BLOCK.BIN", block_size);
    }
    if (prefix) assert(!f_unlink("/HOLE.BIN"));

    FIL file;
    UINT count;
    assert(!f_open(&file, "/SAVE.BIN", FA_READ | FA_WRITE));
    assert(!f_lseek(&file, old_size));
    ALLOCATION before = allocation(&fs);
    unsigned requested = 2 * cluster_bytes;
    BYTE *payload = malloc(requested);
    assert(payload);
    memset(payload, 0xc9, requested);

    stage = 1;
    FRESULT result = seek
        ? f_lseek(&file, old_size + requested)
        : f_write(&file, payload, requested, &count);
    if (seek) count = 0;
    unsigned accepted = count;
    FSIZE_t expected_size = f_size(&file);
    assert(result == FR_OK || result == FR_DISK_ERR);
    assert(expected_size >= old_size && expected_size <= old_size + requested);
    if (!seek) assert(expected_size == old_size + accepted);
    printf("operation result=%d accepted=%u size=%llu phase=%u reads=%u writes=%u syncs=%u\n",
           result, accepted, (unsigned long long)expected_size, fs.dir_pending.phase,
           io[1][0], io[1][1], io[1][2]);

    if (fail_at[1]) assert(failures == 1 && result == FR_DISK_ERR);
    stage = 2;
    FRESULT closed = f_close(&file);
    unsigned close_counts[3];
    memcpy(close_counts, io[2], sizeof close_counts);
    if (closed != FR_OK) {
        assert(file.obj.fs == &fs);
        assert(f_close(&file) != FR_OK && file.obj.fs == &fs);
        if (fs.dir_pending.phase) {
            assert(f_mount(NULL, "0:", 0) != FR_OK && file.obj.fs == &fs);
            FIL other;
            assert(f_open(&other, "/NEW.BIN", FA_CREATE_NEW | FA_WRITE) != FR_OK);
        }
    }
    stage = 0;
    if (closed != FR_OK) assert(!f_close(&file));
    assert(!fs.dir_pending.phase);
    DWORD free_before_remount;
    FATFS *mounted;
    assert(!f_getfree("0:", &free_before_remount, &mounted) && mounted == &fs);
    assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));

    assert(!f_open(&file, "/SAVE.BIN", FA_READ));
    assert(f_size(&file) == expected_size);
    unsigned checked = seek ? old_size : (unsigned)expected_size;
    BYTE block[512];
    for (unsigned at = 0; at < checked; at += count) {
        assert(!f_read(&file, block, sizeof block, &count) && count);
        for (unsigned i = 0; i < count; ++i) assert(block[i] == 0xc9);
    }

    ALLOCATION after = allocation(&fs);
    BYTE *owned = calloc(fs.n_fatent, 1);
    assert(owned);
    DWORD cluster = file.obj.sclust;
    unsigned chain = 0;
    unsigned contiguous = (unsigned)((expected_size + cluster_bytes - 1) / cluster_bytes);
    while (cluster >= 2 && cluster < fs.n_fatent) {
        assert(!owned[cluster] && after.used[cluster]);
        owned[cluster] = 1;
        chain++;
        if (exfat && file.obj.stat == 2) {
            assert(contiguous);
            if (chain == contiguous) break;
            cluster++;
        } else {
            cluster = fat_value(&fs, after.fat, cluster);
        }
    }
    assert(chain >= contiguous && !f_close(&file));
    if (pad_size) verify_uniform_file("/PAD.BIN", pad_size);
    if (block_size) verify_uniform_file("/BLOCK.BIN", block_size);

    unsigned added = 0, removed = 0, orphaned = 0, free_raw = 0;
    for (cluster = 2; cluster < fs.n_fatent; ++cluster) {
        free_raw += !after.used[cluster];
        if (after.used[cluster] && !before.used[cluster]) {
            added++;
            orphaned += !owned[cluster];
        }
        removed += before.used[cluster] && !after.used[cluster];
    }
    DWORD free_report;
    assert(!f_getfree("0:", &free_report, &mounted));
    printf("RESULT fs=%s layout=%s op=%s result=%d accepted=%u added=%u removed=%u "
           "orphaned=%u chain=%u free=%u/%u close_reads=%u close_writes=%u close_syncs=%u\n",
           format, layout, argv[4], result, accepted, added, removed, orphaned, chain,
           free_report, free_raw, close_counts[0], close_counts[1], close_counts[2]);
    assert(!removed && !orphaned && free_report == free_raw && free_before_remount == free_raw);

    release_allocation(&before);
    release_allocation(&after);
    free(owned);
    free(payload);
    assert(!f_mount(NULL, "0:", 0) && !fclose(disk));
    return 0;
}
