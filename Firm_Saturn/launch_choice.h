/* Adapted from paps-g cda33cfa: nonblocking one-session launch choice. */
#ifndef SAROO_LAUNCH_CHOICE_H
#define SAROO_LAUNCH_CHOICE_H

enum { LAUNCH_PASS, LAUNCH_CONSUMED, LAUNCH_OFFER, LAUNCH_CANCEL,
       LAUNCH_NORMAL, LAUNCH_UNPATCHED };
struct launch_choice {
    unsigned int since;
    unsigned short held;
    unsigned char phase;
};
/* Events contain changed bits in the high half and held bits in the low half.
 * Zero is an idle poll, not a release. now uses the existing 1 MHz timer. */
int launch_choice_poll(struct launch_choice *, unsigned int event, unsigned int now);
#endif
