#ifndef SAROO_COMMAND_GATE_H
#define SAROO_COMMAND_GATE_H

/* A successful entry owns the shared command request, backup shadows and
 * journal workspace until leave. Never infer ownership from a global flag:
 * internal callers explicitly use the *_owned functions under this lease.
 * Busy entry does not wait or touch the pending request/caller buffers. */
int command_enter(void);
int command_boot_pending(void);
int backup_enter(void);
/* A protocol fault keeps the lease quarantined. Leave returns true only for
 * an ordinary release; backup wrappers must not report success after a fault
 * in a nested transport operation. */
int command_leave(void);
int command_faulted(void);
void command_fault(void);
int command_reply(int tag, int minimum, int maximum);

#endif
