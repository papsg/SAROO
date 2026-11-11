/* Adapted from paps-g dc3468de. Actual SD command C at a modeled MMIO/RTOS boundary. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t u32;
typedef void *osSemaphoreId_t;
/* CMSIS-RTOS2 status values used by the command path. */
typedef enum {
    osOK = 0,
    osError = -1,
    osErrorTimeout = -2,
    osErrorResource = -3,
    osErrorParameter = -4,
    osErrorNoMemory = -5,
    osErrorISR = -6
} osStatus_t;

/* Bit positions from STM32H750 SDMMC, RM0433 Rev 8. */
#define SDMMC_CMD_CMDTRANS (1u << 6)
#define SDMMC_CMD_CMDSTOP  (1u << 7)
#define SDMMC_STA_CCRCFAIL (1u << 0)
#define SDMMC_STA_DCRCFAIL (1u << 1)
#define SDMMC_STA_CTIMEOUT (1u << 2)
#define SDMMC_STA_DTIMEOUT (1u << 3)
#define SDMMC_STA_TXUNDERR (1u << 4)
#define SDMMC_STA_RXOVERR  (1u << 5)
#define SDMMC_STA_CMDREND  (1u << 6)
#define SDMMC_STA_CMDSENT  (1u << 7)
#define SDMMC_STA_DATAEND  (1u << 8)
#define SDMMC_STA_IDMATE   (1u << 27)
#include "command-defines.inc"

static struct {
    volatile u32 MASK, ARG, CMD, STA, RESP1, RESP2, RESP3, RESP4;
    volatile u32 DCTRL, IDMACTRL, ICR;
} controller;
#define SDIO (&controller)

static int semaphore_token;
static osSemaphoreId_t cmd_sem = &semaphore_token;
static u32 cmd_resp[4];
static u32 wanted_command, wanted_mask, completion_status;
static osStatus_t wait_status;
static unsigned waits;

static void printk(const char *format, ...)
{
    (void)format;
}

static osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore, u32 timeout)
{
    assert(semaphore == cmd_sem && timeout == 100);
    assert(SDIO->CMD == (0x1000u | (wanted_command & 0x03ff)));
    assert(SDIO->ARG == 0x12345678 && SDIO->MASK == wanted_mask);
    waits++;
    SDIO->STA = completion_status;
    return semaphore ? wait_status : osErrorParameter;
}

#include "command.inc"

static void check_command(const char *mode, unsigned shape, int value)
{
    const struct {
        u32 cmd, completion;
        unsigned words;
    } shapes[] = {
        {CMD_00, SDMMC_STA_CMDSENT, 0},
        {CMD_08, SDMMC_STA_CMDREND, 1},
        {CMD_02, SDMMC_STA_CMDREND, 4},
        {CMD_17, SDMMC_STA_DATAEND, 1},
        {CMD_41, SDMMC_STA_CMDREND, 1}
    };
    const u32 unchanged[] = {0x42a10001, 0x42a10002, 0x42a10003, 0x42a10004};
    assert(shape < sizeof(shapes) / sizeof(shapes[0]));
    wanted_command = shapes[shape].cmd;
    completion_status = shapes[shape].completion;
    int data = shape == 3;
    wanted_mask = completion_status | CMD_ERROR | (data ? DATA_ERROR : 0);
    int failed_wait = !strcmp(mode, "wait") || !strcmp(mode, "null");
    int hardware_error = !strcmp(mode, "hardware");
    wait_status = !strcmp(mode, "wait") ? (osStatus_t)value : osOK;
    if (!strcmp(mode, "null")) cmd_sem = NULL;
    if (hardware_error) completion_status |= (u32)value;
    if (!strcmp(mode, "no-crc")) {
        assert(shape == 4);
        completion_status |= SDMMC_STA_CCRCFAIL;
    }
    memcpy(cmd_resp, unchanged, sizeof(cmd_resp));
    SDIO->DCTRL = 0x92;
    SDIO->IDMACTRL = 1;
    SDIO->RESP1 = 0x11223344;
    SDIO->RESP2 = 0x55667788;
    SDIO->RESP3 = 0x99aabbcc;
    SDIO->RESP4 = 0xddeeff00;

    u32 result = (u32)sd_cmd(wanted_command, 0x12345678);
    assert(waits == 1 && !SDIO->CMD && !SDIO->MASK && SDIO->ICR == 0x1fe00fff);
    assert(SDIO->DCTRL == (data ? 0 : 0x92) && SDIO->IDMACTRL == (data ? 0 : 1));
    if (failed_wait && result != UINT32_MAX) {
        fprintf(stderr, "Accepted failed command wait: shape=%u status=%d result=%u\n",
                shape, cmd_sem ? value : osErrorParameter, result);
        abort();
    }
    if (failed_wait || hardware_error) {
        assert(result == (failed_wait ? UINT32_MAX : completion_status));
        assert(!memcmp(cmd_resp, unchanged, sizeof(cmd_resp)));
    } else {
        assert(!result);
        unsigned words = shapes[shape].words;
        if (words == 4) {
            assert(cmd_resp[0] == SDIO->RESP4 && cmd_resp[1] == SDIO->RESP3);
            assert(cmd_resp[2] == SDIO->RESP2 && cmd_resp[3] == SDIO->RESP1);
        } else {
            assert(cmd_resp[0] == (words ? SDIO->RESP1 : unchanged[0]));
            assert(!memcmp(cmd_resp + 1, unchanged + 1, sizeof(cmd_resp) - sizeof(*cmd_resp)));
        }
    }
    printf("command %s shape=%u value=%d passed\n", mode, shape, value);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    check_command(argv[1], (unsigned)strtoul(argv[2], NULL, 10),
                  (int)strtol(argv[3], NULL, 10));
    return 0;
}
