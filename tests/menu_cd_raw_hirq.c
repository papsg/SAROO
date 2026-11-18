/* Adapted from paps-g 232758b3: production raw-auth transaction fixture. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef struct { uint16_t cr1, cr2, cr3, cr4; } CDCMD;
enum { HIRQ_CMOK=1, HIRQ_DRDY=2, HIRQ_DCHG=0x20,
       HIRQ_EHST=0x80, HIRQ_ECPY=0x100, HIRQ_EFLS=0x200 };

static uint16_t HIRQ, CR1, CR2, CR3, CR4;
static int fail_at, put_full, response_command;
static unsigned response_status;
static unsigned wait_count, clear_count, command_count, data_count;
static int wait_masks[8], clear_masks[5];
static const unsigned commands[4]={0x6400,0x0600,0x6600,0xe000};

static void record_clear(int mask)
{
    assert(clear_count < sizeof clear_masks / sizeof *clear_masks);
    clear_masks[clear_count++]=mask;
    HIRQ=0;
}

static void write_data(unsigned value)
{
    assert(value==0x00020002);
    ++data_count;
}

static int __attribute__((unused)) wait_hirq(int mask)
{
    assert(wait_count < sizeof wait_masks / sizeof *wait_masks);
    wait_masks[wait_count]=mask;
    int command=-1;
    if(mask==HIRQ_CMOK) {
        assert(command_count < sizeof commands / sizeof *commands);
        command=(int)command_count;
        assert(CR1==commands[command_count++]);
    }
    if((int)wait_count++==fail_at) return -3;
    if(command>=0) {
        unsigned status=command==response_command ? response_status : 1;
        CR1=(uint16_t)(status<<8);
        CR2=(uint16_t)(0x1200u + wait_count);
        CR3=(uint16_t)(0x2300u + wait_count);
        CR4=(uint16_t)(0x3400u + wait_count);
    }
    HIRQ=(uint16_t)(put_full && wait_count==2 ? HIRQ_EHST : mask);
    return 0;
}

static void __attribute__((unused)) printk(const char *format, ...)
{
    (void)format;
}

#include "actual.inc"

static void reset_fixture(void)
{
    HIRQ=CR1=CR2=CR3=CR4=0;
    fail_at=response_command=-1;put_full=0;response_status=1;
    wait_count=clear_count=command_count=data_count=0;
    memset(wait_masks,0,sizeof wait_masks);
    memset(clear_masks,0,sizeof clear_masks);
}

static int run_auth(void)
{
    CDCMD response={0};
#if EXPECT_FIXED
    return cdc_auth_hram(&response,2);
#else
    cdc_auth_hram(&response,2);
    return 0;
#endif
}

static void protocol(void)
{
    static const int expected_waits[]={HIRQ_CMOK,HIRQ_DRDY|HIRQ_EHST,
        HIRQ_CMOK,HIRQ_EHST,HIRQ_CMOK,HIRQ_ECPY,HIRQ_CMOK,HIRQ_EFLS};
    static const int expected_clears[]={HIRQ_CMOK|HIRQ_EHST,
        HIRQ_DRDY,HIRQ_CMOK|HIRQ_EHST,HIRQ_CMOK|HIRQ_ECPY,
        HIRQ_CMOK|HIRQ_DCHG|HIRQ_EFLS};
    reset_fixture();
    assert(run_auth()==0);
    assert(wait_count==8 && command_count==4 && clear_count==5);
    assert(data_count==2352u*150u/4u);
    assert(!memcmp(wait_masks,expected_waits,sizeof expected_waits));
    assert(!memcmp(clear_masks,expected_clears,sizeof expected_clears));
}

static void failed_wait(int index)
{
    static const unsigned expected_commands[]={1,1,2,2,3,3,4,4};
    static const unsigned expected_clears[]={1,1,3,3,4,4,5,5};
    reset_fixture();
    fail_at=index;
    assert(run_auth()==-3);
    assert(wait_count==(unsigned)index+1);
    assert(command_count==expected_commands[index]);
    assert(clear_count==expected_clears[index]);
    assert(data_count==(index>1 ? 2352u*150u/4u : 0));
}

static void put_storage_full(void)
{
    reset_fixture();put_full=1;
    assert(run_auth()==-4);
    assert(wait_count==2 && command_count==1 && clear_count==1 && !data_count);
}

static void failed_response(int command,unsigned status,int result)
{
    static const unsigned expected_waits[]={1,3,5,7};
    static const unsigned expected_clears[]={1,3,4,5};
    reset_fixture();response_command=command;response_status=status;
    assert(run_auth()==result);
    assert(wait_count==expected_waits[command]);
    assert(command_count==(unsigned)command+1);
    assert(clear_count==expected_clears[command]);
    assert(data_count==(command ? 2352u*150u/4u : 0));
}

int main(int argc,char **argv)
{
    assert(argc==2);
    if(!strcmp(argv[1],"protocol")) protocol();
    else if(!strncmp(argv[1],"fail-",5)) failed_wait(atoi(argv[1]+5));
    else if(!strcmp(argv[1],"put-full")) put_storage_full();
    else if(!strncmp(argv[1],"reject-",7)) failed_response(atoi(argv[1]+7),0xff,-5);
    else if(!strncmp(argv[1],"wait-",5)) failed_response(atoi(argv[1]+5),0x80,-6);
    else assert(0);
    return 0;
}
