/* Adapted from paps-g ce4de2ff. Actual physical-disc menu entry and cleanup. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct { int current; } MENU_DESC;
static MENU_DESC main_menu;
static int sel_mode, use_sys_load, use_sys_bup, update_index=5;
enum { PAD_A=1, PAD_C=2, PAD_LT=4, MENU_EXIT=9, MENU_RESTART=10,
       SSCMD_LOADDISC=7, SSCMD_UPDATE=8 };
#define BUTTON_DOWN(ctrl, key) ((ctrl)&(key))
#define TT(s) (s)
enum { ON, OFF, AUTH, PLAYER, LOAD, BIOS };
static int calls[16], count, on_result, off_result, bios_result, bios_type;
static int auth_results[2], auth_count, errors, last_error;
static uint16_t SS_ARG, command;
static void record(int op) { assert(count<16);calls[count++]=op; }
static uint16_t *command_register(void)
{ assert(SS_ARG==0xffff);record(LOAD);return &command; }
#define SS_CMD (*command_register())
static int cdblock_on(int wait)
{ assert(wait==(main_menu.current==2));record(ON);return on_result; }
static int cdblock_off(void) { record(OFF);return off_result; }
static int menu_default(MENU_DESC *m,int ctrl) { assert(m==&main_menu);(void)ctrl;return 0; }
static void lang_next(void) { assert(0); }
static void select_category(void) { assert(0); }
static void select_bins(void) { assert(0); }
static void sci_init(void) { assert(0); }
static void sci_shell(void) { assert(0); }
static void menu_status(MENU_DESC *m,const char *text) { assert(m==&main_menu && text); }
static void menu_error(MENU_DESC *m,const char *text,int error)
{ assert(m==&main_menu && text);++errors;last_error=error; }
static void my_cdplayer(void) { record(PLAYER); }
static int jhl_auth_hack(int delay)
{ assert(delay==100000 && auth_count<2);record(AUTH);return auth_results[auth_count++]; }
static int bios_cd_cmd(int type) { record(BIOS);bios_type=type;return bios_result; }
#define menu_command_wait() 0
#include "actual.inc"

static void reset(int index)
{
    main_menu.current=index;count=auth_count=errors=0;
    on_result=off_result=bios_result=0;bios_type=-1;
    auth_results[0]=auth_results[1]=2;
    use_sys_load=7;use_sys_bup=8;SS_ARG=command=0;
}
static void sequence(const int *expected,int n)
{ assert(count==n && !memcmp(calls,expected,n*sizeof *expected)); }
static void admission(int retry)
{
    for(int index=1;index<=2;++index) for(int c=0;c<2;++c) {
        if(index==1 && c) continue;
        for(int error=-1;error>=-6;--error) {
            reset(index);on_result=error;
            assert(!main_handle(c?PAD_C:PAD_A));
            sequence((int[]){ON},1);
            assert(errors==1 && last_error==error && use_sys_load==7 && use_sys_bup==8);
            assert(!auth_count && bios_type==-1 && !command);
            if(retry) {
                on_result=0;count=errors=0;
                assert(main_handle(c?PAD_C:PAD_A)==(index==1?MENU_RESTART:0));
                if(index==1) sequence((int[]){ON,PLAYER},2);
                else { sequence((int[]){ON,AUTH,LOAD,BIOS},4);assert(bios_type==(2|(c?0x80:0))); }
                assert(!errors);
            }
        }
    }
}
static void healthy(void)
{
    reset(1);assert(main_handle(PAD_A)==MENU_RESTART);
    sequence((int[]){ON,PLAYER},2);
    assert(use_sys_load==1 && use_sys_bup==1 && !errors);
    for(int type=2;type<=4;++type) for(int c=0;c<2;++c) {
        reset(2);auth_results[0]=type;
        assert(!main_handle(c?PAD_C:PAD_A));
        if(type==3) sequence((int[]){ON,AUTH,AUTH,LOAD,BIOS},5);
        else sequence((int[]){ON,AUTH,LOAD,BIOS},4);
        assert(!errors && bios_type==((type==3?2:type)|(c?0x80:0)) && command==SSCMD_LOADDISC);
    }
}
static void cleanup(void)
{
    for(int path=0;path<4;++path) for(int error=0;error>=-6;--error) {
        reset(2);off_result=error;
        if(path==0) auth_results[0]=-1;
        if(path==1) { auth_results[0]=3;auth_results[1]=5; }
        if(path==2) auth_results[0]=5;
        if(path==3) bios_result=-6;
        assert(!main_handle(PAD_A));
        if(path==1) sequence((int[]){ON,AUTH,AUTH,OFF},4);
        else if(path==3) sequence((int[]){ON,AUTH,LOAD,BIOS,OFF},5);
        else sequence((int[]){ON,AUTH,OFF},3);
        assert(errors==(path!=0)+(error!=0));
        if(error) assert(last_error==error);
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"admission")) admission(0);
    else if(!strcmp(argv[1],"healthy")) healthy();
    else if(!strcmp(argv[1],"cleanup")) cleanup();
    else if(!strcmp(argv[1],"retry")) admission(1);
    else assert(0);
    return 0;
}
