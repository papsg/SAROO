/* Adapted from paps-g 36c4de4b. Exercise the actual menu command handler. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "protocol.h"

static u16 command_value, argument, request_argument;
static u32 sysinfo[1024];
static char scratch[256];
static unsigned events[16], count, checks, supplied;
static int operation_result, listed, first_pcm, second_pcm, pause_result;
static int fpga_result, flash_result;
static u32 buf_fad_start, buf_fad_end, buf_fad_size;
static struct { u32 CR1; } timer;
static struct {
    u32 play_fad_start, play_fad_end, track, index, fad, max_repeat, repcnt;
    u32 options, pause_request, play_type, play_pos_types;
} cdb;
#define PLAYTYPE_SECTOR 7
#define TIM6 (&timer)
#define SS_CMD command_value
#define SS_ARG argument
#define SYSINFO_ADDR ((uintptr_t)sysinfo)
#define TMPBUFF_ADDR ((uintptr_t)scratch)
#define SSLOG(...) ((void)0)

static void event(unsigned id)
{
    /* Every service must retire before completion frees the shared request. */
    assert(command_value && count < sizeof(events) / sizeof(events[0]));
    if (command_value & SSCMD_SAVE_ACK) assert(argument == request_argument);
    events[count++] = id;
}
static int printk(const char *format, ...)
{ (void)format; event(1); return 0; }
static int list_bins(int show)
{ assert(show == 1); event(8); return operation_result; }
static int list_disc(int category, int show)
{
    assert(!show); supplied = category; event(2);
    if ((command_value & ~SSCMD_SAVE_ACK) == SSCMD_STARTUP) {
        assert(!buf_fad_start && !buf_fad_end && !buf_fad_size && !category);
        return listed;
    }
    return operation_result;
}
static void gif_decode_exit(void) { event(20); }
static void gif_decode_init(void) { event(21); }
static int pause_disc(void) { event(22); return pause_result; }
static int unload_disc(void) { event(23); return operation_result; }
static int load_disc(int index, int start_save)
{ assert(start_save == 1); supplied = index; event(3); return operation_result; }
static int fpga_update(int check)
{ assert(check == ((command_value & ~SSCMD_SAVE_ACK) == SSCMD_CHECK)); event(24); return fpga_result; }
static int flash_update(int check)
{ assert(check == ((command_value & ~SSCMD_SAVE_ACK) == SSCMD_CHECK)); event(25); return flash_result; }
static int ss_file_transfer(int mode)
{ supplied = mode; event(mode < 2 ? 6 + mode : 54 + mode - 2); return operation_result; }
static int flush_savefile(void) { event(9); return operation_result; }
static int load_savefile(char *name)
{ assert(name == scratch + 16); event(10); return operation_result; }
static int load_smems(int index) { supplied = index; event(11); return operation_result; }
static int flush_smems(int flags) { supplied = flags; event(12); return operation_result; }
static void cover_init(void) { event(26); }
static int load_pcm(char *name)
{
    if (!strcmp(name, "/SAROO/bgsound.pcm")) { event(27); return first_pcm; }
    assert(!strcmp(name, "/SAROO/bgsound_r.pcm")); event(28); return second_pcm;
}
static u32 track_to_fad(int track)
{ event(29); assert(track == 0x100 || track == 0x163); return track + 150; }
static void disk_task_wakeup(void) { event(30); }
static void load_cover(int index) { supplied = index; event(14); }
#include "command.inc"

static void call(unsigned opcode, unsigned arg)
{
    command_value = opcode; request_argument = argument = arg; count = 0; supplied = ~0u;
    buf_fad_start = buf_fad_end = buf_fad_size = 99;
    timer.CR1 = 0;
    memset(&cdb, 0xa5, sizeof(cdb));
    ss_cmd_handle();
}
static void sequence(const unsigned *expected, unsigned n)
{ assert(n == count && !memcmp(events, expected, n * sizeof(*events))); checks++; }
#define SEQUENCE(...) do { const unsigned expected[] = {__VA_ARGS__}; \
    sequence(expected, sizeof(expected) / sizeof(*expected)); } while (0)

static unsigned normalized(unsigned opcode)
{
    /* Explicit independent wire assignments; no copy of the dispatch range. */
    switch (opcode) {
    case 0x21: return 1; case 0x22: return 2; case 0x23: return 3;
    case 0x24: return 4; case 0x25: return 5; case 0x26: return 6;
    case 0x27: return 7; case 0x28: return 8; case 0x29: return 9;
    case 0x2a: return 10; case 0x2b: return 11; case 0x2c: return 12;
    case 0x2d: return 13; case 0x2e: return 14;
    default: return opcode;
    }
}
static void dispatch(void)
{
    first_pcm = second_pcm = -2;
    fpga_result = flash_result = 1; /* CHECK => 0; UPDATE => 0. */
    for (unsigned opcode = 0; opcode <= UINT16_MAX; opcode++) {
        unsigned base = normalized(opcode), tag = opcode & 32;
        call(opcode, 0x1234);
        if ((base >= 1 && base <= 14) || opcode == 54 || opcode == 55) {
            if ((base == 13 || base == 14) && !tag) assert(command_value == opcode);
            else assert(!command_value);
            u16 expected = tag;
            if (base == 1 && !tag) expected = 0x1234;
            if (base == 13 && !tag) expected = (u16)-2;
            if (base == 14 && !tag) expected = 0x1234;
            assert(argument == expected);
            switch (base) {
            case 1: SEQUENCE(1); break;
            case 2: SEQUENCE(2); assert(supplied == 0x1234); break;
            case 3: SEQUENCE(20, 3); assert(supplied == 0x1234); break;
            case 4: case 5: SEQUENCE(24, 25); break;
            case 6: case 7: case 8: case 9: case 10: case 11: case 12:
            case 54: case 55: { unsigned e[] = {base}; sequence(e, 1); break; }
            case 13: SEQUENCE(2, 26, 27, 28, 21); break;
            case 14: SEQUENCE(1, 14); assert(supplied == 0x1234); break;
            }
        } else assert(!count && command_value == opcode && argument == 0x1234);
        checks++;
    }
}
static void startup(void)
{
    for (unsigned tag = 0; tag <= 32; tag += 32)
    for (listed = -19; listed <= 0; listed++)
    for (first_pcm = -2; first_pcm <= 0; first_pcm++)
    for (second_pcm = -2; second_pcm <= 0; second_pcm++) {
        call(13 | tag, 0);
        int music = first_pcm ? second_pcm : first_pcm;
        assert(argument == (u16)(tag ? listed | tag : music));
        assert(command_value == (tag ? 0 : 13) && timer.CR1 == 5);
        assert(events[count - 1] == 21);
        if (!music) {
            assert(cdb.play_fad_start == 0x100 + 150 && cdb.play_fad_end == 0x163 + 151);
            assert(cdb.track == 1 && !cdb.index && cdb.fad == cdb.play_fad_start);
            assert(cdb.max_repeat == (first_pcm ? 15u : 0u));
            assert(!cdb.repcnt && !cdb.options && !cdb.pause_request && !cdb.play_pos_types);
            assert(cdb.play_type == PLAYTYPE_SECTOR);
            assert(events[count - 2] == 30);
        } else assert(cdb.play_type == 0xa5a5a5a5);
        checks++;
    }
}
static void selection(void)
{
    for (unsigned tag = 0; tag <= 32; tag += 32)
    for (unsigned index = 0; index <= UINT16_MAX; index++) {
        call(14 | tag, index);
        assert(command_value == (tag ? 0 : 14) && argument == (tag ? tag : index));
        if (index & 0x4000) {
            if (index & 0x8000) { SEQUENCE(1, 20, 21); }
            else { SEQUENCE(1, 21); }
            assert(supplied == ~0u);
        } else {
            if (index & 0x8000) { SEQUENCE(1, 20, 14); }
            else { SEQUENCE(1, 14); }
            assert(supplied == index);
        }
    }
}
static void results(void)
{
    for (unsigned tag = 0; tag <= 32; tag += 32)
    for (operation_result = -19; operation_result <= 19; operation_result++) {
        for (unsigned opcode = 2; opcode <= 12; opcode++) if (opcode != 4 && opcode != 5) {
            call(opcode | tag, 0xabcd);
            int result = opcode == 6 || opcode == 7 ? -operation_result : operation_result;
            assert(!command_value && argument == (u16)(result | tag)); checks++;
        }
        for (pause_result = -1; pause_result <= 0; pause_result++) {
            call(3 | tag, 0xffff);
            if (pause_result) { SEQUENCE(20, 22); }
            else { SEQUENCE(20, 22, 23); }
            int result = pause_result ? pause_result : operation_result;
            assert(!command_value && argument == (u16)(result | tag)); checks++;
        }
        for (fpga_result = -2; fpga_result <= 1; fpga_result++)
        for (flash_result = -2; flash_result <= 1; flash_result++)
        for (unsigned opcode = 4; opcode <= 5; opcode++) {
            call(opcode | tag, 0);
            int expected = opcode == 4 ? ((!fpga_result) << 1) | !flash_result :
                ((fpga_result < -1) << 1) | (flash_result < -1);
            assert(!command_value && argument == (expected | tag)); SEQUENCE(24, 25);
        }
    }
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "capability")) {
        memset(sysinfo, 0xa5, sizeof(sysinfo));
#include "publish.inc"
        const u8 expected[] = {1, 0, 0, 0};
        assert(!memcmp((u8 *)sysinfo + 0x2c, expected, sizeof(expected)));
        for (unsigned i = 0; i < sizeof(sysinfo); i++)
            if (i < 0x2c || i >= 0x30) assert(((u8 *)sysinfo)[i] == 0xa5);
        checks++;
    } else if (!strcmp(argv[1], "dispatch")) dispatch();
    else if (!strcmp(argv[1], "startup")) startup();
    else if (!strcmp(argv[1], "selection")) selection();
    else { assert(!strcmp(argv[1], "results")); results(); }
    printf("PASS %s %u checks\n", argv[1], checks);
    return 0;
}
