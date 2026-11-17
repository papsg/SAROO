/* Recovery-record core for atomic save generations. */
#ifndef SAROO_SAVE_JOURNAL_H
#define SAROO_SAVE_JOURNAL_H
#ifdef __sh__
/* The pinned Yaul freestanding target has no libc stdint.h. */
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned int uintptr_t;
#define UINTPTR_MAX 0xffffffffu
_Static_assert(sizeof(uint8_t) == 1 && sizeof(uint32_t) == 4 &&
               sizeof(uintptr_t) == sizeof(void *), "journal target widths");
#else
#include <stdint.h>
#endif

enum {
    SJ_HEADER_BYTES = 512,
    SJ_WRITE_BYTES = 65536, /* Release the file service between bounded chunks. */
    SJ_MAX_BYTES = 512 * 1024,
    SJ_MAX_RECORDS = 481, /* MEMS metadata, save header, 479 payload blocks. */
    SJ_BITMAP_BYTES = 1008,
    SJ_EMPTY = 0,
    SJ_READY = 1,
    SJ_INVALID = -1,
    SJ_IO_ERROR = -2
};

typedef struct {
    uint8_t *bytes, *claimed;
    /* Capacity bounds copied bytes. The first reference closes that prefix by
     * reducing capacity to its actual length; logical length then exceeds it. */
    uint32_t capacity, count, added, length, device;
} SaveJournalBuilder;

typedef struct {
    const uint8_t *bytes;
    uint32_t length, count, device;
} SaveJournalView;

typedef struct {
    uint32_t index, position;
} SaveJournalCursor;

/* Workspace and bitmap must be separate and exclusively owned. Input spans
 * must not alias header/table bytes or previously appended records. No heap,
 * resident payload allocation, file handle or background task is introduced. */
int save_journal_begin(SaveJournalBuilder *, uint8_t *, uint32_t, uint8_t *,
                       uint32_t device, const uint8_t game_id[16], uint32_t records);
int save_journal_append(SaveJournalBuilder *, uint32_t offset, const void *, uint32_t);
int save_journal_seal(SaveJournalBuilder *);
int save_journal_validate(const uint8_t *, uint32_t, uint8_t *, SaveJournalView *);
/* Initialize the cursor to zero. Iterate once without rescanning prior records. */
const uint8_t *save_journal_next(const SaveJournalView *, SaveJournalCursor *,
                                uint32_t *offset, uint32_t *length);

/* The adapter must return a confirmed byte count only after synchronizing and
 * closing the file. Every return, including an error, releases its argument
 * buffers: a timed-out asynchronous transfer cannot return while retaining
 * access. Contain it first or use a separately designed pending-operation API.
 * Offset -1 creates/truncates; other offsets overwrite.
 * Call only after the previous transaction has been applied and cleared.
 * Once publication is attempted, an error may still leave a recoverable record.
 * Never touch primary archives until save_journal_publish returns SJ_READY. */
typedef int (*SaveJournalWrite)(void *, int offset, const uint8_t *, uint32_t);
int save_journal_publish(uint8_t *, uint32_t, SaveJournalWrite, void *);
/* Clear only after every primary change is confirmed durable. If clearing
 * fails, recover/redo this generation before admitting a subsequent change. */
int save_journal_clear(uint8_t *, SaveJournalWrite, void *);

/* Split representation of the same SJR1 bytes. The descriptor
 * table is wholly in prefix; its boundary with payload must fall between
 * records. Payload stays immutable until publication finishes. */
typedef struct {
    uint8_t *prefix;
    const uint8_t *payload;
    uint32_t prefix_bytes, payload_bytes;
} SaveJournalParts;
int save_journal_validate_parts(const SaveJournalParts *, uint8_t *claimed);
/* Close the copied prefix on the first reference. Every remaining record must
 * refer to the next bytes of one stable caller span; append cannot reopen the
 * prefix. This builds the normal descriptors without copying the payload. */
int save_journal_reference(SaveJournalBuilder *, uint32_t offset, uint32_t size);
/* Sealing consumes the builder before assigning the result, so these control
 * objects may share union storage. Neither may alias prefix/payload bytes. */
int save_journal_seal_parts(SaveJournalBuilder *, const uint8_t *payload,
                            SaveJournalParts *);
/* Publication owns a full SJ_WRITE_BYTES allocation at prefix, disjoint from
 * payload. The adapter may reuse that workspace after consuming an argument
 * (as the menu's ordinary file bounce path does). Publication consumes the
 * prefix; rebuild it before a later publication. As with
 * the contiguous API, inspect an uncertain durable journal before retrying. */
int save_journal_publish_parts(SaveJournalParts *, uint8_t *claimed,
                               SaveJournalWrite, void *);

enum {
    SJ_TABLE_BYTES = SJ_HEADER_BYTES + SJ_MAX_RECORDS * 8,
    SJ_REPLAY_BYTES = (SJ_WRITE_BYTES - SJ_TABLE_BYTES) & ~511u,
    SJ_REPLAY_CHUNKS = (SJ_MAX_BYTES + SJ_REPLAY_BYTES - 1) / SJ_REPLAY_BYTES
};

/* A validated on-disk record, retained in one SJ_WRITE_BYTES workspace:
 * transfer window, header/descriptors, then cumulative CRC checkpoints. */
typedef struct {
    uint8_t *workspace;
    uint32_t length, count, device;
} SaveJournalRecovery;

/* Confirmed reads may return a short count at EOF, or a negative error. Only
 * the requested destination may be modified, including on failure. Every return
 * releases that destination; an error is not asynchronous cancellation. */
typedef int (*SaveJournalRead)(void *, uint32_t offset, uint8_t *, uint32_t);
/* Applying is through the existing primary-file owner. A zero-sized request
 * with NULL data means sync; success is its exact byte count. Preserve the
 * table and unconsumed transfer bytes. Clearing the journal is the caller's
 * next step only after replay and sync succeed. */
typedef int (*SaveJournalApply)(void *, uint32_t offset, const uint8_t *, uint32_t);
/* Controls/claims must be outside the exclusively owned workspace. First
 * validate the complete file and EOF without primary writes. Replay checks
 * each reread chunk against that pass before applying it. This detects the
 * modeled corruption, not malicious CRC collisions. Runtime admission must
 * still exclude concurrent transactions and external journal replacement. */
int save_journal_load(SaveJournalRecovery *, uint8_t *, uint8_t *,
                      SaveJournalRead, void *);
int save_journal_replay(const SaveJournalRecovery *, SaveJournalRead,
                        SaveJournalApply, void *);
#endif
