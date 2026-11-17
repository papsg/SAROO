/* Included after save_runtime.c. No command may replace a
 * save session until its previous work is settled. */
int save_launch_begin_owned(void)
{
    if (journal_admit(1) || finish_pending(0) || finish_pending(1))
        return BUP_BROKEN;
    if (journal_state != JOURNAL_LEGACY) {
        journal_slot = 0;
        memset(journal_key, 0, sizeof(journal_key));
    }
    return 0;
}

/* Successful LOADDISC/LSAVE has already loaded and checked its internal shadow.
 * Resolve its canonical slot without rereading that shadow or the MEMS root.
 * The following lookup uses only the separate transfer workspace. A failed
 * lookup keeps recovery required and preserves the new key for a later retry. */
int save_launch_finish_owned(const u8 *key)
{
    if (!key) {
        journal_state = JOURNAL_OFF;
        return BUP_BROKEN;
    }
    if (journal_state == JOURNAL_LEGACY) return 0;
    if (journal_state != JOURNAL_READY || journal_slot) {
        journal_state = JOURNAL_OFF;
        return BUP_BROKEN;
    }
    if (!key[0]) return 0;
    memcpy(journal_key, (void *)key, sizeof(journal_key));
    int slot = archive_find_slot(journal_key, JOURNAL_WORK, SJ_WRITE_BYTES);
    if (slot < 0) {
        journal_state = JOURNAL_RECOVER;
        journal_slot = ~0u;
        return BUP_BROKEN;
    }
    journal_slot = slot;
    return 0;
}
