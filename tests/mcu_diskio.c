/* Adapted from paps-g 2ba4e96b; actual public diskio.c on checked stubs. */
/* Actual diskio.c with CMSIS semaphore/SD boundaries replaced by checked stubs. */
#include "main.h"
#include "ff.h"
#include "diskio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned creates, acquires, releases, transfers, driver_failures;
static int fail_create, held;
static int reset_result, overlap_reset;
static unsigned resets;
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
    if (acquire_result == osOK) held = 1;
    return acquire_result;
}

osStatus_t osSemaphoreRelease(osSemaphoreId_t sem)
{
    assert(sem == first_handle && held);
    held = 0;
    ++releases;
    return osOK;
}

int printk(const char *format, ...) { (void)format;return 0; }

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
int sd_write_sector(uint32_t block, int count, uint8_t *buf)
{ return transfer(block, count, buf, 1); }

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
        assert(disk_read(0, buffer, 31, 2) == RES_ERROR);
        assert(disk_write(0, buffer, 31, 2) == RES_ERROR);
        assert(!transfers && !releases);
        fail_create = 0;
        assert(disk_initialize(0) == 0 && creates == 2);
        assert(disk_status(0) == 0);
        assert(disk_read(0, buffer, 31, 2) == RES_OK);
        assert(transfers == 1 && releases == 1 && !held);
    } else if (!strcmp(mode, "repeat")) {
        for (unsigned i = 0; i < 1000; ++i)
            assert(disk_initialize(0) == 0);
        fprintf(stderr,"initialization calls=1000 objects=%u\n",creates);
        assert(creates == 1);
        assert(disk_read(0, buffer, 31, 2) == RES_OK && !held);
    } else if (!strcmp(mode, "uninitialized")) {
        memset(buffer,0xa5,sizeof(buffer));
        assert(disk_read(0,buffer,31,2)==RES_ERROR);
        assert(disk_write(0,buffer,31,2)==RES_ERROR);
        assert(acquires==2 && !transfers && !releases && !creates && !held);
        for(unsigned i=0;i<sizeof(buffer);i++)assert(buffer[i]==0xa5);
    } else if (!strcmp(mode, "acquire-error")) {
        assert(disk_initialize(0) == 0);
        acquire_result = (osStatus_t)value;
        assert(acquire_result != osOK);
        memset(buffer, 0xa5, sizeof(buffer));
        assert(disk_read(0, buffer, 31, 2) == RES_ERROR);
        assert(disk_write(0, buffer, 31, 2) == RES_ERROR);
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
    } else if (!strcmp(mode, "reidentify-overlap")) {
        assert(disk_initialize(0)==0);
        overlap_reset=1;
        for (unsigned i=0;i<sizeof(buffer);i++) buffer[i]=(uint8_t)i;
        assert((value ? disk_write(0,buffer,31,2) : disk_read(0,buffer,31,2))==RES_OK);
        assert(acquires==2 && releases==1 && transfers==1 && !held && !resets);
        assert(disk_reidentify()==0 && resets==1 && releases==2 && !held);
    } else {
        assert(!strcmp(mode, "read") || !strcmp(mode, "write"));
        assert(disk_initialize(0) == 0);
        driver_failures = (unsigned)value;
        for (unsigned i = 0; i < sizeof(buffer); ++i) buffer[i] = (uint8_t)i;
        DRESULT result = !strcmp(mode, "read") ? disk_read(0, buffer, 31, 2) :
                                                disk_write(0, buffer, 31, 2);
        assert(result == (value < 4 ? RES_OK : RES_ERROR));
        assert(transfers == (value < 4 ? (unsigned)value + 1 : 4));
        assert(acquires == 1 && releases == 1 && !held);
        for (unsigned i = 0; i < sizeof(buffer); ++i) assert(buffer[i] == (uint8_t)i);
    }
    printf("%s %d: creates=%u acquisitions=%u releases=%u transfers=%u\n",
           mode, value, creates, acquires, releases, transfers);
    return 0;
}
