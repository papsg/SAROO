#ifndef SAROO_MENU_CHANNEL_H
#define SAROO_MENU_CHANNEL_H

/* Notify success means queued, not completed. Metadata contention returns -16;
 * a foreign command owner only defers an accepted preview. Polling does not wait.
 * Explicit requests bound their prior-artwork drain before taking the command
 * lease. Timeout retains the active preview for later polling. The explicit
 * request's own completion still requires reset/deadline containment. */
int menu_channel_poll(void);
int menu_select_notify(u16 argument);
int menu_request(int command, int argument);
int menu_command_ack(void);
/* The physical-disc IP is stable WRAM owned by the launch caller. Preparation
 * of the shared LSAVE request belongs to the same lease as its completion. */
int menu_load_save(const u8 *key);

#endif
