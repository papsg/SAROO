/* Target instruction and ABI compile fixture; not linked into the menu. */
#include "../Firm_Saturn/main.h"
#include "../Firm_Saturn/command_gate.h"
#include "../Firm_Saturn/menu_channel.h"
static void file_cache_purge(u32 address, u32 size, int mirrored)
{
    (void)address;
    (void)size;
    (void)mirrored;
    __asm__ volatile ("" ::: "memory");
}
#include "../Firm_Saturn/command_gate.c"
#include "../Firm_Saturn/menu_channel.c"
void target_menu_channel_init(u32 version) { menu_channel_init(version); }
