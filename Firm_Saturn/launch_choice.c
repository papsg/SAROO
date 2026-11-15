/* Original paps-g cda33cfa state machine; no allocation or blocking wait. */
#include "launch_choice.h"
#define LAUNCH_A 0x0400u
#define LAUNCH_HOLD_US 650000u
/* The original freestanding SH-2 toolchain has no libc stdint.h. */
_Static_assert(sizeof(unsigned int) == 4 && sizeof(unsigned short) == 2,
               "Launch events and elapsed arithmetic require 32/16-bit fields");

int launch_choice_poll(struct launch_choice *s, unsigned int event, unsigned int now)
{
    if (event) s->held = event;
    if (!s->phase) {
        if ((event & 0x04000400u) != 0x04000400u) return LAUNCH_PASS;
        s->since = now;
        s->phase = 1;
        return LAUNCH_CONSUMED;
    }
    if ((event & 0x01000100u) == 0x01000100u && s->phase != 4) {
        s->phase = 4; /* Drain held buttons so repeats cannot relaunch. */
        return LAUNCH_CANCEL;
    }
    if (s->phase == 4) {
        if (!s->held) s->phase = 0;
        return LAUNCH_CONSUMED;
    }
    if (s->phase == 1 && now - s->since >= LAUNCH_HOLD_US) {
        s->phase = s->held & LAUNCH_A ? 2 : 3;
        return LAUNCH_OFFER;
    }
    if (s->phase == 1 && !(s->held & LAUNCH_A)) {
        s->phase = 0;
        return LAUNCH_NORMAL;
    }
    if (s->phase == 2) {
        if (!(s->held & LAUNCH_A)) s->phase = 3;
        return LAUNCH_CONSUMED;
    }
    if (s->phase == 3 && (event & 0x04000400u) == 0x04000400u) {
        s->phase = 4;
        return LAUNCH_UNPATCHED;
    }
    return LAUNCH_CONSUMED;
}
