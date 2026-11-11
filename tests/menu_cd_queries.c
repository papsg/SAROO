/* Adapted from paps-g 544f2b10. Actual menu code with scripted CD responses. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef struct { uint16_t cr1, cr2, cr3, cr4; } CDCMD;
enum { HIRQ_ESEL=0x40, HIRQ_EFLS=0x200, STATUS_PAUSE=1, STATUS_SEEK=4, STATUS_OPEN=6, STATUS_NODISC=7,
       STATUS_RETRY=8, STATUS_ERROR=9, STATUS_FATAL=10 };
static unsigned CR1;
static u8 cdc_auth_hram[0x400], auth_copy[0x400];
struct step { unsigned command; int result; CDCMD reply; };
static struct step steps[100];
static unsigned step_count, position, go_count, wait_count, print_count;

static void reset(void)
{
    step_count=position=go_count=wait_count=print_count=0;
    CR1=STATUS_PAUSE<<8;
    memset(auth_copy, 0, sizeof auth_copy);
    memset(cdc_auth_hram, 0x5a, sizeof cdc_auth_hram);
}

static void expect(unsigned command, int result, unsigned cr1, unsigned cr2,
                   unsigned cr3, unsigned cr4)
{
    assert(step_count < sizeof steps / sizeof *steps);
    steps[step_count++]=(struct step){command,result,{cr1,cr2,cr3,cr4}};
}

static int cdc_cmd(int wait, CDCMD *cmd, CDCMD *resp, char *name)
{
    (void)wait; (void)name;
    assert(position < step_count);
    struct step *step=&steps[position++];
    assert(cmd->cr1==step->command);
    /* Deliberately fill even failed replies: callers cannot use those values. */
    *resp=step->reply;
    return step->result;
}

static void clear_hirq(int mask) { (void)mask; }
static int wait_hirq(int mask)
{
    assert(mask==HIRQ_EFLS);
    ++wait_count;
    return 0;
}
static void printk(const char *format, ...)
{
    (void)format;
    ++print_count;
}
static void auth_go(CDCMD *resp, int delay)
{
    (void)resp;
    assert(delay==100000);
    assert(!memcmp(cdc_auth_hram,auth_copy,sizeof auth_copy));
    ++go_count;
}

#include "actual.inc"

static int query(unsigned n, int *a, int *b, int *c)
{
    switch(n) {
    case 0: return cdc_get_status(a);
    case 1: return cdc_get_hwinfo(a);
    case 2: return cdc_get_cddev(a);
    case 3: return cdc_get_filter_range(23,a,b);
    case 4: return cdc_get_filter_mode(23,a);
    case 5: return cdc_get_filter_connect(23,a,b);
    case 6: return cdc_get_buffer_size(a,b,c);
    case 7: return cdc_get_numsector(23,a);
    case 8: return cdc_get_actsize(a);
    case 9: return cdc_get_sector_info(23,199,a);
    case 10: return cdc_auth_status(a);
    case 11: return cdc_get_file_scope(a,b,c);
    default: assert(0); return -99;
    }
}

static const unsigned query_commands[]={0,0x100,0x3100,0x4100,0x4500,0x4700,
                                       0x5000,0x5100,0x5300,0x5400,0xe100,0x7200};
static const int errors[]={-3,-5,-6};

static void test_errors(void)
{
    for(unsigned n=0;n<12;++n) for(unsigned e=0;e<3;++e) {
        reset();
        int a=0x11223344,b=0x55667788,c=0x12345678;
        expect(query_commands[n],errors[e],0x1256,0xabcd,0x3456,0xcdef);
        assert(query(n,&a,&b,&c)==errors[e]);
        assert(a==0x11223344 && b==0x55667788 && c==0x12345678);
        assert(position==step_count);
    }
}

static void test_success(void)
{
    for(unsigned n=0;n<12;++n) for(unsigned byte=0;byte<256;++byte) {
        reset();
        int a=-1,b=-1,c=-1;
        unsigned word=byte*257, cr1=0x1200|byte, cr3=0x3400|byte;
        expect(query_commands[n],0,cr1,word,cr3,word);
        assert(!query(n,&a,&b,&c));
        switch(n) {
        case 0: case 1: assert(a==0x12); break;
        case 2: assert(a==0x34); break;
        case 3: assert(a==(int)((byte<<16)|word) && b==a); break;
        case 4: assert(a==(int)byte); break;
        case 5: assert(a==(int)byte && b==a); break;
        case 6: assert(a==(int)word && b==0x34 && c==a); break;
        case 7: case 10: assert(a==(int)word); break;
        case 8: case 9: assert(a==(int)((byte<<16)|word)); break;
        case 11: assert(a==(int)((byte<<16)|word) && b==(int)word && c==0x34); break;
        }
        assert(position==step_count);
    }
    for(unsigned n=0;n<2;++n) for(unsigned e=0;e<4;++e) {
        reset(); int result=e?errors[e-1]:0;
        expect(query_commands[n],result,0x1256,0,0,0);
        assert(query(n,NULL,NULL,NULL)==result);
    }
}

static void test_auth(void)
{
    for(unsigned e=0;e<3;++e) {
        reset(); expect(0x6003,errors[e],0,0,0,0);
        assert(jhl_auth_hack(100000)==errors[e]);
        assert(!go_count && position==1);
        reset(); expect(0x6003,0,0,0,0,0);
        expect(0,errors[e],STATUS_PAUSE<<8,0,0,0);
        expect(0x6000,0,0,0,0,0);
        assert(jhl_auth_hack(100000)==errors[e]);
        assert(go_count==1 && position==3 && print_count==0);
    }
    for(unsigned result=0;result<10;++result) {
        reset(); expect(0x6003,0,0,0,0,0);
        expect(0,0,STATUS_SEEK<<8,0,0,0);
        expect(0,0,STATUS_PAUSE<<8,0,0,0);
        expect(0xe100,0,0,result,0,0); expect(0x6000,0,0,0,0,0);
        assert(jhl_auth_hack(100000)==(int)result);
        assert(go_count==1 && position==5 && print_count==3);
    }
    for(unsigned status=STATUS_OPEN;status<16;++status) {
        reset(); CR1=status<<8;
        assert(jhl_auth_hack(100000)==(int)status);
        assert(!position && !go_count);
    }
}

static void test_restore(void)
{
    for(unsigned e=0;e<3;++e) for(unsigned cleanup=0;cleanup<4;++cleanup) {
        int last=cleanup?errors[cleanup-1]:0;
        reset(); expect(0x6003,0,0,0,0,0);
        expect(0,0,STATUS_PAUSE<<8,0,0,0);
        expect(0xe100,errors[e],0,4,0,0); expect(0x6000,last,0,0,0,0);
        assert(jhl_auth_hack(100000)==errors[e]);
        assert(position==4 && print_count==1);
        reset(); expect(0x6003,0,0,0,0,0);
        expect(0,0,STATUS_PAUSE<<8,0,0,0);
        expect(0xe100,0,0,4,0,0); expect(0x6000,last,0,0,0,0);
        assert(jhl_auth_hack(100000)==(last?last:4));
        assert(position==4);
        for(unsigned status=STATUS_OPEN;status<=STATUS_NODISC;++status) {
            reset(); expect(0x6003,0,0,0,0,0);
            expect(0,0,status<<8,0,0,0); expect(0x6000,last,0,0,0,0);
            assert(jhl_auth_hack(100000)==-1);
            assert(position==3);
        }
    }
}

static void test_device(void)
{
    for(unsigned e=0;e<3;++e) {
        reset(); expect(0xe000,errors[e],0,0,0,0);
        assert(cdc_auth_device()==errors[e]);
        assert(!wait_count && !print_count && position==1);
        reset(); expect(0xe000,0,0,0,0,0);
        expect(0xe100,errors[e],0,4,0,0);
        assert(cdc_auth_device()==errors[e]);
        assert(wait_count==1 && !print_count && position==2);
    }
    reset(); expect(0xe000,0,0,0,0,0); expect(0xe100,0,0,4,0,0);
    assert(cdc_auth_device()==4 && print_count==1 && wait_count==1);
}

static void test_terminal_auth(void)
{
    for (unsigned flags = 0; flags < 0x80; flags += 0x10) {
        for (unsigned cleanup = 0; cleanup < 4; ++cleanup) {
            int cleanup_error = cleanup ? errors[cleanup - 1] : 0;
            reset();
            expect(0x6003, 0, 0, 0, 0, 0);
            expect(0, 0, (flags | STATUS_ERROR) << 8, 0, 0, 0);
            expect(0x6000, cleanup_error, 0, 0, 0, 0);
            assert(jhl_auth_hack(100000) == -1);
            assert(position == step_count && go_count == 1);

            reset();
            expect(0x6003, 0, 0, 0, 0, 0);
            expect(0, 0, (flags | STATUS_RETRY) << 8, 0, 0, 0);
            expect(0, 0, (flags | STATUS_FATAL) << 8, 0, 0, 0);
            expect(0xe100, 0, 0, 2, 0, 0);
            expect(0x6000, cleanup_error, 0, 0, 0, 0);
            assert(jhl_auth_hack(100000) == (cleanup_error ? cleanup_error : 2));
            assert(position == step_count && go_count == 1);
        }
    }
}

int main(int argc, char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"errors")) test_errors();
    else if(!strcmp(argv[1],"success")) test_success();
    else if(!strcmp(argv[1],"auth")) test_auth();
    else if(!strcmp(argv[1],"restore")) test_restore();
    else if(!strcmp(argv[1],"device")) test_device();
    else if(!strcmp(argv[1],"terminal-auth")) test_terminal_auth();
    else assert(0);
    return 0;
}
