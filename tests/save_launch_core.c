/* Selected launch state machine with a modeled owner, command peer and key. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "menu-channel-protocol.h"
#define BUP_BROKEN 8
#define SJ_WRITE_BYTES 65536u
enum { JOURNAL_OFF, JOURNAL_RECOVER, JOURNAL_READY, JOURNAL_LEGACY };

static unsigned journal_state, journal_slot;
static u8 journal_key[16], journal_work[SJ_WRITE_BYTES];
#define JOURNAL_WORK journal_work
static int save_ack, file_ack, archive_ack;
static int admission_error, pending_error, lookup_result, startup_error;
static unsigned admission_calls, pending_calls[2], lookups, startups, purges;
static unsigned clobber_request, publish_key;
static u8 next_key[16], looked_up_key[16], sent_key[16];
static u8 sysinfo[128], request[64];
#define SYSINFO_ADDR ((uintptr_t)sysinfo)
#define TMPBUFF_ADDR ((uintptr_t)request)

static volatile u16 wire_command, wire_argument;
static u16 reply, last_command, last_argument;
static unsigned auto_complete, completions;
static volatile u16 *wire_register(void)
{
    if (wire_command && auto_complete) {
        last_command = wire_command;
        last_argument = wire_argument;
        if ((wire_command & ~SSCMD_SAVE_ACK) == SSCMD_LSAVE)
            memcpy(sent_key, request + 0x10, 16);
        if (publish_key && ((wire_command & ~SSCMD_SAVE_ACK) == SSCMD_LOADDISC ||
                            (wire_command & ~SSCMD_SAVE_ACK) == SSCMD_LSAVE))
            memcpy(sysinfo + 0x1c, next_key, 16);
        completions++;
        wire_argument = reply;
        wire_command = 0;
    }
    return &wire_command;
}
#define SS_CMD (*wire_register())
#define SS_ARG wire_argument
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

static int journal_admit(int device)
{
    assert(device == 1 && command_lock == 0x80);
    admission_calls++;
    if (clobber_request) memset(request + 0x10, 0xcc, 16);
    return admission_error ? BUP_BROKEN : 0;
}
static int finish_pending(int device)
{
    assert((unsigned)device < 2 && command_lock == 0x80);
    pending_calls[device]++;
    return pending_error == device + 1 ? BUP_BROKEN : 0;
}
static int archive_find_slot(const u8 *key, void *workspace, u32 capacity)
{
    assert(command_lock == 0x80 && workspace == journal_work && capacity == SJ_WRITE_BYTES);
    lookups++;
    memcpy(looked_up_key, key, 16);
    return lookup_result;
}
int journal_start_owned(void)
{
    assert(command_lock == 0x80);
    startups++;
    if (startup_error) return BUP_BROKEN;
    journal_state = JOURNAL_READY;
    journal_slot = 0;
    memset(journal_key, 0, sizeof journal_key);
    return 0;
}
#include "save_launch.c"

#define MENU_CHANNEL_ADDRESS (&menu_channel)
#define MS2TICK(ms) ((u32)(ms) * 1000u)
static u32 get_timer(void) { return 0; }
static int timer_reached(u32 now, u32 deadline) { return now >= deadline; }
#define SAROO_MENU_SAVE_LAUNCH
#include "menu_channel.c"

static void reset(void)
{
    command_lock = command_ready = wire_command = wire_argument = 0;
    memset(&menu_channel, 0, sizeof menu_channel);
    memset(request, 0xa5, sizeof request);
    memset(sysinfo, 0xa5, sizeof sysinfo);
    memset(journal_work, 0, sizeof journal_work);
    memset(next_key, 0, sizeof next_key);
    memset(looked_up_key, 0, sizeof looked_up_key);
    memset(sent_key, 0, sizeof sent_key);
    memcpy(journal_key, "OLD-GAME-KEY-000", 16);
    journal_state = JOURNAL_READY;
    journal_slot = 0x20000;
    save_ack = file_ack = archive_ack = SSCMD_SAVE_ACK;
    admission_error = pending_error = startup_error = 0;
    lookup_result = 0x30000;
    admission_calls = pending_calls[0] = pending_calls[1] = 0;
    lookups = startups = purges = clobber_request = publish_key = 0;
    reply = SSCMD_SAVE_ACK;
    last_command = last_argument = completions = 0;
    auto_complete = 1;
    menu_channel_init(1);
}

static void normal_launches(void)
{
    reset();
    assert(!menu_request(SSCMD_STARTUP, 0));
    assert(startups == 1 && purges == 1 && completions == 1);
    assert(journal_state == JOURNAL_READY && !journal_slot && !command_lock);

    reset();
    memcpy(next_key, "NEW-GAME-KEY-001", 16);
    publish_key = 1;
    assert(!menu_request(SSCMD_LOADDISC, 7));
    assert(last_command == (SSCMD_LOADDISC | SSCMD_SAVE_ACK) && last_argument == 7);
    assert(admission_calls == 1 && pending_calls[0] == 1 && pending_calls[1] == 1);
    assert(purges == 1 && lookups == 1 && journal_state == JOURNAL_READY);
    assert(journal_slot == 0x30000 && !memcmp(journal_key, next_key, 16));
    assert(!memcmp(looked_up_key, next_key, 16) && !command_lock);

    reset();
    assert(!menu_request(SSCMD_LOADDISC, 0xffff));
    assert(!journal_slot && !journal_key[0] && !lookups && !command_lock);

    reset();
    journal_state = JOURNAL_LEGACY;
    publish_key = 1;
    memcpy(next_key, "NEW-GAME-KEY-001", 16);
    assert(!menu_request(SSCMD_LOADDISC, 7));
    assert(journal_state == JOURNAL_LEGACY && journal_slot == 0x20000);
    assert(!memcmp(journal_key, "OLD-GAME-KEY-000", 16) && !lookups);
    puts("PASS launch/session");
}

static void refusals(void)
{
    reset();
    save_ack = 0;
    assert(menu_request(SSCMD_STARTUP, 0) == -12);
    assert(!purges && !startups && !completions && !command_lock);

    reset();
    admission_error = 1;
    assert(menu_request(SSCMD_LOADDISC, 7) == -2);
    assert(admission_calls == 1 && !pending_calls[0] && !completions);
    assert(journal_slot == 0x20000 && !command_lock);

    reset();
    pending_error = 2;
    assert(menu_request(SSCMD_LOADDISC, 7) == -2);
    assert(pending_calls[0] == 1 && pending_calls[1] == 1 && !completions);
    assert(journal_slot == 0x20000 && !command_lock);

    reset();
    publish_key = 1;
    memcpy(next_key, "NEW-GAME-KEY-001", 16);
    lookup_result = -4;
    assert(menu_request(SSCMD_LOADDISC, 7) == -2);
    assert(journal_state == JOURNAL_RECOVER && journal_slot == ~0u);
    assert(!memcmp(journal_key, next_key, 16) && lookups == 1 && !command_lock);

    reset();
    reply = (u16)-4;
    assert(menu_request(SSCMD_LOADDISC, 7) == -4);
    assert(journal_state == JOURNAL_READY && !journal_slot && !journal_key[0]);
    assert(!lookups && !command_lock);

    reset();
    reply = 0; /* Reset-cleared result cannot release an uncertain owner. */
    assert(menu_request(SSCMD_LOADDISC, 7) == -2);
    assert(command_lock == 0x81 && (menu_channel.flags & MENU_FAULT));
    puts("PASS refusals/quarantine");
}

static void physical_key(void)
{
    const u8 *disc_key = (const u8 *)"PHYSICAL-KEY-001";
    reset();
    clobber_request = 1;
    publish_key = 1;
    memcpy(next_key, "CANONICAL-KEY-01", 16);
    assert(!menu_load_save(disc_key));
    assert(completions == 1 && last_command == (SSCMD_LSAVE | SSCMD_SAVE_ACK));
    assert(!memcmp(sent_key, disc_key, 16));
    assert(!memcmp(journal_key, next_key, 16) && !memcmp(looked_up_key, next_key, 16));
    assert(journal_slot == 0x30000 && purges == 1 && !command_lock);
    puts("PASS physical-key order");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (argv[1][0] == 'n') normal_launches();
    else if (argv[1][0] == 'r') refusals();
    else if (argv[1][0] == 'p') physical_key();
    else abort();
    return 0;
}
