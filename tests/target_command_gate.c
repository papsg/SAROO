/* Target instruction and ABI compile fixture; never linked into menu. */
#include "../Firm_Saturn/main.h"
#include "../Firm_Saturn/command_gate.h"
static void file_cache_purge(u32 address, u32 size, int mirrored)
{
    (void)address;
    (void)size;
    (void)mirrored;
    __asm__ volatile ("" ::: "memory");
}
#include "../Firm_Saturn/command_gate.c"
