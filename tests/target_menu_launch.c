/* Target glue compile fixture; runtime source compiles in game_save separately. */
#include "../Firm_Saturn/main.h"
#include "../Firm_Saturn/command_gate.h"
#include "../Firm_Saturn/menu_channel.h"
#include "../Firm_Saturn/save_launch.h"
u16 save_ack;
static u16 file_ack, archive_ack;
static void file_cache_purge(u32 address, u32 size, int mirrored)
{
    (void)address;
    (void)size;
    (void)mirrored;
    __asm__ volatile ("" ::: "memory");
}
int journal_start_owned(void) { return 0; }
int save_launch_begin_owned(void) { return 0; }
int save_launch_finish_owned(const u8 *key) { (void)key; return 0; }
#include "../Firm_Saturn/command_gate.c"
#define SAROO_MENU_SAVE_LAUNCH
#include "../Firm_Saturn/menu_channel.c"
void target_menu_launch_init(u32 version) { menu_channel_init(version); }
