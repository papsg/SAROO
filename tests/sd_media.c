/* Adapted from paps-g 613efd17. Actual identify and sector admission at a modeled SD boundary. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RTX_Config.h"
#include "sd_model.h"

typedef uint32_t u32;
typedef uint8_t u8;
#include "state.inc"
#define __DMB() ((void)0)

static u8 inserted_cid[16];
static unsigned new_rca = 0x1234, sequence[32], issued, selected, width, io_calls;
static unsigned fail_command = 99, fail_ordinal, command_ordinal[64];
static u32 tick;
static int check_during_identification;
static void unavailable(void);
static void printk(const char *format, ...) { (void)format; }
uint32_t osKernelGetTickCount(void) { return tick; }
osStatus_t osDelay(uint32_t delay) { tick += delay; return osOK; }
static void sd_clk_set(int div) { assert(div == SDCLK_HIGH); selected++; }
static void sd_bus_width(int bits) { assert(bits == 4); width++; }

static int sd_cmd(u32 command, u32 argument)
{
    if (check_during_identification) unavailable();
    unsigned cmd = command & 63;
    assert(issued < sizeof(sequence) / sizeof(sequence[0]));
    sequence[issued++] = cmd;
    unsigned ordinal = ++command_ordinal[cmd];
    if (cmd == fail_command && ordinal == fail_ordinal) return -17;
    switch (cmd) {
    case 0: assert(!argument); break;
    case 8: assert(argument == 0x1aa); cmd_resp[0] = 0x1aa; break;
    case 55: assert(argument == (command_ordinal[3] ? new_rca << 16 : 0)); break;
    case 41: assert(argument == 0x40f00000); cmd_resp[0] = 0xc0000000; break;
    case 2: assert(!argument); memcpy(cmd_resp, inserted_cid, 16); break;
    case 3: assert(!argument); cmd_resp[0] = new_rca << 16; break;
    case 9:
        assert(argument == new_rca << 16);
        memset(cmd_resp, 0, sizeof(cmd_resp)); ((u8 *)cmd_resp)[10] = 9;
        break;
    case 7: case 13: assert(argument == new_rca << 16); break;
    case 16: assert(argument == 512); break;
    case 6: assert(argument == 2); break;
    default: assert(!"unexpected identification command");
    }
    return 0;
}

enum { TEST_SECTORS = 17, SECTOR_BYTES = 512, FIRST_SECTOR = 123 };
static u8 sector_data[TEST_SECTORS * SECTOR_BYTES];
static u8 card_data[sizeof sector_data];
static u8 bounce_data[8 * SECTOR_BYTES];
static unsigned model_bank, model_write, model_fail, model_chunk;

static u32 sd_address(u8 *buffer)
{
    if (buffer == sector_data)
        return model_bank << 24;
    return (u32)(uintptr_t)buffer;
}

static int transfer_blocks(u32 block, int count, u8 *buffer, unsigned write)
{
    io_calls++;
    if (!model_bank) {
        assert(block == FIRST_SECTOR && count == 1 && buffer == (u8 *)0x24002000);
        return 0;
    }

    assert(write == model_write);
    unsigned chunk = model_chunk++;
    if (model_bank == 0x20) {
        assert(chunk < 3 && block == FIRST_SECTOR + chunk * 8);
        assert(count == (chunk < 2 ? 8 : 1) && buffer == bounce_data);
    } else {
        assert(chunk == 0 && block == FIRST_SECTOR && count == TEST_SECTORS);
        assert(buffer == sector_data);
    }
    if (model_fail == chunk + 1)
        return -17;

    u8 *card = card_data + (block - FIRST_SECTOR) * SECTOR_BYTES;
    if (write)
        memcpy(card, buffer, (size_t)count * SECTOR_BYTES);
    else
        memcpy(buffer, card, (size_t)count * SECTOR_BYTES);
    return 0;
}

#ifdef HAS_SHARED_SECTOR_WRAPPER
static int sd_blocks(u32 block, int count, u8 *buffer, u32 command)
{
    assert(command == (CMD_17 & 0x3f) || command == (CMD_24 & 0x3f));
    return transfer_blocks(block, count, buffer, command == (CMD_24 & 0x3f));
}
#else
static int sd_read_blocks(u32 block, int count, u8 *buffer)
{
    return transfer_blocks(block, count, buffer, 0);
}

static int sd_write_blocks(u32 block, int count, u8 *buffer)
{
    return transfer_blocks(block, count, buffer, 1);
}
#endif
#include "identify.inc"

static void attempt(void)
{
    /* sdio_reset clears RCA before identification; model that real boundary. */
    sd_rca = 0;
    issued = selected = width = 0;
    memset(command_ordinal, 0, sizeof(command_ordinal));
}
static void usable(void)
{
    attempt(); assert(!sdcard_identify());
    assert((unsigned)sd_rca == new_rca && selected == 1 && width == 1);
    assert(!memcmp(sd_cid, inserted_cid, 16));
    unsigned before = io_calls;
    assert(!sd_read_sector(123, 1, (u8 *)0x24002000));
    assert(!sd_write_sector(123, 1, (u8 *)0x24002000));
    assert(io_calls == before + 2);
}
static void unavailable(void)
{
    assert(!sd_rca);
    unsigned before = io_calls;
    assert(sd_read_sector(123, 1, (u8 *)0x24002000));
    assert(sd_write_sector(123, 1, (u8 *)0x24002000));
    assert(io_calls == before);
}

static void check_sector_transfers(void)
{
    const unsigned banks[] = {0x20, 0x24, 0x61};
    unsigned cases = 0;
    for (unsigned i = 0; i < sizeof(banks) / sizeof(*banks); ++i) {
        model_bank = banks[i];
        const unsigned chunks = i ? 1 : 3;
        for (model_write = 0; model_write <= 1; ++model_write) {
            u8 *source = model_write ? sector_data : card_data;
            u8 *dest = model_write ? card_data : sector_data;
            for (model_fail = 0; model_fail <= chunks; ++model_fail) {
                for (size_t j = 0; j < sizeof sector_data; ++j)
                    source[j] = (u8)(j * 19 + 3);
                memset(dest, 0x55, sizeof sector_data);
                memset(bounce_data, 0xa2, sizeof bounce_data);
                sdbuf = bounce_data;
                sd_rca = 0x1234;
                model_chunk = 0;
                int result = model_write ? sd_write_sector(FIRST_SECTOR, TEST_SECTORS, sector_data)
                                         : sd_read_sector(FIRST_SECTOR, TEST_SECTORS, sector_data);
                assert(result == (model_fail ? -17 : 0));
                assert(model_chunk == (model_fail ? model_fail : chunks));
                size_t completed = model_fail ? (model_fail - 1) * sizeof bounce_data
                                              : sizeof sector_data;
                for (size_t j = 0; j < sizeof sector_data; ++j) {
                    assert(source[j] == (u8)(j * 19 + 3));
                    assert(dest[j] == (j < completed ? source[j] : 0x55));
                }
                cases++;
            }
        }
    }
    assert(cases == 16);
    puts("16 direct and bounced sector transfer cases pass");
}

int main(int argc, char **argv)
{
    assert(argc == 3); const char *mode = argv[1]; unsigned value = strtoul(argv[2], NULL, 0);
    for (unsigned i = 0; i < 16; i++) inserted_cid[i] = 0x80 + i;
    if (!strcmp(mode, "sectors")) {
        check_sector_transfers();
        return 0;
    }
    if (!strcmp(mode, "zero-cid")) {
        memset(inserted_cid, 0, sizeof(inserted_cid)); usable();
        inserted_cid[0] = 1; attempt(); assert(sdcard_identify()); unavailable();
        return 0;
    }
    if (!strcmp(mode, "during")) {
        usable();
        attempt();
        if (value) sd_rca=0x1234;
        check_during_identification=1;
        assert(sdcard_identify()==0);
        assert((u32)sd_rca==new_rca && selected==1 && width==1);
        assert(sd_read_sector(123,1,(u8 *)0x24002000)==0);
        assert(sd_write_sector(123,1,(u8 *)0x24002000)==0);
        return 0;
    }
    if (!strcmp(mode, "first-failure")) {
        fail_command = value; fail_ordinal = 1; attempt();
        assert(sdcard_identify()); unavailable();
        inserted_cid[0] ^= 1; fail_command = 99; usable();
    } else {
        usable();
        if (!strcmp(mode, "same")) {
            new_rca = value; usable();
        } else if (!strcmp(mode, "changed")) {
            u8 original[16]; memcpy(original, inserted_cid, 16);
            inserted_cid[value] ^= 1; attempt();
            assert(sdcard_identify()); unavailable();
            assert(!command_ordinal[3] && !selected && !width);
            assert(!memcmp(sd_cid, original, 16));
            memcpy(inserted_cid, original, 16); new_rca = 0x4321; usable();
        } else if (!strcmp(mode, "failure")) {
            fail_command = value; fail_ordinal = value == 0 || value == 55 ? 2 : 1;
            attempt(); assert(sdcard_identify()); unavailable();
            assert(sequence[issued - 1] == value);
            fail_command = 99;
            inserted_cid[0] ^= 1; attempt(); assert(sdcard_identify()); unavailable();
            inserted_cid[0] ^= 1; usable();
        } else if (!strcmp(mode, "zero")) {
            new_rca = 0; attempt(); assert(sdcard_identify()); unavailable();
            assert(!command_ordinal[9]);
        } else assert(!"unknown scenario");
    }
    printf("SD media passes: %s value=%u commands=%u io=%u\n", mode, value, issued, io_calls);
    return 0;
}
