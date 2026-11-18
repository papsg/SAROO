/* Execute the actual menu_run function with modeled frames and MCU status. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t u32;
typedef struct MENU_DESC { int (*handle)(u32); } MENU_DESC;
enum { MENU_RESTART = 1, MENU_EXIT = 2 };

static unsigned frames, warnings, inputs, visible;
static int scenario;

static void draw_menu_frame(MENU_DESC *menu)
{
    (void)menu;
    frames++;
    visible = 0;
}

static int menu_channel_poll(void)
{
    if (scenario == 2) return 0;
    if (scenario == 3) return -16;
    if (scenario == 1 && frames == 2) return -3;
    return -2;
}

static void menu_error(MENU_DESC *menu, const char *format, ...)
{
    (void)menu;
    (void)format;
    warnings++;
    visible = 1;
}

static u32 conio_getc(void) { return ++inputs; }
static void menu_timer(MENU_DESC *menu) { (void)menu; }
static void menu_launch_choice_idle(MENU_DESC *menu) { (void)menu; }
static void gif_timer(void) {}
static int handle(u32 input) { return input == 1 ? MENU_RESTART : MENU_EXIT; }

#include "actual.inc"

int main(int argc, char **argv)
{
    assert(argc == 2);
    scenario = atoi(argv[1]);
    assert(scenario >= 0 && scenario <= 3);
    MENU_DESC menu = {handle};
    assert(menu_run(&menu) == 0);
    assert(frames == 2 && inputs == 2);
    if (scenario <= 1) assert(warnings == 2 && visible == 1);
    else assert(warnings == 0 && visible == 0);
    return 0;
}
