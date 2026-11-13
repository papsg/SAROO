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
static struct { u32 CR1; } timer2, timer6;
static struct {
    u32 play_fad_start, play_fad_end, track, index, fad, max_repeat, repcnt;
    u32 options, pause_request, play_type, play_pos_types;
} cdb;
#define PLAYTYPE_SECTOR 7
#define TIM2 (&timer2)
#define TIM6 (&timer6)
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
static int list_bin_dir(const char *path)
{ assert(path == scratch + 16); event(56); return operation_result; }
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
static volatile u8 disc_removed __attribute__((unused));
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

static unsigned menu_ticks;
static u32 menu_now;
static void (*advance_menu_clock)(unsigned tick);
static u16 menu_ack, inherited_command, inherited_argument;
static int sel_mode, page;

static u32 get_timer(void)
{
    u32 now = menu_now;
    menu_now += 1000000u;
    ++menu_ticks;
    if (advance_menu_clock) advance_menu_clock(menu_ticks);
    return now;
}

#include "menu-start.inc"

static void call(unsigned opcode, unsigned arg)
{
    command_value = opcode; request_argument = argument = arg; count = 0; supplied = ~0u;
    buf_fad_start = buf_fad_end = buf_fad_size = 99;
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
        if ((base >= 1 && base <= 14) || opcode == 54 || opcode == 55 ||
            (HAS_BIN_DIRECTORY && opcode == 56)) {
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
            case 54: case 55: case 56: { unsigned e[] = {base}; sequence(e, 1); break; }
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
        u32 worker_state = 12u + (unsigned)(second_pcm == 0);
        timer2.CR1 = worker_state;
        timer6.CR1 = 0;
        call(13 | tag, 0);
        int music = first_pcm ? second_pcm : first_pcm;
        assert(argument == (u16)(tag ? listed | tag : music));
        assert(command_value == (tag ? 0 : 13));
        assert(timer2.CR1 == worker_state && !timer6.CR1);
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

static void bin_directory(void)
{
    strcpy(scratch + 16, "/SAROO/BIN/Work");
    for (operation_result = -19; operation_result <= 0; ++operation_result) {
        call(0x0038, 0);
        assert(!command_value && argument == (u16)(operation_result | SSCMD_SAVE_ACK));
        assert(!strcmp(scratch + 16, "/SAROO/BIN/Work"));
        SEQUENCE(56);
    }
    memset(sysinfo, 0xa5, sizeof(sysinfo));
#include "publish-bin.inc"
    const u8 expected[] = {1, 0, 0, 0};
    assert(!memcmp((u8 *)sysinfo + 0x34, expected, sizeof(expected)));
    for (unsigned i = 0; i < sizeof(sysinfo); i++)
        if (i < 0x34 || i >= 0x38) assert(((u8 *)sysinfo)[i] == 0xa5);
    checks++;
}

static void complete_command(u16 command, u16 arg)
{
    assert(command_value == command && argument == arg);
    request_argument = arg;
    count = 0;
    ss_cmd_handle();
    assert(!command_value);
}

static void prepare_menu(int tagged, u16 pending, u16 arg)
{
    memset(sysinfo, 0, sizeof(sysinfo));
    if (tagged) ((u8 *)sysinfo)[0x2c] = MENU_ACK_VERSION;
    command_value = inherited_command = pending;
    argument = inherited_argument = arg;
    memset(scratch, 0xa5, sizeof(scratch));
    count = menu_ticks = menu_ack = sel_mode = page = 0;
    menu_now = UINT32_MAX - 1500000u;
    first_pcm = second_pcm = -2;
    advance_menu_clock = NULL;
}

static void check_payload(void)
{
    for (size_t i = 0; i < sizeof(scratch); ++i)
        assert((u8)scratch[i] == 0xa5);
}

static void finish_inherited_then_startup(unsigned tick)
{
    if (tick == 3) {
        check_payload();
        complete_command(inherited_command, inherited_argument);
    } else if (tick == 6) {
        buf_fad_start = buf_fad_end = buf_fad_size = 99;
        complete_command(SSCMD_STARTUP | SSCMD_SAVE_ACK, 0);
        assert(count && events[count - 1] == 21);
    }
}

static void finish_cold_startup(unsigned tick)
{
    if (tick == 3) {
        complete_command(SSCMD_STARTUP | SSCMD_SAVE_ACK, 0);
        assert(count && events[count - 1] == 21);
    }
}

static void reset_drain(void)
{
    const u16 commands[] = {SSCMD_STARTUP | SSCMD_SAVE_ACK, SSCMD_SELECT | SSCMD_SAVE_ACK,
                            SSCMD_FILEWR | SSCMD_SAVE_ACK, SSCMD_FILEWR};
    for (unsigned i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        prepare_menu(1, commands[i], i == 0 ? 0 : 0x8002);
        advance_menu_clock = finish_inherited_then_startup;
        int result = start_menu_services();
        assert(!command_value && argument == SSCMD_SAVE_ACK);
        assert(!result && menu_ticks == 6);
        check_payload();
        checks++;
    }
    prepare_menu(1, 0, 0);
    advance_menu_clock = finish_cold_startup;
    assert(!start_menu_services() && !command_value && menu_ticks == 3);
    checks++;
}

static void reset_without_reply(unsigned tick)
{
    if (tick == 3) command_value = argument = 0;
}

static void reset_timeout(void)
{
    prepare_menu(1, SSCMD_FILEWR | SSCMD_SAVE_ACK, 0xf00d);
    int result = start_menu_services();
    assert(command_value == inherited_command && argument == inherited_argument);
    check_payload();
    assert(result == -15 && menu_ticks == 11);
    menu_ticks = 0;
    advance_menu_clock = finish_inherited_then_startup;
    assert(!start_menu_services() && menu_ticks == 6 && !command_value);

    prepare_menu(1, 0, 0);
    assert(start_menu_services() == -15 && menu_ticks == 11);
    assert(command_value == (SSCMD_STARTUP | SSCMD_SAVE_ACK) && !argument);
    prepare_menu(1, 0, 0);
    advance_menu_clock = reset_without_reply;
    assert(start_menu_services() == -1 && !command_value);
    checks += 3;
}

static void reset_legacy(void)
{
    for (unsigned command = 0; command <= 2; ++command) {
        prepare_menu(0, command == 0 ? 0 : SSCMD_STARTUP + command - 1, 0x4444);
        assert(!start_menu_services() && !menu_ticks && command_value == SSCMD_STARTUP);
        assert(!argument);
        ss_cmd_handle();
        assert(command_value == SSCMD_STARTUP && argument == (u16)-2);
        select_notify(SELECT_ENTER | 4);
        assert(!menu_ticks && command_value == SSCMD_SELECT && argument == (SELECT_ENTER | 4));
        ss_cmd_handle();
        assert(command_value == SSCMD_SELECT && argument == (SELECT_ENTER | 4));
        checks++;
    }
}

static void finish_preview(unsigned tick)
{
    if (tick == 3) {
        complete_command(SSCMD_SELECT | SSCMD_SAVE_ACK, SELECT_ENTER + 2 * MENU_ITEMS + 1);
        assert(events[count - 1] == 14);
    }
}

static void preview_drain(void)
{
    prepare_menu(1, SSCMD_SELECT | SSCMD_SAVE_ACK, SELECT_ENTER + 2 * MENU_ITEMS + 1);
    menu_ack = SSCMD_SAVE_ACK;
    page = 2;
    advance_menu_clock = finish_preview;
    select_notify(SELECT_EXIT);
    assert(menu_ticks == 3 && command_value == (SSCMD_SELECT | SSCMD_SAVE_ACK));
    assert(argument == SELECT_EXIT + 2 * MENU_ITEMS);

    menu_ticks = 0;
    advance_menu_clock = NULL;
    select_notify(0);
    assert(menu_ticks == 11 && argument == SELECT_EXIT + 2 * MENU_ITEMS);
    assert(command_value == (SSCMD_SELECT | SSCMD_SAVE_ACK));
    stm32_puts("next request");
    assert(menu_ticks == 22 && argument == SELECT_EXIT + 2 * MENU_ITEMS);
    assert(command_value == (SSCMD_SELECT | SSCMD_SAVE_ACK));
    check_payload();
    checks += 4;
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
    else if (!strcmp(argv[1], "bin-directory")) bin_directory();
    else if (!strcmp(argv[1], "reset-drain")) reset_drain();
    else if (!strcmp(argv[1], "reset-timeout")) reset_timeout();
    else if (!strcmp(argv[1], "reset-legacy")) reset_legacy();
    else if (!strcmp(argv[1], "preview-drain")) preview_drain();
    else { assert(!strcmp(argv[1], "results")); results(); }
    printf("PASS %s %u checks\n", argv[1], checks);
    return 0;
}
