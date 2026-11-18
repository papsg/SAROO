/* Adapted from paps-g 3c4823d6: stock menu SCI register contract. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef uint8_t u8;
typedef uint32_t u32;

static volatile u8 test_scr, test_smr, test_brr, test_ssr, test_rdr, test_tdr;
#define SCR test_scr
#define SMR test_smr
#define BRR test_brr
#define SSR test_ssr
#define RDR test_rdr
#define TDR test_tdr

static void (*printk_putc)(int);
static unsigned sleep_calls;
static u32 sleep_duration;
static u8 sleep_scr;

void sci_putc(int ch);

void usleep(u32 duration)
{
    ++sleep_calls;
    sleep_duration = duration;
    sleep_scr = SCR;
}

#include "sci.inc"

static void test_initialization(void)
{
    SCR = 0xff;
    SMR = 0xff;
    BRR = 0xff;
    SSR = 0xfc;
    RDR = 0x5a;
    sleep_calls = 0;
    sleep_duration = 0;
    sleep_scr = 0xff;
    printk_putc = 0;

    sci_init();

    assert(sleep_calls == 1);
    assert(sleep_duration == 5);
    assert(sleep_scr == 0x01);
    assert((sleep_scr & 0xfc) == 0);
    assert(SCR == 0x31);
    assert(SMR == 0);
    assert(BRR == 0);
    assert(SSR == 0x84);
    assert(printk_putc == sci_putc);
}

static void test_receive(void)
{
    SSR = 0x40;
    RDR = 0x2a;
    assert(sci_getc(1) == 0x2a);
    assert(SSR == 0);

    /* On overrun RDR still contains the earlier valid byte. */
    SSR = 0xfc;
    RDR = 0x6b;
    assert(sci_getc(1) == 0x6b);
    assert(SSR == 0x84);

    SSR = 0x38;
    RDR = 0xa5;
    assert(sci_getc(1) == -1);
    assert(SSR == 0);

    SSR = 0x04;
    assert(sci_getc(0) == -1);
    assert(SSR == 0x04);
}

int main(void)
{
    test_initialization();
    test_receive();
    puts("pass");
    return 0;
}
