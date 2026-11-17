/* Included by menu_channel.c when save-generation recovery is enabled. */
#include "save_launch.h"
#define SAVE_RESULT_KEY ((u8 *)(SYSINFO_ADDR + 0x1c))

static int menu_launch_before(int command)
{
    int game = command == SSCMD_LOADDISC || command == SSCMD_FIRSTDISC ||
               command == SSCMD_ROMDISC;
    if (command != SSCMD_STARTUP && !game && command != SSCMD_LSAVE)
        return 0;
    /* These callbacks require the known complete primary-owner contract. The
     * normal legacy menu remains separate; never guess at a partial peer. */
    if (!save_ack || !file_ack || !archive_ack) return -12;
    file_cache_purge(0x02000000u, 0x00100000u, 0);
    int result = command == SSCMD_STARTUP ? journal_start_owned() : save_launch_begin_owned();
    if (result) return -2;
    if (command != SSCMD_STARTUP) {
        /* SOW1's key is output metadata, never the MCU's selected-slot state.
         * With both leases held, initialize it before this request. A completed
         * game load publishes its full key; AUDIO/stop leaves this empty. */
        memset(SAVE_RESULT_KEY, 0, 16);
    }
    return 0;
}

static int menu_launch_after(int command, int result)
{
    if (!result && (command == SSCMD_LOADDISC || command == SSCMD_FIRSTDISC ||
                    command == SSCMD_ROMDISC || command == SSCMD_LSAVE))
        return save_launch_finish_owned(SAVE_RESULT_KEY) ? -2 : 0;
    return result;
}

#undef SAVE_RESULT_KEY
