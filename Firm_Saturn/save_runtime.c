/* Include in game_save.c after the allocator and save_journal_plan.c.
 * The caller must exclusively own both backup devices, the command channel,
 * all BackupState objects and the 64 KiB file workspace for the entire operation. */

#define JOURNAL_FILE "/SAROO/SAVEJRN.BIN"
#define JOURNAL_WORK ((u8 *)(TMPBUFF_ADDR + 0x100))

/* Cached and uncached SH-2 pointers can name the same storage. The protected
 * backup/transfer ranges are below the FPGA's folded expansion-RAM window.
 * Host fixtures substitute their allocation address mapping, not this test. */
#ifndef JOURNAL_INPUT_ADDRESS
#define JOURNAL_INPUT_ADDRESS(pointer) ((uintptr_t)(pointer) & 0x1fffffffu)
#endif

static int journal_input_overlaps(uintptr_t address, uintptr_t last, const void *storage, u32 capacity)
{
    uintptr_t base = JOURNAL_INPUT_ADDRESS(storage);
    return last >= base && address < base + capacity;
}

/* Known input/output extents are checked before metadata access or recovery.
 * Read/verify payload size is learned after recovery and validated lookup.
 * Redo work, backup buffers and private state do not belong to the caller.
 * Empty payloads have no extent. */
static int journal_input_span(const void *data, u32 size)
{
    if (!size) return 1;
    uintptr_t source = (uintptr_t)data, address = JOURNAL_INPUT_ADDRESS(data);
    if (!data || size - 1 > UINTPTR_MAX - source) return 0;
    uintptr_t last = JOURNAL_INPUT_ADDRESS(source + size - 1);
    if (last < address) return 0;
    return !journal_input_overlaps(address, last, (const void *)BUP_MEM_ADDR, 0x23000) &&
           !journal_input_overlaps(address, last, (const void *)TMPBUFF_ADDR, 0x100 + SJ_WRITE_BYTES) &&
           !journal_input_overlaps(address, last, &backup_state, sizeof backup_state);
}

static int journal_read(void *context, uint32_t offset, uint8_t *data, uint32_t size)
{
    (void)context;
    int result = read_file_bounded(JOURNAL_FILE, offset, size, data, size);
    /* Only a missing file is empty. Missing paths, short headers and storage
     * errors must not grant admission or overwrite an uncertain generation. */
    return result == -4 ? 0 : result; /* FR_NO_FILE */
}

static int journal_write(void *context, int offset, const uint8_t *data, uint32_t size)
{
    (void)context;
    return write_file(JOURNAL_FILE, offset, size, (void *)data);
}

/* Initialization writes distinct outputs. Configuration must not replace the
 * function table, and BIOS setup also owns its complete library/work extents.
 * Virtual setup merely stores lib_addr; it never accesses that allocation. */
static int journal_init_input(const void *lib_addr, const void *work_addr, const void *cfg_ptr)
{
    u32 work_size = use_sys_bup ? 8312 : 44;
    u32 cfg_size = 3 * sizeof(BUPCFG);
    if (((uintptr_t)work_addr & 3) || ((uintptr_t)cfg_ptr & 1) ||
        !journal_input_span(work_addr, work_size) || !journal_input_span(cfg_ptr, cfg_size))
        return 0;
    uintptr_t work = JOURNAL_INPUT_ADDRESS(work_addr), cfg = JOURNAL_INPUT_ADDRESS(cfg_ptr);
    if (journal_input_overlaps(work, work + work_size - 1, cfg_ptr, cfg_size)) return 0;
    return !use_sys_bup ||
           (!((uintptr_t)lib_addr & 3) && journal_input_span(lib_addr, 16384) &&
            !journal_input_overlaps(work, work + work_size - 1, lib_addr, 16384) &&
            !journal_input_overlaps(cfg, cfg + cfg_size - 1, lib_addr, 16384));
}

/* Preserve short/empty and fixed eleven-byte names. Invalid devices and null
 * names retain the ordinary API's result precedence without touching memory. */
static int journal_name_input(int dev, const char *name)
{
    if (dev < 0 || dev > 1 || journal_state == JOURNAL_LEGACY || !name) return 1;
    if (!journal_input_span(name, 1)) return 0;
    unsigned size = 0;
    while (size < 11 && name[size]) size++;
    return journal_input_span(name, size < 11 ? size + 1 : 11);
}

typedef struct { unsigned device; u32 base; } JournalTarget;

static int journal_apply(void *context, uint32_t offset, const uint8_t *data, uint32_t size)
{
    const JournalTarget *target = context;
    return archive_command(target->device, size ? target->base + offset : 0,
                            (void *)data, -(int)size);
}

/* Raw owner writes revoke MCU cache/slot selection. Reload through that owner
 * and retire every Saturn dirty window only after the reloads complete. Keep
 * the canonical session key separately: MCU slot invalidation clears its copy. */
static int journal_restore(void)
{
    if (journal_slot == ~0u) {
        int result = archive_find_slot(journal_key, JOURNAL_WORK, SJ_WRITE_BYTES);
        if (result < 0) return BUP_BROKEN;
        journal_slot = result;
    }
    if (journal_slot && archive_command(0, journal_slot, (void *)BUP_MEM_ADDR, 65536) != 65536)
        return BUP_BROKEN;
    if (archive_command(1, 0, (void *)MEMS_HEADER, 8192) != 8192)
        return BUP_BROKEN;
    msav_blk = mblk_start = mblk_end = 0;
    mblk_bmp[0] = mblk_bmp[1] = 0;
    memset(pending_save, 0, sizeof(pending_save));
    return 0;
}

static int journal_recover(void)
{
    SaveJournalRecovery record;
    int result = save_journal_load(&record, JOURNAL_WORK, resize_bitmap, journal_read, NULL);
    if (result < 0) return BUP_BROKEN;
    if (result == SJ_READY) {
        JournalTarget target = {record.device, 0};
        if (!record.device) {
            /* Lookup may overwrite the transfer window, but not the retained
             * table/checkpoints. Replay rereads and revalidates payload bytes. */
            result = archive_find_slot(JOURNAL_WORK + SJ_REPLAY_BYTES + 16,
                                        JOURNAL_WORK, SJ_REPLAY_BYTES);
            if (result < 0) return BUP_BROKEN;
            target.base = result;
            /* Reject a truncated selected slot before replay's first write. */
            if (archive_command(0, target.base + 65535, JOURNAL_WORK, 1) != 1)
                return BUP_BROKEN;
        }
        if (save_journal_replay(&record, journal_read, journal_apply, &target) != SJ_READY ||
            save_journal_clear(JOURNAL_WORK, journal_write, NULL) != SJ_EMPTY)
            return BUP_BROKEN;
    }
    if (journal_restore()) return BUP_BROKEN;
    journal_state = JOURNAL_READY;
    return 0;
}

/* Run before a new disc selection can create/change a slot. An older MCU may
 * continue only when no active record exists. Never send owner commands to it
 * or silently discard a record needing a newer recovery implementation. */
int journal_start(void)
{
    journal_state = JOURNAL_RECOVER;
    journal_slot = 0;
    memset(journal_key, 0, sizeof(journal_key));
    int result = archive_command(1, 0, NULL, 0);
    if (result == -12) { /* FR_NOT_ENABLED */
        SaveJournalRecovery record;
        if (save_journal_load(&record, JOURNAL_WORK, resize_bitmap, journal_read, NULL) != SJ_EMPTY)
            return BUP_BROKEN;
        journal_state = JOURNAL_LEGACY;
        return 0;
    }
    if (result) return BUP_BROKEN;
    return journal_recover();
}

static int journal_admit(int dev)
{
    /* An unselected internal shadow cannot become a selected slot during
     * recovery. Reject it before I/O; cartridge/launch admission can recover.
     * A nonzero slot either survives restoration or makes recovery fail. */
    if (!dev && !journal_slot && journal_state != JOURNAL_LEGACY) return BUP_BROKEN;
    if (journal_state == JOURNAL_RECOVER) return journal_recover();
    return journal_state == JOURNAL_READY || journal_state == JOURNAL_LEGACY ? 0 : BUP_BROKEN;
}

/* Called only after completed MCU game selection and before backup use. A
 * device-1-only session may have no internal slot. Failed lookup keeps the
 * whole session closed, even if a previous game had a valid association. */
static int journal_bind(const u8 *key)
{
    journal_state = JOURNAL_RECOVER;
    memcpy(journal_key, (void *)key, sizeof(journal_key));
    journal_slot = journal_key[0] ? ~0u : 0;
    return journal_recover();
}

/* Only recovery can overwrite the next key before binding it. Keep its small
 * snapshot on this exceptional path, separate from normal launch frames. */
static __attribute__((noinline)) int journal_reselect(const u8 *key)
{
    u8 next[16];
    memcpy(next, (void *)key, sizeof(next));
    if (journal_recover()) return BUP_BROKEN;
    return journal_bind(next);
}

int journal_select(const u8 *key)
{
    if (!key) {
        journal_state = JOURNAL_OFF;
        return BUP_BROKEN;
    }
    if (journal_state == JOURNAL_RECOVER) return journal_reselect(key);
    if (journal_state == JOURNAL_LEGACY) return 0;
    return journal_state == JOURNAL_READY ? journal_bind(key) : BUP_BROKEN;
}

static int journal_publish(SaveJournalParts *parts)
{
    /* Even a failed final reply can have published a valid durable record.
     * No retry, format or other-device operation may bypass this recovery. */
    journal_state = JOURNAL_RECOVER;
    if (save_journal_publish_parts(parts, resize_bitmap, journal_write, NULL) != SJ_READY)
        return BUP_BROKEN;
    return journal_recover();
}

/* Planning and publication never need both controls simultaneously. Sealing
 * consumes the builder before replacing it with the split-span descriptor. */
typedef union { SaveJournalBuilder builder; SaveJournalParts parts; } JournalPlan;

static __attribute__((noinline)) int journal_commit(int dev)
{
    JournalPlan plan;
    if ((!dev && !journal_slot) ||
        journal_plan_parts_root(&plan.builder, &plan.parts, JOURNAL_WORK, resize_bitmap,
                                 journal_key, dev) != SJ_READY) {
        journal_state = JOURNAL_RECOVER;
        return BUP_BROKEN;
    }
    return journal_publish(&plan.parts);
}

static __attribute__((noinline)) int journal_mems_write(const BUPDIR *dir, const u8 *data, const u8 *header)
{
    JournalPlan plan;
    if (journal_plan_mems_parts(&plan.builder, &plan.parts, JOURNAL_WORK, resize_bitmap,
                                journal_key, dir, data, header) != SJ_READY) {
        journal_state = JOURNAL_RECOVER;
        return BUP_BROKEN;
    }
    return journal_publish(&plan.parts);
}
