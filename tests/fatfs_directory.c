/* Adapted from paps-g 18ed08a6 and b483124c. Use the same file-backed
 * sector interface as the FatFs seek/sync tests, with ordinal failures. */
#define main seek_fixture_main
#define disk_read base_read
#define disk_write base_write
#define disk_ioctl base_ioctl
#define disk_status base_status
#define disk_initialize base_initialize
#include "fatfs_seek.c"
#undef main
#undef disk_read
#undef disk_write
#undef disk_ioctl
#undef disk_status
#undef disk_initialize

static const char *fault;
static unsigned ordinal, armed, op_reads, op_writes, op_syncs, unavailable;
static LBA_t clear_sector;
static BYTE payload[4096];

DSTATUS disk_status(BYTE drive)
{ return unavailable ? STA_NOINIT : base_status(drive); }

DSTATUS disk_initialize(BYTE drive)
{ return unavailable ? STA_NOINIT : base_initialize(drive); }

static int inject(const char *kind, unsigned number)
{
    if (strcmp(fault, kind) || number != ordinal) return 0;
    failures++;
    return 1;
}

DRESULT disk_read(BYTE drive, BYTE *out, LBA_t sector, UINT count)
{
    assert(!unavailable);
    if (armed && inject("read", ++op_reads)) return RES_ERROR;
    return base_read(drive, out, sector, count);
}

DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
    assert(!unavailable);
    if (armed && inject("write", ++op_writes)) {
        /* A failed sector write can leave a changed prefix. */
        assert(!fseeko(disk, (off_t)sector*512, SEEK_SET));
        assert(fwrite(data, 1, 256, disk) == 256 && !fflush(disk));
        return RES_ERROR;
    }
    if (armed && sector == clear_sector && inject("clear", 0)) return RES_ERROR;
    return base_write(drive, data, sector, count);
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void *out)
{
    assert(!unavailable);
    if (armed && command == CTRL_SYNC && inject("sync", ++op_syncs)) return RES_ERROR;
    return base_ioctl(drive, command, out);
}

static unsigned entries(const char *path)
{
    DIR dir; FILINFO info; unsigned count = 0;
    assert(!f_opendir(&dir, path));
    do {
        assert(!f_readdir(&dir, &info));
        if (info.fname[0]) ++count;
    } while (info.fname[0]);
    assert(!f_closedir(&dir));
    return count;
}

static DWORD fat_word(const FATFS *fs, DWORD cluster)
{
    BYTE data[1024];
    DWORD at = fs->fs_type == FS_FAT12 ? cluster + cluster/2 :
               fs->fs_type == FS_FAT16 ? cluster*2 : cluster*4;
    assert(disk_read(0, data, fs->fatbase + at/512, 1 + (at%512 == 511)) == RES_OK);
    const BYTE *p = data + at%512;
    if (fs->fs_type == FS_FAT12) return (p[0] | p[1]<<8) >> ((cluster&1)*4) & 0xfff;
    if (fs->fs_type == FS_FAT16) return p[0] | p[1]<<8;
    return little_word(p) & (fs->fs_type == FS_FAT32 ? 0x0fffffff : 0xffffffff);
}

static LBA_t first_sector(const FATFS *fs, DWORD cluster)
{
    assert(cluster >= 2 && cluster < fs->n_fatent);
    return fs->database + (LBA_t)(cluster - 2)*fs->csize;
}

static LBA_t root_slot(const FATFS *fs, DWORD offset)
{
    DWORD cluster = fs->dirbase;
    for (; offset >= fs->csize*512; offset -= fs->csize*512)
        cluster = fat_word(fs, cluster);
    return first_sector(fs, cluster) + offset/512;
}

static unsigned exfat_directory(const FATFS *fs, const char *path, unsigned *active)
{
    DIR dir;
    BYTE sector[512];
    unsigned clusters = 0;
    assert(!f_opendir(&dir, path));
    DWORD cluster = dir.obj.sclust, bytes = fs->csize*512;
    assert(dir.obj.objsize && dir.obj.objsize % bytes == 0);
    *active = 0;
    do {
        LBA_t start = first_sector(fs, cluster);
        assert(disk_read(0, sector, fs->bitbase + (cluster - 2)/4096, 1) == RES_OK);
        assert(sector[((cluster - 2)%4096)/8] & (1u << ((cluster - 2)%8)));
        for (unsigned i = 0; i < fs->csize; i++) {
            assert(disk_read(0, sector, start + i, 1) == RES_OK);
            for (unsigned n = 0; n < 512; n += 32) *active += !!(sector[n] & 0x80);
        }
        assert(++clusters < 16);
        if (dir.obj.stat == 2) {
            if (clusters*bytes == dir.obj.objsize) break;
            ++cluster;
        } else {
            cluster = fat_word(fs, cluster);
            assert(cluster >= 2);
        }
    } while (cluster < fs->n_fatent);
    assert(clusters*bytes == dir.obj.objsize);
    assert(!f_closedir(&dir));
    return clusters;
}

static FRESULT operation(const char *layout, const char *old, const char *path)
{
    if (!strcmp(layout, "mkdir")) return f_mkdir(path);
    if (!strcmp(layout, "rename")) return f_rename(old, path);
    FIL file = {0};
    FRESULT result = f_open(&file, path, FA_WRITE | FA_CREATE_NEW);
    if (!result) result = f_close(&file);
    /* A failed close retains ownership; retry it outside the injected fault. */
    if (result && file.obj.fs) {
        armed = 0;
        assert(!f_close(&file));
    }
    return result;
}

static void exfat_case(const char *layout, unsigned csize)
{
    FATFS fs, *mounted;
    FIL file, held;
    FILINFO info;
    UINT n;
    DWORD free_before, free_after;
    char name[32], path[262] = "/TEST/", old[257] = "/";
    memset(path + 6, 'Q', 255); path[261] = 0;
    memset(old + 1, 'O', 255); old[256] = 0;
    unsigned bytes = csize*512;
    int growth = !strcmp(layout, "growth"), mkdir = !strcmp(layout, "mkdir");
    int rename = !strcmp(layout, "rename");
    format_volume(FM_EXFAT, 1, bytes);
    assert(!f_mount(&fs, "0:", 1) && fs.fs_type == FS_EXFAT && fs.csize == csize);
    unsigned root_fillers = growth ? (16*csize - 4)/3 : 0;
    for (unsigned i = 0; i < root_fillers; i++) {
        snprintf(name, sizeof name, "/R%07u", i);
        create_file(name, payload, i ? 0 : bytes);
    }
    if (growth) {
        assert(!f_open(&file, "/R0000000", FA_WRITE | FA_OPEN_APPEND));
        assert(!f_write(&file, payload + bytes, bytes, &n) && n == bytes && !f_close(&file));
    }
    assert(!f_mkdir("/TEST"));
    if (growth) {
        DIR dir;
        assert(!f_opendir(&dir, "/TEST"));
        DWORD at = dir.obj.c_ofs;
        assert(at % bytes == bytes - 32);
        assert(root_slot(&fs, at + 32) != root_slot(&fs, at) + 1);
        assert(!f_closedir(&dir));
    }
    create_file("/BLOCK.BIN", payload, bytes);
    unsigned keepers = growth ? (16*csize - 1)/3 : 5;
    for (unsigned i = 0; i < keepers; i++) {
        snprintf(name, sizeof name, "/TEST/K%07u", i);
        create_file(name, payload, 0);
    }
    if (!growth) {
        for (unsigned i = 0; i < 8; i++) {
            snprintf(name, sizeof name, "/TEST/H%07u", i);
            create_file(name, payload, 0);
        }
        create_file("/TEST/TAIL.BIN", payload, bytes);
        for (unsigned i = 0; i < 8; i++) {
            snprintf(name, sizeof name, "/TEST/H%07u", i);
            assert(!f_unlink(name));
        }
    }
    if (rename) create_file(old, payload, bytes*2);
    unsigned before = entries("/TEST"), active;
    unsigned clusters_before = exfat_directory(&fs, "/TEST", &active);
    assert(active == 3*before);
    assert(!f_open(&held, "/BLOCK.BIN", FA_READ));
    assert(!f_getfree("0:", &free_before, &mounted) && mounted == &fs);
    armed = 1;
    FRESULT result = operation(layout, old, path);
    armed = 0;
    printf("operation I/O: %u %u %u, result %u, faults %u\n",
           op_reads, op_writes, op_syncs, result, failures);
    if (!strcmp(fault, "none")) assert(!result && !failures);
    else assert(result == FR_DISK_ERR && failures == 1);

    BYTE byte;
    if (!strcmp(layout, "unavailable")) {
        FATFS replacement;
        unavailable = 1;
        assert(f_mount(NULL, "0:", 0) == FR_NOT_READY);
        assert(f_mount(&replacement, "0:", 0) == FR_NOT_READY);
        assert(f_read(&held, &byte, 1, &n) == FR_INVALID_OBJECT && !n);
        unavailable = 0;
    }
    assert(!f_read(&held, &byte, 1, &n) && n == 1 && byte == payload[0] && !f_close(&held));
    assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
    result = f_stat(path, &info);
    assert(result == FR_OK || result == FR_NO_FILE);
    unsigned found = result == FR_OK;
    assert(entries("/TEST") == before + found);
    unsigned clusters_after = exfat_directory(&fs, "/TEST", &active);
    assert(active == 3*before + 19*found);
    assert(!f_getfree("0:", &free_after, &mounted));
    assert(free_before - free_after == clusters_after - clusters_before + (mkdir && found));
    if (!growth) assert(clusters_after == clusters_before);
    if (rename) {
        assert(f_stat(old, &info) == (found ? FR_NO_FILE : FR_OK));
        verify_file(found ? path : old, payload, bytes*2);
    }
    if (!found) assert(!operation(layout, old, path));
    assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
    assert(entries("/TEST") == before + 1);
    assert(!f_stat(path, &info));
    if (mkdir) assert((info.fattrib & AM_DIR) && !entries(path));
    else if (rename) {
        assert(f_stat(old, &info) == FR_NO_FILE);
        verify_file(path, payload, bytes*2);
    } else assert(!info.fsize);
    for (unsigned i = 0; i < keepers; i++) {
        snprintf(name, sizeof name, "/TEST/K%07u", i);
        assert(!f_stat(name, &info) && !info.fsize);
    }
    if (growth) verify_file("/R0000000", payload, bytes*2);
    else verify_file("/TEST/TAIL.BIN", payload, bytes);
    verify_file("/BLOCK.BIN", payload, bytes);
    assert(!f_mount(NULL, "0:", 0));
}

static void classic_case(const char *layout)
{
    unsigned type = !strcmp(layout, "fat12") ? FS_FAT12 : !strcmp(layout, "fat16") ? FS_FAT16 : FS_FAT32;
    FATFS fs;
    FIL file;
    DIR dir;
    format_volume(type == FS_FAT32 ? FM_FAT32 : FM_FAT, 2, 512);
    assert(!f_mount(&fs, "0:", 1) && fs.fs_type == type && fs.csize == 1);
    assert(!f_mkdir("/TEST") && !f_opendir(&dir, "/TEST"));
    DWORD start = dir.obj.sclust;
    assert(!f_closedir(&dir));
    create_file("/BLOCK.BIN", payload, 512);
    assert(!f_open(&file, "/BLOCK.BIN", FA_READ));
    assert(file.obj.sclust == start + 1);
    DWORD free_cluster = file.obj.sclust + 1;
    assert(!f_close(&file) && !fat_word(&fs, free_cluster));
    for (unsigned i = 0; i < 14; i++) {
        char name[24];
        snprintf(name, sizeof name, "/TEST/K%07u", i);
        create_file(name, payload, 0);
    }
    clear_sector = first_sector(&fs, free_cluster);
    armed = 1;
    FRESULT result = operation(layout, "", "/TEST/NEW");
    armed = 0;
    printf("operation I/O: %u %u %u, result %u, faults %u\n",
           op_reads, op_writes, op_syncs, result, failures);
    if (!strcmp(fault, "clear")) {
        assert(result == FR_DISK_ERR && failures == 1);
        assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
        assert(!fat_word(&fs, free_cluster) && fat_word(&fs, start) != free_cluster);
        assert(entries("/TEST") == 14 && !operation(layout, "", "/TEST/NEW"));
    } else assert(!result && !failures);
    assert(entries("/TEST") == 15 && fat_word(&fs, start) == free_cluster);
    verify_file("/BLOCK.BIN", payload, 512);
    assert(!f_mount(NULL, "0:", 0));
}

/* Adapted from paps-g 72ac7fab. Keep a live child beyond the first cluster
 * and verify both its contents and the directory allocation after remount. */
static void unlink_case(const char *layout, unsigned csize)
{
    int exfat = strcmp(layout, "unlink-fat32") != 0 &&
                strcmp(layout, "unlink-fat32-empty") != 0;
    int contiguous = !strcmp(layout, "unlink-contiguous");
    int empty = !strcmp(layout, "unlink-empty") || !strcmp(layout, "unlink-fat32-empty");
    int first_live = !strcmp(layout, "unlink-first");
    unsigned bytes = csize*512;
    unsigned early = exfat ? (csize*16 + 2)/3 : csize*16 - 2;
    unsigned total = early + 2;
    char path[32];
    FATFS fs, *mounted;
    DWORD before, after;
    FILINFO info;

    format_volume(exfat ? FM_EXFAT : FM_FAT32, 1, bytes);
    assert(!f_mount(&fs, "0:", 1) && fs.fs_type == (exfat ? FS_EXFAT : FS_FAT32));
    assert(fs.csize == csize && !f_mkdir("/TEST"));
    if (!contiguous) create_file("/BLOCK.BIN", payload, bytes);
    for (unsigned i = 0; i < total; i++) {
        snprintf(path, sizeof path, "/TEST/K%07u", i);
        create_file(path, payload, i == total-1 ? 512 : 0);
    }
    if (contiguous) create_file("/BLOCK.BIN", payload, bytes);
    if (!first_live)
        for (unsigned i = 0; i < (empty ? total : early); i++) {
            snprintf(path, sizeof path, "/TEST/K%07u", i);
            assert(!f_unlink(path));
        }
    snprintf(path, sizeof path, "/TEST/K%07u", total-1);
    assert(entries("/TEST") == (empty ? 0 : first_live ? total : 2));

    DIR dir;
    assert(!f_opendir(&dir, "/TEST"));
    DWORD start = dir.obj.sclust;
    if (exfat) assert(dir.obj.stat == (contiguous ? 2 : 0) && dir.obj.objsize == 2*bytes);
    DWORD next = contiguous ? start+1 : fat_word(&fs, start);
    assert(next >= 2 && next < fs.n_fatent && (next == start+1) == contiguous);
    assert(!f_closedir(&dir));

    assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
    if (!empty) verify_file(path, payload, 512);
    assert(!f_getfree("0:", &before, &mounted) && mounted == &fs);
    unsigned before_writes = writes;
    FRESULT result = f_unlink("/TEST");
    unsigned deletion_writes = writes-before_writes;
    assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
    FRESULT child = f_stat(path, &info);
    assert(!f_getfree("0:", &after, &mounted));
    printf("operation I/O: 0 %u 0, unlink %u, child %u, free %lu -> %lu\n",
           deletion_writes, result, child, (unsigned long)before, (unsigned long)after);
    if (empty) {
        assert(result == FR_OK && child == FR_NO_PATH && after == before+2);
    } else {
        assert(result == FR_DENIED && child == FR_OK && after == before && !deletion_writes);
        if (exfat) {
            unsigned active;
            assert(exfat_directory(&fs, "/TEST", &active) == 2);
        } else {
            assert(fat_word(&fs, start) == next);
        }
        verify_file(path, payload, 512);
    }
    verify_file("/BLOCK.BIN", payload, bytes);
    assert(!f_mount(NULL, "0:", 0));
}

int main(int argc, char **argv)
{
    assert(argc == 6);
    setvbuf(stdout, NULL, _IONBF, 0);
    disk = fopen(argv[1], "r+b");
    assert(disk && !fseeko(disk, 0, SEEK_END));
    sectors = ftello(disk)/512;
    fault = argv[4]; ordinal = (unsigned)atoi(argv[5]);
    for (unsigned i = 0; i < sizeof payload; i++) payload[i] = pattern(i + 31);
    if (!strncmp(argv[2], "unlink-", 7)) unlink_case(argv[2], (unsigned)atoi(argv[3]));
    else if (!strncmp(argv[2], "fat", 3)) classic_case(argv[2]);
    else exfat_case(argv[2], (unsigned)atoi(argv[3]));
    assert(!fclose(disk));
    return 0;
}
