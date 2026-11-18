/* paps-g 6491b42b / SSR-111; public adaptation by papsg.
 * Actual language module, startup tail, shell clause and LT branch. Device,
 * complete UI, transport and startup prefix are explicit host substitutes. */
#include <assert.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#define lang_init provider_lang_init
#include "language.c"
#undef lang_init
typedef char integer_widths[(sizeof(int)==4 && sizeof(u32)==4)?1:-1];
typedef char selected_languages[(sizeof(lang_list)/sizeof(lang_list[0])==19)?1:-1];
static unsigned initialized;
static void lang_init(void) {initialized++;provider_lang_init();}
#include "buttons.inc"
static jmp_buf finished;
static int status,cycles,expected,cycling,listing,restore_error,in_console,menus,console_menus,takeovers;
static int catalog, listbin_requests, listdisc_requests;
static int error_messages, error_display;
static int sel_mode;
static int menu_statuses, menu_errors, sci_starts;
static u32 debug_flag;
#include "opcodes.inc"

static __attribute__((unused)) int menu_request(int command, int argument)
{
    assert(argument == 0);
    if(command == SSCMD_LISTBINS) {catalog = 1; listbin_requests++; return 0;}
    assert(command == SSCMD_LISTDISC);
    listdisc_requests++;
    if(restore_error) return -5;
    catalog = 0; return 0;
}
static __attribute__((unused)) void printk(const char *format, ...) {(void)format;error_messages++;}
static __attribute__((unused)) void nbg1_on(void) {error_display++;}
static int SS_CMD, SS_ARG;
static __attribute__((unused)) int menu_command_wait(void)
{
    if(SS_CMD==SSCMD_LISTDISC) {
        assert(SS_ARG==0);
        SS_CMD=0;catalog=0;listdisc_requests++;
    }
    return 0;
}
static void sci_init(void) {sci_starts++;}
static void menu_status(void *menu, const char *message)
{(void)menu;(void)message;menu_statuses++;}
static __attribute__((unused)) void menu_error(void *menu, const char *format, ...)
{(void)menu;(void)format;menu_errors++;}
static int main_menu;

static void translation(void)
{
    int selected=(expected+cycles)%lang_nr;
    for(int key=0;key<LANG_STR_NR;key++) {
        if(strcmp(TT(lang_zhcn[key]),lang_list[selected][key])) {
            if(!status)fprintf(stderr,"startup translation mismatch: ID=%d expected=%d key=%d\n",
                               lang_id,selected,key);
            status=42;
        }
    }
    if(lang_id!=selected)status=42;
}
static int cycle_action(u32 ctrl)
{
#include "cycle.inc"
    return 0;
}
static void menu_init(void)
{
    /* The real UI translates before its installed input handler executes. A
     * wrong label is recorded without preventing a later real cycle action. */
    translation();
    if(in_console) {
        console_menus++;
        if(cycling) {
            assert(cycle_action((u32)PAD_LT*65537u)==MENU_EXIT);
            cycles++;
        }
        return;
    }
    menus++;
    assert(initialized==1 && takeovers==1);
    if(listing) assert(catalog==0);
    assert(console_menus==(((debug_flag&3)==3)?(cycling?2:1):0));
    if(menus==1 && cycling) {
        assert(cycle_action((u32)PAD_LT*65537u)==MENU_EXIT);
        cycles++;return;
    }
    longjmp(finished,1);
}
static void sci_shell(void)
{
    assert(takeovers==1);
    in_console=1;
    const char *cmd="menu";
    for(int i=0;i<(cycling?2:1);i++) {
#define CMD(x) else if(!strcmp(cmd,#x))
        if(0){}
#include "shell_menu.inc"
#undef CMD
    }
    if(listing) {
        cmd="listb";
        do {
#define CMD(x) else if(!strcmp(cmd,#x))
        if(0){}
#include "shell_listb.inc"
#undef CMD
        } while(0);
    }
    in_console=0;
}
static void cdblock_off(void)
{
    assert(!initialized && !console_menus);
    takeovers++;
}
#include "restore.inc"
#include "startup.inc"
#include "main_console.inc"
int main(int argc,char **argv)
{
    assert(argc==7);setvbuf(stdout,NULL,_IONBF,0);
    long supplied=strtol(argv[1],0,10);
    assert(supplied>=INT_MIN && supplied<=INT_MAX);
    lang_id=(int)supplied;
    expected=lang_id>=0 && lang_id<lang_nr?lang_id:0;
    debug_flag=(u32)strtoul(argv[2],0,10);cycling=atoi(argv[3]);listing=atoi(argv[4]);
    restore_error=atoi(argv[5]);
    int menu_entry=atoi(argv[6]);
    assert(cycling==0 || cycling==1);
    assert(listing==0 || (listing==1 && (debug_flag&3)==3));
    assert(restore_error==0 || (restore_error==1 && listing==1));
    assert(menu_entry==0 || menu_entry==1);
    if(menu_entry) {
        assert((debug_flag&3)==3 && !cycling);
        if(!listing) catalog=1;
        lang_init();takeovers=1;
        int result=main_console_case();
        assert(sci_starts==1 && menu_statuses==1 && listbin_requests==listing && listdisc_requests==1);
        if(restore_error) assert(result==0 && catalog==1 && sel_mode==1 && menu_errors==1);
        else assert(result==MENU_EXIT && catalog==0 && sel_mode==0 && menu_errors==0);
        return 0;
    }
    if(restore_error) {
        assert(startup_tail()==-1);
        assert(catalog==1 && menus==0 && listbin_requests==1 && listdisc_requests==1);
        assert(error_display==1 && error_messages==1);
        return 0;
    }
    if(!setjmp(finished)) {(void)startup_tail();assert(0);}
    if(listing) assert(listbin_requests==1 && listdisc_requests==1);
    printf("startup: supplied=%ld debug=%u cycles=%d init=%u console=%d status=%d\n",
           supplied,(unsigned)debug_flag,cycles,initialized,console_menus,status);
    return status;
}
