/* Adapted from paps-g SSR-165 / 14edd9cd. Real FatFs with host sectors. */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FILE *disk;
static LBA_t capacity, *data_map;
static size_t map_count;
static FSIZE_t initial_valid;
static pthread_mutex_t grant;
static _Thread_local unsigned held;
static unsigned reads, writes, syncs, active, ordinal, hits, tail_guard, tail_hits, total_writes;
static char fault_kind;
int ff_mutex_create(int volume) { assert(!volume); return !pthread_mutex_init(&grant, 0); }
void ff_mutex_delete(int volume) { assert(!volume); assert(!pthread_mutex_destroy(&grant)); }
int ff_mutex_take(int volume) { assert(!volume && !held); assert(!pthread_mutex_lock(&grant)); held = 1; return 1; }
void ff_mutex_give(int volume) { assert(!volume && held == 1); held = 0; assert(!pthread_mutex_unlock(&grant)); }
DSTATUS disk_initialize(BYTE drive) { return drive ? STA_NOINIT : 0; }
DSTATUS disk_status(BYTE drive) { return disk_initialize(drive); }
static int failed(char kind, unsigned count) { if (active && fault_kind == kind && count == ordinal) { hits++; return 1; } return 0; }
DRESULT disk_read(BYTE drive, BYTE *out, LBA_t sector, UINT count)
{
    assert(!drive && count && sector < capacity && count <= capacity - sector);
    if (active && failed('r', ++reads)) return RES_ERROR;
    if (tail_guard) for (UINT i = 0; i < count; i++) for (size_t k = 0; k < map_count; k++) {
        if (sector + i == data_map[k] && (FSIZE_t)k * 512 >= initial_valid + (512 - initial_valid % 512) % 512) {
            tail_hits++; return RES_ERROR;
        }
    }
    assert(!fseeko(disk, (off_t)sector * 512, SEEK_SET));
    return fread(out, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drive, const BYTE *out, LBA_t sector, UINT count)
{
    assert(!drive && count && sector < capacity && count <= capacity - sector);
    total_writes++;
    if (active && failed('w', ++writes)) return RES_ERROR;
    assert(!fseeko(disk, (off_t)sector * 512, SEEK_SET));
    return fwrite(out, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *out)
{
    assert(!drive);
    if (command == CTRL_SYNC) { if (active && failed('s', ++syncs)) return RES_ERROR; return fflush(disk) ? RES_ERROR : RES_OK; }
    if (command == GET_SECTOR_COUNT) { *(LBA_t *)out = capacity; return RES_OK; }
    if (command == GET_SECTOR_SIZE) { *(WORD *)out = 512; return RES_OK; }
    if (command == GET_BLOCK_SIZE) { *(DWORD *)out = 1; return RES_OK; }
    return RES_PARERR;
}
static void read_at(FIL *file, const BYTE *model, size_t size, size_t position, UINT request)
{
    assert(position <= size && !f_lseek(file, position) && f_tell(file) == position);
    BYTE *guard = malloc((size_t)request + 2); assert(guard); memset(guard, 0x73, (size_t)request + 2);
    UINT count = 0; assert(!f_read(file, guard + 1, request, &count));
    size_t expected = request < size - position ? request : size - position;
    assert(count == expected && f_tell(file) == position + expected && f_size(file) == size);
    assert(guard[0] == 0x73 && guard[(size_t)request + 1] == 0x73);
    assert(!memcmp(guard + 1, model + position, expected));
    for (size_t i = expected; i < request; i++) assert(guard[i + 1] == 0x73);
    free(guard);
}
static void check(FIL *file, const BYTE *model, size_t size)
{
    read_at(file, model, size, 0, (UINT)size + 17);
    size_t positions[] = {size, 0, size / 2, 1, 511, 512, 513, 2047, 2048, 2049, initial_valid, initial_valid + 1, 0};
    for (size_t i = 0; i < sizeof positions / sizeof *positions; i++) if (positions[i] <= size) {
        read_at(file, model, size, positions[i], 733);
    }
}
static void neighbor(void)
{
    FIL file = {0}; BYTE data[513]; UINT count;
    assert(!f_open(&file, "/NEIGHBOR.BIN", FA_READ));
    assert(f_size(&file) == sizeof data && !f_read(&file, data, sizeof data, &count) && count == sizeof data);
    for (unsigned i = 0; i < sizeof data; i++) assert(data[i] == 0xb4);
    assert(!f_close(&file));
}
struct reader_job { FIL *file; const BYTE *model; size_t size; };
static void *parallel_keep(void *argument)
{
    struct reader_job *job = argument;
    for (unsigned i = 0; i < 12; i++) check(job->file, job->model, job->size);
    return 0;
}
static void *parallel_neighbor(void *argument)
{
    (void)argument;
    for (unsigned i = 0; i < 40; i++) neighbor();
    return 0;
}
#if !FF_FS_READONLY
static void make_file(const char *name, size_t length, BYTE value)
{
    FIL file = {0}; BYTE block[4096]; UINT count; memset(block, value, sizeof block);
    assert(!f_open(&file, name, FA_CREATE_NEW | FA_WRITE));
    while (length) { UINT chunk = length > sizeof block ? sizeof block : (UINT)length; assert(!f_write(&file, block, chunk, &count) && count == chunk); length -= chunk; }
    assert(!f_close(&file));
}
#endif
int main(int argc, char **argv)
{
    assert(argc == 10); setvbuf(stdout, 0, _IONBF, 0);
    const char *mode = argv[2]; initial_valid = strtoull(argv[3], 0, 10);
    size_t size = strtoull(argv[4], 0, 10), position = strtoull(argv[5], 0, 10); UINT amount = (UINT)strtoul(argv[6], 0, 10);
    fault_kind = argv[7][0]; ordinal = (unsigned)strtoul(argv[8], 0, 10);
    disk = fopen(argv[1], "r+b"); assert(disk && !fseeko(disk, 0, SEEK_END)); capacity = ftello(disk) / 512;
    FATFS fs = {0};
    if (!strcmp(mode, "format")) {
#if FF_USE_MKFS && !FF_FS_READONLY
        BYTE work[32768]; MKFS_PARM options = {.fmt = FM_EXFAT | FM_SFD, .n_fat = 1, .au_size = (DWORD)position * 512};
        assert(!f_mkfs("0:", &options, work, sizeof work) && !f_mount(&fs, "0:", 1));
        make_file("/KEEP.BIN", size, 0xc9); make_file("/NEIGHBOR.BIN", 513, 0xb4);
        assert(!f_mount(0, "0:", 0) && !fclose(disk)); printf("{\"formatted\":true}\n"); return 0;
#else
        assert(0);
#endif
    }
    if (strcmp(argv[9], "-")) {
        FILE *map = fopen(argv[9], "r"); assert(map); unsigned long long sector;
        while (fscanf(map, "%llu", &sector) == 1) { data_map = realloc(data_map, (map_count + 1) * sizeof *data_map); assert(data_map); data_map[map_count++] = (LBA_t)sector; }
        assert(!fclose(map));
    }
    assert(!f_mount(&fs, "0:", 1));
    FIL file = {0}; BYTE access = FA_READ;
#if !FF_FS_READONLY
    if (strcmp(mode, "read") && strcmp(mode, "tail-fault") && strcmp(mode, "refuse") && strcmp(mode, "threads") && strcmp(mode, "baseline-tail")) access |= FA_WRITE;
    if (!strcmp(mode, "append")) access |= FA_OPEN_APPEND;
    if (!strcmp(mode, "replace") || !strcmp(mode, "refuse-truncate")) access |= FA_CREATE_ALWAYS;
#endif
    FRESULT result = f_open(&file, "/KEEP.BIN", access);
    if (!strcmp(mode, "refuse") || !strcmp(mode, "refuse-truncate")) { assert(result == FR_INT_ERR && !file.obj.fs); assert(!f_mount(0, "0:", 0) && !fclose(disk)); assert(!total_writes); printf("{\"refused\":true}\n"); return 0; }
    if (!strcmp(mode, "replace")) { size = 0; initial_valid = 0; }
    assert(!result && f_size(&file) == size && initial_valid <= size);
#ifdef SSR165_BASELINE
    if (!strcmp(mode, "baseline-tail")) {
        BYTE observed[512]; UINT count = 0;
        assert(!initial_valid && !f_read(&file, observed, sizeof observed, &count) && count == sizeof observed);
        for (unsigned i = 0; i < sizeof observed; i++) assert(observed[i] == 0xc9);
        assert(!f_close(&file) && !f_mount(0, "0:", 0) && !fclose(disk) && !held);
        printf("{\"baseline_raw_tail\":true,\"writes\":%u}\n", total_writes);
        return 0;
    }
#endif
    size_t reserve = size + position + amount + 4096; BYTE *model = calloc(reserve ? reserve : 1, 1); assert(model); memset(model, 0xc9, (size_t)initial_valid);
    unsigned operation_reads = 0, operation_writes = 0, operation_syncs = 0, operation_hits = 0; UINT user_count = 0; unsigned status = 0, sync_status = 0; char requested_fault = fault_kind; unsigned requested_ordinal = ordinal;
    if (!strcmp(mode, "read") || !strcmp(mode, "tail-fault")) {
        tail_guard = !strcmp(mode, "tail-fault"); check(&file, model, size); tail_guard = 0; assert(!tail_hits); neighbor();
    } else if (!strcmp(mode, "threads")) {
        struct reader_job job = {&file, model, size}; pthread_t a, b;
        assert(!pthread_create(&a, 0, parallel_keep, &job) && !pthread_create(&b, 0, parallel_neighbor, 0));
        assert(!pthread_join(a, 0) && !pthread_join(b, 0));
    } else {
#if !FF_FS_READONLY
        if (!strcmp(mode, "truncate")) {
            assert(position <= size && !f_lseek(&file, position) && !f_truncate(&file)); size = position;
        } else if (!strcmp(mode, "seek-expand")) {
            assert(position >= size && !f_lseek(&file, position) && f_size(&file) == position); size = position;
        } else {
            BYTE *payload = malloc(amount ? amount : 1); assert(payload);
            for (UINT i = 0; i < amount; i++) payload[i] = (BYTE)(0xa5 ^ (i % 47));
            if (!strcmp(mode, "append")) { assert(f_tell(&file) == size); position = size; }
            else assert(!f_lseek(&file, position) && f_tell(&file) == position);
            if (position > size) size = position;
            if (!strcmp(mode, "sync-fault")) fault_kind = 'n';
            active = 1; result = f_write(&file, payload, amount, &user_count); active = 0;
            operation_reads = reads; operation_writes = writes; operation_syncs = syncs; operation_hits = hits; status = result;
            assert(user_count <= amount && f_tell(&file) == position + user_count);
            memcpy(model + position, payload, user_count); if (position + user_count > size) size = position + user_count;
            if (hits) assert(result == FR_DISK_ERR && file.err == FR_DISK_ERR);
            else assert(result == FR_OK && user_count == amount);
            free(payload);
        }
        fault_kind = 'n'; ordinal = 0; active = 0;
        if (!status && strcmp(mode, "sync-fault")) check(&file, model, size); /* EOF reads must not promote VDL. */
        if (!strcmp(mode, "sync-fault")) {
            reads = writes = syncs = hits = 0; fault_kind = requested_fault; ordinal = requested_ordinal; active = 1;
            result = f_sync(&file); active = 0;
            operation_reads = reads; operation_writes = writes; operation_syncs = syncs; operation_hits = hits; sync_status = result;
            assert(!file.err);
            if (hits) assert(result == FR_DISK_ERR); else assert(result == FR_OK);
            fault_kind = 'n'; ordinal = 0;
        }
        assert(!f_sync(&file));
#else
        assert(0);
#endif
    }
    assert(!f_close(&file) && !f_mount(0, "0:", 0) && !f_mount(&fs, "0:", 1));
    assert(!f_open(&file, "/KEEP.BIN", FA_READ)); check(&file, model, size); assert(!f_close(&file)); neighbor();
    assert(!f_mount(0, "0:", 0) && !fclose(disk) && !held); free(model); free(data_map);
    printf("{\"mode\":\"%s\",\"size\":%zu,\"write_status\":%u,\"sync_status\":%u,\"user_count\":%u,\"reads\":%u,\"writes\":%u,\"syncs\":%u,\"fault_hits\":%u,\"tail_hits\":%u,\"filesystem_writes\":%u,\"fil_bytes\":%zu}\n", mode, size, status, sync_status, user_count, operation_reads, operation_writes, operation_syncs, operation_hits, tail_hits, total_writes, sizeof(FIL));
    return 0;
}
