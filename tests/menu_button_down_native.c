/* Forced compile-only future callers; never linked into the menu.
 * paps-g 3ed0238f / SSR-110; public adaptation by papsg. */
#include "main.h"
typedef char integer_widths[(sizeof(int)==4 && sizeof(u32)==4)?1:-1];
int press_fields(u32 event,int buttons) {return BUTTON_DOWN(event,buttons);}
int right_press(u32 event) {return BUTTON_DOWN(event,PAD_RIGHT);}
