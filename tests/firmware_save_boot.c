/* Adapted from paps-g 1f90bc28; actual post-command caller gates only. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int result_arg, started, statuses, use_sys_load;
#define SS_ARG result_arg
#define TT(text) (text)
static void my_cdplayer(void) { started++; }
static void menu_error(void *menu, const char *text, int result)
{ assert(!menu && text[0] && result == result_arg); statuses++; }
static int optical_gate(void)
{
#include "optical-gate.inc"
    started++; return 0;
}
static void cdplayer_gate(void)
{
    int retv = 0; void *menu = NULL;
#include "cdplayer-gate.inc"
    (void)retv; (void)menu;
}
int main(int argc, char **argv)
{
    assert(argc == 3); result_arg = atoi(argv[2]); use_sys_load = 99;
    if (!strcmp(argv[1], "optical")) {
        assert(optical_gate() == (result_arg ? -1 : 0));
        assert(started == !result_arg && !statuses);
    } else {
        cdplayer_gate(); assert(started == !result_arg && statuses == !!result_arg);
        assert(use_sys_load == (result_arg ? 99 : 0));
    }
    return 0;
}
