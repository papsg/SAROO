/* Private declarations for the game_save.c recovery binding. */
#ifndef SAROO_SAVE_RUNTIME_H
#define SAROO_SAVE_RUNTIME_H
#include <stdint.h>
enum { JOURNAL_OFF, JOURNAL_RECOVER, JOURNAL_READY, JOURNAL_LEGACY };
/* Every mutable allocator/journal object has the same backup-operation lease.
 * Keep its exact existing capacity in one object so admission can protect all
 * of it without a pointer table or a separate test for each private variable. */
#define SAROO_SAVE_STATE_GROUPED 1
typedef struct {
    int bup_dev;
    int mblk_start;
    int mblk_end;
    int mblk_bmp[2];
    int msav_blk;
    PendingSave pending_save[2];
    int checked_free;
    int checked_saves;
    unsigned journal_state;
    u32 journal_slot;
    u8 journal_key[16];
    u8 resize_bitmap[1008];
    u16 resize_blocks[510];
} BackupState;
_Static_assert(sizeof(BackupState) == 2100, "retain complete backup state capacity");
static BackupState backup_state;
#define bup_dev backup_state.bup_dev
#define mblk_start backup_state.mblk_start
#define mblk_end backup_state.mblk_end
#define mblk_bmp backup_state.mblk_bmp
#define msav_blk backup_state.msav_blk
#define pending_save backup_state.pending_save
#define checked_free backup_state.checked_free
#define checked_saves backup_state.checked_saves
#define journal_state backup_state.journal_state
#define journal_slot backup_state.journal_slot
#define journal_key backup_state.journal_key
#define resize_bitmap backup_state.resize_bitmap
#define resize_blocks backup_state.resize_blocks
static int journal_admit(int dev);
static int journal_input_span(const void *, u32);
static int journal_name_input(int, const char *);
static int journal_commit(int dev);
static int journal_mems_write(const BUPDIR *, const u8 *, const u8 *);
int journal_start(void);
int journal_select(const u8 *key);
#endif
