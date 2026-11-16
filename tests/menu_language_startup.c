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
static int status,cycles,expected,cycling,in_console,menus,console_menus,takeovers;
static u32 debug_flag;

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
    in_console=0;
}
static void cdblock_off(void)
{
    assert(!initialized && !console_menus);
    takeovers++;
}
#include "startup.inc"
int main(int argc,char **argv)
{
    assert(argc==4);setvbuf(stdout,NULL,_IONBF,0);
    long supplied=strtol(argv[1],0,10);
    assert(supplied>=INT_MIN && supplied<=INT_MAX);
    lang_id=(int)supplied;
    expected=lang_id>=0 && lang_id<lang_nr?lang_id:0;
    debug_flag=(u32)strtoul(argv[2],0,10);cycling=atoi(argv[3]);
    assert(cycling==0 || cycling==1);
    if(!setjmp(finished)) {(void)startup_tail();assert(0);}
    printf("startup: supplied=%ld debug=%u cycles=%d init=%u console=%d status=%d\n",
           supplied,(unsigned)debug_flag,cycles,initialized,console_menus,status);
    return status;
}
