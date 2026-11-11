/* Adapted from paps-g aa74eb3e: actual public startup and unmodified FatFs. */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
static int lock_depth, require_lock;
static void fs_lock(void) { assert(!lock_depth); lock_depth = 1; }
static void fs_unlock(void) { assert(lock_depth == 1); lock_depth = 0; }
static void memcpy32(void *to, const void *from, int size) { memcpy(to, from, size); }
static u8 test_scratch[65536], test_shared[0x23000];
#define TEST_SCRATCH_ADDR ((uintptr_t)test_scratch)
#define SAVINFO_ADDR ((uintptr_t)test_shared)
static u32 test_save_sysinfo[1024] __attribute__((unused));
#define TEST_SAVE_SYSINFO_ADDR ((uintptr_t)test_save_sysinfo)
#define printk(...) ((void)0)
static void (*before_open)(FIL *, const TCHAR *, BYTE);
static unsigned media_checks, media_restarts;
static int media_unready, media_failure;

static __attribute__((unused)) int disk_resume(void)
{
    assert(lock_depth == 1);
    media_checks++;
    if (!media_unready)
        return 0;
    media_restarts++;
    if (media_failure)
        return -1;
    media_unready = 0;
    return 0;
}

static FRESULT checked_open(FIL *, const TCHAR *, BYTE);
static FRESULT checked_read(FIL *, void *, UINT, UINT *);
static FRESULT checked_write(FIL *, const void *, UINT, UINT *);
static FRESULT checked_seek(FIL *, FSIZE_t);
static FRESULT checked_sync(FIL *);
static FRESULT checked_close(FIL *);
#define f_open checked_open
#define f_read checked_read
#define f_write checked_write
#define f_lseek checked_seek
#define f_sync checked_sync
#define f_close checked_close
#include "save-startup.inc"
#undef f_open
#undef f_read
#undef f_write
#undef f_lseek
#undef f_sync
#undef f_close

static int started, error_led, retv;
#define LEDEV_FILE_ERROR 3
static void led_event(int event) { assert(event == LEDEV_FILE_ERROR); error_led++; }
static void startup_gate(void)
{
#include "save-startup-gate.inc"
    started++;
}

static FILE *disk;
static LBA_t disk_sectors = 131072;
static int disk_read_fault;
static unsigned disk_writes, api_reads, api_writes, api_seeks, api_syncs, api_opens;
static unsigned read_bytes, write_bytes;
static const char *fault;
static int fault_target, fault_value, armed, hits;
static int cleanup_calls;
static int fault_nth = 1, fault_seen;
static FSIZE_t write_offsets[1024];
static UINT write_sizes[1024];
static pthread_mutex_t fs_mutex;
int ff_mutex_create(int vol) { assert(!vol); return pthread_mutex_init(&fs_mutex, NULL) == 0; }
void ff_mutex_delete(int vol) { assert(!vol); assert(pthread_mutex_destroy(&fs_mutex) == 0); }
int ff_mutex_take(int vol) { assert(!vol); return pthread_mutex_lock(&fs_mutex) == 0; }
void ff_mutex_give(int vol) { assert(!vol); assert(pthread_mutex_unlock(&fs_mutex) == 0); }
DSTATUS disk_initialize(BYTE drive) { return drive ? STA_NOINIT : 0; }
DSTATUS disk_status(BYTE drive) { return drive ? STA_NOINIT : 0; }
DRESULT disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
    assert(!drive && count && sector + count <= disk_sectors);
    if (disk_read_fault) { disk_read_fault = 0; return RES_ERROR; }
    assert(fseek(disk, (long)sector * 512, SEEK_SET) == 0);
    return fread(data, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
    assert(!drive && count && sector + count <= disk_sectors); disk_writes++;
    assert(fseek(disk, (long)sector * 512, SEEK_SET) == 0);
    return fwrite(data, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer)
{
    assert(!drive);
    if (cmd == GET_SECTOR_COUNT) { *(LBA_t *)buffer = disk_sectors; return RES_OK; }
    if (cmd == GET_SECTOR_SIZE) { *(WORD *)buffer = 512; return RES_OK; }
    if (cmd == GET_BLOCK_SIZE) { *(DWORD *)buffer = 1; return RES_OK; }
    assert(cmd == CTRL_SYNC);
    return fflush(disk) ? RES_ERROR : RES_OK;
}
static int selected(FIL *fp, const char *operation)
{
    if (require_lock) assert(lock_depth == 1);
    return armed && fp == (fault_target ? &sm_fp : &ss_fp) && !strcmp(fault, operation)
           && ++fault_seen == fault_nth;
}
static FRESULT checked_open(FIL *fp, const TCHAR *name, BYTE mode)
{
    api_opens++;
    if (before_open) before_open(fp, name, mode);
    if (!(mode & (FA_CREATE_NEW | FA_CREATE_ALWAYS)) && selected(fp, "cleanup-error")) {
        hits++; return FR_DENIED;
    }
    if (!(mode & (FA_CREATE_NEW | FA_CREATE_ALWAYS)) && selected(fp, "open-error")) {
        hits++; return fault_value;
    }
    if ((mode & (FA_CREATE_NEW | FA_CREATE_ALWAYS)) && selected(fp, "create-error")) {
        hits++; return fault_value;
    }
    return f_open(fp, name, mode);
}
static FRESULT checked_read(FIL *fp, void *data, UINT size, UINT *count)
{
    api_reads++; read_bytes += size;
    if (selected(fp, "read-disk-error")) {
        assert(size == 8 || size == 16);
        hits++;
        disk_read_fault = 1;
    }
    if (selected(fp, "read-error")) { hits++; *count = 0; return fault_value ? fault_value : FR_DISK_ERR; }
    if (selected(fp, "read-partial-error")) {
        hits++; FRESULT result = f_read(fp, data, size / 2, count);
        return result ? result : FR_DISK_ERR;
    }
    if (selected(fp, "read-short")) { hits++; return f_read(fp, data, fault_value ? size - 1 : 0, count); }
    return f_read(fp, data, size, count);
}
static FRESULT checked_write(FIL *fp, const void *data, UINT size, UINT *count)
{
    assert(api_writes < sizeof(write_offsets) / sizeof(write_offsets[0]));
    write_offsets[api_writes] = f_tell(fp); write_sizes[api_writes] = size;
    api_writes++; write_bytes += size;
    if (selected(fp, "write-error") || selected(fp, "write-short")) {
        hits++;
        FRESULT result = f_write(fp, data, size / 2, count);
        if (result) return result;
        return !strcmp(fault, "write-error") ? FR_DISK_ERR : FR_OK;
    }
    return f_write(fp, data, size, count);
}
static FRESULT checked_seek(FIL *fp, FSIZE_t offset)
{
    api_seeks++;
    if (selected(fp, "seek-any-error")) { hits++; return FR_DISK_ERR; }
    if (selected(fp, "seek-any-short")) { hits++; return f_lseek(fp, offset ? offset - 1 : 1); }
    if (selected(fp, "seek-error") && offset == 0) { hits++; return FR_DISK_ERR; }
    if (selected(fp, "extend-error") && offset == 0x800000) { hits++; return FR_DISK_ERR; }
    if (selected(fp, "extend-short") && offset == 0x800000) { hits++; return f_lseek(fp, offset - 512); }
    return f_lseek(fp, offset);
}
static FRESULT checked_sync(FIL *fp)
{
    api_syncs++;
    if (selected(fp, "sync-error")) { hits++; return fault_value ? fault_value : FR_DISK_ERR; }
    return f_sync(fp);
}
static FRESULT checked_close(FIL *fp)
{
    cleanup_calls++;
    FRESULT result = f_close(fp);
    return armed && !strcmp(fault, "cleanup-error") ? FR_DISK_ERR : result;
}
static const char *filename(int mems) { return mems ? SMEMS_FILE : SSAVE_FILE; }
static void fill_header(u8 *b, int mems)
{
    memset(b, 0, mems ? 8192 : 65536);
    if (mems) {
        /* Independent expected on-disk bytes, not host-endian typed fields. */
        static const u8 prefix[17] = {'S','a','r','o','M','e','m','s',0,0x80,0,0,0x1f,0x78,0,0,0xff};
        memcpy(b, prefix, sizeof(prefix));
    } else memcpy(b, "Saroo Save File", 15);
}
static void make_archive(int mems, long size, int damaged)
{
    FIL fp; UINT n;
    u8 *b = malloc(65536); assert(b); fill_header(b, mems);
    if (damaged == 3) b[15] = 0xa5;
    else if (damaged) memset(b, damaged == 1 ? 'X' : 0, mems ? 8192 : 65536);
    assert(f_open(&fp, filename(mems), FA_READ | FA_WRITE | FA_CREATE_NEW) == FR_OK);
    UINT first = mems ? 8192 : 65536;
    if (size < first) first = size;
    assert(f_write(&fp, b, first, &n) == FR_OK && n == first);
    if (size > first) {
        assert(f_lseek(&fp, size - 1) == FR_OK && f_tell(&fp) == (FSIZE_t)size - 1);
        b[0] = 0xa5; assert(f_write(&fp, b, 1, &n) == FR_OK && n == 1);
    }
    assert(f_close(&fp) == FR_OK); free(b);
}
static uint64_t file_digest(int mems, FSIZE_t *size)
{
    FIL fp; UINT count; u8 buffer[8192]; uint64_t hash = UINT64_C(14695981039346656037);
    FRESULT r = f_open(&fp, filename(mems), FA_READ);
    if (r == FR_NO_FILE) { *size = 0; return 0; }
    assert(r == FR_OK); *size = f_size(&fp);
    do {
        assert(f_read(&fp, buffer, sizeof(buffer), &count) == FR_OK);
        for (UINT i = 0; i < count; i++) { hash ^= buffer[i]; hash *= UINT64_C(1099511628211); }
    } while (count);
    assert(f_close(&fp) == FR_OK); return hash;
}
static void reset_service(void)
{
    memset(&ss_fp, 0, sizeof(ss_fp)); memset(&sm_fp, 0, sizeof(sm_fp));
    ss_index = sm_index0 = sm_index1 = 0;
    /* Leave a valid signature in scratch so ignoring a failed read is visible. */
    fill_header(test_scratch, 0); fill_header((u8 *)SMEMS_HDR, 1);
    started = error_led = retv = 0;
    api_reads = api_writes = api_seeks = api_syncs = api_opens = read_bytes = write_bytes = 0;
    cleanup_calls = 0;
    fault_seen = 0;
}
int main(int argc, char **argv)
{
    assert(argc == 6);
    disk = fopen(argv[1], "r+b"); assert(disk);
    fault = argv[2]; fault_target = atoi(argv[3]); fault_value = atoi(argv[4]);
    int use_gate = atoi(argv[5]);
    FATFS fs; assert(f_mount(&fs, "0:", 1) == FR_OK);
    assert(f_mkdir("/SAROO") == FR_OK);
    int missing[2] = {0,0};
    if (!strcmp(fault, "new-both")) missing[0] = missing[1] = 1;
    if (!strcmp(fault, "missing") || !strcmp(fault, "create-error") ||
        !strcmp(fault, "write-error") || !strcmp(fault, "write-short") ||
        !strcmp(fault, "sync-error") || !strcmp(fault, "extend-error") || !strcmp(fault, "extend-short"))
        missing[fault_target] = 1;
    for (int mems = 0; mems < 2; mems++) if (!missing[mems]) {
        long size = mems ? 0x800000 : 0x20000;
        int damage = 0;
        if (mems == fault_target) {
            if (!strcmp(fault, "size") || !strcmp(fault, "compatible-size")) size = fault_value;
            if (!strcmp(fault, "signature")) damage = fault_value;
        }
        make_archive(mems, size, damage);
    }
    FSIZE_t sizes[2]; uint64_t hashes[2];
    for (int i=0;i<2;i++) hashes[i]=file_digest(i,&sizes[i]);
    unsigned writes_before = disk_writes;
    reset_service(); armed = 1;
    int success = !strcmp(fault, "healthy") || !strcmp(fault, "new-both") || !strcmp(fault, "missing") || !strcmp(fault, "compatible-size");
    int result;
    if (use_gate) { startup_gate(); result = retv; }
    else result = open_savefile();
    armed = 0;
    assert((result == 0) == success);
    FRESULT expected_error = FR_OK;
    if (!success) {
        if (!strcmp(fault, "open-error")) expected_error = fault_value == FR_NO_FILE ? FR_EXIST : fault_value;
        else if (!strcmp(fault, "create-error")) expected_error = fault_value;
        else if (!strcmp(fault, "read-short") || !strcmp(fault, "size") || !strcmp(fault, "signature")) expected_error = FR_INT_ERR;
        else if (!strcmp(fault, "extend-short") || !strcmp(fault, "cleanup-error")) expected_error = FR_DENIED;
        else expected_error = FR_DISK_ERR;
    }
    assert(result == expected_error);
    if (!strcmp(fault, "cleanup-error")) assert(cleanup_calls == 2);
    if (use_gate) {
        assert(started == success && error_led == !success);
    }
    if (success) {
        assert(ss_index == 0 && sm_index0 == 0 && sm_index1 == 0);
        u8 expected[8192]; fill_header(expected, 1);
        assert(memcmp((void *)SMEMS_HDR, expected, sizeof(expected)) == 0);
        assert(api_reads == 2 && read_bytes == 8256);
        assert(write_bytes == (missing[0] ? 65536u : 0) + (missing[1] ? 8192u : 0));
        assert(f_close(&ss_fp) == FR_OK && f_close(&sm_fp) == FR_OK);
    } else {
        assert(ss_index == 0 && !ss_fp.obj.fs && !sm_fp.obj.fs);
        if (!missing[0] && !missing[1]) assert(disk_writes == writes_before);
    }
    for (int i=0;i<2;i++) if (!missing[i]) {
        FSIZE_t size; assert(file_digest(i,&size)==hashes[i] && size==sizes[i]);
    }
    if (strstr(fault,"error") || strstr(fault,"short")) assert(hits == 1);
    if (success) {
        writes_before = disk_writes; reset_service();
        assert(open_savefile() == 0 && disk_writes == writes_before);
        assert(f_close(&ss_fp) == FR_OK && f_close(&sm_fp) == FR_OK);
    }
    printf("startup %s target=%d result=%d reads=%u read_bytes=%u writes=%u write_bytes=%u\n",
           fault, fault_target, result, api_reads, read_bytes, api_writes, write_bytes);
    assert(fclose(disk) == 0); return 0;
}
