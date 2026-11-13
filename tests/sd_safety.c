/* Adapted from paps-g dc3468de. Actual SD command C at a modeled MMIO/RTOS boundary. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RTX_Config.h"

typedef uint32_t u32;
typedef uint8_t u8;
#include "sd_model.h"
#include "command-defines.inc"

static struct {
    volatile u32 MASK, ARG, CMD, STA, RESP1, RESP2, RESP3, RESP4;
    volatile u32 DCTRL, IDMACTRL, ICR, CLKCR, DTIMER, POWER;
} controller;
#define SDIO (&controller)
#ifdef TEST_SD_RECOVERY
static struct { volatile u32 AHB3RSTR; } rcc;
#define RCC (&rcc)
static void __DSB(void) { assert(rcc.AHB3RSTR & RCC_AHB3RSTR_SDMMC1RST); }
#endif

static int semaphore_token;
static osSemaphoreId_t cmd_sem = &semaphore_token;
static u32 cmd_resp[4];
static u32 wanted_command, wanted_mask, completion_status;
static osStatus_t wait_status;
static unsigned waits, drains;

/* Adapted from paps-g 96c9dd85. Drive the actual identify and command paths. */
static u8 sd_cid[16];
static volatile u32 sd_rca;
#ifdef HAS_SD_MEDIA_DETECT
static volatile u8 sd_removed;
static struct { u32 IDR; } gpio;
#define GPIOA (&gpio)
#endif
static int is_sdhc;
#ifdef HAS_CSD_CAPACITY_PRINT
static u8 sd_csd[16];
static int bsize, csize, cblock;
#endif
#ifdef TEST_SD_SESSION
static u8 sd_known;
#define __DMB() ((void)0)
#endif
static struct {
    u32 tick, first_tick, last_tick, ready_after, prefix_ticks, command_ticks;
    unsigned prefixes, requests, retry_delays, post_ready;
    unsigned fail_prefix, fail_request;
    int active, legacy;
} identify;

static void printk(const char *format, ...)
{
    (void)format;
}

static u32 osKernelGetTickCount(void)
{
    return identify.tick;
}

static osStatus_t osDelay(u32 ticks)
{
    assert(ticks == 10 || ticks == 1);
    if ((SDIO->CMD & (SDMMC_CMD_CMDSTOP | SDMMC_CMD_CPSMEN)) ==
        (SDMMC_CMD_CMDSTOP | SDMMC_CMD_CPSMEN)) {
        SDIO->CMD &= ~SDMMC_CMD_CPSMEN;
        SDIO->STA = SDMMC_STA_CMDREND;
        identify.tick += ticks;
        return osOK;
    }
    if (ticks == 1 && !identify.post_ready) identify.retry_delays++;
    identify.tick += ticks;
    return osOK;
}

static osStatus_t identify_completion(void)
{
    /* A bounded fixture exposes the previous endless loop deterministically. */
    if (waits > 8u * OS_TICK_FREQ + 32u) {
        fputs("Card-ready polling did not return\n", stderr);
        abort();
    }
    u32 command = SDIO->CMD & 63, argument = SDIO->ARG;
    SDIO->STA = command ? SDMMC_STA_CMDREND : SDMMC_STA_CMDSENT;
    SDIO->RESP1 = 0;
    if (command == 0 || command == 8) {
        assert(!identify.prefixes && argument == (command ? 0x1aau : 0));
        if (command == 8 && identify.legacy) SDIO->STA |= SDMMC_STA_CTIMEOUT;
        return osOK;
    }
    if (command == 55 && !identify.post_ready) {
        assert(!argument && identify.prefixes == identify.requests);
        identify.prefixes++;
        identify.tick += identify.prefixes == 1 ? identify.prefix_ticks : identify.command_ticks;
        SDIO->RESP1 = 0x120; /* READY_FOR_DATA on the prefix does not make OCR ready. */
        return identify.prefixes == identify.fail_prefix ? osErrorResource : osOK;
    }
    if (command == 41) {
        assert(!identify.post_ready && identify.prefixes == identify.requests + 1);
        assert(argument == (identify.legacy ? 0x00f00000u : 0x40f00000u));
        if (!identify.requests) identify.first_tick = osKernelGetTickCount();
        identify.requests++;
        identify.tick += identify.command_ticks;
        identify.last_tick = identify.tick;
        if (identify.requests == identify.fail_request) {
            SDIO->RESP1 = 0xc0000000;
            SDIO->STA |= SDMMC_STA_CTIMEOUT;
            return osOK;
        }
        if ((u32)(identify.tick - identify.first_tick) >= identify.ready_after)
            SDIO->RESP1 = identify.legacy ? 0x80000000u : 0xc0000000u;
        return osOK;
    }
    assert(identify.requests && (identify.post_ready || (cmd_resp[0] & 0x80000000)));
    const u32 post_commands[] = {2, 3, 9, 7, 16, 55, 6, 13};
    const u32 post_arguments[] = {0, 0, 0x12340000, 0x12340000, 512, 0x12340000, 2, 0x12340000};
    assert(identify.post_ready < sizeof(post_commands) / sizeof(post_commands[0]));
    assert(command == post_commands[identify.post_ready] && argument == post_arguments[identify.post_ready]);
    identify.post_ready++;
    if (command == 3) SDIO->RESP1 = 0x12340000;
    if (command == 9) {
        u8 csd[16] = {0};
        csd[10] = 9; /* Read block length is 512 bytes. */
        u32 words[4];
        memcpy(words, csd, sizeof(words));
        SDIO->RESP4 = words[0];
        SDIO->RESP3 = words[1];
        SDIO->RESP2 = words[2];
        SDIO->RESP1 = words[3];
    }
    return osOK;
}

static osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore, u32 timeout)
{
    assert(semaphore == cmd_sem);
    if (!timeout) {
        assert(!SDIO->MASK);
        drains++;
        return semaphore ? osErrorResource : osErrorParameter;
    }
    assert(timeout == 100);
    waits++;
    if (identify.active) {
        osStatus_t result = identify_completion();
        SDIO->CMD &= ~SDMMC_CMD_CPSMEN;
        return result;
    }
    assert(SDIO->CMD == (0x1000u | (wanted_command & 0x03ff)));
    assert(SDIO->ARG == 0x12345678 && SDIO->MASK == wanted_mask);
    SDIO->CMD &= ~SDMMC_CMD_CPSMEN;
    SDIO->STA = completion_status;
    return semaphore ? wait_status : osErrorParameter;
}

#include "command.inc"
#include "identify.inc"

static void check_command(const char *mode, unsigned shape, int64_t value)
{
    const struct {
        u32 cmd, completion;
        unsigned words;
    } shapes[] = {
        {CMD_00, SDMMC_STA_CMDSENT, 0},
        {CMD_08, SDMMC_STA_CMDREND, 1},
        {CMD_02, SDMMC_STA_CMDREND, 4},
        {CMD_17, SDMMC_STA_DATAEND, 1},
        {CMD_41, SDMMC_STA_CMDREND, 1},
        {CMD_03, SDMMC_STA_CMDREND, 1},
        {CMD_07, SDMMC_STA_CMDREND, 1},
        {CMD_12, SDMMC_STA_CMDREND, 1},
        {CMD_13, SDMMC_STA_CMDREND, 1},
        {CMD_16, SDMMC_STA_CMDREND, 1},
        {CMD_18, SDMMC_STA_DATAEND, 1},
        {CMD_23, SDMMC_STA_CMDREND, 1},
        {CMD_55, SDMMC_STA_CMDREND, 1},
        {CMD_24, SDMMC_STA_DATAEND, 1},
        {CMD_25, SDMMC_STA_DATAEND, 1},
        {CMD_06, SDMMC_STA_CMDREND, 1}
    };
    const u32 unchanged[] = {0x42a10001, 0x42a10002, 0x42a10003, 0x42a10004};
    assert(shape < sizeof(shapes) / sizeof(shapes[0]));
    wanted_command = shapes[shape].cmd;
    completion_status = shapes[shape].completion;
    int data = HAS_DATA(wanted_command) != 0;
    wanted_mask = completion_status | CMD_ERROR | (data ? DATA_ERROR : 0);
    int failed_wait = !strcmp(mode, "wait") || !strcmp(mode, "null");
    int hardware_error = !strcmp(mode, "hardware");
    int card_error = !strcmp(mode, "card-error");
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
    SDIO->RESP1 = card_error || !strcmp(mode, "card-ok") ? (u32)value :
                  data ? 0x900 : 0x11223344;
    SDIO->RESP2 = 0x55667788;
    SDIO->RESP3 = 0x99aabbcc;
    SDIO->RESP4 = 0xddeeff00;

    u32 result = (u32)sd_cmd(wanted_command, 0x12345678);
    if (card_error && result != (u32)value) {
        fprintf(stderr, "Wrong card error result for CMD%u: %08x, expected %08x\n",
                wanted_command & 63, result, (u32)value);
        abort();
    }
    assert(waits == 1 && !SDIO->CMD && !SDIO->MASK && SDIO->ICR == 0x1fe00fff);
    int clear_data = data;
#ifdef TEST_SD_RECOVERY
    clear_data |= failed_wait || hardware_error || card_error;
    assert(drains == (unsigned)(failed_wait || hardware_error || card_error));
#else
    assert(!drains);
#endif
    assert(SDIO->DCTRL == (clear_data ? 0 : 0x92) && SDIO->IDMACTRL == (clear_data ? 0 : 1));
    if (failed_wait && result != UINT32_MAX) {
        fprintf(stderr, "Accepted failed command wait: shape=%u status=%d result=%u\n",
                shape, cmd_sem ? (int)value : osErrorParameter, result);
        abort();
    }
    if (failed_wait || hardware_error || card_error) {
        assert(result == (failed_wait ? UINT32_MAX : hardware_error ? completion_status : (u32)value));
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
    printf("command %s shape=%u passed\n", mode, shape);
}

static void check_identification(const char *mode, unsigned shape, int value)
{
    const u32 budget = 2u * OS_TICK_FREQ;
    const u32 initial_clock = SDCLK_LOW | (1u << 16);
    int expect_ready = 0;
    identify.active = 1;
    identify.ready_after = UINT32_MAX;
    SDIO->CLKCR = initial_clock;

    if (!strcmp(mode, "ready")) {
        const u32 times[] = {0, 1, OS_TICK_FREQ - 1, OS_TICK_FREQ, OS_TICK_FREQ + 1,
                             budget - 1, budget};
        assert(value >= 0 && (unsigned)value < sizeof(times) / sizeof(times[0]));
        identify.legacy = shape != 0;
        identify.ready_after = times[value];
        expect_ready = 1;
    } else if (!strcmp(mode, "not-ready") || !strcmp(mode, "wrap-ready")) {
        if (shape) identify.tick = UINT32_MAX - (30u + OS_TICK_FREQ / 2u);
        if (!strcmp(mode, "wrap-ready")) {
            assert(shape);
            identify.ready_after = budget;
            expect_ready = 1;
        }
    } else if (!strcmp(mode, "latency")) {
        assert(shape < 4);
        identify.prefix_ticks = OS_TICK_FREQ - 1;
        if (shape == 0) {
            identify.ready_after = budget - 1;
            expect_ready = 1;
        } else {
            identify.command_ticks = shape == 1 ? OS_TICK_FREQ * 13u / 100u : OS_TICK_FREQ - 1;
            if (shape == 3) {
                identify.ready_after = budget;
                expect_ready = 1;
            }
        }
    } else {
        assert(!strcmp(mode, "identify-error") && (value == 1 || value == 2));
        if (shape) identify.fail_request = (unsigned)value;
        else identify.fail_prefix = (unsigned)value;
    }

    int result = sdcard_identify();
    assert(!SDIO->CMD && !SDIO->MASK && SDIO->ICR == 0x1fe00fff);
    u32 elapsed = identify.last_tick - identify.first_tick;
    if (expect_ready) {
        assert(result == 0 && identify.post_ready == 8);
        assert(sd_rca == 0x1234 && is_sdhc == !identify.legacy);
#ifdef HAS_CSD_CAPACITY_PRINT
        assert(bsize == 512);
#endif
        assert((SDIO->CLKCR & 0x3ff) == SDCLK_HIGH && (SDIO->CLKCR & 0xc000) == 0x4000);
        if (!identify.command_ticks) {
            assert(identify.requests == identify.ready_after + 1 && elapsed == identify.ready_after);
        } else {
            assert(identify.requests == 2 && elapsed > budget);
        }
    } else {
        assert(result == -1 && !identify.post_ready && !sd_rca && !is_sdhc);
        assert(SDIO->CLKCR == initial_clock);
        if (identify.fail_prefix || identify.fail_request) {
            assert(identify.prefixes == (unsigned)value);
            assert(identify.requests == (unsigned)(value - !shape));
            assert(identify.retry_delays == (unsigned)value - 1);
        } else {
            assert(elapsed >= budget && elapsed <= budget + 2 * identify.command_ticks + 1);
            if (identify.command_ticks) assert(identify.requests >= 2 && identify.requests < 20);
            else assert(identify.requests == budget + 1 && elapsed == budget);
        }
    }
    if (!identify.fail_prefix && !identify.fail_request) {
        assert(identify.prefixes == identify.requests && identify.retry_delays == identify.requests - 1);
        if (!strcmp(mode, "latency")) assert(identify.first_tick == 30 + identify.prefix_ticks);
        if (shape && (!strcmp(mode, "not-ready") || !strcmp(mode, "wrap-ready")))
            assert(identify.last_tick < identify.first_tick);
    }
    printf("identify %s shape=%u value=%d requests=%u elapsed=%u passed\n",
           mode, shape, value, identify.requests, elapsed);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    const char *mode = argv[1];
    unsigned shape = (unsigned)strtoul(argv[2], NULL, 10);
    int64_t value = strtoll(argv[3], NULL, 10);
    if (!strcmp(mode, "wait") || !strcmp(mode, "null") || !strcmp(mode, "success") ||
        !strcmp(mode, "hardware") || !strcmp(mode, "no-crc") ||
        !strcmp(mode, "card-error") || !strcmp(mode, "card-ok"))
        check_command(mode, shape, value);
    else
        check_identification(mode, shape, value);
    return 0;
}
