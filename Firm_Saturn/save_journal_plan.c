/* Projection from the existing backup allocator into a bounded SJR1 record.
 * Include after the game_save.c allocation helpers; it uses their native
 * structures and byte order. Planning performs no I/O. Original design by
 * paps-g, adapted from private save-generation recovery work. */
#include "save_journal.h"
_Static_assert(sizeof(u16) == 2 && sizeof(u32) == 4, "backup field widths");

/* Root-only operations can borrow the already prepared RAM shadow. Device-0
 * writes will use this after their RAM-only payload/link update; admission and
 * recovery must gate access to that shadow before enabling the runtime path. */
static int journal_plan_parts_root(SaveJournalBuilder *builder, SaveJournalParts *parts,
                                   u8 *workspace, u8 *claims, const u8 *game_id, int dev)
{
    if (save_journal_begin(builder, workspace, SJ_WRITE_BYTES, claims, dev,
                           game_id, 1) != SJ_READY ||
        save_journal_reference(builder, 0, dev ? 8192 : 65536) != SJ_READY)
        return SJ_INVALID;
    return save_journal_seal_parts(builder,
                (const u8 *)(dev ? MEMS_HEADER : BUP_MEM_ADDR), parts);
}

/* MEMS has one contiguous caller payload despite fragmented destination blocks.
 * Copy only the root/header that need projected metadata changes; descriptors
 * reference the caller bytes. The maximum copied prefix is 13,576 bytes. */
static int journal_plan_mems_parts(SaveJournalBuilder *builder, SaveJournalParts *parts,
                                   u8 *workspace, u8 *claims, const u8 *game_id,
                                   const BUPDIR *dir, const u8 *data, const u8 *bp)
{
    if (bup_dev != 1 || !dir || !bp ||
        (((uintptr_t)workspace | (uintptr_t)bp) & 3) ||
        (dir->data_size && !data) || dir->data_size > 479u * 1024 ||
        *(const u32 *)(bp + 12) != dir->data_size)
        return SJ_INVALID;
    const PendingSave *pending = &pending_save[1];
    if (pending->header < 8 || pending->header >= 8064 ||
        pending->last < -1 || pending->last >= 448)
        return SJ_INVALID;
    unsigned blocks = dir->data_size > 960 ? (dir->data_size + 1023) / 1024 : 0;
    if (save_journal_begin(builder, workspace, SJ_WRITE_BYTES, claims, 1, game_id,
                           2 + blocks) != SJ_READY)
        return SJ_INVALID;
    u8 *root = workspace + builder->length;
    if (save_journal_append(builder, 0, (const void *)MEMS_HEADER, 8192) != SJ_READY)
        return SJ_INVALID;
    if (pending->last >= 0) {
        u8 *entry = root + 1024 + pending->last * 16;
        memcpy(entry, (void *)bp, 11);
        *(u16 *)(entry + 14) = pending->header;
    }
    u8 *header = workspace + builder->length;
    if (save_journal_append(builder, pending->header * 1024, bp, 1024) != SJ_READY)
        return SJ_INVALID;
    if (!blocks && dir->data_size) memcpy(header + 64, (void *)data, dir->data_size);
    unsigned remaining = dir->data_size;
    for (unsigned i = 0; i < blocks; i++) {
        unsigned block = *(const u16 *)(bp + 64 + i * 2);
        unsigned size = remaining > 1024 ? 1024 : remaining;
        if (save_journal_reference(builder, block * 1024, size) != SJ_READY)
            return SJ_INVALID;
        remaining -= size;
    }
    return save_journal_seal_parts(builder, blocks ? data : NULL, parts);
}
