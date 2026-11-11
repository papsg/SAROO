/* Adapted from paps-g d3d124d3. Full boot caller with modeled BIOS boundaries. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef struct { u16 cr1, cr2, cr3, cr4; } CDCMD;
#include "constants.inc"
enum { INIT, IP, ROM, BIOS_READ, SAVE, DIRECTORY, FILE_READ, PAD_SUSPEND, BOOT, PAD_RESUME, PLAYER };
static _Alignas(4) u8 high[0x9000], low[0x1000], temporary[64];
static u8 before_high[sizeof high], before_low[sizeof low], before_tmp[sizeof temporary];
#define TMPBUFF_ADDR ((uintptr_t)temporary)
static int use_sys_bup, use_sys_load, disc_type;
static int failure, error, enforce_publication, expected_type;
static int calls[16], call_count, pads_suspended;
static const int types[]={0,2,4,0x80,0x82,0x84};
static const int errors[]={-3,-5,-6};

static void *memory_at(u32 address)
{
    if(address>=0x06000000 && address<0x06000000+sizeof high)
        return high+(address-0x06000000);
    assert(address>=0x02000000 && address<0x02000000+sizeof low);
    return low+(address-0x02000000);
}
static void snapshot(void)
{
    memcpy(before_high,high,sizeof high);
    memcpy(before_low,low,sizeof low);
    memcpy(before_tmp,temporary,sizeof temporary);
}
static void unchanged(void)
{
    assert(!memcmp(before_high,high,sizeof high));
    assert(!memcmp(before_low,low,sizeof low));
    assert(!memcmp(before_tmp,temporary,sizeof temporary));
}
static int operation(int op)
{
    assert(call_count<(int)(sizeof calls/sizeof *calls));
    calls[call_count++]=op;
    return op==failure ? error : 0;
}
static void reset(int type, int fail, int result)
{
    for(unsigned i=0;i<sizeof high;++i) high[i]=(i*37+13)&255;
    for(unsigned i=0;i<sizeof low;++i) low[i]=(i*19+31)&255;
    memset(temporary,0x3c,sizeof temporary);
    expected_type=type; failure=fail; error=result;
    call_count=0; enforce_publication=0; pads_suspended=0;
    use_sys_bup=use_sys_load=disc_type=-1;
    snapshot();
}
static void printk(const char *format, ...) { (void)format; }
static int my_bios_loadcd_init(void) { return operation(INIT); }
static int my_bios_loadcd_read(void) { return operation(IP); }
static int prepare_game_rom(const char *id)
{ assert(id==memory_at(0x06002020));return operation(ROM); }
static void my_cdplayer(void) { operation(PLAYER); }
static void read_1st(void) { assert(0); }
static int cdc_read_sector(int fad, int size, u8 *buffer)
{
    (void)fad; (void)size; (void)buffer;
    abort();
}
static int bios_loadcd_read(void)
{
    assert(*(u32*)memory_at(0x06000290)==3);
    operation(BIOS_READ);
    snapshot();
    return 0x8000; /* Boundary assumption, not a proof of ROM return semantics. */
}
static int save_command(int command, int argument)
{
    assert(command==SSCMD_LSAVE && !argument);
    assert(!memcmp(temporary+0x10,high+0x2020,16) && !temporary[0x20]);
    snapshot();
    return operation(SAVE);
}
static int cdc_cmd(int wait, CDCMD *cmd, CDCMD *response, char *name)
{
    (void)response;
    assert(cmd->cr2==0);
    int op;
    if(cmd->cr1==0x7000) {
        assert(wait==HIRQ_EFLS && cmd->cr3==0xff && cmd->cr4==0xffff);
        assert(!strcmp(name,"cdc_change_dir")); op=DIRECTORY;
    } else {
        assert(cmd->cr1==0x7400 && !wait && !cmd->cr3 && cmd->cr4==2);
        assert(!strcmp(name,"cdc_read_file")); op=FILE_READ;
    }
    if(enforce_publication) unchanged();
    return operation(op);
}
static void expected_published(void)
{
    memcpy(before_high+0x2a0,before_high+0x20e0,32);
    memcpy(before_high+0xc00,before_high+0x2000,256);
    u32 value=0x6002100;
    memcpy(before_high+0x254,&value,4);
    value=(u32)(uintptr_t)read_1st;
    memcpy(before_high+0x2270,&value,4);
    value=(u32)(uintptr_t)cdc_read_sector;
    memcpy(before_low+0xf04,&value,4);
    u16 instruction=9;
    memcpy(before_high+0x220c,&instruction,2);
    unchanged();
}
static void pad_suspend(void)
{
    assert(!pads_suspended);
    pads_suspended = 1;
    operation(PAD_SUSPEND);
}
static void pad_resume(void)
{
    assert(pads_suspended);
    pads_suspended = 0;
    operation(PAD_RESUME);
}
static int bios_boot(void)
{
    assert(pads_suspended);
    expected_published();
    operation(BOOT);
    return 37; /* Preserve a nonzero BIOS result without inventing its meaning. */
}
#include "actual.inc"

static void sequence(int stop)
{
    int expected[12], n=0;
    for(int op=INIT;op<=PAD_RESUME;++op) {
        if(op==SAVE && (!(expected_type&0x7f) || (expected_type&0x80))) continue;
        expected[n++]=op;
        if(op==stop) break;
    }
    if(stop==IP && error==-2) expected[n++]=PLAYER;
    assert(call_count==n && !memcmp(calls,expected,n*sizeof *expected));
    assert(!pads_suspended);
    assert(use_sys_bup==!!(expected_type&0x80) && disc_type==(expected_type&0x7f));
    assert(use_sys_load==(stop==IP && error==-2 ? !!(expected_type&0x7f) : 0));
}
static void healthy(int publication)
{
    for(unsigned t=0;t<sizeof types/sizeof *types;++t) {
        reset(types[t],-1,0);enforce_publication=publication;
        assert(bios_cd_cmd(types[t])==37);sequence(PAD_RESUME);
    }
}
static void early(void)
{
    for(unsigned t=0;t<sizeof types/sizeof *types;++t)
    for(int stage=INIT;stage<=SAVE;++stage) {
        if(stage==BIOS_READ || (stage==SAVE && (!(types[t]&0x7f) || (types[t]&0x80)))) continue;
        for(int e=-1;e>=-6;--e) {
            reset(types[t],stage,e);
            assert(bios_cd_cmd(types[t])==(stage==SAVE ? -1 : e));
            sequence(stage);unchanged();
        }
    }
}
static void late(int retry)
{
    for(unsigned t=0;t<sizeof types/sizeof *types;++t)
    for(int stage=DIRECTORY;stage<=FILE_READ;++stage)
    for(unsigned e=0;e<sizeof errors/sizeof *errors;++e) {
        reset(types[t],stage,errors[e]);
        assert(bios_cd_cmd(types[t])==errors[e]);sequence(stage);unchanged();
        if(retry) {
            failure=-1;call_count=0;enforce_publication=1;
            assert(bios_cd_cmd(types[t])==37);sequence(PAD_RESUME);
        }
    }
}
int main(int argc, char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"healthy")) healthy(0);
    else if(!strcmp(argv[1],"early")) early();
    else if(!strcmp(argv[1],"late")) late(0);
    else if(!strcmp(argv[1],"publication")) healthy(1);
    else if(!strcmp(argv[1],"retry")) late(1);
    else assert(0);
    return 0;
}
