#ifndef SAROO_SAVE_LAUNCH_H
#define SAROO_SAVE_LAUNCH_H

/* Internal launch callbacks. The caller owns command metadata, the primary
 * command lease and the allocator/workspace, with current calling-CPU caches. */
int journal_start_owned(void);
int save_launch_begin_owned(void);
int save_launch_finish_owned(const u8 *key);

#endif
