/* Adapted from paps-g 2ba4e96b; actual public diskio.c on checked stubs. */
/* Actual diskio.c with CMSIS semaphore/SD boundaries replaced by checked stubs. */
#include "main.h"
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned creates, acquires, releases, transfers, driver_failures;
static int fail_create, held;
static int reset_result, overlap_reset;
static int card_ready, ready_on_acquire;
static unsigned resets;
static int expected_write;
static char transfer_log[384];
static size_t log_size;
static osStatus_t acquire_result;
static unsigned char handles[1024], buffer[1024];
static osSemaphoreId_t first_handle;

osSemaphoreId_t osSemaphoreNew(uint32_t max, uint32_t initial,
                             const osSemaphoreAttr_t *attr)
{
    assert(max == 1 && initial == 1 && attr == NULL);
    assert(creates < sizeof(handles));
    ++creates;
    if (fail_create) return NULL;
    osSemaphoreId_t handle = &handles[creates - 1];
    if (!first_handle) first_handle = handle;
    return handle;
}

osStatus_t osSemaphoreAcquire(osSemaphoreId_t sem, uint32_t timeout)
{
    ++acquires;
    assert(timeout == 100);
    if (!sem) return osErrorParameter;
    assert(sem == first_handle);
    if (held) return osErrorTimeout;
    if (acquire_result == osOK) {
        held = 1;
        if (ready_on_acquire) card_ready = 1;
    }
    return acquire_result;
}

osStatus_t osSemaphoreRelease(osSemaphoreId_t sem)
{
    assert(sem == first_handle && held);
    held = 0;
    ++releases;
    return osOK;
}

int printk(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(transfer_log + log_size, sizeof transfer_log - log_size,
                           format, arguments);
    va_end(arguments);
    assert(length >= 0 && (size_t)length < sizeof transfer_log - log_size);
    log_size += (size_t)length;
    return length;
}

int sdio_ready(void) { return card_ready; }

int sdio_reset(void)
{
    assert(held);
    resets++;
    return reset_result;
}

static int transfer(uint32_t block, int count, uint8_t *buf, int write)
{
    /* No DMA/driver access may happen without an acquired semaphore. */
    assert(held && block == 31 && count == 2 && buf == buffer);
    assert(write == expected_write);
    if (overlap_reset) {
        assert(disk_reidentify()==-1 && held && !resets);
    }
    ++transfers;
    if (transfers <= driver_failures) return -1;
    for (unsigned i = 0; i < sizeof(buffer); ++i) {
        if (write) assert(buf[i] == (uint8_t)i);
        else buf[i] = (uint8_t)i;
    }
    return 0;
}

int sd_read_sector(uint32_t block, int count, uint8_t *buf)
{ return transfer(block, count, buf, 0); }
#if FF_FS_READONLY == 0
int sd_write_sector(uint32_t block, int count, uint8_t *buf)
{ return transfer(block, count, buf, 1); }
#endif

static DRESULT run_transfer(BYTE drive, int write)
{
    expected_write = write;
#if FF_FS_READONLY == 0
    if (write)
        return disk_write(drive, buffer, 31, 2);
#else
    assert(!write);
#endif
    return disk_read(drive, buffer, 31, 2);
}

static void check_retries(BYTE drive, int write, unsigned failures)
{
    acquires = releases = transfers = 0;
    driver_failures = failures;
    log_size = 0;
    transfer_log[0] = '\0';
    for (unsigned i = 0; i < sizeof(buffer); ++i)
        buffer[i] = (uint8_t)i;
    DRESULT result = run_transfer(drive, write);
    assert(result == (failures < 4 ? RES_OK : RES_ERROR));
    assert(transfers == (failures < 4 ? failures + 1 : 4));
    assert(acquires == 1 && releases == 1 && !held);
    for (unsigned i = 0; i < sizeof(buffer); ++i)
        assert(buffer[i] == (uint8_t)i);

    char failure[96];
    const char *direction = write ? "write" : "read";
    int length = snprintf(failure, sizeof failure,
                          "disk_%s: pdrv=%u sector=0000001f count=2\n    retv=ffffffff\n",
                          direction, (unsigned)drive);
    assert(length > 0 && (size_t)length < sizeof failure);
    const char *log = transfer_log;
    for (unsigned i = 0; i < failures; ++i) {
        assert(strncmp(log, failure, (size_t)length) == 0);
        log += length;
    }
    const char *recovery = failures && failures < 4 ?
                           (write ? "    retry write OK.\n" : "    retry read OK.\n") : "";
    assert(strcmp(log, recovery) == 0);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    const char *mode = argv[1];
    int value = atoi(argv[2]);
    if (!strcmp(mode, "status")) {
        assert(disk_status(0) == STA_NOINIT);
        assert(disk_initialize(0) == 0 && creates == 1);
        assert(disk_status(0) == 0);
    } else if (!strcmp(mode, "create-failure")) {
        fail_create = 1;
        assert(disk_initialize(0) == STA_NOINIT && creates == 1);
        assert(disk_status(0) == STA_NOINIT);
        assert(run_transfer(0, 0) == RES_ERROR);
        assert(run_transfer(0, 1) == RES_ERROR);
        assert(!transfers && !releases);
        fail_create = 0;
        assert(disk_initialize(0) == 0 && creates == 2);
        assert(disk_status(0) == 0);
        assert(run_transfer(0, 0) == RES_OK);
        assert(transfers == 1 && releases == 1 && !held);
    } else if (!strcmp(mode, "repeat")) {
        for (unsigned i = 0; i < 1000; ++i)
            assert(disk_initialize(0) == 0);
        fprintf(stderr,"initialization calls=1000 objects=%u\n",creates);
        assert(creates == 1);
        assert(run_transfer(0, 0) == RES_OK && !held);
    } else if (!strcmp(mode, "uninitialized")) {
        memset(buffer,0xa5,sizeof(buffer));
        assert(run_transfer(0,0)==RES_ERROR);
        assert(run_transfer(0,1)==RES_ERROR);
        assert(acquires==2 && !transfers && !releases && !creates && !held);
        for(unsigned i=0;i<sizeof(buffer);i++)assert(buffer[i]==0xa5);
    } else if (!strcmp(mode, "acquire-error")) {
        assert(disk_initialize(0) == 0);
        acquire_result = (osStatus_t)value;
        assert(acquire_result != osOK);
        memset(buffer, 0xa5, sizeof(buffer));
        assert(run_transfer(0, 0) == RES_ERROR);
        assert(run_transfer(0, 1) == RES_ERROR);
        assert(acquires == 2 && !transfers && !releases && !held);
        for (unsigned i = 0; i < sizeof(buffer); ++i) assert(buffer[i] == 0xa5);
    } else if (!strcmp(mode, "reidentify")) {
        if (value==0) {
            assert(disk_reidentify()==-1);
            assert(!creates && !acquires && !releases && !resets);
        } else if (value<=2) {
            assert(disk_initialize(0)==0);
            acquire_result=value==1 ? osErrorTimeout : osErrorResource;
            assert(disk_reidentify()==-1);
            assert(acquires==1 && !resets && !releases && !held);
        } else {
            assert(disk_initialize(0)==0);
            reset_result=value==3 ? 0 : -17;
            assert(disk_reidentify()==reset_result);
            assert(acquires==1 && resets==1 && releases==1 && !held);
        }
    } else if (!strcmp(mode, "resume")) {
        assert(disk_initialize(0) == 0);
        card_ready = value == 0;
        ready_on_acquire = value == 1;
        reset_result = value == 3 ? -17 : 0;
        if (value == 4) acquire_result = osErrorTimeout;
        assert(disk_resume() == (value == 4 ? -1 : reset_result));
        assert(acquires == (value != 0));
        assert(releases == (value != 0 && value != 4));
        assert(resets == (value == 2 || value == 3) && !held);
    } else if (!strcmp(mode, "reidentify-overlap")) {
        assert(disk_initialize(0)==0);
        overlap_reset=1;
        for (unsigned i=0;i<sizeof(buffer);i++) buffer[i]=(uint8_t)i;
        assert(run_transfer(0,value)==RES_OK);
        assert(acquires==2 && releases==1 && transfers==1 && !held && !resets);
        assert(disk_reidentify()==0 && resets==1 && releases==2 && !held);
    } else if (!strcmp(mode, "all-drives")) {
        assert(disk_initialize(0) == 0);
        for (unsigned drive = 0; drive < 256; ++drive)
            for (unsigned failures = 0; failures <= 4; ++failures) {
                check_retries(drive, 0, failures);
#if FF_FS_READONLY == 0
                check_retries(drive, 1, failures);
#endif
            }
    } else {
        assert(!strcmp(mode, "read") || !strcmp(mode, "write"));
        assert(disk_initialize(0) == 0);
        check_retries(0, !strcmp(mode, "write"), (unsigned)value);
    }
    printf("%s %d: creates=%u acquisitions=%u releases=%u transfers=%u\n",
           mode, value, creates, acquires, releases, transfers);
    return 0;
}
