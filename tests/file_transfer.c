/* Adapted from paps-g 52ba5498. Actual commands/FatFs with synthetic media. */
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
static u8 *test_shared;
#define TMPBUFF_ADDR ((uintptr_t)(test_shared + 0xc7000))
static u32 log_mask;
static char log_text[512];
static unsigned logs;
static void printk(const char *format, ...)
{
    va_list args; va_start(args,format);
    int n=vsnprintf(log_text,sizeof log_text,format,args); va_end(args);
    assert(n>=0 && (unsigned)n<sizeof log_text); ++logs;
}
#include "log.inc"
static volatile uint16_t SS_CMD, SS_ARG;
static FRESULT checked_open(FIL *, const TCHAR *, BYTE);
static FRESULT checked_seek(FIL *, FSIZE_t);
static FRESULT checked_read(FIL *, void *, UINT, UINT *);
static FRESULT checked_write(FIL *, const void *, UINT, UINT *);
static FRESULT checked_close(FIL *);
#define f_open checked_open
#define f_lseek checked_seek
#define f_read checked_read
#define f_write checked_write
#define f_close checked_close
#include "file-transfer.inc"
#undef f_open
#undef f_lseek
#undef f_read
#undef f_write
#undef f_close

static FILE *disk;
static FATFS fs;
static int fail_disk_writes, disk_failures, fail_disk_reads, disk_read_failures;
static unsigned opens, seeks, reads, writes, closes;
static const char *fault;
DSTATUS disk_initialize(BYTE drive) { return drive ? STA_NOINIT : 0; }
DSTATUS disk_status(BYTE drive) { return drive ? STA_NOINIT : 0; }
DRESULT disk_read(BYTE drive, BYTE *data, LBA_t sector, UINT count)
{
    assert(!drive && count && sector + count <= 131072);
    if (fail_disk_reads) { ++disk_read_failures; return RES_ERROR; }
    assert(!fseek(disk, (long)sector * 512, SEEK_SET));
    return fread(data, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count)
{
    assert(!drive && count && sector + count <= 131072);
    if (fail_disk_writes) { ++disk_failures; return RES_ERROR; }
    assert(!fseek(disk, (long)sector * 512, SEEK_SET));
    return fwrite(data, 512, count, disk) == count ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer)
{
    (void)buffer; assert(!drive && cmd == CTRL_SYNC);
    return fflush(disk) ? RES_ERROR : RES_OK;
}
static int is(const char *name) { return !strcmp(fault, name); }
static FRESULT checked_open(FIL *fp, const TCHAR *name, BYTE flags)
{
    assert(SS_CMD && !opens && !seeks && !reads && !writes && !closes); ++opens;
    return is("open") ? FR_NO_FILE : f_open(fp, name, flags);
}
static FRESULT checked_seek(FIL *fp, FSIZE_t offset)
{
    assert(SS_CMD && opens == 1 && !seeks && !reads && !writes && !closes); ++seeks;
    if (is("seek") || is("seek-close")) return FR_DISK_ERR;
    fail_disk_reads = is("disk-seek");
    /* Force the actual FatFs no-free-clusters branch, without filling 64 MiB. */
    if (is("full-seek")) fs.free_clst = 0;
    FRESULT result = f_lseek(fp, is("short-seek") ? offset-1 : offset);
    fail_disk_reads = 0;
    if (is("full-seek")) {
        assert(result == FR_OK && f_tell(fp) < offset);
        fs.free_clst = 0xffffffff;
    }
    return result;
}
static FRESULT checked_read(FIL *fp, void *data, UINT size, UINT *count)
{
    assert(SS_CMD && seeks == 1 && !reads && !writes && !closes); ++reads;
    *count = 0;
    if (is("transfer") || is("transfer-close")) return FR_DISK_ERR;
    FRESULT result = f_read(fp, data, is("short") || is("partial-error") ? size/2 : size, count);
    return !result && is("partial-error") ? FR_DISK_ERR : result;
}
static FRESULT checked_write(FIL *fp, const void *data, UINT size, UINT *count)
{
    assert(SS_CMD && seeks == 1 && !reads && !writes && !closes); ++writes;
    *count = 0;
    if (is("transfer") || is("transfer-close")) return FR_DISK_ERR;
    FRESULT result = f_write(fp, data, is("short") || is("partial-error") ? size/2 : size, count);
    return !result && is("partial-error") ? FR_DISK_ERR : result;
}
static FRESULT checked_close(FIL *fp)
{
    assert(SS_CMD && opens == 1 && !closes); ++closes;
    fail_disk_writes = is("disk-close");
    FRESULT result = f_close(fp);
    fail_disk_writes = 0;
    return !result && (is("close") || is("transfer-close") || is("seek-close")) ? FR_INT_ERR : result;
}

int main(int argc, char **argv)
{
    assert(argc == 5); log_mask = atoi(argv[4]) ? _INFO : 0;
    const char *mode = argv[2]; fault = argv[3];
    int writing = !strncmp(mode, "write", 5);
    const char *name = "/SAROO/File transfer \xc3\xa9.bin";
    disk = fopen(argv[1], "r+b"); assert(disk);
    assert(f_mount(&fs, "0:", 1) == FR_OK && fs.fs_type == FS_FAT32);
    assert(f_mkdir("/SAROO") == FR_OK);
    u8 original[4096], actual[8192];
    for (unsigned i=0; i<sizeof original; ++i) original[i]=(i*7+11)%251;
    UINT count; FIL file;
    assert(f_open(&file, name, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
    UINT file_size = !strcmp(mode, "read-empty") ? 0 : sizeof original;
    assert(f_write(&file, original, file_size, &count) == FR_OK && count == file_size);
    assert(f_close(&file) == FR_OK);
    test_shared = malloc(0x1000000); assert(test_shared);
    u8 *buffer = test_shared + 0x1000;
    memset(buffer-16, 0xa5, 8192+32);
    for (unsigned i=0; i<17; ++i) if (writing) buffer[i]=0xe0+i;
    u8 payload[17]; memcpy(payload, buffer, sizeof payload);
    memset((void *)TMPBUFF_ADDR, 0x5a, 256);
    int offset = 32; UINT size = 17;
    if (!strcmp(mode, "read-whole") || !strcmp(mode, "read-empty")) { offset=0; size=0; }
    if (!strcmp(mode, "read-eof")) offset=file_size;
    if (!strcmp(mode, "read-beyond")) offset=file_size+100;
    if (!strcmp(mode, "write-create")) offset=-1;
    if (!strcmp(mode, "write-negative")) offset=-2;
    if (!strcmp(mode, "write-extend")) offset=file_size+32;
    if (!strcmp(mode, "write-zero")) size=0;
    /* Tiny public FatFs seeks through the FAT only when crossing clusters. */
    if (is("disk-seek")) offset=1537;
    *(u32 *)(TMPBUFF_ADDR+0x00) = offset;
    *(u32 *)(TMPBUFF_ADDR+0x04) = size;
    *(u32 *)(TMPBUFF_ADDR+0x08) = 0x22001000;
    strcpy((char *)(TMPBUFF_ADDR+0x10), name);
    SS_CMD = writing ? SSCMD_FILEWR : SSCMD_FILERD; SS_ARG = 0;
    command();
    assert(SS_CMD == 0 && opens == 1);
    int result = (int16_t)SS_ARG;
    unsigned moved = *(u32 *)(TMPBUFF_ADDR+0x04);
    int expected = is("open") ? FR_NO_FILE :
                   is("short-seek") || is("full-seek") || is("close") ? FR_INT_ERR :
                   is("none") || is("short") ? FR_OK : FR_DISK_ERR;
    assert(result == -expected);
    unsigned stopped = is("open") || is("seek") || is("seek-close") || is("disk-seek") || is("short-seek") || is("full-seek");
    if (is("disk-seek")) assert(disk_read_failures > 0);
    assert(seeks == !is("open") && closes == !is("open"));
    assert(reads == (unsigned)(!writing && !stopped));
    assert(writes == (unsigned)(writing && !stopped));
    unsigned requested = !writing && !size ? file_size : size;
    unsigned position = offset < 0 ? 0 : (unsigned)offset;
    unsigned wanted = requested;
    if (!writing && wanted > (position < file_size ? file_size-position : 0))
        wanted = position < file_size ? file_size-position : 0;
    if (is("short") || is("partial-error")) wanted /= 2;
    if (stopped || is("transfer") || is("transfer-close")) wanted=0;
    assert(moved == wanted);
    assert(logs == (unsigned)(!is("open") && log_mask));
    if (logs) {
        char expected_log[512];
        snprintf(expected_log,sizeof expected_log,
                 writing ? "\nSSCMD_FILEWR: retv=%d wsize=%08x  %s\n" :
                           "\nSSCMD_FILERD: retv=%d rsize=%08x  %s\n",expected,wanted,name);
        assert(!strcmp(log_text,expected_log));
    }
    assert(!memcmp((void *)(TMPBUFF_ADDR+0x10), name, strlen(name)+1));
    for (unsigned i=0; i<16; ++i) assert(buffer[-16+(int)i] == 0xa5 && buffer[8192+i] == 0xa5);
    if (writing) {
        assert(!memcmp(buffer, payload, sizeof payload));
        if (is("disk-close")) assert(disk_failures > 0);
        else {
            assert(f_open(&file, name, FA_READ) == FR_OK);
            UINT length = f_size(&file);
            assert(length <= sizeof actual);
            assert(f_read(&file, actual, sizeof actual, &count) == FR_OK && count == length);
            assert(f_close(&file) == FR_OK);
            if (!strcmp(mode, "write-create")) assert(length == moved);
            else assert(length == (!stopped && position+moved > file_size ? position+moved : file_size));
            if (moved) assert(!memcmp(actual+position, payload, moved));
            if (position < file_size && strcmp(mode, "write-create")) {
                assert(!memcmp(actual, original, position));
                assert(!memcmp(actual+position+moved, original+position+moved, file_size-position-moved));
            }
        }
    } else {
        if (moved) assert(!memcmp(buffer, original+position, moved));
        for (unsigned i=moved; i<8192; ++i) assert(buffer[i] == 0xa5);
    }
    assert(fclose(disk) == 0); free(test_shared);
    puts("File command result, count, I/O order and content checks pass.");
    return 0;
}
