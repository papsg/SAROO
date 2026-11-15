/* Adapted from paps-g a5ac9179: qualify a source edit before user patches. */
#include "main.h"

#ifndef SD_GUARD_POINTER
#define SD_GUARD_POINTER(address) ((u8 *)(address))
#define SD_GUARD_READ16(address) REG16(address)
#define SD_GUARD_WRITE16(address, value) (REG16(address) = (value))
#define SD_GUARD_FIRST_LOAD() REG32(0x060020f0)
#endif

static u32 guard_be32(const u8 *p)
{
    return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
}

u32 sd_overlay_checked_action(u32 action)
{
    if ((action & 0xffffff80u) != 0x534f5780u) return action;
    if (action != 0x534f5780u && action != 0x534f5781u && action != 0x534f5784u)
        return 0xffffffffu;
    const u8 *d = SD_GUARD_POINTER(SYSINFO_ADDR + 0x40);
    u32 load = guard_be32(d), size = guard_be32(d + 4), offset = guard_be32(d + 12);
    u16 before = (u16)((u16)d[16] << 8 | d[17]);
    u16 after = (u16)((u16)d[18] << 8 | d[19]);
    /* Bound every derived access, including metadata changed in transit. */
    if (crc32((u8 *)d, 20, 0) != guard_be32(d + 20) ||
        (load & 3) || size - 2u > 0xfdffeu ||
        load - 0x06002000u > 0xfe000u - size ||
        (offset & 1) || offset > size - 2 || before == after ||
        load != SD_GUARD_FIRST_LOAD()) return 0xffffffffu;
    u32 address = load + offset;
    if (SD_GUARD_READ16(address) != after) return 0;
    if (skip_patch ||
        crc32(SD_GUARD_POINTER(load | 0x20000000u), (int)size, 0) != guard_be32(d + 8)) {
        /* P1 write-through updates a resident instruction line too. The old
         * callback then sees its original complete-file guard and user edits
         * still run afterward. P2 is used only for observing loaded memory. */
        SD_GUARD_WRITE16(address, before);
        return 0;
    }
    return action ^ 0x100u;
}
