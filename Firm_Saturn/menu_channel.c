/* When integrated, include this after command_gate.c. The metadata lock never
 * grants another caller a command lease: only this channel retires its own
 * SELECT request. Explicit requests retain both locks until completion. */
enum { MENU_ENABLED = 1, MENU_QUEUED = 2, MENU_ACTIVE = 4, MENU_FAULT = 8 };
static struct { u8 lock, flags; u16 argument; } menu_channel;
#ifndef MENU_CHANNEL_ADDRESS
#define MENU_CHANNEL_ADDRESS ((volatile __typeof__(menu_channel) *)((u32)&menu_channel | 0x20000000u))
#endif
#define CHANNEL MENU_CHANNEL_ADDRESS

/* Called once at menu startup, before publishing any command. No renegotiation
 * after a reset: the unchanged old MCU does not complete STARTUP or SELECT. */
static void menu_channel_init(u32 version)
{
    CHANNEL->flags = version == 1 ? MENU_ENABLED : 0;
}

/* Console completion is a menu capability, independent of file/save tags. */
int menu_command_ack(void)
{
    return CHANNEL->flags & MENU_ENABLED ? SSCMD_SAVE_ACK : 0;
}

/* Metadata lock held. SELECT has no error result in protocol v1: only its
 * exact tagged zero proves completion. Reset-cleared/unexpected replies retain
 * the command lease; merely returning an error must not expose its workspace. */
static int menu_channel_step(void)
{
    unsigned flags = CHANNEL->flags;
    if (!(flags & MENU_ENABLED)) return -12;
    if ((flags & MENU_FAULT) || command_faulted()) {
        CHANNEL->flags = flags | MENU_FAULT;
        return -2;
    }
    if (flags & MENU_ACTIVE) {
        if (SS_CMD) return 1;
        __asm__ volatile ("" ::: "memory");
        if (command_reply(SSCMD_SAVE_ACK, 0, 0)) {
            CHANNEL->flags = flags | MENU_FAULT;
            return -2;
        }
        command_leave();
        flags &= ~MENU_ACTIVE;
        CHANNEL->flags = flags;
    }
    if (flags & MENU_QUEUED) {
        if (!command_enter()) return -16;
        SS_ARG = CHANNEL->argument;
        __asm__ volatile ("" ::: "memory");
        SS_CMD = SSCMD_SELECT | SSCMD_SAVE_ACK;
        CHANNEL->flags = (flags & ~MENU_QUEUED) | MENU_ACTIVE;
        return 1;
    }
    return 0;
}

int menu_channel_poll(void)
{
    /* Advisory idle observation grants no ownership. A concurrent enqueue is
     * seen on the next poll; synchronous entry still always takes the lock. */
    unsigned flags = CHANNEL->flags;
    if (!(flags & (MENU_QUEUED | MENU_ACTIVE | MENU_FAULT)) && !command_faulted())
        return flags & MENU_ENABLED ? 0 : -12;
    if (!command_try_lock(&CHANNEL->lock)) return -16;
    int result = menu_channel_step();
    COMMAND_GATE_RELEASE(&CHANNEL->lock);
    return result;
}

int menu_select_notify(u16 argument)
{
    if (!command_try_lock(&CHANNEL->lock)) return -16;
    unsigned flags = CHANNEL->flags;
    if (!(flags & MENU_ENABLED) || (flags & MENU_FAULT) || command_faulted()) {
        COMMAND_GATE_RELEASE(&CHANNEL->lock);
        return (flags & MENU_FAULT) || command_faulted() ? -2 : -12;
    }
    /* A hover replaces an obsolete pending preview, but must retain entry's
     * GIF-stop/cache-reset work. A hover superseding an unsent exit likewise
     * requests game-view entry. In-flight transitions always finish first. */
    if (!(argument & (SELECT_ENTER | SELECT_EXIT)) && (flags & MENU_QUEUED) &&
        (CHANNEL->argument & (SELECT_ENTER | SELECT_EXIT)))
        argument |= SELECT_ENTER;
    CHANNEL->argument = argument;
    CHANNEL->flags = flags | MENU_QUEUED;
    int result = menu_channel_step();
    COMMAND_GATE_RELEASE(&CHANNEL->lock);
    /* A foreign file/save owner defers an accepted selection. Only metadata
     * contention above rejects it; completion is observed by later polling. */
    return result == 1 || result == -16 ? 0 : result;
}

/* An explicit action owns metadata while draining its previously queued
 * previews, so no new selection can name a catalog being replaced. Contending
 * UI calls return busy. Foreign backup/file ownership is never waited upon.
 * A drain timeout submits no explicit request and retains the active preview's
 * command lease. Later polling may retire it; this does not cancel MCU work. */
static int menu_channel_admit(void)
{
    int result = menu_channel_step();
    /* Observe a hot-reset-inherited command before trying to acquire the fresh
     * Saturn-side lease. If it retires between observations, the next bounded
     * step acquires normally; no request or payload is abandoned. */
    if (!result && command_boot_pending()) result = 1;
    else if (!result && !command_enter()) result = command_faulted() ? -2 : -16;
    return result;
}

static int menu_channel_enter(void)
{
    if (!command_try_lock(&CHANNEL->lock)) return -16;
    int result = menu_channel_admit();
    if (result == 1) {
        u32 deadline = get_timer() + MS2TICK(10000);
        while ((result = menu_channel_admit()) == 1) {
            if (timer_reached(get_timer(), deadline)) {
                result = -3;
                break;
            }
        }
    }
    if (result) COMMAND_GATE_RELEASE(&CHANNEL->lock);
    return result;
}

static int menu_request_owned(int command, int argument)
{
    if (command_faulted()) return -2;
    SS_ARG = argument;
    __asm__ volatile ("" ::: "memory");
    SS_CMD = command | SSCMD_SAVE_ACK;
    while (SS_CMD);
    __asm__ volatile ("" ::: "memory");
    /* v1 uses bounded FatFs/loader errors and two update status bits. An
     * untagged zero is not success and cannot retire ownership after reset. */
    int maximum = command == SSCMD_CHECK || command == SSCMD_UPDATE ? 3 : 19;
    return command_reply(SSCMD_SAVE_ACK, -19, maximum);
}

static int menu_channel_finish(int result)
{
    if (!command_leave()) CHANNEL->flags |= MENU_FAULT;
    COMMAND_GATE_RELEASE(&CHANNEL->lock);
    return result;
}

#ifdef SAROO_MENU_SAVE_LAUNCH
#include "menu_launch.c"
#endif

int menu_request(int command, int argument)
{
    int result = menu_channel_enter();
    if (result) return result;
#ifdef SAROO_MENU_SAVE_LAUNCH
    result = menu_launch_before(command);
    if (result) return menu_channel_finish(result);
#endif
    result = menu_request_owned(command, argument);
#ifdef SAROO_MENU_SAVE_LAUNCH
    result = menu_launch_after(command, result);
#endif
    return menu_channel_finish(result);
}

int menu_load_save(const u8 *key)
{
    if (!key) return -19;
    int result = menu_channel_enter();
    if (result) return result;
#ifdef SAROO_MENU_SAVE_LAUNCH
    result = menu_launch_before(SSCMD_LSAVE);
    if (result) return menu_channel_finish(result);
#endif
    memcpy((void *)(TMPBUFF_ADDR + 0x10), (void *)key, 16);
    *(u8 *)(TMPBUFF_ADDR + 0x20) = 0;
    result = menu_request_owned(SSCMD_LSAVE, 0);
#ifdef SAROO_MENU_SAVE_LAUNCH
    result = menu_launch_after(SSCMD_LSAVE, result);
#endif
    return menu_channel_finish(result);
}

#undef CHANNEL
