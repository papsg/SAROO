/* Adapted from paps-g 49a1c53d. Real FatFs with host block-device/grant seams. */
#define main seek_fixture_main
#define disk_read ordinary_disk_read
#define ff_mutex_take ordinary_mutex_take
#include "fatfs_seek.c"
#undef ff_mutex_take
#undef disk_read
#undef main

#define ITEMS 12
static char names[ITEMS][768];
static FILINFO expected[2][ITEMS];
static unsigned workspace_calls, read_calls, failed_read;
static int enumerating, grant_failure;

int ff_mutex_take(int volume)
{
    if (grant_failure) { grant_failure = 0; return 0; }
    return ordinary_mutex_take(volume);
}

DRESULT disk_read(BYTE drive, BYTE *out, LBA_t sector, UINT count)
{
    if (enumerating && ++read_calls == failed_read) return RES_ERROR;
    return ordinary_disk_read(drive, out, sector, count);
}

/* Injected into the copied f_readdir after its workspace initialization only. */
void enumeration_workspace_probe(FATFS *fs)
{
    assert((void *)fs->lfnbuf == (void *)fs->dirbuf ? EXPECT_SHARED : !EXPECT_SHARED);
    assert(!((uintptr_t)fs->lfnbuf % _Alignof(WCHAR)));
    if (fs->fs_type == FS_EXFAT) fs->lfnbuf = NULL;
    else fs->dirbuf = NULL;
    workspace_calls++;
}

static void unlocked(void)
{
    assert(!pthread_mutex_trylock(&mutex));
    assert(!pthread_mutex_unlock(&mutex));
}

static void make_name(unsigned slot, const char *unit, unsigned count)
{
    size_t width = strlen(unit);
    assert(width * count < sizeof names[slot]);
    for (unsigned i = 0; i < count; i++) memcpy(names[slot] + width * i, unit, width);
    names[slot][width * count] = 0;
}

static void path_for(char *path, unsigned owner, unsigned item)
{
    int count = snprintf(path, 800, "/%c/%s", 'A' + owner, names[item]);
    assert(count > 0 && count < 800);
}

static FRESULT next(DIR *dir, FILINFO *result)
{
    struct { unsigned before; FILINFO info; unsigned after; } guarded;
    memset(&guarded, 0, sizeof guarded);
    guarded.before = 0x184fb093; guarded.after = 0xb7d4316e;
    enumerating = 1;
    FRESULT status = f_readdir(dir, &guarded.info);
    enumerating = 0;
    assert(guarded.before == 0x184fb093 && guarded.after == 0xb7d4316e);
    unlocked();
    if (!status) *result = guarded.info;
    return status;
}

static void same_info(const FILINFO *a, const FILINFO *b)
{
    assert(!strcmp(a->fname, b->fname) && !strcmp(a->altname, b->altname));
    assert(a->fsize == b->fsize && a->fattrib == b->fattrib);
    assert(a->ftime == b->ftime && a->fdate == b->fdate);
}

static void rewind_dir(DIR *dir)
{
    unsigned before = workspace_calls;
    assert(!f_readdir(dir, NULL));
    assert(workspace_calls == before);
    unlocked();
}

static unsigned scan(DIR *dir, unsigned owner)
{
    read_calls = 0;
    for (unsigned i = 0; i < ITEMS; i++) {
        FILINFO info;
        assert(!next(dir, &info)); same_info(&info, &expected[owner][i]);
    }
    for (unsigned i = 0; i < 2; i++) {
        FILINFO info;
        assert(!next(dir, &info) && !info.fname[0]);
    }
    return read_calls;
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    int kind = atoi(argv[2]);
    unsigned cluster = (unsigned)atoi(argv[3]);
    disk = fopen(argv[1], "r+b");
    assert(disk && !fseeko(disk, 0, SEEK_END));
    sectors = ftello(disk) / 512;
    BYTE work[32768];
    MKFS_PARM options = {.fmt = (kind == 4 ? FM_EXFAT : kind == 3 ? FM_FAT32 : FM_FAT) | FM_SFD,
                         .n_fat = 1, .au_size = cluster * 512};
    assert(!f_mkfs("0:", &options, work, sizeof work));
    FATFS fs;
    assert(!f_mount(&fs, "0:", 1) && fs.fs_type == kind);
    strcpy(names[0], "SHORT.TXT"); strcpy(names[1], "MixedCase.txt");
    make_name(2, "C", 13); make_name(3, "D", 15); make_name(4, "E", 16);
    make_name(5, "F", 233); make_name(6, "G", 255);
    make_name(7, "\xe4\xb8\xad", 85); /* Exactly 255 UTF-8 output bytes. */
    make_name(8, "\xf0\x9f\x8c\x9f", 63); /* UTF-16 surrogate pairs. */
    make_name(9, "\xe6\x96\x87", 255); /* 255 UTF-16 units, 765 UTF-8 bytes. */
    assert(strlen(names[9]) == 765);
    strcpy(names[10], "caf\xc3\xa9.txt"); strcpy(names[11], "last.bin");
    assert(!f_mkdir("/A") && !f_mkdir("/B"));
    for (unsigned i = 0; i < ITEMS; i++) for (unsigned owner = 0; owner < 2; owner++) {
        char path[800]; FIL file; BYTE payload[128]; UINT count;
        unsigned size = i + 1 + owner * 32;
        path_for(path, owner, i); memset(payload, i + owner, size);
        assert(!f_open(&file, path, FA_CREATE_NEW | FA_WRITE));
        assert(!f_write(&file, payload, size, &count) && count == size);
        assert(!f_close(&file));
        assert(!f_stat(path, &expected[owner][i]));
        assert(expected[owner][i].fsize == size);
        if (strlen(names[i]) < sizeof expected[owner][i].fname) {
            assert(!strcmp(expected[owner][i].fname, names[i]));
        } else {
            assert(!expected[owner][i].altname[0]);
            if (kind == FS_EXFAT) assert(!strcmp(expected[owner][i].fname, "?"));
            else assert(strchr(expected[owner][i].fname, '~') &&
                        strlen(expected[owner][i].fname) <= 12);
        }
    }
    assert(!f_mount(NULL, "0:", 0) && !f_mount(&fs, "0:", 1));
    DIR dirs[2];
    assert(!f_opendir(&dirs[0], "/A") && !f_opendir(&dirs[1], "/B"));
    for (unsigned i = 0; i < ITEMS; i++) for (unsigned owner = 0; owner < 2; owner++) {
        FILINFO info;
        assert(!next(&dirs[owner], &info)); same_info(&info, &expected[owner][i]);
        /* An intervening pathname operation must initialize its own buffers. */
        char path[800]; FILINFO stat;
        path_for(path, owner ^ 1, i); assert(!f_stat(path, &stat));
        same_info(&stat, &expected[owner ^ 1][i]);
    }
    for (unsigned owner = 0; owner < 2; owner++) {
        rewind_dir(&dirs[owner]); scan(&dirs[owner], owner);
        assert(!f_closedir(&dirs[owner]));
    }
    assert(!f_opendir(&dirs[0], "/A"));
    unsigned saved_calls = workspace_calls;
    grant_failure = 1;
    FILINFO info;
    assert(next(&dirs[0], &info) == FR_TIMEOUT && workspace_calls == saved_calls);
    unsigned count_reads = scan(&dirs[0], 0);
    assert(count_reads && !f_closedir(&dirs[0]));
    for (unsigned failure = 1; failure <= count_reads; failure++) {
        assert(!f_opendir(&dirs[0], "/A"));
        failed_read = failure; read_calls = 0;
        FRESULT status = FR_OK;
        unsigned i = 0;
        while (i <= ITEMS && (status = next(&dirs[0], &info)) == FR_OK) {
            if (!info.fname[0]) { assert(i == ITEMS); break; }
            same_info(&info, &expected[0][i++]);
        }
        assert(status == FR_DISK_ERR && read_calls == failure);
        failed_read = 0;
        rewind_dir(&dirs[0]); scan(&dirs[0], 0);
        assert(!f_closedir(&dirs[0]));
    }
    /* Corrupt one on-media name/checksum and exercise real parser refusal/fallback. */
    FIL file;
    assert(!f_open(&file, "/A/MixedCase.txt", FA_READ));
    LBA_t sector = file.dir_sect;
    unsigned offset = file.dir_ptr - fs.win;
    if (kind == FS_EXFAT) {
        /* The second file's primary is in the directory's first cluster. */
        assert(file.obj.c_ofs < fs.csize * 512);
        sector = fs.database + (file.obj.c_scl - 2) * fs.csize + file.obj.c_ofs / 512;
        offset = file.obj.c_ofs % 512 + 2;
    } else {
        assert(offset >= 32); offset = offset - 32 + 13;
    }
    assert(!f_close(&file) && !fs.wflag);
    BYTE original[512], corrupted[512];
    assert(disk_read(0, original, sector, 1) == RES_OK);
    memcpy(corrupted, original, sizeof original); corrupted[offset] ^= 1;
    assert(disk_write(0, corrupted, sector, 1) == RES_OK);
    fs.winsect = (LBA_t)-1;
    assert(!f_opendir(&dirs[0], "/A"));
    assert(!next(&dirs[0], &info)); same_info(&info, &expected[0][0]);
    FRESULT corrupt_result = next(&dirs[0], &info);
    if (kind == FS_EXFAT) assert(corrupt_result == FR_INT_ERR);
    else {
        assert(!corrupt_result && expected[0][1].altname[0]);
        assert(!strcmp(info.fname, expected[0][1].altname));
        assert(info.fsize == expected[0][1].fsize);
    }
    assert(disk_write(0, original, sector, 1) == RES_OK);
    fs.winsect = (LBA_t)-1;
    rewind_dir(&dirs[0]); scan(&dirs[0], 0);
    assert(!f_closedir(&dirs[0]));
    for (unsigned i = 0; i < ITEMS; i++) for (unsigned owner = 0; owner < 2; owner++) {
        char path[800]; BYTE payload[128]; UINT count;
        unsigned size = i + 1 + owner * 32;
        path_for(path, owner, i);
        assert(!f_open(&file, path, FA_READ));
        assert(!f_read(&file, payload, sizeof payload, &count) && count == size);
        for (unsigned at = 0; at < size; at++) assert(payload[at] == i + owner);
        assert(!f_close(&file));
    }
    assert(!f_mount(NULL, "0:", 0) && !fclose(disk));
    assert(workspace_calls > 0);
    printf("enumeration fs=%d cluster=%u owners=2 files=24 failed_reads=%u workspace_checks=%u\n",
           kind, cluster, count_reads, workspace_calls);
    return 0;
}
