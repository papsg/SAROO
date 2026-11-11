/* Adapted from paps-g b7720d00. Exercise the actual MCU command boundary. */
#include <stdint.h>
static unsigned owner_device, owner_calls;
static uint32_t owner_offset;
static void *owner_buffer;
static int owner_length, owner_result;
static __attribute__((unused)) int save_archive_io(unsigned device, uint32_t offset, void *buffer, int length)
{
    owner_calls++; owner_device = device; owner_offset = offset;
    owner_buffer = buffer; owner_length = length; return owner_result;
}
#define TEST_ARCHIVE_IO
#define main existing_file_main
#include "file_transfer.c"
#undef main
static u32 owner_sysinfo[1024];
#define SYSINFO_ADDR ((uintptr_t)owner_sysinfo)
static void publish_owner(void)
{
#include "archive-publish.inc"
}

static void request(unsigned writing, u32 offset, u32 size, u32 address, unsigned device)
{
    memset(test_shared + 0xc7000, 0xa5, 0x100);
    u32 *words = (u32 *)(test_shared + 0xc7000);
    words[0] = offset; words[1] = size; words[2] = address; words[3] = device;
    owner_calls = 0; SS_ARG = 0xa5c3;
    SS_CMD = writing ? SSCMD_ARCHIVEWR : SSCMD_ARCHIVERD;
    command();
    assert(!SS_CMD);
    /* Names are intentionally all non-NUL: owner requests cannot open paths. */
    for (unsigned i = 16; i < 256; i++) assert(test_shared[0xc7000 + i] == 0xa5);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_shared = malloc(0x1000000); assert(test_shared);
    u32 *words = (u32 *)(test_shared + 0xc7000);
    unsigned checks = 0;
    if (!strcmp(argv[1], "capability")) {
        memset(owner_sysinfo, 0xa5, sizeof owner_sysinfo); publish_owner();
        const u8 expected[] = {0x31, 0x57, 0x4f, 0x53};
        assert(owner_sysinfo[6] == ARCHIVE_IO_VERSION);
        assert(!memcmp((u8 *)owner_sysinfo + 24, expected, sizeof expected));
        for (unsigned i = 0; i < sizeof owner_sysinfo; i++)
            if (i < 24 || i >= 28) assert(((u8 *)owner_sysinfo)[i] == 0xa5);
        checks++;
    } else if (!strcmp(argv[1], "bounds")) {
        const struct { u32 size, address; } invalid[] = {
            {1, 0x220c7000}, {1, 0x220c70ff}, {2, 0x220c6fff},
            {2, 0x22ffffff}, {65537, 0x220c7100}, {0x80000000u, 0x220c7100},
            {0xffffffffu, 0x220c7100}, {0x7fffffffu, 0x220c7100}
        };
        for (unsigned writing = 0; writing < 2; writing++)
            for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; i++) {
                request(writing, 65536, invalid[i].size, invalid[i].address, 0);
                assert(!owner_calls && !words[1] && SS_ARG == (uint16_t)-FR_INVALID_PARAMETER);
                checks++;
            }
    } else if (!strcmp(argv[1], "transfer")) {
        const struct { u32 size, address; } valid[] = {
            {65536, 0x220c7100}, {65536, 0x22ff0000}, {1, 0x22ffffff},
            {65536, 0x220b7000}, {1, 0x220c6fff}, {1, 0x620c7100}
        };
        for (unsigned device = 0; device < 2; device++)
            for (unsigned writing = 0; writing < 2; writing++)
                for (unsigned i = 0; i < sizeof valid / sizeof *valid; i++) {
                    request(writing, 65537, valid[i].size, valid[i].address, device);
                    assert(owner_calls == 1 && owner_device == device && owner_offset == 65537);
#ifdef HAS_MAPPED_FILE_BUFFER
                    assert((uintptr_t)owner_buffer == (0x02000000u | (valid[i].address & 0xffffffu)));
#else
                    assert(owner_buffer == test_shared + (valid[i].address & 0xffffffu));
#endif
                    assert(owner_length == (writing ? -(int)valid[i].size : (int)valid[i].size));
                    assert(words[1] == valid[i].size && SS_ARG == SSCMD_SAVE_ACK); checks++;
                }
    } else if (!strcmp(argv[1], "sync")) {
        for (unsigned device = 0; device < 2; device++) {
            request(0, 0, 0, 0x220c7100, device);
            assert(!owner_calls && !words[1] && SS_ARG == (uint16_t)-FR_INVALID_PARAMETER);
            request(1, 0, 0, 0x220c7000, device);
            assert(owner_calls == 1 && owner_device == device && !owner_offset);
            assert(!owner_buffer && !owner_length && !words[1] && SS_ARG == SSCMD_SAVE_ACK);
            checks += 2;
        }
    } else {
        assert(!strcmp(argv[1], "errors"));
        for (unsigned writing = 0; writing < 2; writing++)
            for (owner_result = 0; owner_result <= FR_INVALID_PARAMETER; owner_result++) {
                request(writing, 65536, 123, 0x220c7100, 1);
                assert(owner_calls == 1);
                assert(words[1] == (owner_result ? 0u : 123u));
                assert(SS_ARG == (uint16_t)(owner_result ? -owner_result : SSCMD_SAVE_ACK));
                checks++;
            }
    }
    assert(!opens && !seeks && !reads && !writes && !closes);
    free(test_shared); printf("PASS %u archive protocol checks\n", checks);
}
