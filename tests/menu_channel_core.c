/* Model only the peer's command completion and the target's atomic primitive. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "menu-channel-protocol.h"

static volatile u16 wire_command, wire_argument;
static u16 reply, last_command, last_argument;
static unsigned auto_complete, completions, purges, timer_reads;
static u32 timer_ticks;
static u8 request[64];
static volatile u16 *wire_register(void)
{
    if (wire_command && auto_complete) {
        last_command = wire_command;
        last_argument = wire_argument;
        completions++;
        wire_argument = reply;
        wire_command = 0;
    }
    return &wire_command;
}
#define SS_CMD (*wire_register())
#define SS_ARG wire_argument
#define TMPBUFF_ADDR ((uintptr_t)request)
#define COMMAND_GATE_ADDRESS (&command_lock)
#define COMMAND_READY_ADDRESS (&command_ready)
#define COMMAND_GATE_LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define COMMAND_GATE_RELEASE(p) __atomic_store_n((p), 0, __ATOMIC_RELEASE)
#define COMMAND_GATE_POISON(p) ((void)__atomic_fetch_or((p), 1, __ATOMIC_RELEASE))
#define COMMAND_GATE_READY(p) __atomic_store_n((p), 1, __ATOMIC_RELEASE)
static int command_try_lock(volatile u8 *p)
{ return __atomic_fetch_or(p, 0x80, __ATOMIC_ACQUIRE) == 0; }
static void file_cache_purge(u32 address, u32 size, int mirrored)
{
    assert(address == 0x02000000u && size == 0x00100000u && !mirrored);
    purges++;
}
#include "command_gate.c"

#define MENU_CHANNEL_ADDRESS (&menu_channel)
#define MS2TICK(ms) ((u32)(ms) * 1000u)
static u32 get_timer(void)
{
    timer_reads++;
    timer_ticks += 1000000u;
    return timer_ticks;
}
static int timer_reached(u32 now, u32 deadline)
{ return (int32_t)(now - deadline) >= 0; }
#include "menu_channel.c"

static void reset(unsigned version)
{
    command_lock = command_ready = wire_command = wire_argument = 0;
    memset(&menu_channel, 0, sizeof menu_channel);
    memset(request, 0xa5, sizeof request);
    reply = SSCMD_SAVE_ACK;
    last_command = last_argument = 0;
    auto_complete = completions = purges = timer_reads = timer_ticks = 0;
    menu_channel_init(version);
}

static void capability_and_ownership(void)
{
    const unsigned invalid[] = {0, 2, UINT32_MAX};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        reset(invalid[i]);
        assert(menu_channel_poll() == -12);
        assert(menu_select_notify(5) == -12);
        assert(menu_request(SSCMD_STARTUP, 0) == -12);
        assert(menu_load_save((const u8 *)"0123456789abcdef") == -12);
        assert(!wire_command && !command_lock && !completions);
        for (unsigned j = 0; j < sizeof request; j++) assert(request[j] == 0xa5);
    }

    reset(1);
    assert(command_enter());
    assert(!menu_select_notify(SELECT_ENTER | 5));
    assert(menu_channel_poll() == -16);
    assert(menu_request(SSCMD_LISTDISC, 0) == -16);
    assert(menu_channel.flags == (MENU_ENABLED | MENU_QUEUED));
    assert(!wire_command && !completions && command_leave());
    assert(menu_channel_poll() == 1);
    assert(wire_command == (SSCMD_SELECT | SSCMD_SAVE_ACK));
    assert(wire_argument == (SELECT_ENTER | 5));
    assert(command_lock == 0x80);
    assert(!menu_select_notify(9));
    wire_command = 0;
    wire_argument = SSCMD_SAVE_ACK;
    assert(menu_channel_poll() == 1);
    assert(wire_command == (SSCMD_SELECT | SSCMD_SAVE_ACK));
    assert(wire_argument == 9 && command_lock == 0x80);
    wire_command = 0;
    wire_argument = SSCMD_SAVE_ACK;
    assert(!menu_channel_poll() && !command_lock && menu_channel.flags == MENU_ENABLED);

    reset(1);
    assert(command_enter());
    command_fault();
    assert(!command_leave());
    assert(menu_select_notify(4) == -2 && menu_request(SSCMD_CHECK, 0) == -2);
    assert(menu_channel_poll() == -2 && (menu_channel.flags & MENU_FAULT));
    assert(command_lock == 0x81 && !wire_command);
    puts("PASS capability/owners");
}

static void reply_domains(void)
{
    const int commands[] = {SSCMD_CHECK, SSCMD_UPDATE, SSCMD_LISTDISC,
                            SSCMD_STARTUP, SSCMD_LOADDISC};
    unsigned cases = 0;
    for (unsigned raw = 0; raw <= UINT16_MAX; raw++) {
        reset(1);
        assert(!menu_select_notify(3));
        assert(command_lock == 0x80);
        wire_command = 0;
        wire_argument = raw;
        int valid = raw == SSCMD_SAVE_ACK;
        assert(menu_channel_poll() == (valid ? 0 : -2));
        assert(!!command_faulted() == !valid);
        assert(command_lock == (valid ? 0 : 0x81));
        assert(!!(menu_channel.flags & MENU_FAULT) == !valid);
        cases++;
    }
    for (unsigned c = 0; c < sizeof commands / sizeof commands[0]; c++) {
        int max = c < 2 ? 3 : 19;
        for (unsigned raw = 0; raw <= UINT16_MAX; raw++) {
            reset(1);
            auto_complete = 1;
            reply = raw;
            int result = menu_request(commands[c], 7);
            int signed_reply = raw >= 32768 ? (int)raw - 65536 : (int)raw;
            int valid = raw >= 32768 ? signed_reply >= -19 :
                raw >= SSCMD_SAVE_ACK && raw <= SSCMD_SAVE_ACK + (unsigned)max;
            int expected = raw >= 32768 ? signed_reply :
                (int)raw - SSCMD_SAVE_ACK;
            assert(result == (valid ? expected : -2));
            assert(!!command_faulted() == !valid);
            assert(command_lock == (valid ? 0 : 0x81));
            assert(completions == 1 && last_command == (commands[c] | SSCMD_SAVE_ACK));
            assert(last_argument == 7 && !wire_command);
            cases++;
        }
    }
    assert(cases == 6u * 65536u);
    printf("PASS reply cases=%u\n", cases);
}

static void key_and_drain(void)
{
    const u8 *key = (const u8 *)"0123456789abcdef";
    reset(1);
    assert(command_enter());
    assert(menu_load_save(key) == -16);
    for (unsigned j = 0; j < sizeof request; j++) assert(request[j] == 0xa5);
    assert(command_leave());
    auto_complete = 1;
    assert(!menu_load_save(key));
    assert(completions == 1 && last_command == (SSCMD_LSAVE | SSCMD_SAVE_ACK));
    assert(!last_argument && !memcmp(request + 0x10, key, 16));
    assert(!request[0x20] && !command_lock && !purges);
    assert(menu_load_save(NULL) == -19);

    reset(1);
    assert(!menu_select_notify(3));
    assert(wire_command == (SSCMD_SELECT | SSCMD_SAVE_ACK));
    assert(menu_request(SSCMD_LISTDISC, 4) == -3);
    assert(timer_reads && !completions && command_lock == 0x80);
    assert(wire_command == (SSCMD_SELECT | SSCMD_SAVE_ACK));
    assert(menu_channel.flags == (MENU_ENABLED | MENU_ACTIVE));
    wire_command = 0;
    wire_argument = SSCMD_SAVE_ACK;
    assert(!menu_channel_poll() && !command_lock);
    auto_complete = 1;
    assert(!menu_request(SSCMD_LISTDISC, 4));
    assert(completions == 1 && last_argument == 4);
    puts("PASS key/drain");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (argv[1][0] == 'c') capability_and_ownership();
    else if (argv[1][0] == 'r') reply_domains();
    else if (argv[1][0] == 'k') key_and_drain();
    else abort();
    return 0;
}
