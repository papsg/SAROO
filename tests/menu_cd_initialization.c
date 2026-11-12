/* Adapted from paps-g 56980390. Actual startup code with scripted CD commands. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
enum { STATUS_PAUSE=1, STATUS_OPEN=6, STATUS_NODISC=7, STATUS_RETRY=8 };
enum { ABORT, END, RESET, SIZE, HW };
static unsigned cr1, cr_reads, HIRQ;
static unsigned *read_cr1(void) { cr_reads++; return &cr1; }
#define CR1 (*read_cr1())
#define HIRQ_DCHG 0x20
static u32 bios_state[2];
static char game_id[17]="TEST_GAME";
static int ipstat, use_sys_bup, use_sys_load, disc_type;
static int off_calls, on_calls, auth_calls, read_calls, rom_calls, players;
static int off_result, on_result, player_init_calls;
static int auth_result=2, read_result, rom_result, boot_admitted;
static struct { int op, result; } expected[8];
static unsigned used, next;
static unsigned flags_before_abort;
static int change_during;

static void reset(void)
{
    used=next=0; off_calls=on_calls=auth_calls=read_calls=rom_calls=players=0;
    boot_admitted=0;
    auth_result=2;read_result=rom_result=0;cr1=STATUS_PAUSE<<8;
    off_result=on_result=player_init_calls=0;cr_reads=HIRQ=0;
    bios_state[0]=0x12345678;bios_state[1]=0xabcdef01;
    flags_before_abort=0;
    change_during=-1;
}
static void expect(int op, int result)
{
    assert(used < sizeof expected / sizeof *expected);
    expected[used].op=op;expected[used++].result=result;
}
static int operation(int op)
{
    assert(next<used && expected[next].op==op);
    if(op==ABORT) flags_before_abort=HIRQ;
    if((int)next==change_during) HIRQ|=HIRQ_DCHG;
    return expected[next++].result;
}
static void script(int failure, int error)
{
    static const int ops[] = {ABORT, END, RESET, SIZE, HW};
    for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++) {
        expect(ops[i], (int)i == failure ? error : 0);
        if ((int)i == failure)
            return;
    }
}
static void printk(const char *format, ...) { (void)format; }
static __attribute__((unused)) void clear_hirq(int mask)
{
    assert(mask==HIRQ_DCHG && !next && !bios_state[0] && !bios_state[1]);
    HIRQ&=~mask;
}
static int cdc_abort_file(void) { return operation(ABORT); }
static int cdc_end_trans(int *count) { assert(!count);return operation(END); }
static int cdc_reset_selector(int flags,int number)
{ assert(flags==0xfc && !number);return operation(RESET); }
static int cdc_set_size(int mode) { assert(!mode);return operation(SIZE); }
static int cdc_get_hwinfo(int *status)
{ int result=operation(HW);if(!result && status)*status=0x1234;return result; }
static int cdblock_off(void) { ++off_calls; return off_result; }
static int cdblock_on(int wait) { assert(!wait && off_calls>on_calls);++on_calls;return on_result; }
static int bios_player_init(void) { ++player_init_calls; return 17; }
static int jhl_auth_hack(int delay) { assert(delay==100000);++auth_calls;return auth_result; }
static int my_bios_loadcd_read(void) { ++read_calls;return read_result; }
static void my_cdplayer(void) { ++players; }
static int prepare_game_rom(const char *id) { assert(id==game_id);++rom_calls;return rom_result; }
#include "actual.inc"

static void initialization(int failures)
{
    const int errors[] = {-3, -5, -6};
    for (int stage = failures ? 0 : -1; stage < (failures ? 5 : 0); stage++) {
        for (unsigned e = 0; e < (failures ? 3u : 1u); e++) {
            reset();
            script(stage, errors[e]);
            assert(my_bios_loadcd_init() == (failures ? errors[e] : 0));
            assert(next == used && !bios_state[0] && !bios_state[1]);
        }
    }
}
static void boot(void)
{
    const int types[]={0,2,4,0x80,0x82,0x84},errors[]={-3,-5,-6};
    for(unsigned t=0;t<6;++t) for(int stage=0;stage<5;++stage) for(unsigned e=0;e<3;++e) {
        reset();script(stage,errors[e]);
        assert(bios_cd_cmd(types[t])==errors[e] && next==used);
        assert(!read_calls && !rom_calls && !players && !boot_admitted);
    }
    for(unsigned t=0;t<6;++t) for(int read=0;read<3;++read) {
        reset();script(-1,0);read_result=-read;
        assert(bios_cd_cmd(types[t])==-read && next==used);
        assert(read_calls==1 && use_sys_bup==!!(types[t]&0x80));
        assert(boot_admitted==!read && rom_calls==!read && players==(read==2));
        if(read==2) assert(use_sys_load==!!(types[t]&0x7f));
    }
}
static void auth_errors(void)
{
    const int errors[]={-3,-5,-6};
    for(int stage=0;stage<5;++stage) for(unsigned e=0;e<3;++e) {
        reset();ipstat=1;script(stage,errors[e]);
        assert(cdp_auth()==-1 && !ipstat && next==used);
        assert(!auth_calls && !read_calls && !off_calls);
        assert(cdp_auth()==1 && ipstat==1 && on_calls==1 && off_calls==1);
    }
}
static void auth_healthy(void)
{
    reset();ipstat=-1;
    assert(cdp_auth()==1 && ipstat==1 && on_calls==1 && off_calls==1);
    script(-1,0);assert(cdp_auth()==1 && ipstat==2 && next==used);
    assert(cdp_auth()==1 && ipstat==3);
    assert(cdp_auth()==1 && ipstat==12 && auth_calls==1);
    assert(cdp_auth()==0 && !ipstat && read_calls==1);
    reset();ipstat=3;auth_result=3;
    for(int i=0;i<8;++i) assert(cdp_auth()==1 && ipstat==4+i && auth_calls==i+1);
    assert(cdp_auth()==-1 && !ipstat && auth_calls==8);
    reset();ipstat=12;read_result=-1;
    for(int i=0;i<4;++i) assert(cdp_auth()==1 && ipstat==13+i && read_calls==i+1);
    assert(cdp_auth()==-2 && !ipstat && read_calls==4);
    for(int status=0;status<6;++status) if(status!=STATUS_PAUSE) {
        reset();ipstat=1;CR1=status<<8;
        assert(cdp_auth()==1 && ipstat==1 && !next && !auth_calls && !read_calls);
    }
}
static void media(void)
{
    for(int state=-1;state<=16;++state) for(int status=STATUS_OPEN;status<=STATUS_NODISC;++status) {
        reset();ipstat=state;CR1=status<<8;
        assert(cdp_auth()==-2 && !ipstat && !next && !off_calls && !auth_calls && !read_calls);
        CR1=STATUS_PAUSE<<8;
        assert(cdp_auth()==1 && ipstat==1 && off_calls==1 && on_calls==1 && !read_calls);
    }
}

static void power_retry(void)
{
    const unsigned stale[] = {STATUS_OPEN, STATUS_NODISC, STATUS_RETRY, STATUS_PAUSE};
    for (unsigned i = 0; i < sizeof stale / sizeof *stale; i++) {
        for (int fail_on = 0; fail_on < 2; fail_on++) {
            reset();
            ipstat = 0;
            disc_type = 4;
            off_result = fail_on ? 0 : -3;
            on_result = fail_on ? -1 : 0;
            assert(cdp_auth() == -1 && ipstat == -2);
            assert(off_calls == 1 && on_calls == fail_on && cr_reads == 1);
            cr1 = stale[i] << 8;
            HIRQ = HIRQ_DCHG;
            assert(!cdp_init() && ipstat == -2 && cr_reads == 1);
            off_result = on_result = 0;
            assert(cdp_auth() == 1 && ipstat == 1 && cr_reads == 1);
            assert(off_calls == 2 && on_calls == fail_on + 1 && !auth_calls && !read_calls);
        }
    }
    reset();
    disc_type = 0;
    ipstat = -2;
    assert(cdp_init() == 17 && player_init_calls == 1);
}

static void virtual_power(void)
{
    for (int type = 0; type <= 0x80; type += 0x80) {
        reset();
        off_result = -3;
        use_sys_bup = 3;
        use_sys_load = 4;
        disc_type = 2;
        assert(bios_cd_cmd(type) == -3 && off_calls == 1 && !next && !boot_admitted);
        assert(use_sys_bup == 3 && use_sys_load == 4 && disc_type == 2);
        assert(bios_state[0] == 0x12345678 && bios_state[1] == 0xabcdef01);
    }
}

/* Media notification cases adapted from paps-g 200a447b. */
static void old_disc_change(void)
{
    for (unsigned flags=0; flags<=UINT16_MAX; flags++) {
        reset();
        script(-1,0);
        HIRQ=flags;
        assert(!my_bios_loadcd_init() && next==used);
        assert(flags_before_abort==(flags & ~HIRQ_DCHG));
        assert(HIRQ==flags_before_abort);
    }
}

static void later_disc_change(void)
{
    const int errors[]={0,-2,-3,-5,-6};
    for (int stage=0; stage<5; stage++) {
        for (unsigned e=0; e<sizeof errors/sizeof *errors; e++) {
            reset();
            script(errors[e] ? stage : -1,errors[e]);
            HIRQ=0x0251;
            change_during=stage;
            assert(my_bios_loadcd_init()==errors[e] && next==used);
            assert(flags_before_abort==0x0251 && HIRQ==(0x0251 | HIRQ_DCHG));
        }
    }
}

int main(int argc,char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"healthy")) initialization(0);
    else if(!strcmp(argv[1],"init-errors")) initialization(1);
    else if(!strcmp(argv[1],"boot")) boot();
    else if(!strcmp(argv[1],"auth-errors")) auth_errors();
    else if(!strcmp(argv[1],"auth-healthy")) auth_healthy();
    else if(!strcmp(argv[1],"media")) media();
    else if(!strcmp(argv[1],"power-retry")) power_retry();
    else if(!strcmp(argv[1],"virtual-power")) virtual_power();
    else if(!strcmp(argv[1],"old-disc-change")) old_disc_change();
    else if(!strcmp(argv[1],"later-disc-change")) later_disc_change();
    else assert(0);
    return 0;
}
