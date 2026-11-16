/* Forced compile-only future callers; not linked into the ordinary menu.
 * paps-g fac81eb1 / SSR-066, public adaptation by papsg. */
#include "main.h"
typedef char integer_widths[(sizeof(int)==4 && sizeof(u32)==4)?1:-1];
int release_fields(u32 event,int buttons) {return BUTTON_UP(event,buttons);}
int right_release(u32 event) {return BUTTON_UP(event,PAD_RIGHT);}
